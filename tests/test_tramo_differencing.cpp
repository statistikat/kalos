// Item 23-C replay test: drives kalos::decide_tramo_differencing (the live
// round-1 tramo.internal.DifferencingModule decision) over the generated
// golden JVM corpus (tests/jvm_tramo_differencing.h). Every DIFF/BAT case
// rebuilds the request from the captured protocol header and series bits,
// runs the transcribed ladder and compares the module-level fact lines -
// diff_result, the IDifferencingModule$Info quadruple (diff_info), the
// description update (after), the private field panel, the module spec
// (mspec) and the last-model parameters - against the captured probe lines,
// %.17g for bit pattern for bit pattern. The battery-stage lines of the
// corpus are inert here; they are the pinned landing facts of the 23-C.2
// battery-replay unit (docs/contracts/tramo-differencing-decision-contract.md).
#include <cstdlib>
#include "kalos/tramo_differencing_decision.hpp"

#include "jvm_tramo_differencing.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(cond)                                                                            \
    if (!(cond)) {                                                                             \
        throw std::runtime_error("check failed: " #cond " (line " + std::to_string(__LINE__)   \
            + ")");                                                                            \
    }

namespace {

using kalos::TramoDifferencingDecision;
using kalos::TramoDifferencingRequest;

std::string g(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

// Java's %.17g rounds its shortest round-trip decimal (0.97 renders
// "0.97000000000000000" where C expands the exact binary value
// "0.96999999999999997"); both renderings reparse to the same bits, so the
// replay compares numeric tokens by bit pattern and every other token
// verbatim (item 23-B bit-pattern discipline).
bool token_equal(const std::string& a, const std::string& b) {
    if (a == b) {
        return true;
    }
    char* ea = nullptr;
    char* eb = nullptr;
    const double va = std::strtod(a.c_str(), &ea);
    const double vb = std::strtod(b.c_str(), &eb);
    if (ea == a.c_str() + a.size() && eb == b.c_str() + b.size()) {
        std::uint64_t ba = 0, bb = 0;
        std::memcpy(&ba, &va, sizeof(ba));
        std::memcpy(&bb, &vb, sizeof(bb));
        return ba == bb;
    }
    return false;
}

bool line_equal(const std::string& want, const std::string& got) {
    std::vector<std::string> wa, wb;
    const auto split = [](const std::string& s) {
        std::vector<std::string> out;
        std::size_t pos = 0;
        while (pos < s.size()) {
            const std::size_t next = s.find(' ', pos);
            if (next == std::string::npos) {
                out.push_back(s.substr(pos));
                break;
            }
            out.push_back(s.substr(pos, next - pos));
            pos = next + 1;
        }
        return out;
    };
    wa = split(want);
    wb = split(got);
    if (wa.size() != wb.size()) {
        return false;
    }
    for (std::size_t i = 0; i < wa.size(); ++i) {
        if (!token_equal(wa[i], wb[i])) {
            return false;
        }
    }
    return true;
}

std::vector<double> to_values(const std::uint64_t* bits, std::size_t count) {
    std::vector<double> out(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t b = bits[i];
        std::memcpy(&out[i], &b, sizeof(double));
    }
    return out;
}

const char* result_name(const TramoDifferencingDecision& d) {
    switch (d.result) {
    case TramoDifferencingDecision::Result::Changed:
        return "Changed";
    case TramoDifferencingDecision::Result::Unchanged:
        return "Unchanged";
    case TramoDifferencingDecision::Result::Unprocessed:
        return "Unprocessed";
    }
    return "?";
}

// The probe's echoed module-level lines, rebuilt from the decision.
std::vector<std::string> produced(const std::string& id, const TramoDifferencingRequest& r,
    const TramoDifferencingDecision& d) {
    std::vector<std::string> out;
    out.push_back("diff_result " + id + " " + result_name(d));
    out.push_back("diff_info " + id + " d " + std::to_string(d.mspec_d) + " bd "
        + std::to_string(d.mspec_bd) + " mean " + (d.mean_correction ? "1" : "0")
        + " tmean " + g(d.tmean));
    out.push_back("after " + id + " spec " + std::to_string(d.spec_p) + ","
        + std::to_string(d.spec_d) + "," + std::to_string(d.spec_q) + ","
        + std::to_string(d.spec_bp) + "," + std::to_string(d.spec_bd) + ","
        + std::to_string(d.spec_bq) + "@" + std::to_string(r.period) + " mean "
        + (d.mean ? "1" : "0"));
    out.push_back("panel " + id + " ylen set lastModel "
        + (d.has_last_model ? "set" : "null") + " rmax " + g(d.rmax) + " rsmax "
        + g(d.rsmax) + " c " + g(d.c) + " din " + g(d.din) + " tmean " + g(d.tmean)
        + " iter " + std::to_string(d.iter) + " ml " + (d.ml ? "1" : "0") + " useml "
        + (d.useml ? "1" : "0") + " mlused " + (d.mlused ? "1" : "0")
        + " maxd 2 maxbd " + (r.seasonal ? "1" : "0") + " ub1 " + g(r.ub1) + " ub2 "
        + g(r.ub2) + " cancel " + g(r.cancel) + " eps " + g(r.eps) + " seasonal "
        + (r.seasonal ? "1" : "0") + " initial " + (r.initial_round ? "1" : "0"));
    out.push_back("mspec " + id + " (" + std::to_string(d.mspec_p) + ","
        + std::to_string(d.mspec_d) + "," + std::to_string(d.mspec_q) + ")("
        + std::to_string(d.mspec_bp) + "," + std::to_string(d.mspec_bd) + ","
        + std::to_string(d.mspec_bq) + ")");
    if (d.has_last_model) {
        // echoParams: the full parameter blocks in JVM order, labels even
        // when a block is empty.
        std::string line = "lastmodel " + id;
        const char* labels[4] = {" phi", " bphi", " theta", " btheta"};
        const std::size_t sizes[4] = {static_cast<std::size_t>(d.mspec_p),
            static_cast<std::size_t>(d.mspec_bp), static_cast<std::size_t>(d.mspec_q),
            static_cast<std::size_t>(d.mspec_bq)};
        std::size_t off = 0, total = 0;
        for (std::size_t s = 0; s < 4; ++s) {
            total += sizes[s];
        }
        if (total == d.last_model.size()) {
            for (std::size_t s = 0; s < 4; ++s) {
                line += labels[s];
                for (std::size_t i = 0; i < sizes[s]; ++i) {
                    line += " " + g(d.last_model[off + i]);
                }
                off += sizes[s];
            }
            out.push_back(line);
        }
    }
    return out;
}

void replay_case(const kalos::test::TdCase& tc) {
    const std::string id = tc.id;
    TramoDifferencingRequest r;
    r.series = to_values(tc.series_bits, tc.series_len);
    r.log_input = tc.log_input != 0;
    r.period = tc.period;
    r.d0 = tc.d0;
    r.bd0 = tc.bd0;
    r.p0 = tc.p0;
    r.q0 = tc.q0;
    r.bp0 = tc.bp0;
    r.bq0 = tc.bq0;
    r.mean0 = tc.mean0 != 0;
    r.initial_round = tc.initial != 0;
    r.seasonal = tc.seasonal != 0;
    r.cancel = tc.cancel;
    r.ub1 = tc.ub1;
    r.ub2 = tc.ub2;
    r.eps = tc.eps;

    const TramoDifferencingDecision d = kalos::decide_tramo_differencing(r);
    const std::vector<std::string> lines = produced(id, r, d);
    for (std::size_t i = 0; i < tc.expect_len; ++i) {
        const std::string want = tc.expect[i];
        bool found = false;
        for (const auto& ln : lines) {
            if (line_equal(want, ln)) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::string dump;
            for (const auto& ln : lines) {
                dump += "\n  produced: " + ln;
            }
            throw std::runtime_error("missing fact line:\n  want: " + want + dump);
        }
    }
}

void test_envelope_throws() {
    TramoDifferencingRequest r;
    bool threw = false;
    try {
        kalos::decide_tramo_differencing(r);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);

    r.series = {1.0, 2.0, std::nan("")};
    threw = false;
    try {
        kalos::decide_tramo_differencing(r);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);

    // The log channel rejects non-positive observations (transform_series
    // guard) and carries the pinned fdlibm log otherwise.
    r.series = {1.0, 0.0, 2.0};
    r.log_input = true;
    threw = false;
    try {
        kalos::decide_tramo_differencing(r);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    r.log_input = false;
}

// The comespd size gate: the pinned autlar heuristics truncate toward zero,
// so the airline fallback (remark "default model selected") only fires when
// the observation count falls under the differencing degree by four or more
// (n <= 10 at period 12 with seasonal). It sets the airline spec and drops
// the mean; an already-airline description stays Unprocessed.
void test_comespd_airline() {
    TramoDifferencingRequest r;
    r.series.assign(10, 1.0);
    for (std::size_t i = 0; i < r.series.size(); ++i) {
        r.series[i] += 0.01 * static_cast<double>(i);
    }
    r.period = 12;
    r.seasonal = true;
    auto d = kalos::decide_tramo_differencing(r);
    CHECK(d.default_model);
    CHECK(d.airline_applied);
    CHECK(d.result == TramoDifferencingDecision::Result::Changed);
    CHECK(d.spec_d == 1 && d.spec_bd == 1 && d.spec_q == 1 && d.spec_bq == 1);
    CHECK(!d.mean);

    // Dated 2026-11-23: the jar airline shape is ARIMA(0,1,1)(0,1,1) -
    // SarimaOrders.isAirline tests p == 0 and bp == 0 (javap; the earlier
    // p0/bp0 = 1 here transcribed the landing bug this test guards).
    r.p0 = 0;
    r.d0 = 1;
    r.q0 = 1;
    r.bp0 = 0;
    r.bd0 = 1;
    r.bq0 = 1;
    d = kalos::decide_tramo_differencing(r);
    CHECK(d.default_model);
    CHECK(!d.airline_applied);
    CHECK(d.result == TramoDifferencingDecision::Result::Unprocessed);
}

} // namespace

int main() {
    int failures = 0;
    try {
        test_envelope_throws();
        test_comespd_airline();

        std::size_t count = 0;
        const kalos::test::TdCase* cases = kalos::test::td_cases(count);
        for (std::size_t i = 0; i < count; ++i) {
            try {
                replay_case(cases[i]);
            } catch (const std::exception& e) {
                std::printf("FAIL case %s: %s\n", cases[i].id, e.what());
                ++failures;
            }
        }
    } catch (const std::exception& e) {
        std::puts(e.what());
        return 1;
    }
    return failures ? 1 : 0;
}
