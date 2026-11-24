// M7 (item 27 cutover gap): the 24-case double-JVM SA/irregular trading-day
// residual diagnostics oracle, validation/evidence/native/
// residual-trading-days/public-x86_64.txt, replayed BIT-exact through
// kalos::residual_trading_days_tests. Each case embeds the four public
// scalars (diagnostics.td-sa-all / td-sa-last / td-i-all / td-i-last, all
// bit claims) and one component block per series: its domain and
// observations (the only capture lines the producer consumes) plus the
// JVM-side witness rows of the fit handed to the pinned JointTest
// ("parameters" = fitted SarimaModel parameters, "likelihood" =
// cl.dim() and cl.ssq(), "coefficients" = the six cl.coefficients()). The
// witnesses are checked for shape only (one MA word, dim = observations - 1
// of the (0,1,1)[12] difference, six coefficients, monthly domain) and are
// never fed to kalos: the replay runs on observations alone. The pinned
// option is flast = 0
// (verified per case from the "case <id> <flast>" line), where the JVM
// computes one fit per component and publishes it under both names, so the
// "-all" and "-last" expectations are compared against the same produced
// pair. All eight doubles of every case are bit-compared (192 doubles).
#include "kalos/residual_trading_days_tests.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
double decode(const std::string& bits) {
    const std::uint64_t raw = std::stoull(bits, nullptr, 16);
    double value;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}
std::uint64_t bits_of(double value) {
    std::uint64_t raw;
    std::memcpy(&raw, &value, sizeof(raw));
    return raw;
}

struct Scalar {
    double value;
    double probability;
};

struct Component {
    std::string name;
    int year = 0, month = 0, frequency = 0;
    std::vector<double> observations;
    int witness_coefficients = 0;
    int witness_parameters = 0;
    long long witness_dim = -1;
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void compare(const std::string& id, const std::string& key, const kalos::TradingDaysFTest& test,
    const Scalar& expected) {
    const std::uint64_t expected_bits[2] = {bits_of(expected.value), bits_of(expected.probability)};
    const std::uint64_t actual_bits[2] = {bits_of(test.statistic), bits_of(test.p_value)};
    for (int index = 0; index < 2; ++index)
        require(expected_bits[index] == actual_bits[index],
            id + " " + key + " " + (index == 0 ? "statistic" : "probability") +
            " expected " + std::to_string(expected.value) + " actual " + std::to_string(test.statistic));
}
} // namespace

int main() {
    std::ifstream input(KALOS_RESIDUAL_TRADING_DAYS_PANEL);
    require(static_cast<bool>(input), "missing residual trading-day panel");
    std::string line, id;
    int cases = 0, components = 0, compared = 0;
    Scalar td[4];
    Component block[2];
    int stage = 0; // 0: header, 1: td scalars, 2: components, 3: awaiting next case
    for (std::size_t lineno = 1; std::getline(input, line); ++lineno) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        const auto fail = [&](const char* what) -> std::string {
            return id + " line " + std::to_string(lineno) + ": " + what;
        };
        if (key.empty()) continue;
        if (key == "case") {
            int flast = -1;
            row >> id >> flast;
            require(flast == 0, id + " pinned option flast must be 0 (got " + std::to_string(flast) + ")");
            stage = 1;
            ++cases;
        } else if (key == "td-sa-all" || key == "td-sa-last" || key == "td-i-all" || key == "td-i-last") {
            require(stage == 1, fail("diagnostic outside a case header"));
            std::string value, probability;
            row >> value >> probability;
            Scalar* slot = td + (key == "td-sa-all" ? 0 : key == "td-sa-last" ? 1 : key == "td-i-all" ? 2 : 3);
            slot->value = decode(value);
            slot->probability = decode(probability);
            if (key == "td-i-last") stage = 2;
        } else if (key == "domain") {
            require(stage == 2, fail("component outside the diagnostic block"));
            Component& c = block[components % 2];
            c = Component{};
            row >> c.name >> c.year >> c.month >> c.frequency;
            require(c.frequency == 12, id + " " + c.name + " component is not monthly");
        } else if (key == "observations") {
            require(stage == 2, fail("observations outside a component"));
            Component& c = block[components % 2];
            std::string bits;
            while (row >> bits) c.observations.push_back(decode(bits));
        } else if (key == "parameters") {
            Component& c = block[components % 2];
            std::string bits;
            while (row >> bits) ++c.witness_parameters; // witness: shape only
        } else if (key == "likelihood") {
            Component& c = block[components % 2];
            std::string ssq;
            row >> c.witness_dim >> ssq; // witness: shape only, never consumed
        } else if (key == "coefficients") {
            Component& c = block[components % 2];
            std::string bits;
            while (row >> bits) ++c.witness_coefficients; // witness: shape only
            // Component complete: check witness shapes, then replay.
            require(components % 2 == 0 ? c.name == "sa" : c.name == "i", id + " component order");
            require(c.witness_parameters == 1, id + " " + c.name + " witness parameter count");
            require(c.witness_coefficients == 6, id + " " + c.name + " witness coefficient count");
            require(c.witness_dim == static_cast<long long>(c.observations.size()) - 1,
                id + " " + c.name + " witness likelihood dimension");
            ++components;
            if (components % 2 == 0) {
                const auto result = kalos::residual_trading_days_tests(block[0].observations,
                    block[0].year, block[0].month, block[1].observations, block[1].year, block[1].month);
                compare(id, "td-sa-all", result.sa_all, td[0]);
                compare(id, "td-sa-last", result.sa_last, td[1]);
                compare(id, "td-i-all", result.irregular_all, td[2]);
                compare(id, "td-i-last", result.irregular_last, td[3]);
                compared += 8;
                stage = 3;
            }
        } else {
            throw std::runtime_error(id + " line " + std::to_string(lineno) + ": unknown record " + key);
        }
    }
    require(stage == 3, "panel ends inside a case");
    require(cases == 24, "expected 24 reference cases, got " + std::to_string(cases));
    require(components == 48, "expected 48 components, got " + std::to_string(components));
    require(compared == 192, "expected 192 bit claims, got " + std::to_string(compared));
    std::cout << cases << " reference cases, " << compared << " public diagnostic doubles replayed bit-exact\n";
}
