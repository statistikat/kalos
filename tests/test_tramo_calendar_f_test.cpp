#include "kalos/tramo_calendar_f_test.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
double read_bits(const std::string& text) {
    const std::uint64_t bits = std::stoull(text, nullptr, 16);
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}

int main() {
    std::ifstream oracle(KALOS_CALENDAR_F_TEST_PANEL);
    std::map<int, std::vector<double>> series;
    int cases = 0;
    bool ok = true;
    for (std::string line; std::getline(oracle, line);) {
        std::istringstream row(line);
        std::string kind, token, label;
        int id;
        row >> kind;
        if (kind == "input") {
            row >> id;
            while (row >> token) series[id].push_back(read_bits(token));
            continue;
        }
        int mean, nhp, df1, df6, selected;
        std::string alpha, ss0, ss1, ss6, ms1, ms6, p1, p6;
        row >> label >> id >> mean >> alpha >> ss0 >> nhp >> df1 >> ss1 >> df6 >> ss6
            >> selected >> ms1 >> ms6 >> p1 >> p6;
        if (!row || kind != "fit" || nhp != 2) return 1;
        const auto fit = kalos::tramo_calendar_f_test(series.at(id), 1990, 1, mean, read_bits(alpha), 1e-3);
        const auto& wd = fit.working_days;
        const auto& td = fit.trading_days;
        const auto choice = selected == 6 ? kalos::TramoTradingDays::TradingDays :
            selected == 1 ? kalos::TramoTradingDays::WorkingDays : kalos::TramoTradingDays::None;
        if (fit.base_ssq != read_bits(ss0) || wd.residual_ssq != read_bits(ss1) ||
            td.residual_ssq != read_bits(ss6) || wd.degrees_of_freedom != df1 ||
            td.degrees_of_freedom != df6 || wd.mean_square != read_bits(ms1) ||
            td.mean_square != read_bits(ms6) || wd.lower_probability != read_bits(p1) ||
            td.lower_probability != read_bits(p6) || fit.selected != choice) {
            ok = false;
            std::cerr << "TRAMO calendar likelihoods or selection differ for " << id << " " << label << "\n";
        }
        ++cases;
    }
    if (series.size() != 4 || cases != 14) return 1;
    const auto invalid = [](const std::vector<double>& y, double alpha) {
        try { (void)kalos::tramo_calendar_f_test(y, 1990, 1, true, alpha, 1e-3); }
        catch (const std::invalid_argument&) { return true; }
        return false;
    };
    ok &= invalid(std::vector<double>(22, 1.0), .01);
    ok &= invalid(series.at(0), std::numeric_limits<double>::quiet_NaN());
    std::cout << cases << " public TRAMO calendar F-test comparisons\n";
    return ok ? 0 : 1;
}
