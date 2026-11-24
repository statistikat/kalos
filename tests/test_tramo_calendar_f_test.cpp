#include "kalos/tramo_calendar_f_test.hpp"
#include "kalos/arma_diagnostics.hpp"
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

std::vector<double> read_vector(std::istream& row) {
    std::vector<double> result;
    for (std::string word; row >> word;) result.push_back(read_bits(word));
    return result;
}

bool same_bits(const std::vector<double>& left, const std::vector<double>& right) {
    return left.size() == right.size() && (left.empty() ||
        std::memcmp(left.data(), right.data(), left.size() * sizeof(double)) == 0);
}

bool check_trading_days_controller(const char* path, std::optional<double> critical_value = std::nullopt) {
    std::ifstream oracle(path);
    std::vector<double> series;
    std::map<int, std::vector<double>> residuals;
    kalos::TramoTradingDaysControllerResult result{};
    const kalos::TramoCalendarAirlineFit* fit = nullptr;
    int attempts = 0, cases = 0, length = 0, effect = 0;
    for (std::string line; std::getline(oracle, line);) {
        std::istringstream row(line);
        std::string kind;
        row >> kind >> std::boolalpha;
        bool ok = true;
        if (kind == "series") {
            row >> length >> effect;
            series = read_vector(row);
            ok = static_cast<int>(series.size()) == length;
        } else if (kind == "case") {
            bool mean;
            std::string alpha;
            row >> mean >> alpha;
            result = kalos::tramo_airline_trading_days_controller(series, 1990, 1, mean, read_bits(alpha), 1e-7, critical_value);
            attempts = 0;
            fit = nullptr;
            ++cases;
        } else if (kind == "fit") {
            std::string label;
            bool mean;
            int columns;
            row >> label >> mean >> columns;
            if (label == "original") fit = &result.original;
            else if (label == "candidate" && attempts < static_cast<int>(result.candidates.size()))
                fit = &result.candidates[attempts++];
            else return false;
            const auto& panel = fit->statistics;
            const auto tests = kalos::tramo_comparator_residual_tests(fit->residuals);
            const std::vector<double> actual{fit->residual_ssq, panel.bic, panel.ljung_box,
                panel.ljung_box_p, panel.season_box, panel.season_box_p, panel.skew_abs,
                panel.skew_p, tests.mean_statistic, panel.stable_mean_p, tests.variance_ratio};
            const auto expected = read_vector(row);
            ok = mean == fit->mean && columns == int(fit->working_days) + int(fit->outliers.size()) && same_bits(actual, expected);
            if (!ok && actual.size() == expected.size()) {
                for (std::size_t i = 0; i < actual.size(); ++i)
                    if (expected[i] != actual[i])
                        std::cerr << "  " << label << " field " << i << " got " << std::hexfloat << actual[i]
                                  << ", expected " << expected[i] << "\n";
            }
        } else if (kind == "outliers") {
            std::vector<std::array<int, 2>> outliers;
            int position, type;
            while (row >> position >> type) outliers.push_back({position, type});
            ok = fit && fit->outliers == outliers;
        } else if (kind == "params" || kind == "beta") {
            ok = fit && same_bits(read_vector(row), kind == "params" ? fit->ma_parameters : fit->coefficients);
        } else if (kind == "variance_p") {
            std::string value;
            row >> value;
            ok = fit && read_bits(value) == fit->statistics.stable_var_p;
        } else if (kind == "residual_values") {
            int id;
            row >> id;
            ok = residuals.emplace(id, read_vector(row)).second;
        } else if (kind == "residual_id") {
            int id;
            row >> id;
            ok = fit && same_bits(fit->residuals, residuals.at(id));
        } else if (kind == "result") {
            std::string status;
            bool mean;
            int count, columns;
            row >> status >> count >> mean >> columns;
            ok = status == (result.changed ? "Changed" : "Unchanged")
                && count == attempts && attempts == static_cast<int>(result.candidates.size())
                && mean == result.selected().mean && columns == int(result.selected().working_days) + int(result.selected().outliers.size());
        } else return false;
        if (!ok) {
            std::cerr << "Trading-day controller differs in case " << cases << " (length " << length
                      << ", effect " << effect << "), " << kind << "\n";
            return false;
        }
    }
    if (critical_value) {
        try {
            kalos::tramo_airline_trading_days_controller(series, 1990, 1, true, .01, 1e-7, 0.0);
            return false;
        } catch (const std::invalid_argument&) {}
    }
    std::cout << cases << " late trading-day controller cases\n";
    return cases == 32;
}

bool check_residual_trading_days() {
    std::ifstream oracle(KALOS_RESIDUAL_TD_PANEL);
    std::vector<double> residuals;
    int year = 0, month = 0, length = 0, cases = 0;
    for (std::string line; std::getline(oracle, line);) {
        std::istringstream row(line);
        std::string kind, token;
        row >> kind;
        if (kind == "residuals") {
            row >> year >> month >> length;
            residuals.clear();
            while (row >> token) residuals.push_back(read_bits(token));
            continue;
        }
        std::string threshold, statistic, probability;
        bool selected;
        row >> threshold >> statistic >> probability >> std::boolalpha >> selected;
        if (!row || kind != "gate") return false;
        const auto gate = kalos::tramo_residual_trading_days(residuals, year, month, length, read_bits(threshold));
        if (!gate || gate->statistic != read_bits(statistic) || gate->upper_probability != read_bits(probability) ||
            gate->needs_processing != selected) {
            std::cerr << "TradingDaysController gate differs at " << year << "-" << month << "\n";
            return false;
        }
        ++cases;
    }
    if (kalos::tramo_residual_trading_days(std::vector<double>(36, 0.0), 1990, 1, 48, .01))
        return false;
    std::cout << cases << " residual trading-day controller decisions\n";
    return cases == 32;
}

bool check_full_template() {
    std::ifstream oracle(KALOS_FULL_CALENDAR_PANEL);
    kalos::TramoFullCalendarResult fit{};
    int count = 0, id = 0;
    bool log = false;
    for (std::string line; std::getline(oracle, line);) {
        std::istringstream row(line);
        std::string kind, token;
        row >> kind;
        if (kind == "input") {
            std::vector<double> values;
            row >> id >> log;
            while (row >> token) values.push_back(read_bits(token));
            fit = kalos::tramo_full_initial_calendar(values, 1990, 1, log);
            continue;
        }
        int columns;
        bool mean, lp, easter;
        std::string status, base, one, six, p1, p6, mean_t, lp_t, easter_t;
        row >> status >> columns >> std::boolalpha >> mean >> lp >> easter >> base >> one >> six >>
            p1 >> p6 >> mean_t >> lp_t >> easter_t;
        if (!row || kind != "result") return false;
        const auto& r = fit.comparison;
        const int selected = r.selected == kalos::TramoTradingDays::None ? 0 :
            r.selected == kalos::TramoTradingDays::WorkingDays ? 1 : 6;
        const auto expected_lp = lp_t == "-" ? std::nullopt : std::optional<double>(read_bits(lp_t));
        if (r.base_ssq != read_bits(base) || r.working_days.residual_ssq != read_bits(one) ||
            r.trading_days.residual_ssq != read_bits(six) || r.working_days.lower_probability != read_bits(p1) ||
            r.trading_days.lower_probability != read_bits(p6) || columns != selected ||
            fit.keep_mean != mean || fit.keep_leap_year != lp || fit.keep_easter != easter ||
            fit.module_changed() != (status == "Changed") || fit.mean_t != read_bits(mean_t) ||
            fit.leap_year_t != expected_lp || fit.easter_t != read_bits(easter_t)) {
            std::cerr << "TRfull calendar mismatch at " << id << " log=" << log << "\n";
            return false;
        }
        ++count;
    }
    std::cout << count << " stock TRfull calendar comparisons\n";
    return count == 20;
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
    return check_trading_days_controller(KALOS_TD_CONTROLLER_PANEL) &&
        check_trading_days_controller(KALOS_TD_CONTROLLER_OUTLIERS, 3.5) && check_residual_trading_days() && check_full_template() && ok ? 0 : 1;
}
