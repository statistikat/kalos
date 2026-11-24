// Frozen public stock X-13 models on three different dated monthly inputs.
#include "kalos/x13_automatic.hpp"

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
void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

double from_bits(const std::string& raw) {
    const auto bits = std::stoull(raw, nullptr, 16);
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void compare(std::istream& input, const std::vector<double>& found, double tolerance, const std::string& name) {
    std::size_t i = 0;
    for (std::string raw; input >> raw; ++i) {
        const double expected = from_bits(raw);
        check(i < found.size() && std::isfinite(found[i]) &&
              std::abs(found[i] - expected) <= tolerance * std::max(1.0, std::abs(expected)),
              name + " mismatch at " + std::to_string(i));
    }
    check(i == found.size(), name + " length");
}

kalos::X13AutomaticFitResult fit(const std::string& id, int& year, int& month) {
    std::ifstream input(std::string(KALOS_X13_TEMPLATE_INPUTS) + "/" + id + ".csv");
    char comma;
    check(static_cast<bool>(input >> year >> comma >> month) && comma == ',', "input calendar");
    std::vector<double> values;
    for (double value; input >> value;) values.push_back(value);
    return kalos::x13_estimated_rsa0(values, year, month);
}
} // namespace

int main() {
    std::ifstream input(KALOS_X13_TEMPLATE_PANEL);
    std::string id;
    kalos::X13AutomaticFitResult result;
    std::map<std::string, const std::vector<double>*> tables;
    bool selected = false;
    int cases = 0, checked_rows = 0, year = 0, month = 0;
    for (std::string line; std::getline(input, line);) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "case") {
            std::string preset;
            in >> preset >> id;
            selected = preset == "rsa0";
            if (!selected) continue;
            result = fit(id, year, month);
            check(result.status == "available", id + " " + result.status);
            const auto& t = result.fit;
            const auto& s = t.selection;
            check(t.forecast == 12 && t.backcast == 0, id + " horizon");
            tables = {{"b1",&t.preadjustment.out},{"y_f",&t.preadjustment.a1a},
                      {"b7",&s.b.b7.values},{"c7",&s.c.c7.values},{"d7",&s.d.d7.values},
                      {"d10",&t.d10},{"d11",&t.d11},{"d12",&t.d12},{"d13",&t.d13},
                      {"final_sa",&t.finalSA},{"final_trend",&t.finalTrend},{"final_irregular",&t.finalIrregular},
                      {"forecast_sa",&t.finals.d11a},{"forecast_trend",&t.finals.d12a}};
            ++cases;
        } else if (selected && tag == "model") {
            bool log, mean;
            int p,d,q,bp,bd,bq;
            in >> std::boolalpha >> log >> mean >> p >> d >> q >> bp >> bd >> bq;
            check(result.log == log && result.mean == mean && result.p == p && result.d == d &&
                  result.q == q && result.bp == bp && result.bd == bd && result.bq == bq, id + " model");
        } else if (selected && tag == "coefficients") {
            check(result.variables.empty() && in.peek() == std::char_traits<char>::eof(), id + " regressors");
        } else if (selected && tag == "arma") {
            compare(in, {result.coefficients[1].at(0), result.coefficients[3].at(0)}, 0.0, id + " ARIMA");
        } else if (selected && tag == "selected") {
            std::string mode, filter;
            int d7, last;
            in >> mode >> d7 >> last >> filter;
            const auto& s = result.fit.selection;
            check(mode == "Additive" && result.fit.mode_effective == kalos::X11DecompositionMode::additive &&
                  s.d7.length == d7 && s.final.length == last && filter.size() == 4 &&
                  s.msr.selected_filter == std::stoi(filter.substr(3)), id + " X-11 decisions");
        } else if (selected && tables.count(tag)) {
            int start_year, start_month, n;
            in >> start_year >> start_month >> n;
            const auto& values = *tables.at(tag);
            check(n == static_cast<int>(values.size()), id + " " + tag + " length");
            const bool forecast = tag == "y_f" || tag == "forecast_sa" || tag == "forecast_trend";
            const int offset = forecast ? static_cast<int>(result.fit.finalSA.size()) : 0;
            check((start_year - year)*12 + start_month - month == offset, id + " " + tag + " calendar");
            compare(in, values, 1e-8, id + " " + tag);
            ++checked_rows;
        }
    }
    check(cases == 3 && checked_rows == 42, "incomplete stock RSA0 reference");
    std::cout << cases << " stock RSA0 fits, " << checked_rows << " dated table rows\n";
}
