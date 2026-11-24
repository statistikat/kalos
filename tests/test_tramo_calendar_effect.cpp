#include "kalos/calendar_generation.hpp"
#include "kalos/tramo_calendar_f_test.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct CalendarModel {
    kalos::TramoFittedCalendar calendar;
    bool log = false;
    int year = 0, month = 0, size = 0;
};

std::map<std::string, CalendarModel> read_models() {
    std::ifstream input(std::string(KALOS_CALENDAR_EFFECT_DIR) + "/rsafull-x86_64.txt");
    std::map<std::string, CalendarModel> models;
    std::string id;
    for (std::string line; std::getline(input, line);) {
        std::istringstream fields(line);
        std::string tag, name;
        fields >> tag;
        if (tag == "case") { fields >> id; require(models.emplace(id, CalendarModel{}).second, id + " duplicated"); continue; }
        auto& model = models.at(id);
        if (tag == "input") fields >> model.year >> model.month >> model.size;
        if (tag == "model") fields >> name >> std::boolalpha >> model.log;
        if (tag != "variable") continue;
        fields >> name;
        auto& selection = model.calendar.selection;
        if (name == "td") selection.trading_days = kalos::TramoTradingDays::WorkingDays;
        else if (name.rfind("td[", 0) == 0) selection.trading_days = kalos::TramoTradingDays::TradingDays;
        else if (name == "lp") selection.leap_year = true;
        else if (name == "easter") selection.easter = true;
        else continue;
        std::uint64_t bits;
        fields >> std::hex >> bits;
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        model.calendar.coefficients.push_back(value);
    }
    return models;
}
} // namespace

int main() {
    const auto models = read_models();
    std::ifstream input(std::string(KALOS_CALENDAR_EFFECT_DIR) + "/rsafull-calendar-effect-x86_64.txt");
    int cases = 0, observed = 0, forecasts = 0;
    std::string id;
    for (std::string line; std::getline(input, line);) {
        std::istringstream fields(line);
        std::string tag;
        fields >> tag;
        if (tag == "case") { fields >> id; ++cases; continue; }
        const auto& model = models.at(id);
        bool forecast = tag == "cal_f";
        require(forecast || tag == "cal", id + " field");
        int year, month, size;
        fields >> year >> month >> size;
        const int start = model.year * 12 + model.month - 1 + (forecast ? model.size : 0);
        require(year == start / 12 && month == start % 12 + 1 && size == (forecast ? 24 : model.size), id + " domain");
        auto result = kalos::tramo_stock_calendar_effect_original_scale(model.calendar, {year, month, 12, size}, model.log);
        require(result.size() == static_cast<std::size_t>(size), id + " result length");
        for (int i = 0; i < size; ++i) {
            const double value = result[i];
            std::uint64_t expected, actual;
            require(bool(fields >> std::hex >> expected), id + " short oracle");
            std::memcpy(&actual, &value, sizeof(actual));
            require(actual == expected, id + " " + tag + " differs at month " + std::to_string(i + 1));
            ++(forecast ? forecasts : observed);
        }
        std::string extra;
        require(!(fields >> extra), id + " extra values");
    }
    require(models.size() == 26 && cases == 26 && observed == 4248 && forecasts == 624, "incomplete calendar panel");
    std::cout << cases << " RSAfull calendars, " << observed << " observed and " << forecasts << " forecast values exact\n";
}
