// Public-kernel oracle from raw series through RSA0 products. The native
// call receives only the same series and date that entered the JVM kernel.
#include "kalos/tramo_seats_pipeline.hpp"
#include "kalos/forecast_information.hpp"
#include "kalos/preparation.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {

void check(bool ok, const std::string& context) {
    if (!ok) throw std::runtime_error(context);
}

double read_bits(std::istream& row) {
    std::string token;
    row >> token;
    const auto bits = static_cast<std::uint64_t>(std::stoull(token, nullptr, 16));
    double out;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

bool exact(double a, double b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

void check_residuals(const std::string& id, const kalos::tramo_tr0_result& fit, std::istream& evidence) {
    std::string key, case_id;
    evidence >> key >> case_id;
    check(key == "case" && case_id == id, id + " residual case");
    int year, month, count;
    evidence >> key >> year >> month >> count;
    const auto& residuals = fit.residuals;
    check(key == "residuals.tsres" && residuals.start_year == year && residuals.start_month == month &&
          count == fit.effective_observations && residuals.values.size() == static_cast<std::size_t>(count),
          id + " public residual domain");
    for (auto value : residuals.values)
        check(exact(value, read_bits(evidence)), id + " public residual");
    evidence >> key;
    check(key == "residuals.ser" && exact(residuals.standard_error, read_bits(evidence)),
          id + " public residual standard error");
}

void check_forecast_errors(const std::string& id, const kalos::tramo_seats_rsa0_result& result,
                           std::istream& evidence) {
    std::string key, case_id;
    evidence >> key >> case_id;
    check(key == "case" && case_id == id, id + " forecast-error case");
    const auto& wk = result.decomposition.extraction.final_components();
    auto row = std::find_if(wk.begin(), wk.end(), [](const auto& r) {
        return r.component == kalos::seats_component::series &&
               r.information == kalos::seats_information::stdev_forecast;
    });
    check(row != wk.end(), id + " missing WK forecast errors");
    int year, month, count;
    evidence >> key >> year >> month >> count;
    check(key == "y_ef" && count == 24 && year == row->start_year && month == row->start_month,
          id + " public forecast-error domain");
    const auto predicted = kalos::tramo_tr0_forecast_errors(result.preprocessing, count);
    check(predicted.status == "available" && predicted.standard_errors.size() == row->values.size() &&
          predicted.standard_errors.size() == static_cast<std::size_t>(count), id + " public forecast errors");
    for (double actual : predicted.standard_errors) {
        const double expected = read_bits(evidence);
        check(std::abs(actual - expected) <= 2e-8 * (1 + std::abs(expected)), id + " public y_ef");
    }
    const auto one = kalos::tramo_tr0_forecast_errors(result.preprocessing, 1);
    check(one.status == "available" && one.standard_errors.size() == 1 &&
          std::abs(one.standard_errors[0] - predicted.standard_errors[0]) < 1e-9, id + " one-month forecast");
    bool negative_refused = false;
    try { (void)kalos::tramo_tr0_forecast_errors(result.preprocessing, -1); }
    catch (const std::invalid_argument&) { negative_refused = true; }
    check(negative_refused, id + " negative horizon");
    const auto none = kalos::tramo_tr0_forecast_errors(result.preprocessing, 0);
    check(none.status == "no_requested_predictions" && none.standard_errors.empty(), id + " zero horizon");
    evidence >> key >> year >> month >> count;
    check(key == "wk_series_stdev_forecast" && count == 24 && year == row->start_year && month == row->start_month,
          id + " WK forecast-error domain");
    for (double actual : row->values)
        check(exact(actual, read_bits(evidence)), id + " WK forecast errors");
}

// Compare the specialized finite-MA conditioning with the independent
// general covariance calculation at positive, negative and zero MA values.
void check_general_forecast(const kalos::tramo_tr0_result& original) {
    using namespace kalos;
    for (const auto coefficients : {std::pair{0., 0.}, std::pair{.75, -.88}, std::pair{-.98, .91}}) {
        auto fit = original;
        auto& model = fit.model;
        model.theta = {coefficients.first};
        model.btheta = {coefficients.second};
        constexpr int horizon = 39;
        std::array<ArmaParameterBlock, 4> blocks{};
        blocks[1] = {model.theta, {1}};
        blocks[3] = {model.btheta, {1}};
        const TimeDomain observed(model.start_year, model.start_month, 12, model.series.size());
        auto drift = prepare_regression(align_regressors(observed, {}, 0, horizon), {}, 1, 1, 0, false, true);
        drift.design.values = std::move(drift.regressors.values);
        const MaskedSeries series{model.series, std::vector<std::uint8_t>(model.series.size(), 0)};
        const auto dense = forecast_information(series, drift.design, 0, horizon, ArmaParameterMap(blocks), {}, 12, 1, 1);
        const auto result = tramo_tr0_forecast_errors(fit, horizon);
        check(result.status == "available" && dense.status == "available", "independent forecast covariance");
        const int df = fit.effective_observations - fit.estimated_parameters;
        for (int i = 0; i < horizon; ++i) {
            const double process = (*dense.known_regression_covariance)(i, i) / dense.variance;
            const double regression = (*dense.regression_covariance)(i, i) / dense.variance;
            const double expected = std::sqrt(process * (fit.residual_ssq / df) + regression * (fit.residual_ssq / (df + 1)));
            check(std::abs(result.standard_errors[i] - expected) <= 2e-8 * (1. + expected),
                  "non-default horizon or MA coefficient forecast error");
        }
    }
}

void refusals() {
    auto check_refused = [](const std::vector<double>& values, int month) {
        try { (void)kalos::estimate_tramo_tr0(values, 1990, month); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("invalid raw TR0 input accepted");
    };
    check_refused(std::vector<double>(17, 1.), 1);
    std::vector<double> values(30, 1.);
    check_refused(values, 13);
    values.back() = std::numeric_limits<double>::quiet_NaN();
    check_refused(values, 1);
}
}  // namespace

int main() {
    using C = kalos::seats_component;
    using I = kalos::seats_information;
    const std::map<std::string, C> types{{"Series", C::series}, {"Trend", C::trend},
        {"Seasonal", C::seasonal}, {"SeasonallyAdjusted", C::seasonally_adjusted}, {"Irregular", C::irregular}};
    const std::map<std::string, I> kinds{{"Value", I::value}, {"Forecast", I::forecast}};
    std::ifstream input(KALOS_RSA0_FACTS);
    std::ifstream forecast_errors(KALOS_RSA0_ERRORS);
    std::ifstream residuals(KALOS_RSA0_RESIDUALS);
    std::size_t cases = 0, rows = 0, values = 0, index = 0;
    kalos::tramo_seats_rsa0_result result;
    std::string id;
    for (std::string line; std::getline(input, line);) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "case") {
            check(index == result.decomposition.finals.size(), id + " extra native rows");
            row >> id;
            ++cases;
            index = 0;
        } else if (key == "input") {
            int year, month;
            std::size_t count;
            row >> year >> month >> count;
            std::vector<double> series(count);
            for (auto& v : series) v = read_bits(row);
            result = kalos::run_tramo_seats_rsa0(series, year, month);
            if (id == "fit_tr0_air") check_general_forecast(result.preprocessing);
            check_residuals(id, result.preprocessing, residuals);
            check_forecast_errors(id, result, forecast_errors);
        } else if (key == "fit") {
            const auto& fit = result.preprocessing;
            for (double v : {fit.model.theta.at(0), fit.model.btheta.at(0), fit.mean_coefficient,
                             fit.log_likelihood, fit.residual_ssq})
                check(exact(v, read_bits(row)), id + " fitted-model value");
            int effective, estimated;
            row >> effective >> estimated;
            check(fit.effective_observations == effective && fit.estimated_parameters == estimated, id + " df");
        } else if (key == "product") {
            check(line == "product present changed false" && result.decomposition.extraction.fact("status") == "ok",
                  id + " extraction outcome");
        } else {
            check(key == "merged", id + " unknown evidence row");
            const auto& component = result.decomposition.finals.at(index++);
            std::string type, info;
            std::size_t count;
            int year, month;
            row >> type >> info >> year >> month >> count;
            check(component.component == types.at(type) && component.information == kinds.at(info) &&
                  component.start_year == year && component.start_month == month && component.values.size() == count,
                  id + " component identity/domain");
            for (double v : component.values) check(exact(v, read_bits(row)), id + " " + type + " " + info);
            values += count;
            ++rows;
        }
        check(!row.fail(), id + " truncated evidence");
    }
    check(cases == 6 && rows == 60 && values == 3960 && index == result.decomposition.finals.size(),
          "incomplete public RSA0 reference");
    refusals();
    std::cout << cases << " raw fits, " << rows << " dated product rows, " << values << " bit-exact values\n";
}
