// Item 21-H: unit qualification of the final-estimator decision layer
// (kalos/final_estimator.hpp): a raw-bits replay of the FilterUtility.
// checkRoots JVM goldens (degrees the qualified corpus reaches) plus an
// exhaustive truth-table walk of the FinalEstimator.test cascade (flag
// combinations, takeover orderings, the sig2 plateau and the guard
// surfaces). The corpus (validation/evidence/native/x13-automatic, slice 3)
// carries the end-to-end decision pinning; the pinned JVM >= 3 root solver
// is Random-seeded and out of envelope.
#include "kalos/final_estimator.hpp"

#include "kalos/detail/filter_utility.hpp"

#include "jvm_check_roots.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

using kalos::detail::check_roots;
using kalos::final_estimator_test;
using kalos::SarimaOrders;

int failures = 0;

constexpr double kLimit = 1.0 / 0.95;

double bits(std::uint64_t u) {
    double d = 0.0;
    // Bit replay without <bit_cast> (C++17).
    static const std::size_t kBytes = sizeof(d);
    auto* p = reinterpret_cast<unsigned char*>(&d);
    for (std::size_t i = 0; i < kBytes; ++i) {
        p[i] = static_cast<unsigned char>((u >> (8 * i)) & 0xff);
    }
    return d;
}

void check_roots_replay() {
    std::size_t count = 0;
    const golden_roots::Row* rows = golden_roots::table(count);
    std::size_t bad = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const golden_roots::Row& r = rows[i];
        double c[4];
        for (int k = 0; k < 4; ++k) {
            c[k] = bits(r.c[k]);
        }
        const bool got = check_roots(c, r.len, bits(r.limit));
        const bool want = r.res != 0;
        if (got != want) {
            std::printf("FAIL checkRoots len=%d limit=%016llx c={%016llx,%016llx,"
                        " %016llx,%016llx}: got %d want %d\n",
                        r.len, static_cast<unsigned long long>(r.limit),
                        static_cast<unsigned long long>(r.c[0]),
                        static_cast<unsigned long long>(r.c[1]),
                        static_cast<unsigned long long>(r.c[2]),
                        static_cast<unsigned long long>(r.c[3]), got ? 1 : 0,
                        want ? 1 : 0);
            ++bad;
        }
    }
    failures += static_cast<int>(bad);
    std::printf("checkRoots replay: %zu rows, %zu failures\n", count, bad);
}

void expect(const char* name, int want_status, const SarimaOrders& want_orders,
    const SarimaOrders& orders, const std::vector<double>& pars,
    const std::vector<double>& asy, int len, double tsig = 1.0) {
    const auto got = final_estimator_test(orders, pars, asy, len, tsig);
    if (got.status != want_status || got.orders.p != want_orders.p
        || got.orders.bp != want_orders.bp || got.orders.q != want_orders.q
        || got.orders.bq != want_orders.bq) {
        std::printf("FAIL cascade %s: status %d orders (%d,%d,%d,%d), want %d (%d,%d,%d,%d)\n",
                    name, got.status, got.orders.p, got.orders.bp, got.orders.q,
                    got.orders.bq, want_status, want_orders.p, want_orders.bp,
                    want_orders.q, want_orders.bq);
            ++failures;
    }
}

void cascade_table() {
    // v = 1 puts t at exactly |coef|; fired coefficients stay under the
    // sig2 = 0.15 plateau (len 144); blocks with magnitude ~ 1.5 are root-
    // unstable and skipped (degree 1 |a| >= 1/0.95).
    // 1) Nothing unstable: unstable coefficient -> fP false -> status 0.
    expect("unstable block skipped", 0, {1, 0, 0, 0}, {1, 0, 0, 0}, {1.5}, {1.0}, 144);
    // 2) P alone fires: small coef, t = 0.1 < 1.
    expect("P fires", 1, {0, 0, 0, 0}, {1, 0, 0, 0}, {0.1}, {1.0}, 144);
    // 3) Bp takes over P (smaller t).
    expect("Bp takeover", 1, {1, 0, 0, 0}, {1, 1, 0, 0}, {0.1, 0.05}, {1.0, 1.0}, 144);
    // 4) Bp present with larger t: P keeps the win.
    expect("Bp larger t", 1, {0, 1, 0, 0}, {1, 1, 0, 0}, {0.05, 0.1}, {1.0, 1.0}, 144);
    // 5) Q takes over P and Bp.
    expect("Q takeover", 1, {1, 1, 0, 0}, {1, 1, 1, 0}, {0.1, 0.05, 0.01},
        {1.0, 1.0, 1.0}, 144);
    // 6) Bq takes over Q (ratio not updated by Bq itself).
    expect("Bq takeover", 1, {1, 1, 1, 0}, {1, 1, 1, 1}, {0.1, 0.05, 0.01, 0.005},
        {1.0, 1.0, 1.0, 1.0}, 144);
    // 7) Only Bq fires from the 999 initial ratio.
    expect("Bq alone", 1, {0, 0, 0, 0}, {0, 0, 0, 1}, {0.005}, {1.0}, 144);
    // 8) a >= sig2 blocks the fire (coef 0.2 > 0.15).
    expect("sig2 gate", 0, {1, 0, 0, 0}, {1, 0, 0, 0}, {0.2}, {1.0}, 144);
    // 9) sig2 plateau: 0.12 fires at len 150 (0.15) but not at 151 (0.1).
    expect("sig2 plateau low", 1, {0, 0, 0, 0}, {1, 0, 0, 0}, {0.12}, {1.0}, 150);
    expect("sig2 plateau high", 0, {1, 0, 0, 0}, {1, 0, 0, 0}, {0.12}, {1.0}, 151);
    // 10) Non-positive variance skips the block.
    expect("v <= 0", 0, {1, 0, 0, 0}, {1, 0, 0, 0}, {0.1}, {0.0}, 144);
    expect("v < 0", 0, {0, 1, 0, 0}, {0, 1, 0, 0}, {0.1}, {-1.0}, 144);
    // 11) t >= tsig: tsig 0.1 with t = 0.1 (dcmpg ifge skips on equality).
    expect("t == tsig", 0, {1, 0, 0, 0}, {1, 0, 0, 0}, {0.1}, {1.0}, 144, 0.1);
    // 12) Equal t between P and Bp: ratio > t fails on equality, P stays.
    expect("equal t", 1, {0, 1, 0, 0}, {1, 1, 0, 0}, {0.05, 0.05}, {1.0, 1.0}, 144);
    // 13) Degree-2 stability of the fired block matters: phi = (0.5, 0.05)
    // has roots inside and the LAST coefficient (0.05) is tested.
    expect("degree2 fires on last coef", 1, {1, 0, 0, 0}, {2, 0, 0, 0},
        {0.5, 0.05}, {1.0, 1.0}, 144);
    // 14) Degree-2 stable block, but the last coefficient passes the sig2
    // gate boundary (0.5): no fire.
    expect("degree2 sig2 gate", 0, {2, 0, 0, 0}, {2, 0, 0, 0}, {0.05, 0.5},
        {1.0, 1.0}, 144);
    // 15) Degree-2 unstable block skipped (disc < 0, sqrt(1.2) > 1/0.95)
    // even with vanishing variance.
    {
        const auto got = final_estimator_test({2, 0, 0, 0}, {0.05, 1.2},
            {1e-6, 1e-6}, 144, 1.0);
        if (got.status != 0) {
            std::printf("FAIL cascade degree2 unstable skipped: status %d\n",
                        got.status);
            ++failures;
        }
    }
    // 16) Envelope violations throw.
    bool threw = false;
    try {
        final_estimator_test({1, 0, 0, 0}, {}, {1.0}, 144, 1.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) {
        std::printf("FAIL cascade envelope (parameter count)\n");
        ++failures;
    }
    threw = false;
    try {
        final_estimator_test({1, 0, 0, 0}, {0.5}, {}, 144, 1.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) {
        std::printf("FAIL cascade envelope (covariance size)\n");
        ++failures;
    }
}

} // namespace

int main() {
    check_roots_replay();
    cascade_table();
    std::printf("final-estimator: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
