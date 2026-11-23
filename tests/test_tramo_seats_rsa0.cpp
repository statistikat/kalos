// Public-kernel oracle from raw series through RSA0 products. The native
// call receives only the same series and date that entered the JVM kernel.
#include "kalos/tramo_seats_pipeline.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
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
