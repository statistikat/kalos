// Raw public preset comparisons. Native calls receive only dated observations.
#include "kalos/tramo_seats_pipeline.hpp"
#include "kalos/preparation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {

void check(bool ok, const std::string& field) {
    if (!ok) throw std::runtime_error(field);
}

double value(std::istream& row) {
    std::string word;
    row >> word;
    const auto bits = static_cast<std::uint64_t>(std::stoull(word, nullptr, 16));
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

void number(std::istream& row, double actual, const std::string& field) {
    const double expected = value(row);
    check(!std::memcmp(&actual, &expected, sizeof actual), field);
}

void series(std::istream& row, const std::vector<double>& data, int year, int month, const std::string& field) {
    int y, m, n;
    row >> y >> m >> n;
    check(y == year && m == month && n >= 0 && static_cast<std::size_t>(n) == data.size(), field + " domain");
    for (double actual : data) number(row, actual, field);
}

struct ForecastReference {
    int year, month;
    std::vector<double> errors, wk_errors;
};

std::map<std::string, ForecastReference> read_forecasts(const std::string& preset, std::size_t expected) {
    std::ifstream source(std::string(KALOS_AUTOMATIC_ORACLES) + "/" + preset + "-forecast-errors-x86_64.txt");
    check(source.good(), "missing forecast reference");
    std::map<std::string, ForecastReference> out;
    std::string id;
    for (std::string line; std::getline(source, line);) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "case") { row >> id; continue; }
        if (key != "y_ef" && key != "wk_series_stdev_forecast") continue;
        int year, month, n;
        row >> year >> month >> n;
        check(n == 24, id + " forecast horizon");
        auto& ref = out[id];
        if (key == "y_ef") { ref.year = year; ref.month = month; }
        else check(year == ref.year && month == ref.month, id + " inconsistent forecast domains");
        auto& values = key == "y_ef" ? ref.errors : ref.wk_errors;
        for (int h = 0; h < n; ++h) values.push_back(value(row));
    }
    check(out.size() == expected, "incomplete forecast reference");
    return out;
}

void check_wk_errors(const kalos::tramo_seats_automatic_result& fit, const ForecastReference& ref,
                     const std::string& id) {
    const auto& rows = fit.decomposition.extraction.final_components();
    auto wk = std::find_if(rows.begin(), rows.end(), [](const auto& r) {
        return r.component == kalos::seats_component::series && r.information == kalos::seats_information::stdev_forecast;
    });
    check(wk != rows.end() && wk->start_year == ref.year && wk->start_month == ref.month &&
          wk->values == ref.wk_errors, id + " WK forecast domain and errors");
}

void check_forecasts(const kalos::TramoFitResult& model, const ForecastReference& ref,
                     const std::string& id, double& worst_error, bool rsa3) {
    const auto project = rsa3 ? kalos::tramo_tr3_forecast_errors : kalos::tramo_tr1_forecast_errors;
    for (int count : {0, 1, 7, 24}) {
        const auto result = project(model, count);
        check(result.status == (count ? "available" : "no_requested_predictions") &&
              result.standard_errors.size() == static_cast<std::size_t>(count), id + " public horizon");
        for (int h = 0; h < count; ++h) {
            const double scaled = std::abs(result.standard_errors[h] - ref.errors.at(h)) / (1 + ref.errors[h]);
            check(scaled <= 2e-8, id + " public y_ef");
            worst_error = std::max(worst_error, scaled);
        }
    }
    if (id == (rsa3 ? "fit_noout" : "fit_tr0_air")) {
        auto divergent = model;
        divergent.covariance.covariance.front() = std::numeric_limits<double>::infinity();
        const auto failed = project(divergent, 1);
        check(failed.status == "covariance_range" && failed.standard_errors.empty(), id + " unavailable projection");
        if (rsa3) {
            auto invalid = model;
            invalid.covariance.covariance.clear();
            try {
                (void)project(invalid, 1);
                throw std::runtime_error("invalid regression covariance accepted");
            } catch (const std::invalid_argument&) {}
            invalid = model;
            ++invalid.orders.d;
            try {
                (void)project(invalid, 1);
                throw std::runtime_error("invalid effective sample accepted");
            } catch (const std::invalid_argument&) {}
        }
        try {
            (void)project(model, std::numeric_limits<int>::max());
            throw std::runtime_error("oversized forecast horizon accepted");
        } catch (const std::length_error&) {}
    }
    try { (void)project(model, -1); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("negative forecast horizon accepted");
}

void run(const std::string& preset, std::size_t expected_cases, std::size_t expected_values,
         const std::string& prefix = "", const std::string& preprocessing_case = "") {
    static const auto rsa1_forecasts = read_forecasts("rsa1", 12);
    static const auto rsa3_forecasts = read_forecasts("rsa3", 13);
    const bool rsa3 = preset.rfind("rsa3", 0) == 0;
    static const auto retries = read_forecasts("rsa3-ref-retry", 7);
    const auto& forecasts = preset == "rsa3-ref-retry" ? retries : rsa3 ? rsa3_forecasts : rsa1_forecasts;
    using C = kalos::seats_component;
    using I = kalos::seats_information;
    const std::map<std::string, C> components{{"Series", C::series}, {"Trend", C::trend}, {"Seasonal", C::seasonal},
        {"SeasonallyAdjusted", C::seasonally_adjusted}, {"Irregular", C::irregular}};
    const std::map<std::string, I> kinds{{"Value", I::value}, {"Forecast", I::forecast}};
    std::ifstream stream(std::string(KALOS_AUTOMATIC_ORACLES) + "/" + preset + "-x86_64.txt");
    check(stream.good(), "missing oracle " + preset);
    kalos::tramo_seats_automatic_result result;
    std::size_t cases = 0, nrows = 0, nvalues = 0, variable = 0, product = 0;
    double worst_forecast_error = 0;
    std::string id;
    for (std::string line; std::getline(stream, line);) {
        std::istringstream row(line);
        std::string key, label;
        row >> key;
        const auto& fit = result.preprocessing;
        const bool preprocessing_only = !id.empty() && id == preprocessing_case;
        if (preprocessing_only && (key == "product" || key == "merged")) continue;
        if (key == "case") {
            check(product == result.decomposition.finals.size() && variable == fit.regression_coefficients.size(), id + " extra fields");
            row >> id;
            ++cases;
            product = variable = 0;
        } else if (key == "input") {
            int year, month, n;
            row >> year >> month >> n;
            std::vector<double> observations(n);
            for (auto& v : observations) v = value(row);
            if (preprocessing_only) {
                result = {};
                result.preprocessing = kalos::estimate_tramo_tr3(observations, year, month);
            } else result = rsa3 ? kalos::run_tramo_seats_rsa3(observations, year, month)
                               : kalos::run_tramo_seats_rsa1(observations, year, month);
            check(fit.final_model_computed && fit.status == "available", id + " estimation");
            if (preset.rfind("rsa3", 0) == 0)
                check(fit.reference_model_computed && fit.first_candidate_computed && fit.initial_regression_checked,
                      id + " automatic identification");
            const auto& ref = forecasts.at(prefix + id);
            if (!preprocessing_only) check_wk_errors(result, ref, prefix + id);
            check_forecasts(fit, ref, prefix + id, worst_forecast_error, rsa3);
        } else if (key == "model") {
            bool log, mean;
            row >> label >> std::boolalpha >> log >> label >> mean >> label;
            std::array<int, 6> orders;
            for (int& o : orders) row >> o;
            const auto& o = fit.orders;
            check(log == fit.log && mean == fit.mean && orders == std::array<int, 6>{o.p,o.d,o.q,o.bp,o.bd,o.bq}, id + " model");
        } else if (key == "parameters") {
            for (double param : fit.parameters) number(row, param, id + " SARIMA coefficient");
        } else if (key == "variable") {
            row >> label;
            if (label != "const") {
                std::string date;
                row >> date;
                check(fit.variables.at(variable - fit.mean).name == label + " " + date, id + " regression order");
            } else check(fit.mean && variable == 0, id + " mean registration");
            number(row, fit.regression_coefficients.at(variable++), id + " regression coefficient");
        } else if (key == "likelihood") {
            for (double x : {fit.ls_ll, fit.ls_tadj, fit.ls_all, fit.ls_ssq}) number(row, x, id + " likelihood");
            int eff, npar;
            row >> eff >> npar;
            check(eff == fit.ls_eff && npar == fit.ls_npar, id + " estimated / effective counts");
        } else if (key == "residuals") {
            const auto& b = fit.builder;
            const auto start = kalos::TimeDomain(b.domain_year, b.domain_position + 1, 12, b.domain_length)
                .at(b.domain_length - fit.ls_eff);
            series(row, fit.full_residuals, start.first, start.second, id + " full residuals");
        } else if (key == "product") {
            std::string present;
            bool changed;
            row >> present >> label >> std::boolalpha >> changed;
            check(present == "present" && result.decomposition.extraction.model_changed == changed, id + " approximation flag");
        } else if (key == "merged") {
            std::string type, kind;
            row >> type >> kind;
            const auto& actual = result.decomposition.finals.at(product++);
            check(actual.component == components.at(type) && actual.information == kinds.at(kind), id + " component");
            series(row, actual.values, actual.start_year, actual.start_month, id + " " + type + " " + kind);
            ++nrows;
            nvalues += actual.values.size();
        } else throw std::runtime_error(id + " unknown record " + key);
        check(!row.fail(), id + " truncated " + key);
        std::string extra;
        check(!(row >> extra), id + " extra " + key);
    }
    check(cases == expected_cases && nrows == 10 * (expected_cases - !preprocessing_case.empty()) && nvalues == expected_values && product == result.decomposition.finals.size() &&
          variable == result.preprocessing.regression_coefficients.size(), preset + " incomplete corpus");
    std::cout << preset << ": " << cases << " public fits, " << nrows << " dated rows, " << nvalues << " product values, maximum scaled public forecast error " << worst_forecast_error << "\n";
}
}

int main() {
    run("rsa1", 6, 3960);
    run("rsa3", 6, 3960);
    run("rsa3-additional", 3, 1800);
    run("rsa3-mean-retry", 4, 2580);
    run("rsa3-ref-retry", 7, 4560, "", "fit_ref_retry_18_2");
    run("rsa1-level-effects", 2, 1200, "level_");
    run("rsa1-level-tc", 1, 600, "level_tc_");
    run("rsa1-log-tc", 3, 1800, "log_tc_");
    auto invalid = [](std::vector<double> values, int month) {
        try { (void)kalos::estimate_tramo_tr1(values, 1990, month); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("invalid TR1 input accepted");
    };
    invalid(std::vector<double>(23, 1.0), 1);
    invalid(std::vector<double>(36, 1.0), 13);
    auto missing = std::vector<double>(36, 1.0);
    missing.front() = std::numeric_limits<double>::quiet_NaN();
    invalid(missing, 1);
}
