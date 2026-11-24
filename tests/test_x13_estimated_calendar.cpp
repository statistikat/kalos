#include "kalos/x13_estimated_calendar.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace kalos;

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
double read_double(std::istream& in) {
    std::string hex;
    in >> hex;
    std::uint64_t bits = std::stoull(hex, nullptr, 16);
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
std::vector<double> values(std::istream& in) {
    std::vector<double> v;
    while (in >> std::ws && !in.eof()) v.push_back(read_double(in));
    return v;
}
void compare(double native, double reference, const std::string& field) {
    if (std::memcmp(&native, &reference, sizeof(native)) != 0) {
        std::ostringstream msg;
        msg << field << " expected " << std::hexfloat << reference << " got " << native;
        throw std::runtime_error(msg.str());
    }
}
void compare(const std::vector<double>& native, const std::vector<double>& reference, const std::string& field) {
    require(native.size() == reference.size(), field + " length");
    for (std::size_t i = 0; i < native.size(); ++i) compare(native[i], reference[i], field + " at " + std::to_string(i));
}
struct Reference {
    std::string id;
    int year = 0, month = 0;
    std::vector<double> observed, transformed, preadjust, restore;
    std::vector<std::vector<double>> columns;
    std::vector<X13Rsa2cCalendarCandidate> fits;
    X13Rsa2cCalendar selected;
    bool log = false;
};
std::size_t replay(const Reference& ref) {
    const auto r = x13_rsa2c_calendar_test(ref.observed, ref.year, ref.month, ref.log);
    require(ref.fits.size() == 6, "candidate count");
    for (std::size_t i = 0; i < 6; ++i) {
        const auto& actual = i < 2 ? r.trading_days[i] : r.easter[i - 2];
        const auto& expected = ref.fits[i];
        const auto label = "candidate " + std::to_string(i);
        require(actual.nobs == expected.nobs && actual.parameters == expected.parameters, label + " degrees");
        compare(actual.transformation_adjustment, expected.transformation_adjustment, label + " adjustment");
        compare(actual.log_likelihood, expected.log_likelihood, label + " likelihood");
        compare(actual.ssq, expected.ssq, label + " SSQ");
        compare(actual.aicc, expected.aicc, label + " AICc");
    }
    require(r.selected.working_days == ref.selected.working_days && r.selected.easter_duration == ref.selected.easter_duration,
            "selected model");
    compare(r.transformed, ref.transformed, "selected scale");
    const calendar_domain extended{ref.year, ref.month, 12, int(ref.observed.size()) + 12};
    const auto columns = x13_rsa2c_calendar_columns(r.selected, ref.log, extended);
    require(columns.size() == ref.columns.size(), "design dimensions");
    std::size_t cells = ref.transformed.size();
    for (std::size_t i = 0; i < columns.size(); ++i) {
        compare(columns[i], ref.columns[i], "design " + std::to_string(i));
        cells += columns[i].size();
    }
    if (ref.log && r.selected.working_days) {
        compare(x13_rsa2c_leap_factors(extended, false), ref.preadjust, "forward leap factors");
        compare(x13_rsa2c_leap_factors(extended, true), ref.restore, "reverse leap factors");
        cells += ref.preadjust.size() + ref.restore.size();
    }
    return cells;
}
} // namespace

int main() {
    Reference ref;
    try {
        std::ifstream input(KALOS_ESTIMATED_CALENDAR_PANEL);
        require(bool(input), "missing calendar panel");
        int cases = 0;
        std::size_t cells = 0;
        for (std::string line; std::getline(input, line);) {
            std::istringstream in(line);
            std::string tag, name;
            in >> tag >> std::boolalpha;
            if (tag == "case") {
                if (!ref.id.empty()) { cells += replay(ref); ++cases; }
                ref = {};
                in >> ref.id;
            } else if (tag == "input") {
                in >> ref.year >> ref.month;
                ref.observed = values(in);
            } else if (tag == "log") in >> ref.log;
            else if (tag == "candidate") {
                X13Rsa2cCalendarCandidate c{};
                in >> name >> c.nobs >> c.parameters;
                c.transformation_adjustment = read_double(in);
                c.log_likelihood = read_double(in);
                c.ssq = read_double(in);
                c.aicc = read_double(in);
                ref.fits.push_back(c);
            } else if (tag == "selected") in >> ref.selected.working_days >> ref.selected.easter_duration;
            else if (tag == "transformed") ref.transformed = values(in);
            else if (tag == "design") { in >> name; ref.columns.push_back(values(in)); }
            else if (tag == "preadjust") ref.preadjust = values(in);
            else if (tag == "restore") ref.restore = values(in);
            else throw std::runtime_error("unknown record " + tag);
        }
        cells += replay(ref); ++cases;
        require(cases == 15 && cells == 8304, "case or value count");
        for (bool invalid_log : {false, true}) {
            try {
                std::vector<double> series(48, 12.0);
                series[0] = 0;
                x13_rsa2c_calendar_test(series, 1990, invalid_log ? 1 : 13, invalid_log);
                throw std::runtime_error("invalid date or log sample accepted");
            } catch (const std::invalid_argument&) {}
        }
        std::cout << cases << " stock RSA2c calendar runs, " << 6 * cases << " physical likelihoods, "
                  << cells << " transformed series, regression and leap-factor values\n";
    } catch (const std::exception& error) {
        std::cerr << ref.id << ": " << error.what() << '\n';
        return 1;
    }
}
