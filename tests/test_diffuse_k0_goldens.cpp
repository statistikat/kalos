// Item-30 option (ii) adoption: k = 0 byte-identity golden pins (acceptance
// object per docs/item30-k0-routing-probe-2026-11-28.md S7 — maintainer
// adoption ruling of 2026-11-28). This test replays the frozen oracle panel
// validation/evidence/native/k0-structural-goldens/k0-goldens-x86_64.txt,
// captured at HEAD 64c5804 (before the structural k = 0 steps shipped) with
// drivers/m16-item30-goldens.cpp, through the current library line for line
// at byte level. Coverage: both frozen k = 0 shapes, the S7 coverage row's
// gapped / mean != 0 / near-boundary-root arms, the zero-horizon arm, the
// canonical k = 0 refusal arms, and the thirteen non-k0 controls of the
// item-29 corpus (the D4-armed flagship, gapped, near-unit-root, period-4
// and differenced small shapes) that must stay byte-unchanged by the
// fail-closed witness. Panel lines are the dossier S7 dump protocol
// (m15-item30-route_probe.cpp dump) at %.17g. The corpus inputs are read
// from the frozen item-29 corpus; nothing here re-mints a series.
#include "kalos/diffuse_state_space.hpp"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef KALOS_K0_GOLDENS
#error "KALOS_K0_GOLDENS must name the frozen oracle panel"
#endif
#ifndef KALOS_K0_CORPUS
#error "KALOS_K0_CORPUS must name the frozen item-29 corpus directory"
#endif

namespace {

using namespace kalos;

struct Case {
    std::string id;
    int freq, d, seasonal_d;
    std::vector<double> ar, ma, sar, sma;
    std::vector<double> values;
    std::vector<std::uint8_t> missing;
};

std::vector<double> split_list(const std::string& text, char sep = ';') {
    std::vector<double> out;
    std::stringstream in(text);
    std::string item;
    while (std::getline(in, item, sep)) if (!item.empty()) out.push_back(std::stod(item));
    return out;
}

std::vector<std::string> split_fields(const std::string& line, char sep) {
    std::vector<std::string> out;
    std::stringstream in(line);
    std::string item;
    while (std::getline(in, item, sep)) out.push_back(item);
    return out;
}

void load_series(const std::string& path, std::vector<double>& values,
                 std::vector<std::uint8_t>& missing) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("corpus series not found: " + path);
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        auto fields = split_fields(line, ',');
        if (fields.empty() || fields[0].empty() || fields[0] == "NA") {
            values.push_back(0);
            missing.push_back(1);
            continue;
        }
        values.push_back(std::stod(fields[0]));
        missing.push_back(fields.size() > 1 && fields[1] == "1" ? 1 : 0);
    }
}

Case load_case(const std::string& corpus_dir, const std::string& want) {
    std::ifstream cases(corpus_dir + "/cases.csv");
    if (!cases) throw std::runtime_error("corpus cases.csv not found");
    std::string line;
    std::getline(cases, line);
    while (std::getline(cases, line)) {
        if (line.empty()) continue;
        auto f = split_fields(line, ',');
        if (f.size() < 11 || f[0] != want) continue;
        Case c;
        c.id = f[0];
        c.freq = std::stoi(f[1]);
        c.d = std::stoi(f[2]);
        c.seasonal_d = std::stoi(f[3]);
        c.ar = split_list(f[4], ':');
        c.ma = split_list(f[5], ':');
        c.sar = split_list(f[6], ':');
        c.sma = split_list(f[7], ':');
        load_series(corpus_dir + "/" + f[8], c.values, c.missing);
        for (const auto index : split_list(f[10])) {
            const auto i = static_cast<std::size_t>(index) - 1;
            if (i < c.missing.size()) c.missing[i] = 1;
        }
        return c;
    }
    throw std::runtime_error("case not found: " + want);
}

// ---- panel capture ----
void line(std::string& sink, const char* format, ...) {
    char buffer[4096];
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (written < 0 || static_cast<std::size_t>(written) >= sizeof(buffer))
        throw std::runtime_error("golden line exceeds the capture buffer");
    sink.append(buffer, static_cast<std::size_t>(written));
    sink.push_back('\n');
}

void dump_likelihood(std::string& sink, const DiffuseLikelihood& l) {
    line(sink, "STATUS likelihood=%s profile=%s", l.likelihood_status.c_str(), l.profile_status.c_str());
    line(sink, "COUNTS observed=%zu diffuse=%zu finite=%zu initrank=%zu remain=%zu lastdiff=%zu",
         l.observed_count, l.diffuse_count, l.finite_count, l.initial_diffuse_rank,
         l.remaining_diffuse_rank, l.last_diffuse_index);
    line(sink, "SUM ssq %.17g", l.sum_squared_standardized);
    line(sink, "SUM logfinite %.17g", l.sum_log_finite_variances);
    line(sink, "SUM logdiffuse %.17g", l.sum_log_diffuse_variances);
    line(sink, "SUM fixed_ll %.17g", l.fixed_log_likelihood);
    line(sink, "SUM profiled_variance %.17g", l.profiled_variance);
    line(sink, "SUM profiled_ll %.17g", l.profiled_log_likelihood);
    const auto& n = l.numerics;
    line(sink, "NUMERICS rankchecks=%zu exactcert=%d zerocount=%zu pivotcount=%zu stable=%d rebuilds=%zu sqr=%d compressions=%zu clipped=%zu routes=%zu route=%zu poststeps=%zu postrebuilds=%zu",
         n.propagation_rank_checks, n.exact_history_certification ? 1 : 0,
         n.exact_history_zero_count, n.exact_history_pivot_count,
         n.stable_history_propagation ? 1 : 0, n.history_reconstructions,
         n.square_root_finite_covariance ? 1 : 0, n.finite_factor_compressions,
         n.finite_factor_clipped_eigenvalues, n.history_frame_rebuilds,
         n.post_pivot_route, n.post_pivot_certified_steps, n.post_pivot_rebuilds);
    line(sink, "RES projection=%.17g historynorm=%.17g factorinit=%.17g postres=%.17g postrebuildres=%.17g",
         n.maximum_projection_residual, n.maximum_history_normalization_residual,
         n.finite_factor_initialization_residual, n.post_pivot_certificate_residual,
         n.post_pivot_rebuild_residual);
    {
        // The capture protocol emits UKIND as one line: "UKIND" then a
        // space-prefixed token per step, then the newline.
        sink += "UKIND";
        char token[16];
        for (const auto kind : l.update_kind) {
            const int written = std::snprintf(token, sizeof(token), " %d", kind);
            if (written < 0) throw std::runtime_error("UKIND token failed");
            sink.append(token, static_cast<std::size_t>(written));
        }
        sink.push_back('\n');
    }
    for (std::size_t t = 0; t < l.predictions.size(); ++t) line(sink, "PRED %.17g", l.predictions[t]);
    for (std::size_t t = 0; t < l.finite_variances.size(); ++t) line(sink, "FVAR %.17g", l.finite_variances[t]);
    for (std::size_t t = 0; t < l.innovations.values.size(); ++t) line(sink, "INNO %.17g", l.innovations.values[t]);
    for (std::size_t t = 0; t < l.standardized_innovations.values.size(); ++t) line(sink, "STDI %.17g", l.standardized_innovations.values[t]);
    for (const auto v : l.final_state) line(sink, "FSTATE %.17g", v);
    const auto emit_matrix = [&sink](const char* tag, const DenseMatrix& p) {
        for (const auto v : p.values()) line(sink, "%s %.17g", tag, v);
    };
    emit_matrix("INITCOV", l.initial_finite_covariance);
    emit_matrix("INITDIFF", l.initial_diffuse_covariance);
    emit_matrix("FINALCOV", l.final_finite_covariance);
    emit_matrix("FINALDIFF", l.final_diffuse_covariance);
}

std::string run_case_lines(const Case& c, double mean) {
    std::string sink;
    line(sink, "CFG freq=%d d=%d D=%d mean=%.17g n=%zu", c.freq, c.d, c.seasonal_d,
         mean, c.values.size());
    try {
        const MaskedSeries series{c.values, c.missing};
        const auto lik = sarima_innovations(series, c.ar, c.ma, c.sar, c.sma,
                                            c.freq, c.d, c.seasonal_d, mean);
        dump_likelihood(sink, lik.likelihood);
    } catch (const std::exception& error) {
        line(sink, "THROW %s", error.what());
    }
    return sink;
}

std::string run_zero_horizon_lines(const Case& c) {
    std::string sink;
    line(sink, "CFG freq=%d d=%d D=%d mean=0 n=0", c.freq, c.d, c.seasonal_d);
    const MaskedSeries series{{}, {}};
    const auto lik = sarima_innovations(series, c.ar, c.ma, c.sar, c.sma,
                                        c.freq, c.d, c.seasonal_d);
    dump_likelihood(sink, lik.likelihood);
    return sink;
}

std::string run_refusal_lines(const std::vector<double>& ar, const std::vector<double>& ma,
                              const std::vector<double>& sar, const std::vector<double>& sma,
                              int freq, int d, int seasonal_d, double mean,
                              const Case& series_case) {
    std::string sink;
    line(sink, "CFG freq=%d d=%d D=%d mean=%.17g n=%zu", freq, d, seasonal_d, mean,
         series_case.values.size());
    try {
        const MaskedSeries series{series_case.values, series_case.missing};
        const auto lik = sarima_innovations(series, ar, ma, sar, sma, freq, d, seasonal_d, mean);
        line(sink, "NO-THROW observed=%zu", lik.likelihood.observed_count);
    } catch (const std::exception& error) {
        line(sink, "THROW %s", error.what());
    }
    return sink;
}

std::vector<std::string> read_lines(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("panel not found: " + path);
    std::vector<std::string> lines;
    std::string text;
    while (std::getline(in, text)) lines.push_back(text);
    return lines;
}

std::size_t find_arm_start(const std::vector<std::string>& lines, const std::string& id,
                           std::size_t from) {
    const std::string header = "ARM " + id;
    for (std::size_t i = from; i < lines.size(); ++i)
        if (lines[i] == header) return i;
    throw std::runtime_error("arm missing from the frozen panel: " + id);
}

// The expected payload of one arm: everything from the CFG line up to (not
// including) the next ARM header or end of file.
std::vector<std::string> arm_expected(const std::vector<std::string>& panel, std::size_t start) {
    std::vector<std::string> block;
    for (std::size_t i = start + 1; i < panel.size() && panel[i].rfind("ARM ", 0) != 0; ++i)
        block.push_back(panel[i]);
    return block;
}

std::vector<std::string> split_payload(const std::string& produced) {
    std::vector<std::string> lines;
    std::istringstream in(produced);
    std::string text;
    while (std::getline(in, text)) lines.push_back(text);
    return lines;
}

std::size_t mismatches = 0;
std::size_t compared_lines = 0;

void check_arm(const std::vector<std::string>& panel, const std::string& id,
               const std::string& produced) {
    const auto start = find_arm_start(panel, id, 0);
    const auto expected = arm_expected(panel, start);
    const auto actual = split_payload(produced);
    if (expected.size() != actual.size()) {
        std::printf("ARM %s LINE-COUNT expected=%zu actual=%zu\n", id.c_str(),
                    expected.size(), actual.size());
        ++mismatches;
    }
    const auto count = std::min(expected.size(), actual.size());
    std::size_t first = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < count; ++i) {
        ++compared_lines;
        if (expected[i] != actual[i] && first == static_cast<std::size_t>(-1)) {
            first = i;
        }
    }
    if (first != static_cast<std::size_t>(-1)) {
        ++mismatches;
        std::printf("ARM %s FIRST-DIFF at %zu\n  expected: %s\n  actual:   %s\n",
                    id.c_str(), first, expected[first].substr(0, 96).c_str(),
                    actual[first].substr(0, 96).c_str());
    } else if (expected.size() == actual.size()) {
        std::printf("ARM %s BYTE-IDENTICAL (%zu lines)\n", id.c_str(), count);
    }
}

} // namespace

int main() {
    const std::string corpus = KALOS_K0_CORPUS;
    const auto panel = read_lines(KALOS_K0_GOLDENS);
    assert(!panel.empty());

    const char* case_ids[] = {"real-airline-noreg", "real-airline-x", "real-airline-gaps-x",
                              "real-airline-redundant-x", "real-airline-zerocol-x", "syn-airline12",
                              "syn-stat-arma21", "syn-stat-sarma11", "syn-airline4", "syn-mixed4",
                              "nearur-airline", "nearni-airline", "d2-arma12", "slow-seasonal",
                              "leadgap-airline-x"};
    for (const auto* id : case_ids) {
        const auto c = load_case(corpus, id);
        check_arm(panel, id, run_case_lines(c, 0.0));
    }

    // Dossier S7 coverage-row arms assigned to this slice (captured with the
    // driver in this evidence directory; index lists below are reproduced
    // from drivers/m16-item30-goldens.cpp one-for-one).
    {
        auto c = load_case(corpus, "syn-stat-sarma11");
        for (const auto index : {18U, 42U, 43U, 101U, 200U}) c.missing[index] = 1;
        check_arm(panel, "k0-sarma11-gap", run_case_lines(c, 0.0));
    }
    {
        const auto c = load_case(corpus, "syn-stat-arma21");
        check_arm(panel, "k0-arma21-mean", run_case_lines(c, 1.25));
    }
    {
        auto c = load_case(corpus, "syn-stat-arma21");
        c.ar = {0.999};
        c.ma = {-0.3};
        check_arm(panel, "k0-nearboundary", run_case_lines(c, 0.0));
    }
    {
        auto c = load_case(corpus, "syn-stat-sarma11");
        c.ar = {0.5};
        c.ma = {-0.3};
        c.sar = {0.999};
        c.sma = {-0.2};
        check_arm(panel, "k0-nearboundary-seasonal", run_case_lines(c, 0.0));
    }
    {
        const auto c = load_case(corpus, "syn-stat-sarma11");
        check_arm(panel, "k0-zerohorizon", run_zero_horizon_lines(c));
    }
    {
        const auto c = load_case(corpus, "syn-stat-arma21");
        check_arm(panel, "k0-refuse-period",
                  run_refusal_lines(c.ar, c.ma, c.sar, c.sma, 6, 0, 0, 0.0, c));
        check_arm(panel, "k0-refuse-dorder",
                  run_refusal_lines(c.ar, c.ma, c.sar, c.sma, 12, 3, 0, 0.0, c));
        check_arm(panel, "k0-refuse-dmean",
                  run_refusal_lines(c.ar, c.ma, c.sar, c.sma, 12, 1, 0, 0.5, c));
        check_arm(panel, "k0-refuse-nanmean",
                  run_refusal_lines(c.ar, c.ma, c.sar, c.sma, 12, 0, 0,
                                    std::numeric_limits<double>::quiet_NaN(), c));
    }

    std::printf("K0-GOLDENS arms=24 compared_lines=%zu mismatches=%zu\n",
                compared_lines, mismatches);
    assert(mismatches == 0);
    assert(compared_lines > 40000);
    return 0;
}
