// Item 23-B replay test: drives kalos::scan_tramo_outliers (round-0 TRAMO
// outlier scan) over the generated golden JVM corpus
// (tests/jvm_tramo_outlier_scan.h) and additionally checks the static
// request envelope. The replay reconstructs each request from the captured
// SCAN protocol line, runs the round-0 machine and compares the decision
// status and the first selected [position, type] pair against the captured
// OutliersDetectionModule facts; the request echoes (critical value, factory
// count, resolved bounds) are checked on every case. The two divergent cases
// (so, spanout) carry the multi-round removal of the round-0 pick (register
// D3 evidence): the replay there pins the request echoes and the round-0
// selection itself. The pexao47 case pins the documented fail-closed throw
// of the warm re-estimation seam (non-constant design, D3 decision-brief
// evidence).
#include "kalos/tramo_outlier_scan.hpp"

#include "jvm_tramo_outlier_scan.h"
#include "tramo_outlier_scan_replay.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

#define CHECK(cond)                                                                            \
    if (!(cond)) {                                                                             \
        throw std::runtime_error("check failed: " #cond " (line " + std::to_string(__LINE__)   \
            + ")");                                                                            \
    }

using kalos::TramoOutlierKind;
using kalos::TramoOutlierScanDecision;
using kalos::TramoOutlierScanRequest;

std::vector<std::string> to_lines(const char* const* lines, std::size_t count) {
    return {lines, lines + count};
}

std::vector<double> to_values(const std::uint64_t* bits, std::size_t count) {
    std::vector<double> out(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t b = bits[i];
        std::memcpy(&out[i], &b, sizeof(double));
    }
    return out;
}

TramoOutlierScanDecision::Status status_of(const std::string& captured) {
    if (captured == "Failed") {
        return TramoOutlierScanDecision::Status::Failed;
    }
    if (captured == "Unchanged") {
        return TramoOutlierScanDecision::Status::Unchanged;
    }
    if (captured == "Changed") {
        return TramoOutlierScanDecision::Status::Selected;
    }
    throw std::runtime_error("unknown captured status: " + captured);
}

void replay_case(const golden_tos::Case& c) {
    const auto lines = to_lines(c.lines, c.nlines);
    if (lines.size() != 1) {
        throw std::runtime_error("case carries more than one protocol line");
    }
    auto ps = tosr::parse_scan(tosr::split(lines[0]), to_values(c.values, c.nvalues));
    if (ps.cv_is_calc) {
        ps.req.cv = tosr::cv_from_facts(to_lines(c.facts, c.nfacts));
    }

    if (c.seam_throw != nullptr && c.seam_throw[0] != '\0') {
        // Documented fail-closed construction (warm re-estimation over a
        // non-constant design): the seam throws, it never approximates.
        bool thrown = false;
        std::string message;
        try {
            kalos::scan_tramo_outliers(ps.req);
        } catch (const std::exception& e) {
            thrown = true;
            message = e.what();
        }
        CHECK(thrown);
        CHECK(message.rfind(c.seam_throw, 0) == 0);
        return;
    }

    const auto d = kalos::scan_tramo_outliers(ps.req);

    // Request echoes on every case.
    CHECK(d.critical_value == ps.req.cv);
    int nfac = 0;
    for (const bool on : {ps.req.ao, ps.req.ls, ps.req.tc, ps.req.so}) {
        nfac += on ? 1 : 0;
    }
    CHECK(d.factories == nfac);
    CHECK(d.lower_bound == ps.req.lower_bound);
    CHECK(d.upper_bound == ps.req.upper_bound);

    if (c.divergent) {
        // The live multi-round driver later empties the set (D3 evidence);
        // the round-0 machine still selects the pick the module observed.
        CHECK(d.status == TramoOutlierScanDecision::Status::Selected);
        return;
    }

    CHECK(d.status == status_of(c.status));
    const std::string first = c.outliers;
    if (!first.empty()) {
        if (first.rfind("[", 0) != 0) {
            throw std::runtime_error("malformed outliers grammar");
        }
        const auto close = first.find(']');
        const auto comma = first.find(',');
        const int pos = std::stoi(first.substr(1, comma - 1));
        const int type = std::stoi(first.substr(comma + 1, close - comma - 1));
        CHECK(d.status == TramoOutlierScanDecision::Status::Selected);
        CHECK(d.position == pos);
        CHECK(d.type == type);
    }
    if (d.status == TramoOutlierScanDecision::Status::Selected) {
        CHECK(std::isfinite(d.max_t));
        CHECK(std::fabs(d.max_t) >= d.critical_value);
        CHECK(std::isfinite(d.coefficient));
    }
}

// ---- Static envelope ----------------------------------------------------------

TramoOutlierScanRequest base_request(std::vector<double> y) {
    TramoOutlierScanRequest r;
    r.y = std::move(y);
    r.frequency = 12;
    r.mean = true;
    r.log = true;
    r.p = 1;
    r.d = 1;
    r.q = 1;
    r.bp = 0;
    r.bd = 1;
    r.bq = 1;
    r.ao = r.ls = r.tc = r.so = true;
    r.tc_rate = 0.7;
    r.precision = 1e-7;
    r.maximum_likelihood = false;
    r.max_round = 50;
    r.max_outliers = 30;
    r.cv = 3.415;
    r.interpolate = true;
    r.lower_bound = 0;
    r.upper_bound = static_cast<int>(r.y.size());
    return r;
}

std::vector<double> series(int n, double base) {
    std::vector<double> y;
    y.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        y.push_back(base + 0.01 * i);
    }
    return y;
}

void test_envelope_throws() {
    auto r = base_request(series(96, 50.0));

    r.frequency = 4;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.d = 0;
    r.bd = 0;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.y[10] = std::nan("");
    r.interpolate = false;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.prespec.emplace_back(20, TramoOutlierKind::So);
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.y[3] = -1.0;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
}

void test_short_series_failed() {
    // n = 20: need = max(1 + 1 + 12, 1 + 12) + 1 + 3 + 12 = 30 > 20, the
    // module reports ProcessingResult.Failed (comatip < 0).
    auto r = base_request(series(20, 50.0));
    r.cv = 3.3;
    const auto d = kalos::scan_tramo_outliers(r);
    CHECK(d.status == TramoOutlierScanDecision::Status::Failed);
    CHECK(d.factories == 4);
    CHECK(d.critical_value == 3.3);
}

void test_collapsed_bounds_unchanged() {
    auto r = base_request(series(96, 50.0));
    r.lower_bound = 0;
    r.upper_bound = 0;
    const auto d = kalos::scan_tramo_outliers(r);
    CHECK(d.status == TramoOutlierScanDecision::Status::Unchanged);
    CHECK(d.lower_bound == 0 && d.upper_bound == 0);
}

void test_empty_factories_unchanged() {
    auto r = base_request(series(96, 50.0));
    r.ao = r.ls = r.tc = r.so = false;
    const auto d = kalos::scan_tramo_outliers(r);
    CHECK(d.status == TramoOutlierScanDecision::Status::Unchanged);
    CHECK(d.factories == 0);
}

} // namespace

int main() {
    try {
        test_envelope_throws();
        test_short_series_failed();
        test_collapsed_bounds_unchanged();
        test_empty_factories_unchanged();

        std::size_t count = 0;
        const golden_tos::Case* cases = golden_tos::table(count);
        for (std::size_t i = 0; i < count; ++i) {
            try {
                replay_case(cases[i]);
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string("case ") + cases[i].id + ": " + e.what());
            }
        }
    } catch (const std::exception& e) {
        std::puts(e.what());
        return 1;
    }
    return 0;
}
