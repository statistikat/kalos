// Raw public preset comparisons. Native calls receive only dated observations.
#include "kalos/tramo_seats_pipeline.hpp"
#include "kalos/preparation.hpp"

#include <array>
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

void run(const std::string& preset, std::size_t expected_cases, std::size_t expected_values) {
    using C = kalos::seats_component;
    using I = kalos::seats_information;
    const std::map<std::string, C> components{{"Series", C::series}, {"Trend", C::trend}, {"Seasonal", C::seasonal},
        {"SeasonallyAdjusted", C::seasonally_adjusted}, {"Irregular", C::irregular}};
    const std::map<std::string, I> kinds{{"Value", I::value}, {"Forecast", I::forecast}};
    std::ifstream stream(std::string(KALOS_AUTOMATIC_ORACLES) + "/" + preset + "-x86_64.txt");
    check(stream.good(), "missing oracle " + preset);
    kalos::tramo_seats_automatic_result result;
    std::size_t cases = 0, nrows = 0, nvalues = 0, variable = 0, product = 0;
    std::string id;
    for (std::string line; std::getline(stream, line);) {
        std::istringstream row(line);
        std::string key, label;
        row >> key;
        const auto& fit = result.preprocessing;
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
            result = kalos::run_tramo_seats_rsa1(observations, year, month);
            check(fit.final_model_computed && fit.status == "available", id + " estimation");
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
    check(cases == expected_cases && nrows == 10 * expected_cases && nvalues == expected_values && product == result.decomposition.finals.size() &&
          variable == result.preprocessing.regression_coefficients.size(), preset + " incomplete corpus");
    std::cout << preset << ": " << cases << " public fits, " << nrows << " dated rows, " << nvalues << " product values\n";
}
}

int main() {
    run("rsa1", 6, 3960);
    run("rsa1-level-effects", 2, 1200);
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
