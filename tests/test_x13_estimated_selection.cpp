// Run X-11 on the preadjusted observations and forecasts from real stock X-13 fits.
#include "kalos/x11_selection_pass.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace kalos;

void require(bool condition, const std::string& detail) {
    if (!condition) throw std::runtime_error(detail);
}

double decode(const std::string& text) {
    auto bits = static_cast<std::uint64_t>(std::stoull(text, nullptr, 16));
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool same_bits(double actual, double expected) {
    return std::memcmp(&actual, &expected, sizeof(double)) == 0;
}

int filter_length(const std::string& value) {
    require(value == "S3X3" || value == "S3X5" || value == "S3X9", "unexpected MSR filter");
    return value.back() - '0';
}

struct Panel {
    std::vector<std::string> lines;
};

std::map<std::string, Panel> read_panel(const std::string& path) {
    std::ifstream input(path);
    require(input.is_open(), "missing panel " + path);
    std::map<std::string, Panel> panels;
    std::string id;
    for (std::string line; std::getline(input, line);) {
        if (line.rfind("case ", 0) == 0) {
            id = line.substr(5);
            require(panels.emplace(id, Panel{}).second, "repeated case " + id);
        } else {
            panels.at(id).lines.push_back(line);
        }
    }
    require(panels.size() == 9, "expected nine stock fits");
    return panels;
}

struct Row {
    int year, month;
    std::vector<double> values;
};

Row read_row(std::istream& in) {
    Row row{};
    std::size_t count;
    in >> row.year >> row.month >> count;
    for (std::string hex; in >> hex;) row.values.push_back(decode(hex));
    require(row.values.size() == count, "bad dated series");
    return row;
}

std::size_t check(const Panel& product, const Panel& decisions) {
    std::map<std::string, Row> rows;
    std::string mode;
    for (const auto& line : product.lines) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "selected") in >> mode;
        if (tag == "y_f" || tag == "b1" || tag == "b7" || tag == "c7" || tag == "d7" ||
            tag == "d10" || tag == "d11" || tag == "d12" || tag == "d13") {
            rows.emplace(tag, read_row(in));
        }
    }
    require(mode == "Additive" || mode == "Multiplicative", "unexpected stock mode");
    const auto& b1 = rows.at("b1");
    const auto horizon = static_cast<int>(rows.at("y_f").values.size());
    require(horizon == 12, "effective annual extension");
    auto pass = x11_selection_pass({b1.values, std::vector<std::uint8_t>(b1.values.size())}, 12, b1.month,
        std::vector<X11SeasonalRequest>(12, X11SeasonalRequest::msr), 0, X11SigmaPolicy::legacy,
        X11MsrIterationPolicy::jdplus, 1.5, 2.5, 0, horizon,
        mode == "Additive" ? X11DecompositionMode::additive : X11DecompositionMode::multiplicative);
    require(pass.status == "available", pass.status);
    const std::map<std::string, X11TrendStage> trends{
        {"b7", pass.b7}, {"c7", pass.c7}, {"d7", pass.d7}, {"final", pass.final}};
    require(decisions.lines.size() == 5, "decision count");
    for (const auto& line : decisions.lines) {
        std::istringstream in(line);
        std::string tag, name, hex;
        in >> tag >> name;
        if (tag == "trend") {
            int length;
            in >> length >> hex;
            const auto& stage = trends.at(name);
            require(stage.status == X11TrendStageStatus::selected && stage.length == length, name + " length");
            require(same_bits(stage.ic_ratio, decode(hex)), name + " I/C ratio");
        } else {
            require(tag == "msr", "unknown decision");
            std::size_t attempts;
            bool fallback;
            in >> attempts >> std::boolalpha >> fallback >> hex;
            require(pass.msr.engaged && pass.msr.selected_filter == filter_length(name) &&
                pass.msr.iterations == attempts && pass.msr.using_default == fallback, "MSR decision");
            require(same_bits(pass.msr.global_msr, decode(hex)), "MSR ratio");
            require(pass.d.seas_filter == std::vector<int>(12, filter_length(name)), "per-month seasonal plan");
        }
    }
    const std::map<std::string, const MaskedSeries*> tables{{"b7", &pass.b.b7}, {"c7", &pass.c.c7},
        {"d7", &pass.d.d7}, {"d10", &pass.d.d10}, {"d11", &pass.d.d11}, {"d12", &pass.d.d12}, {"d13", &pass.d.d13}};
    std::size_t count = 0;
    for (const auto& [name, table] : tables) {
        const auto& expected = rows.at(name);
        require(expected.year == b1.year && expected.month == b1.month && expected.values.size() == table->values.size(),
                name + " domain");
        for (std::size_t i = 0; i < expected.values.size(); ++i) {
            require(!table->missing[i] && same_bits(table->values[i], expected.values[i]), name + " at " + std::to_string(i));
        }
        count += expected.values.size();
    }
    return count;
}
} // namespace

int main() {
    std::string current;
    try {
        const std::string path = KALOS_ESTIMATED_TEMPLATE_PANELS;
        const auto products = read_panel(path + "/templates-x86_64.txt");
        const auto decisions = read_panel(path + "/selection-x86_64.txt");
        std::size_t values = 0;
        for (const auto& [id, product] : products) {
            current = id;
            values += check(product, decisions.at(id));
        }
        require(values == 7812, "unexpected candidate table coverage");
        std::cout << "9 stock fits, 36 Henderson stages, 9 MSR decisions, " << values
                  << " bit-exact table values" << '\n';
    } catch (const std::exception& error) {
        std::cerr << current << ": " << error.what() << '\n';
        return 1;
    }
}
