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

WkReference read_wk_reference() {
    std::ifstream input(KALOS_SELECTED_WK_PANEL);
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

void compare(const Reference& ref, const WkReference& wk) {
    auto fit = kalos::refit_tramo_selected_model(ref.series, ref.year, ref.month, ref.orders, ref.log, ref.mean,
                                                 ref.calendar, ref.outliers);
    check(fit.status == "available" && fit.final_model_computed, "fit status");
    numbers(fit.parameters, ref.parameters, "ARIMA");
    numbers(fit.regression_coefficients, ref.coefficients, "regression coefficients");
    numbers({fit.ls_ll, fit.ls_tadj, fit.ls_all, fit.ls_ssq}, ref.likelihood, "likelihood");
    check(fit.ls_eff == ref.effective && fit.ls_npar == ref.parameter_count, "likelihood counts");
    numbers(fit.full_residuals, ref.residuals, "residuals");
    const auto result = kalos::run_tramo_seats_pipeline(fit, 0, 24);
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

int main() {
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
    const auto wk = read_wk_reference();
    std::ifstream stream(KALOS_SELECTED_CALENDAR_PANEL);
    Reference ref;
    int cases = 0, failures = 0, rows = 0;
    std::size_t values = 0, residuals = 0;
    const auto replay = [&] {
        if (ref.id.empty()) return;
        try {
            compare(ref, wk);
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
    std::cout << cases << " selected RSAfull models, " << residuals << " residuals, "
              << rows << " rows, " << values << " component values\n";
    return cases == 26 && residuals == 3914 && rows == 260 && values == 24360 && !failures ? 0 : 1;
}
