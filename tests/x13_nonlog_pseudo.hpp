#pragma once

#include "kalos/x13_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace nonlog_pseudo {

struct Case {
    std::string id;
    std::map<std::string, std::vector<double>> rows;
};

inline std::vector<Case> cases(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("missing X-13 pseudo-additive fixture: " + path);
    std::vector<Case> result;
    for (std::string line; std::getline(file, line);) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "case") {
            result.emplace_back();
            row >> result.back().id;
            continue;
        }
        auto& values = result.back().rows[key];
        for (std::string token; row >> token;) {
            const std::uint64_t bits = std::stoull(token, nullptr, 16);
            double value;
            std::memcpy(&value, &bits, sizeof(value));
            values.push_back(value);
        }
    }
    if (result.size() != 2) throw std::runtime_error("incomplete X-13 pseudo-additive panel");
    return result;
}

inline void check(const Case& ref, const kalos::X13FitResult& fit, double tolerance) {
    if (fit.status != "available" || fit.log || fit.mode_effective != kalos::X11DecompositionMode::pseudo_additive)
        throw std::runtime_error("wrong X-13 transform or decomposition mode for " + ref.id);
    const auto& f = fit.finals;
    const std::pair<const char*, const std::vector<double>*> tables[] = {
        {"d10", &fit.d10}, {"d11", &fit.d11}, {"d12", &fit.d12}, {"d13", &fit.d13}, {"c17", &fit.c17},
        {"d11final", &f.d11final}, {"d12final", &f.d12final}, {"d13final", &f.d13final},
        {"d16", &f.d16}, {"d18", &f.d18}, {"e1", &f.e1}, {"e2", &f.e2}, {"e3", &f.e3}, {"e11", &f.e11}};
    double worst = 0;
    for (const auto& [name, actual] : tables) {
        const auto& expected = ref.rows.at(name);
        if (actual->size() != expected.size()) throw std::runtime_error(std::string("incorrect length: ") + name);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const double v = (*actual)[i];
            if (v == expected[i] || (std::isnan(v) && std::isnan(expected[i]))) continue;
            const double difference = std::abs(v - expected[i]) / (1 + std::abs(expected[i]));
            if (!(difference <= tolerance)) throw std::runtime_error(ref.id + "/" + name + "/" + std::to_string(i)
                + " actual " + std::to_string(v) + " expected " + std::to_string(expected[i]));
            worst = std::max(worst, difference);
        }
    }
    std::cout << ref.id << " pseudo-additive 14 tables, maximum scaled difference " << worst << "\n";
}

} // namespace nonlog_pseudo
