#include "kalos/tramo_seats_bridge.hpp"
#include "jvm_tramo_fit.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace kalos;

double unpack(const std::string& token) {
    const auto bits = static_cast<std::uint64_t>(std::stoull(token, nullptr, 16));
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool identical(const std::vector<double>& a, const std::vector<double>& b) {
    return a.size() == b.size() &&
        (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0);
}

struct Case {
    std::string id;
    bool log = false, mean = false;
    int frequency = 0, d = 0, bd = 0, effective = 0, estimated = 0, year = 0, month = 0;
    double ssq = 0, transmitted = 0;
    std::vector<double> phi, bphi, theta, btheta, input, series;
    std::vector<std::string> outlier_names;
    std::vector<double> outlier_coefs;
};

std::vector<Case> read_cases() {
    std::ifstream in(KALOS_BRIDGE_FACTS);
    assert(in);
    std::vector<Case> cases;
    for (std::string line; std::getline(in, line);) {
        std::istringstream row(line);
        std::string key, token;
        row >> key;
        if (key == "case") {
            cases.emplace_back();
            row >> cases.back().id;
            continue;
        }
        Case& c = cases.back();
        if (key == "model") {
            row >> token >> std::boolalpha >> c.log >> token >> c.mean >> token >> c.frequency;
            row >> token >> c.d >> token >> c.bd;
        } else if (key == "likelihood") {
            row >> token >> token;
            c.ssq = unpack(token);
            row >> token >> c.effective >> token >> c.estimated >> token >> token;
            c.transmitted = unpack(token);
        } else if (key == "phi" || key == "bphi" || key == "theta" || key == "btheta") {
            auto& block = key == "phi" ? c.phi : key == "bphi" ? c.bphi :
                          key == "theta" ? c.theta : c.btheta;
            std::size_t count;
            row >> count;
            for (std::size_t i = 0; i < count; ++i) {
                row >> token;
                assert(token.substr(16) == "/Fixed");
                block.push_back(unpack(token.substr(0, 16)));
            }
        } else if (key == "variable") {
            const auto sep = line.find(" effect ");
            const auto name = line.substr(9, sep - 9);
            if (name != "const") {
                c.outlier_names.push_back(name);
                c.outlier_coefs.push_back(unpack(line.substr(line.rfind(' ') + 1)));
            }
        } else if (key == "interpolated" || key == "seats_series") {
            int y, m;
            std::size_t length;
            row >> y >> m >> length;
            if (key == "interpolated") { c.year = y; c.month = m; }
            else assert(y == c.year && m == c.month);
            auto& values = key == "interpolated" ? c.input : c.series;
            for (std::size_t i = 0; i < length; ++i) {
                row >> token;
                values.push_back(unpack(token));
            }
        }
        assert(!row.fail());
    }
    return cases;
}

void refusals(const TramoFitResult& fit) {
    auto refuses = [&](auto change) {
        auto bad = fit;
        change(bad);
        try { (void)make_tramo_seats_model(bad); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("bridge accepted an invalid or out-of-envelope fit");
    };
    refuses([](auto& f) { f.status = "refused:d10-calendar"; });
    refuses([](auto& f) { f.builder.annual_frequency = 4; });
    refuses([](auto& f) { f.builder.estimation_length--; });
    refuses([](auto& f) { f.builder.missing = {1}; });
    refuses([](auto& f) { f.builder.preadjustment = tramo_lop::leap_year; });
    refuses([](auto& f) { f.ls_eff = f.ls_npar; });
    refuses([](auto& f) { f.parameters.pop_back(); });
    if (!fit.variables.empty()) {
        refuses([](auto& f) { f.log = false; });
        refuses([](auto& f) { f.variables.front().type = TramoOutlierKind::Tc; });
        refuses([](auto& f) { f.variables.front().position = -1; });
        refuses([](auto& f) { f.variables.front().column = {1}; });
    }
}
}  // namespace

int main() {
    const auto cases = read_cases();
    assert(cases.size() == 5);
    std::size_t observations = 0;
    for (const auto& c : cases) {
        TramoFitRequest request;
        build_fit(c.id, request);
        auto fit = tramo_fit(request);
        // Exercise the actual 23-F result. Its estimator witnesses were
        // captured on a different host. Compare decisions across the captures
        // and isolate the bridge arithmetic at identical fitted inputs below.
        const auto actual = make_tramo_seats_model(fit);
        assert(actual.series.size() == c.series.size());
        assert(actual.start_year == c.year && actual.start_month == c.month);
        assert(actual.log == c.log && actual.mean_correction == c.mean);
        assert(actual.d == c.d && actual.bd == c.bd && actual.frequency == c.frequency);
        assert(identical(fit.builder.interpolated, c.input));
        assert(fit.variables.size() == c.outlier_names.size());
        assert(fit.ls_eff == c.effective && fit.ls_npar == c.estimated);
        assert(actual.phi.size() == c.phi.size() && actual.bphi.size() == c.bphi.size());
        assert(actual.theta.size() == c.theta.size() && actual.btheta.size() == c.btheta.size());
        refusals(fit);

        fit.ls_ssq = c.ssq;
        fit.parameters.clear();
        for (const auto* block : {&c.phi, &c.bphi, &c.theta, &c.btheta})
            fit.parameters.insert(fit.parameters.end(), block->begin(), block->end());
        for (std::size_t i = 0; i < fit.variables.size(); ++i) {
            assert(fit.variables[i].name == c.outlier_names[i]);
            fit.variables[i].coefficient = c.outlier_coefs[i];
        }
        const auto replay = make_tramo_seats_model(fit);
        assert(identical(replay.phi, c.phi) && identical(replay.bphi, c.bphi));
        assert(identical(replay.theta, c.theta) && identical(replay.btheta, c.btheta));
        assert(replay.innovation_variance == c.transmitted);
        if (!identical(replay.series, c.series)) {
            for (std::size_t i = 0; i < c.series.size(); ++i) {
                if (replay.series[i] != c.series[i]) {
                    std::cerr << c.id << " observation " << i << " got " << std::hexfloat
                              << replay.series[i] << " expected " << c.series[i] << "\n";
                    break;
                }
            }
            return 1;
        }
        observations += replay.series.size();
    }
    std::cout << cases.size() << " fitted bridge handoffs, " << observations
              << " bit-exact adjusted observations\n";
}
