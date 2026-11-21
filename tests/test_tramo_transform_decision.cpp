// Item 23-B replay test: drives kalos::tramo_critical_value,
// kalos::tramo_seasonality and kalos::decide_tramo_transform from the
// generated tramo-transform golden (tests/jvm_tramo_transform_decision.h,
// double-captured against the pinned jdplus 3.9.0 jars) and compares the
// rendered probe-line grammar byte for byte: the calcCv grid, the raw
// Qs/Friedman getters with their Chi2 descriptions and validity flags, the
// 95-value differenced block, and the full TramoKernel pre-stage transform
// facts (verdict, downgrade, seasonal flag, loglevel status, the ln(ssq *
// factor) log/level readouts, slog, the preference sweep and the precision
// variant) plus the ProcessingLog grammar. The four robust-correction cases
// are refused fail-closed (register D3). A rejection matrix qualifies the
// envelope and a determinism pass pins run-to-run stability.
#include "jvm_tramo_transform_decision.h"
#include "tramo_transform_decision_replay.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string& msg) {
    ++failures;
    std::printf("FAIL %s\n", msg.c_str());
}

double from_bits(std::uint64_t u) {
    double v;
    std::memcpy(&v, &u, sizeof(v));
    return v;
}

std::vector<std::string> to_lines(const char* const* arr, std::size_t n) {
    return std::vector<std::string>(arr, arr + n);
}

std::vector<double> to_values(const std::uint64_t* bits, std::size_t n) {
    std::vector<double> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) out.push_back(from_bits(bits[i]));
    return out;
}

void expect_eq(const std::vector<std::string>& got, const std::vector<std::string>& want,
    const std::string& what) {
    if (got.size() != want.size()) {
        fail(what + ": " + std::to_string(got.size()) + " lines vs "
            + std::to_string(want.size()));
        for (std::size_t i = 0; i < want.size(); ++i) {
            std::printf("  want[%zu] %s\n", i, want[i].c_str());
        }
        return;
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (got[i] != want[i]) {
            fail(what + " line " + std::to_string(i));
            std::printf("  got  %s\n", got[i].c_str());
            std::printf("  want %s\n", want[i].c_str());
        }
    }
}

void expect_throw(const std::string& what, const std::function<void()>& run) {
    try {
        run();
        fail(what + ": expected rejection");
    } catch (const std::exception&) {
    }
}

void replay_case(const golden_ttd::Case& c) {
    const std::string id = c.id;
    const auto values = to_values(c.values, c.nvalues);
    ttdreplay::ParsedCase parsed;
    try {
        parsed = ttdreplay::parse_case(to_lines(c.lines, c.nlines), values);
    } catch (const std::exception& e) {
        if (c.reject) return; // robust refusal happens in the driver (D3)
        fail(id + ": driver: " + e.what());
        return;
    }
    if (c.reject) {
        fail(id + ": robust case parsed despite D3 refusal");
        return;
    }

    if (parsed.kind == "calccv") {
        expect_eq(ttdreplay::render_calccv(parsed.n, kalos::tramo_critical_value(parsed.n)),
            to_lines(c.facts, c.nfacts), id + " calccv");
        return;
    }
    if (parsed.kind == "season") {
        kalos::TramoSeasonalityReport report;
        try {
            report = kalos::tramo_seasonality(parsed.values);
        } catch (const std::exception& e) {
            fail(id + ": seasonality threw: " + e.what());
            return;
        }
        expect_eq(ttdreplay::render_season(parsed, report), to_lines(c.facts, c.nfacts),
            id + " season");
        if (c.nevents != 0) fail(id + ": season case carries events");
        return;
    }

    kalos::TramoTransformDecisionRequest request;
    request.values = parsed.values;
    request.period = parsed.freq;
    request.mean = parsed.mean;
    request.detect_seasonality = parsed.detect;
    request.spec_seasonality = parsed.specseas;
    request.log_preference = parsed.log_preference;
    request.precision = parsed.precision;
    kalos::TramoTransformDecisionResult result;
    try {
        result = kalos::decide_tramo_transform(request);
    } catch (const std::exception& e) {
        fail(id + ": decision threw: " + e.what());
        return;
    }
    expect_eq(ttdreplay::render_transform_facts(parsed, result),
        to_lines(c.facts, c.nfacts), id + " facts");
    expect_eq(ttdreplay::render_transform_events(result),
        to_lines(c.events, c.nevents), id + " events");

    // Determinism: the decision is a pure function of the request.
    const auto again = kalos::decide_tramo_transform(request);
    if (again.loglevel_status != result.loglevel_status || again.log_ll != result.log_ll
        || again.level_ll != result.level_ll || again.log_correction != result.log_correction
        || again.choosing_log != result.choosing_log) {
        fail(id + ": non-deterministic decision");
    }
}

void rejection_matrix() {
    const std::vector<double> ok(96, 10.0);
    expect_throw("non-monthly period", [&] {
        kalos::TramoTransformDecisionRequest r;
        r.values = ok;
        r.period = 4;
        r.precision = 1e-5;
        r.log_preference = 0.0;
        kalos::decide_tramo_transform(r);
    });
    expect_throw("empty series", [&] {
        kalos::TramoTransformDecisionRequest r;
        r.period = 12;
        r.precision = 1e-5;
        r.log_preference = 0.0;
        kalos::decide_tramo_transform(r);
    });
    expect_throw("non-finite value", [&] {
        kalos::TramoTransformDecisionRequest r;
        r.values = std::vector<double>(95, 10.0);
        r.values.push_back(std::nan(""));
        r.period = 12;
        r.precision = 1e-5;
        r.log_preference = 0.0;
        kalos::decide_tramo_transform(r);
    });
    expect_throw("non-positive precision", [&] {
        kalos::TramoTransformDecisionRequest r;
        r.values = ok;
        r.period = 12;
        r.precision = 0.0;
        r.log_preference = 0.0;
        kalos::decide_tramo_transform(r);
    });
    expect_throw("non-finite log preference", [&] {
        kalos::TramoTransformDecisionRequest r;
        r.values = ok;
        r.period = 12;
        r.precision = 1e-5;
        r.log_preference = std::numeric_limits<double>::infinity();
        kalos::decide_tramo_transform(r);
    });
    expect_throw("calccv overflow",
        [&] { kalos::tramo_critical_value(std::numeric_limits<std::size_t>::max()); });
    expect_throw("seasonality empty",
        [&] { kalos::tramo_seasonality(std::vector<double>{}); });
    expect_throw("seasonality non-finite", [&] {
        kalos::tramo_seasonality(std::vector<double>{1.0, std::nan("")});
    });
}

} // namespace

int main() {
    std::size_t count = 0;
    const golden_ttd::Case* cases = golden_ttd::table(count);
    for (std::size_t i = 0; i < count; ++i) replay_case(cases[i]);
    rejection_matrix();
    if (failures != 0) {
        std::printf("tramo transform decision: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("tramo transform decision: %zu cases OK\n", count);
    return 0;
}
