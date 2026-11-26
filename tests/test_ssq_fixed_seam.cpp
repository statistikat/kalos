// Reduced-coordinate seam for fixed ARMA coefficients (item 21-D.2 eligibility
// widening, M9-H qualified): unit pins for detail::SsqFixedSeam branch-by-branch
// against the pinned jdplus 3.9.0 SarimaFixedMapping/SarimaMapping bytecode
// (expand fixed-clone, checkBoundaries at limit 1, validate = checkBoundaries
// then per-block stabilize(..., 0.99) for fully free blocks / fstabilize for
// blocks carrying fixed entries), plus the reduced-index vs full-slot threading
// regression the qualification recorded as its own defect class (M9-H report
// §7.4: the prototype differenced probe[j] instead of probe[seam->free()[j]],
// which steered the LM to non-stationary landings and missed the JVM pins of
// the mixed pinned matrix by -1.3/-8.9 adjustedll with coefficients off by
// <= 0.51). The end-to-end section lands the §9.4 synthetics — the middle-slot
// fixed-mask geometry (reduced index != full slot, so the defect class moves
// the landing) and the fixed near-unit-root AR with a free partner — and
// asserts LM-vs-BFGS landing agreement on the surface adjusted-ll yardstick
// with the qualification's measured spreads (fixed-ma-middle 7 hex-digit ulp
// at 10 mantissa digits, fixed-ar-nearlimit 1 ulp; eval counts 46/201 and
// 87/620 on the probe streams, 26/333 and 66/1072 on this test's own streams,
// where the committed build's LM-vs-BFGS gaps measured 1.7e-11 and 8.5e-14). The single-thread setting mirrors the
// qualification lane (drivers/run_*.sh).
//
// Branch coverage of the transcription (SarimaFixedMapping javap, M9-H report
// §2-§3): expand splices the reduced vector into the non-fixed slots of the
// pinned clone (fixed entries keep their values bit-exactly, signed zero
// included); checkBoundaries runs FilterUtility.checkRoots per block at limit
// 1 (trailing zeros trimmed, the degree-1/2 closed forms decide every vector
// used here); validate first checks boundaries, then per SarimaOrders block
// stabilizes: fully free blocks through the pinned stabilize(..., 0.99) (the
// single-coefficient +-limit pull inside 1 +- 0.01 and the 1/v inversion, the
// multi-coefficient root inversion left to the qualified SarimaMapping2
// witness), blocks with fixed entries through fstabilize (per free slot:
// zeros the other free slots, seeds 0.2, flips by x(-1.25) at most 10 times).
// The pinned "cannot stabilize" FunctionException escape and the per-slot
// round counter (>20) cannot fire from estimate_sarima_fixed — unstable fixed
// factors are statically rejected upstream (M9-H §7.1) and the counter counts
// slots, at most 4 — so the transcription reports those branches as no-change
// and the exhaustion case is pinned here as no-change.
#include "kalos/detail/ssq_levmar.hpp"
#include "kalos/estimation_control.hpp"
#include "kalos/arma_objective.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using kalos::detail::SsqFixedSeam;
using kalos::detail::SsqParamValidation;
using kalos::detail::SarimaSsqDomain;

bool is_negative_zero(double v) { return v == 0.0 && std::signbit(v); }

template <typename Function>
bool throws(Function function) {
    try { function(); } catch (const std::exception&) { return true; }
    return false;
}

// --- Seam construction and expand: the pinned fullParameters clone ---------

void test_expand() {
    // JVM coordinate order [phi, bphi, theta, btheta]; q=3 with the middle
    // MA slot fixed (the fixed-ma-middle geometry), bq=1.
    const SarimaSsqDomain domain(0, 0, 3, 1);
    const SsqFixedSeam seam(domain, {0, 1, 0, 0}, {0.0, 0.2, 0.0, -0.3});
    assert(seam.dim() == 3);
    assert(seam.full_dim() == 4);
    assert(seam.free() == (std::vector<std::size_t>{0, 2, 3})); // reduced j -> full slot
    const auto full = seam.expand({1.0, 2.0, 3.0});
    assert(full.size() == 4);
    assert(full[0] == 1.0 && full[1] == 0.2 && full[2] == 2.0 && full[3] == 3.0);
    // Fixed slots come from the pinned clone, not the reduced vector.
    assert(full[1] == 0.2);
    // Signed zero at a fixed slot survives the clone.
    const SsqFixedSeam zseam(domain, {1, 0, 0, 0},
        {-0.0, 0.5, 0.7, -0.3});
    const auto zf = zseam.expand({0.25, 0.5, 0.75});
    assert(is_negative_zero(zf[0]));
    assert(zf[1] == 0.25 && zf[2] == 0.5 && zf[3] == 0.75);
    // An over-long reduced vector cannot touch the fixed slots; a short one
    // leaves NaN at the missing free slots (defensive, never produced by the
    // minimizer, which always carries getDim() entries).
    assert(zseam.expand({0.1, 0.2, 0.3, 0.4})[0] == -0.0);
    const auto shorty = zseam.expand({0.1});
    assert(shorty[1] == 0.1 && std::isnan(shorty[2]) && std::isnan(shorty[3]));
    assert(is_negative_zero(shorty[0]));
    // State length must match the domain.
    assert(throws([&] { SsqFixedSeam(domain, {0, 1}, {0.0, 0.2, 0.0, 0.0}); }));
    assert(throws([&] { SsqFixedSeam(domain, {0, 0, 0, 0}, {0.0, 0.0, 0.0}); }));
    std::printf("  expand: ok\n");
}

// --- checkBoundaries: FilterUtility.checkRoots per block at limit 1 --------

void test_check_boundaries() {
    const SarimaSsqDomain domain(0, 0, 2, 1); // [theta1, theta2, btheta1]
    const SsqFixedSeam seam(domain, {1, 0, 0}, {0.2, 0.0, 0.0});
    // Degree-1/2 closed forms: 1 + a z stable iff |a| < 1; the quadratic
    // branch additionally needs |b| < 1 and |a| < 1 + b.
    assert(seam.check_boundaries({0.2, 0.5, -0.3}));
    assert(!seam.check_boundaries({0.2, -0.9, -0.3})); // 0.2 > 1 - 0.9
    assert(!seam.check_boundaries({0.2, 1.5, -0.3}));  // |b| >= 1
    // The second block decides independently (per-block check).
    assert(!seam.check_boundaries({0.2, 0.5, 1.25}));
    assert(!seam.check_boundaries({0.2, 0.5, -1.0})); // boundary is exclusive
    // Trailing zeros trim: [0.5, 0] decides as degree 1.
    assert(seam.check_boundaries({0.2, 0.5, 0.0}));
    const SsqFixedSeam trimmed(domain, {0, 0, 0}, {0.5, 0.0, 0.9});
    assert(trimmed.check_boundaries({0.5, 0.0, 0.9})); // theta2 == 0: degree 1
    // NaN never passes the boundary check (pinned comparison directions).
    const double nan = std::numeric_limits<double>::quiet_NaN();
    assert(!seam.check_boundaries({0.2, nan, -0.3}));
    // A wrong-length vector is not a candidate.
    assert(!seam.check_boundaries({0.2, 0.5}));
    std::printf("  check_boundaries: ok\n");
}

// --- validate: checkBoundaries, then stabilize / fstabilize ----------------

void test_validate_fully_free() {
    // Single fully free MA coefficient: validate short-circuits on stable
    // vectors (checkBoundaries first), so the 0.99 stabilization only sees
    // unstable candidates (SarimaSsqDomain::check_boundaries at limit 1).
    const SarimaSsqDomain domain(0, 0, 1, 0);
    const SsqFixedSeam seam(domain, {0}, {0.0}); // fully-free mask (all zero)
    auto x = std::vector<double>{0.5};
    assert(seam.validate(x) == SsqParamValidation::Valid);
    assert(x[0] == 0.5);
    x = {0.995}; // stable at limit 1: the 0.99 window is never consulted
    assert(seam.validate(x) == SsqParamValidation::Valid);
    assert(x[0] == 0.995);
    x = {1.5}; // outside the 1 +- 0.01 window: the pinned stabilize inverts
    assert(seam.validate(x) == SsqParamValidation::Changed);
    assert(x[0] == 1.0 / 1.5);
    x = {-1.5};
    assert(seam.validate(x) == SsqParamValidation::Changed);
    assert(x[0] == -1.0 / 1.5);
    x = {1.0}; // the window includes the boundary itself: pulled to +limit
    assert(seam.validate(x) == SsqParamValidation::Changed);
    assert(x[0] == 0.99);
    x = {-1.0};
    assert(seam.validate(x) == SsqParamValidation::Changed);
    assert(x[0] == -0.99);
    x = {1.005}; // inside the window the pull wins over the inversion
    assert(seam.validate(x) == SsqParamValidation::Changed);
    assert(x[0] == 0.99);
    x = {1.01}; // the double 1.01 sits above the closed window: inverted
    assert(seam.validate(x) == SsqParamValidation::Changed);
    assert(x[0] == 1.0 / 1.01);
    x = {-1.02};
    assert(seam.validate(x) == SsqParamValidation::Changed);
    assert(x[0] == -1.0 / 1.02);
    // The multi-coefficient root-inversion branch (qualified z-root
    // recurrence, shared with SarimaMapping2::stabilize_block): the contract
    // is Changed plus stability; the bit-level trajectory of this branch is
    // witnessed by the fully-free pinned corpus landings.
    const SarimaSsqDomain two(0, 0, 2, 0);
    const SsqFixedSeam free2(two, {0, 0}, {0.0, 0.0});
    auto both = std::vector<double>{1.5, 0.5};
    assert(!free2.check_boundaries(both)); // |a| < 1 + b fails at 1.5
    assert(free2.validate(both) == SsqParamValidation::Changed);
    assert(free2.check_boundaries(both));
    std::printf("  validate fully-free: ok\n");
}

void test_validate_fstabilize() {
    // A block carrying fixed entries is stabilized slot by slot; the fixed
    // slot keeps its value, the free slot is seeded at 0.2 and flipped by
    // x(-1.25) until the block passes the stability check (quadratic branch:
    // stable iff |b| < 1 and |a| < 1 + b for [a, b]).
    const SarimaSsqDomain domain(0, 0, 2, 0);
    const std::vector<std::uint8_t> mask{1, 0};
    { // Candidate [0.9, -0.9] unstable; the free slot lands on the seed 0.2.
        const SsqFixedSeam seam(domain, mask, {0.9, 0.0});
        auto full = seam.expand({-0.9});
        assert(full[0] == 0.9 && full[1] == -0.9);
        assert(seam.validate(full) == SsqParamValidation::Changed);
        assert(full[0] == 0.9); // fixed slot untouched, bit-exact
        assert(full[1] == 0.2); // seed passes: 0.9 < 1 + 0.2
    }
    { // Seed and one flip fail ([1.2, 0.2]: 1.2 > 1.2; [1.2, -0.25] fails),
      // the second flip lands exactly on 0.2 * 1.25^2 = 0.3125.
        const SsqFixedSeam seam(domain, mask, {1.2, 0.0});
        auto full = seam.expand({-0.95});
        assert(seam.validate(full) == SsqParamValidation::Changed);
        assert(full[0] == 1.2);
        assert(full[1] == 0.3125);
        assert(seam.check_boundaries(full));
    }
    { // Unsatisfiable block (a fixed 2.0 needs b > 1 and |b| < 1): all 10
      // flips fail, the pinned escape reports no change — the candidate
      // stays unstable and the machine rejects it at evaluation time.
        const SsqFixedSeam seam(domain, mask, {2.0, 0.0});
        auto full = seam.expand({-0.5});
        assert(seam.validate(full) == SsqParamValidation::Valid);
        assert(full[0] == 2.0 && full[1] == -0.5);
    }
    { // Both branches in one call: the fixed-entry MA block goes through
      // fstabilize, the fully free seasonal block through stabilize(0.99).
        const SarimaSsqDomain both(0, 0, 2, 1);
        const SsqFixedSeam seam(both, {1, 0, 0}, {0.9, 0.0, 0.0});
        auto full = seam.expand({-0.9, 1.4});
        assert(!seam.check_boundaries(full));
        assert(seam.validate(full) == SsqParamValidation::Changed);
        assert(full[0] == 0.9);          // fixed, untouched
        assert(full[1] == 0.2);          // fstabilize seed
        assert(full[2] == 1.0 / 1.4);    // fully free inversion
    }
    std::printf("  validate fstabilize: ok\n");
}

// --- End-to-end: reduced-index vs full-slot threading (M9-H §7.4) ----------

// The probe's seeded generator (drivers/lmblock_probe.cpp), fresh stream per
// series so each case is independent of construction order.
double urand(unsigned long long& state) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<double>((state >> 11) & 0x1FFFFFFFFFFFFFULL) /
           static_cast<double>(1ULL << 53);
}
double unorm(unsigned long long& state) {
    const double u1 = std::max(urand(state), 1e-300);
    const double kPi = 3.14159265358979323846; // the same double as M_PI
    return std::sqrt(-2 * std::log(u1)) * std::cos(2 * kPi * urand(state));
}
double seasonal(int t) {
    static const double s[12] = {-2.4, -1.9, -0.9, 0.3, 1.2, 2.1, 2.6, 2.3, 1.3, 0.1, -1.1, -1.9};
    return s[t % 12];
}
std::vector<double> airline_series(int n, double level, double noise, unsigned long long seed) {
    unsigned long long state = seed;
    std::vector<double> y(n, level);
    for (int t = 1; t < n; ++t) y[t] = y[t - 1] + noise * unorm(state);
    for (int t = 0; t < n; ++t) y[t] += level + seasonal(t) + 0.004 * level * t / 120.0;
    return y;
}

kalos::SarimaEstimationRequest request(std::vector<double> y,
        std::array<std::vector<double>, 4> initial,
        std::array<std::vector<std::uint8_t>, 4> fixed, kalos::EstimationSolver solver) {
    kalos::SarimaEstimationRequest q;
    q.series.values = std::move(y);
    q.series.missing.assign(q.series.values.size(), 0);
    for (std::size_t b = 0; b < 4; ++b) {
        q.blocks[b].initial = initial[b];
        q.blocks[b].fixed = fixed[b];
    }
    q.period = 12;
    q.d = 1;
    q.seasonal_d = 1;
    // The probe control: eps 1e-12, feps 1e-4, ml, no mt flag effect, cdf,
    // fast derivatives, Zero start (drivers/lmblock_probe.cpp).
    q.control = kalos::EstimationControl{1e-12, 1e-4, true, false, true, true,
        kalos::EstimationStartingPoint::Zero, solver};
    q.optimizer.max_iterations = 200;
    q.optimizer.max_evaluations = 4000;
    return q;
}

// The probe's common yardstick: the surface adjusted log-likelihood at the
// landing, comparable across the two solvers.
double surface_ll(const kalos::SarimaEstimationRequest& q,
        const kalos::SarimaEstimationResult& result) {
    const auto objective = kalos::evaluate_arma_physical_objective(
        q.series, q.design, kalos::ArmaParameterMap(q.blocks), result.coefficients,
        q.period, q.d, q.seasonal_d);
    assert(objective.status == "available");
    return objective.adjusted_log_likelihood;
}

long long total_evaluations(const kalos::SarimaEstimationResult& result) {
    long long total = 0;
    for (const auto& pass : result.passes) total += static_cast<long long>(pass.evaluations);
    return total;
}

void check_fixed_slot(const kalos::SarimaEstimationRequest& q,
        const kalos::SarimaEstimationResult& result) {
    for (std::size_t b = 0; b < 4; ++b) {
        for (std::size_t j = 0; j < q.blocks[b].fixed.size(); ++j) {
            if (q.blocks[b].fixed[j] == 0) continue;
            assert(result.coefficients[b].size() == q.blocks[b].initial.size());
            // Bit-exact, signed zero included (M9-H §9.3 landing contract).
            assert(result.coefficients[b][j] == q.blocks[b].initial[j]);
            assert(std::signbit(result.coefficients[b][j]) ==
                   std::signbit(q.blocks[b].initial[j]));
        }
    }
}

void check_landing_agreement(const char* id, std::vector<double> y,
        std::array<std::vector<double>, 4> initial,
        std::array<std::vector<std::uint8_t>, 4> fixed) {
    const auto base = request(std::move(y), std::move(initial), std::move(fixed),
        kalos::EstimationSolver::Bfgs);
    auto lm_request = base;
    lm_request.control.solver = kalos::EstimationSolver::PinnedLevenbergMarquardt;
    const auto lm = kalos::estimate_sarima_fixed(lm_request, {});
    assert(lm.status == "converged");
    assert(lm.converged);
    const auto bfgs = kalos::estimate_sarima_fixed(base, {});
    // Route sensitivity at the near-unit-root knife-edge (M9 seam memo,
    // sarima13-bound 3.6e-07 class): with the SLICOT stationary init
    // (m >= 8 gate) the reference BFGS stalls 4.3e-9 ll below its QR-route
    // landing (rel 2.4e-11) on fixed-ar-nearlimit, while the pinned-LM
    // landing is bit-identical between routes and every budget below still
    // binds with margin (surface gap 4.273e-09 vs 1e-4). The reference
    // optimizer's termination status at this geometry is route-dependent;
    // its answer is not.
    assert(bfgs.status == "converged" || bfgs.status == "stalled");
    check_fixed_slot(base, lm);
    check_fixed_slot(base, bfgs);

    const double lm_ll = surface_ll(lm_request, lm);
    const double bfgs_ll = surface_ll(base, bfgs);
    // Wrong-slot differencing (§7.4) moved the landing by >= 1e-1 on the
    // mixed pinned matrix; legitimate route divergence sits at <= 1e-9 here
    // (M9-H probe: fixed-ma-middle 7 hex-digit ulp, fixed-ar-nearlimit 1).
    const double ll_gap = std::abs(lm_ll - bfgs_ll);
    assert(ll_gap < 1e-4);
    for (std::size_t b = 0; b < 4; ++b) {
        assert(lm.coefficients[b].size() == bfgs.coefficients[b].size());
        for (std::size_t j = 0; j < lm.coefficients[b].size(); ++j) {
            const double ref = std::abs(bfgs.coefficients[b][j]);
            assert(std::abs(lm.coefficients[b][j] - bfgs.coefficients[b][j]) <
                   1e-4 * (1.0 + ref)); // the defect class was 0.51
        }
    }
    // Eval-count direction (M9-H §5): the pinned machine never costs more.
    const long long lm_evals = total_evaluations(lm);
    const long long bfgs_evals = total_evaluations(bfgs);
    assert(lm_evals < bfgs_evals);
    std::printf("  %s: ok (surface_ll gap %.3e, evals %lld < %lld, lm ll %a)\n", id,
        ll_gap, lm_evals, bfgs_evals, lm.log_likelihood);
}

void test_end_to_end() {
    // The qualification lane is single-threaded; landings are deterministic
    // run-to-run under that setting (drivers/determinism_check.py, 36/36).
#ifdef _WIN32
    assert(_putenv_s("KALOS_NUM_THREADS", "1") == 0);
    assert(_putenv_s("OMP_NUM_THREADS", "1") == 0);
    assert(_putenv_s("OPENBLAS_NUM_THREADS", "1") == 0);
#else
    assert(setenv("KALOS_NUM_THREADS", "1", 1) == 0);
    assert(setenv("OMP_NUM_THREADS", "1", 1) == 0);
    assert(setenv("OPENBLAS_NUM_THREADS", "1", 1) == 0);
#endif
    const auto y = airline_series(180, 100.0, 0.6, 42);

    // Middle-slot fixed mask [free, fixed 0.2, free]: reduced coordinates
    // [theta1, theta3, btheta1] -> full slots [0, 2, 3], so two of the three
    // differencing probes must thread reduced j through seam->free().
    check_landing_agreement("fixed-ma-middle", y,
        {{ {}, {0.0, 0.2, 0.0}, {}, {0.0} }}, {{ {}, {0, 1, 0}, {}, {0} }});
    // Fixed AR 0.9995 with a free partner (the near-unit-root geometry):
    // reduced [phi2, theta1, btheta1] -> full slots [1, 2, 3].
    check_landing_agreement("fixed-ar-nearlimit", y,
        {{ {0.9995, 0.0}, {0.0}, {}, {0.0} }}, {{ {1, 0}, {0}, {}, {0} }});

    // free_count == 0 keeps the pinned likelihood-only short circuit and the
    // coefficients come back exactly as pinned (M9-H §9.3), LM route.
    {
        const std::array<std::vector<double>, 4> initial{
            {{0.3}, {0.2}, {}, {-0.3}}};
        const std::array<std::vector<std::uint8_t>, 4> fixed{
            {{1}, {1}, {}, {1}}};
        const auto q = request(y, initial, fixed,
            kalos::EstimationSolver::PinnedLevenbergMarquardt);
        const auto result = kalos::estimate_sarima_fixed(q, {});
        assert(result.status == "likelihood_only");
        assert(!result.converged);
        assert(total_evaluations(result) == 0);
        check_fixed_slot(q, result);
        assert(result.coefficients[0][0] == 0.3);
        assert(result.coefficients[1][0] == 0.2);
        assert(result.coefficients[3][0] == -0.3);
        assert(std::signbit(result.coefficients[3][0]));
        std::printf("  all-fixed: ok (likelihood_only, coefficients pinned)\n");
    }
}

} // namespace

int main() {
    test_expand();
    test_check_boundaries();
    test_validate_fully_free();
    test_validate_fstabilize();
    test_end_to_end();
    std::printf("test_ssq_fixed_seam: all checks passed\n");
    return 0;
}
