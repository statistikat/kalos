// The independently selected stock RSAfull descriptions are the inputs here.
// Refit every numerical estimate, then check native extraction and calendar restoration.
#include "kalos/tramo_seats_pipeline.hpp"
#include "kalos/tramo_seats_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using kalos::seats_component;
using kalos::seats_information;
struct Reference {
    std::string id;
    int year = 0, month = 0, effective = 0, parameter_count = 0;
    bool log = false, mean = false, changed = false;
    kalos::TramoOrders orders{};
    kalos::TramoStockCalendar calendar;
    std::vector<std::pair<int, kalos::TramoOutlierKind>> outliers;
    std::vector<double> series, parameters, coefficients, likelihood, residuals;
    std::vector<kalos::seats_component_series> finals;
};

std::vector<double> read_values(std::istream& in) {
    std::vector<double> values;
    std::uint64_t raw;
    while (in >> std::hex >> raw) {
        double x;
        std::memcpy(&x, &raw, sizeof(x));
        values.push_back(x);
    }
    return values;
}

void check(bool value, const std::string& detail) {
    if (!value) throw std::runtime_error(detail);
}

void numbers(const std::vector<double>& actual, const std::vector<double>& expected, const std::string& label) {
    check(actual.size() == expected.size(), label + " size");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (std::memcmp(&actual[i], &expected[i], sizeof(double))) {
            std::ostringstream msg;
            msg << label << " at " << i << " got " << std::hexfloat << actual[i] << " expected " << expected[i];
            throw std::runtime_error(msg.str());
        }
    }
}

const std::map<std::string, seats_component> components{{"Series", seats_component::series},
    {"Trend", seats_component::trend}, {"Seasonal", seats_component::seasonal},
    {"SeasonallyAdjusted", seats_component::seasonally_adjusted}, {"Irregular", seats_component::irregular}};
const std::map<std::string, seats_information> information{{"Value", seats_information::value},
    {"Forecast", seats_information::forecast}, {"Stdev", seats_information::stdev},
    {"StdevForecast", seats_information::stdev_forecast}};

kalos::seats_component_series read_component(std::istream& in) {
    kalos::seats_component_series item{};
    std::string name, kind;
    int n;
    in >> name >> kind >> item.start_year >> item.start_month >> n;
    item.component = components.at(name);
    item.information = information.at(kind);
    item.values = read_values(in);
    check(n == static_cast<int>(item.values.size()), "reference component count");
    return item;
}

struct WkReference {
    std::string id;
    std::vector<double> adjusted;
    std::vector<kalos::seats_component_series> components;
};

WkReference read_wk_reference(const char* path) {
    std::ifstream input(path);
    WkReference wk;
    for (std::string line; std::getline(input, line);) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "case") in >> wk.id;
        else if (tag == "bridge") {
            int year, month, length;
            in >> year >> month >> length;
            wk.adjusted = read_values(in);
            check(length == int(wk.adjusted.size()), "WK input length");
        } else if (tag == "wk") wk.components.push_back(read_component(in));
        else throw std::runtime_error("unknown WK reference record");
    }
    check(!wk.id.empty() && !wk.adjusted.empty() && !wk.components.empty(), "missing WK reference");
    return wk;
}

std::map<std::string, std::vector<double>> read_forecast_errors() {
    std::ifstream input(KALOS_SELECTED_FORECAST_PANEL);
    std::map<std::string, std::vector<double>> result;
    std::string id;
    for (std::string line; std::getline(input, line);) {
        std::istringstream row(line);
        std::string tag;
        row >> tag;
        if (tag == "case") row >> id;
        if (tag == "y_ef") {
            int year, month, length;
            row >> year >> month >> length;
            auto expected = read_values(row);
            check(length == 24 && expected.size() == 24, id + " public forecast count");
            check(result.emplace(id, std::move(expected)).second, id + " duplicate forecast case");
        }
    }
    check(result.size() == 26, "incomplete public forecast reference");
    return result;
}

void compare_fit(const Reference& ref, const kalos::TramoFitResult& fit) {
    check(fit.log == ref.log && fit.mean == ref.mean, "transformation or mean");
    const auto& o = fit.orders;
    const auto& r = ref.orders;
    check(o.p == r.p && o.d == r.d && o.q == r.q && o.bp == r.bp && o.bd == r.bd && o.bq == r.bq,
          "SARIMA orders");
    check(fit.calendar.selection.trading_days == ref.calendar.trading_days &&
          fit.calendar.selection.leap_year == ref.calendar.leap_year &&
          fit.calendar.selection.easter == ref.calendar.easter, "calendar selection");
    check(fit.variables.size() == ref.outliers.size(), "outlier count");
    for (std::size_t i = 0; i < fit.variables.size(); ++i)
        check(fit.variables[i].position == ref.outliers[i].first && fit.variables[i].type == ref.outliers[i].second,
              "outlier identity");
    numbers(fit.parameters, ref.parameters, "ARIMA");
    numbers(fit.regression_coefficients, ref.coefficients, "regression coefficients");
    numbers({fit.ls_ll, fit.ls_tadj, fit.ls_all, fit.ls_ssq}, ref.likelihood, "likelihood");
    check(fit.ls_eff == ref.effective && fit.ls_npar == ref.parameter_count, "likelihood counts");
    numbers(fit.full_residuals, ref.residuals, "residuals");
}

void compare(const Reference& ref, const WkReference& wk, const std::vector<double>& expected_errors, double& worst_error, bool automatic) {
    if (ref.finals.empty()) {
        auto fit = kalos::estimate_tramo_trfull_reference(ref.series, ref.year, ref.month);
        check(fit.reference_model_computed && !fit.final_model_computed, "reference status");
        compare_fit(ref, fit);
        return;
    }
    kalos::tramo_seats_automatic_result run;
    if (automatic) {
        run = kalos::run_tramo_seats_rsafull(ref.series, ref.year, ref.month, 0, 24);
    } else {
        run.preprocessing = kalos::refit_tramo_selected_model(ref.series, ref.year, ref.month, ref.orders,
            ref.log, ref.mean, ref.calendar, ref.outliers);
        run.decomposition = kalos::run_tramo_seats_pipeline(run.preprocessing, 0, 24);
    }
    const auto& fit = run.preprocessing;
    const auto& result = run.decomposition;
    check(fit.status == "available" && fit.final_model_computed, "fit status");
    compare_fit(ref, fit);
    if (!expected_errors.empty()) for (int horizon : {0, 1, 7, 24}) {
        auto forecast = kalos::tramo_selected_forecast_errors(fit, horizon);
        check(forecast.status == (horizon ? "available" : "no_requested_predictions") &&
              forecast.standard_errors.size() == static_cast<std::size_t>(horizon), "selected model forecast status");
        for (int h = 0; h < horizon; ++h) {
            const double error = std::abs(forecast.standard_errors[h] - expected_errors[h]) / (1 + expected_errors[h]);
            check(error <= 2e-8, "selected model forecast error at " + std::to_string(h + 1));
            worst_error = std::max(worst_error, error);
        }
    }
    check(result.extraction.model_changed == ref.changed, "approximation flag");
    // The higher-order WK extraction has an independent regression in progress.
    // Its public provider output isolates calendar restoration on that same fit.
    auto finals = result.finals;
    if (ref.id == wk.id) {
        numbers(kalos::make_tramo_seats_model(fit).series, wk.adjusted, "WK input");
        finals = kalos::merge_tramo_seats_components(fit, wk.components);
    }
    check(finals.size() == ref.finals.size(), "product size");
    for (std::size_t i = 0; i < finals.size(); ++i) {
        const auto& r = finals[i];
        const auto& e = ref.finals[i];
        check(r.component == e.component && r.information == e.information && r.start_year == e.start_year &&
              r.start_month == e.start_month, "product domain");
        numbers(r.values, e.values, "product row " + std::to_string(i));
    }
}
}

int main(int argc, char** argv) {
    const std::vector<double> positive(48, 4.0);
    try {
        kalos::refit_tramo_selected_model(positive, 1990, 0, {}, false, false, {}, {});
        return 1;
    } catch (const std::invalid_argument&) {}
    try {
        kalos::refit_tramo_selected_model(positive, 1990, 1, {}, false, false, {},
                                         {{48, kalos::TramoOutlierKind::Ao}});
        return 1;
    } catch (const std::invalid_argument&) {}
    const auto wk = read_wk_reference(argc == 3 ? KALOS_LATE_WK_PANEL : KALOS_SELECTED_WK_PANEL);
    const auto forecasts = read_forecast_errors();
    double worst_error = 0.0;
    const bool automatic = argc >= 2 && std::string(argv[1]) == "automatic";
    const char* panel = argc == 3 ? argv[2] : (argc == 2 && !automatic ? argv[1] : KALOS_SELECTED_CALENDAR_PANEL);
    std::ifstream stream(panel);
    Reference ref;
    int cases = 0, failures = 0, rows = 0;
    std::size_t values = 0, residuals = 0;
    const auto replay = [&] {
        if (ref.id.empty()) return;
        try {
            compare(ref, wk, argc == 3 ? std::vector<double>{} : forecasts.at(ref.id), worst_error, automatic);
        } catch (const std::exception& error) {
            std::cerr << ref.id << ": " << error.what() << "\n";
            ++failures;
        }
        ++cases;
        residuals += ref.residuals.size();
        rows += static_cast<int>(ref.finals.size());
        for (const auto& row : ref.finals) values += row.values.size();
    };
    for (std::string line; std::getline(stream,line);) {
        std::istringstream in(line);
        std::string tag, word;
        in >> tag >> std::boolalpha;
        if (tag == "case") {
            replay();
            ref = {};
            in >> ref.id;
        } else if (tag == "input") {
            int n;
            in >> ref.year >> ref.month >> n;
            ref.series = read_values(in);
            check(int(ref.series.size()) == n, "input length");
        } else if (tag == "model") {
            auto& o = ref.orders;
            in >> word >> ref.log >> word >> ref.mean >> word >> o.p >> o.d >> o.q >> o.bp >> o.bd >> o.bq;
        } else if (tag == "parameters") {
            ref.parameters = read_values(in);
        } else if (tag == "variable") {
            in >> word;
            if (word == "td") ref.calendar.trading_days = kalos::TramoTradingDays::WorkingDays;
            else if (word.rfind("td[",0)==0) ref.calendar.trading_days = kalos::TramoTradingDays::TradingDays;
            else if (word == "lp") ref.calendar.leap_year = true;
            else if (word == "easter") ref.calendar.easter = true;
            else if (word != "const") {
                const std::map<std::string,kalos::TramoOutlierKind> types{{"AO",kalos::TramoOutlierKind::Ao},
                    {"LS",kalos::TramoOutlierKind::Ls},{"TC",kalos::TramoOutlierKind::Tc}};
                std::string date;
                in >> date;
                const int index = (std::stoi(date.substr(1,4))-ref.year)*12 + std::stoi(date.substr(6,2))-ref.month;
                ref.outliers.emplace_back(index, types.at(word));
            }
            ref.coefficients.push_back(read_values(in).at(0));
        } else if (tag == "likelihood") {
            std::ostringstream values;
            for (int i = 0; i < 4; ++i) { in >> word; values << word << ' '; }
            std::istringstream numbers(values.str());
            ref.likelihood = read_values(numbers);
            in >> ref.effective >> ref.parameter_count;
        } else if (tag == "residuals") {
            int year, month, n;
            in >> year >> month >> n;
            ref.residuals = read_values(in);
            check(n == int(ref.residuals.size()), "reference residual count");
        } else if (tag == "product") {
            in >> word;
            check(word == "present", "missing reference product");
            in >> word >> ref.changed;
        } else if (tag == "merged") {
            ref.finals.push_back(read_component(in));
        } else throw std::runtime_error("unknown reference row " + tag);
    }
    replay();
    std::cout << cases << (automatic ? " automatic RSAfull models, " : " selected RSAfull models, ") << residuals << " residuals, "
              << rows << " rows, " << values << " component values\n";
    if (argc == 3 && automatic) return cases == 3 && residuals == 251 && rows == 30 && values == 1800 && !failures ? 0 : 1;
    if (argc == 2 && !automatic) return cases == 26 && residuals == 3934 && !failures && rows == 0 ? 0 : 1;
    std::cout << "maximum scaled original-series forecast error " << worst_error << "\n";
    return cases == 26 && residuals == 3914 && rows == 260 && values == 24360 && !failures ? 0 : 1;
}
