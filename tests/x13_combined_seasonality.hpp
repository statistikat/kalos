#pragma once

#include "kalos/combined_seasonality.hpp"
#include "kalos/x13_automatic.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace automatic_si {

struct Reference {
    std::string id, mode, summary;
    int backcast = 0, forecast = 0;
    std::array<double, 6> statistics{};
};

inline std::string last_field(std::string& id) {
    const auto split = id.rfind('_');
    if (split == std::string::npos) throw std::runtime_error("invalid automatic SI case name");
    const auto field = id.substr(split + 1);
    id.resize(split);
    return field;
}

inline std::vector<Reference> read(const char* path) {
    std::ifstream in(path);
    std::vector<Reference> panel;
    for (std::string line; std::getline(in, line);) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "case") {
            auto& ref = panel.emplace_back();
            row >> ref.id;
            ref.forecast = std::stoi(last_field(ref.id));
            ref.backcast = std::stoi(last_field(ref.id));
            ref.mode = last_field(ref.id);
        } else if (key == "statistics") {
            for (double& value : panel.back().statistics) {
                std::string field;
                row >> field;
                const std::uint64_t bits = std::stoull(field, nullptr, 16);
                std::memcpy(&value, &bits, sizeof(value));
            }
        } else if (key == "summary") row >> panel.back().summary;
    }
    if (panel.size() != 6) throw std::runtime_error("incomplete automatic SI panel");
    return panel;
}

inline double check(const Reference& ref, kalos::X13AutomaticRequest request) {
    if (ref.mode == "PseudoAdditive") request.mode = kalos::X11DecompositionMode::pseudo_additive;
    else if (ref.mode == "LogAdditive") request.mode = kalos::X11DecompositionMode::log_additive;
    else if (ref.mode != "Undefined") throw std::runtime_error("unrecognized automatic SI request");
    request.backcast = ref.backcast;
    request.forecast = ref.forecast;
    const auto output = kalos::x13_automatic_fit(request);
    const auto test = kalos::x13_si_seasonality(output.fit);
    if (!test || test->summary != ref.summary) throw std::runtime_error(ref.id + " incorrect automatic SI verdict");
    std::array<double, 6> values = {test->stable_f, test->stable_p, test->moving_f,
                                    test->moving_p, test->kruskal_wallis, test->kruskal_p};
    double worst = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double expected = ref.statistics[i];
        const double diff = std::abs(values[i] - expected) / (1 + std::abs(expected));
        if (!(diff <= 1e-12)) throw std::runtime_error(ref.id + " automatic SI statistic " + std::to_string(i)
            + " actual " + std::to_string(values[i]) + " expected " + std::to_string(expected));
        worst = std::max(worst, diff);
    }
    return worst;
}

} // namespace automatic_si
