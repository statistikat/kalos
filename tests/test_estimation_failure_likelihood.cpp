// Stale-likelihood pinning (adaption Remaining item 3, 2026-11-27): an
// estimation chain that retains no available candidate — the diffuse states
// are unresolved at the starting point and the solver reports
// initial_unavailable — must return a MISSING log-likelihood, the quiet NaN
// this unit's failure paths already use and the estimation_control header
// documents, never an indeterminate one. The pre-fix defect: both failure
// returns left the double field unwritten, so the result carried whatever the
// reused storage last held; the re-derived repro (drivers/llrepro.cpp of the
// M9-H lane, lost in a scratch purge and rebuilt as tmp_opencode/m13-stalell)
// had a process fit zero_r11 first and then run an initial_unavailable
// estimation whose result reported the fit's -100.76884498726261 bit-for-bit.
// The geometry below is the unresolved-diffuse one already pinned in
// test_regression_likelihood (two observations under the 12-step seasonal
// differencing): every diffuse state stays unresolved, the objective is
// unavailable at the start, and the optimizer evaluates once. Both estimation
// entry points share the defect and are pinned here, and the zero_r11 success
// path (test_estimation_control corpus) is pinned unchanged with that suite's
// 5e-4 likelihood tolerance.
#include "kalos/estimation_control.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

// The zero_r11 corpus series (tests/test_estimation_control.cpp): ar(1)ma(1),
// d = 1, trend regressor, Zero start; pinned likelihood -100.76884714356329.
std::vector<double> corpus_series() {
    std::vector<double> y = {
#include "zero_r11_series.inc"
    };
    return y;
}

kalos::EstimationControl control() {
    kalos::EstimationControl k;
    k.eps = 9.9999999999999995e-08;
    k.feps = 0.0001;
    k.maximum_likelihood = true;
    k.unit_root_search = false;
    k.corrected_degrees_of_freedom = true;
    k.fast_derivatives = true;
    k.starting_point = kalos::EstimationStartingPoint::Zero;
    k.solver = kalos::EstimationSolver::Bfgs;
    return k;
}

kalos::SarimaEstimationRequest success_request() {
    const auto y = corpus_series();
    kalos::SarimaEstimationRequest r;
    r.series.values = y;
    r.series.missing.assign(y.size(), 0);
    r.design.columns = 1;
    for (int t = 1; t <= 60; ++t) r.design.values.push_back(t);
    r.design.coefficients.assign(1, 0.0);
    r.design.fixed.assign(1, 0);
    r.blocks[0].initial.assign(1, 0.0); r.blocks[0].fixed.assign(1, 0);
    r.blocks[1].initial.assign(1, 0.0); r.blocks[1].fixed.assign(1, 0);
    r.period = 12;
    r.d = 1;
    r.seasonal_d = 0;
    r.control = control();
    return r;
}

kalos::SarimaEstimationRequest failing_request() {
    kalos::SarimaEstimationRequest r;
    r.series.values = {1.0, 2.0};
    r.series.missing = {0, 0};
    r.blocks[1].initial.assign(1, 0.0); r.blocks[1].fixed.assign(1, 0);
    r.period = 12;
    r.d = 0;
    r.seasonal_d = 1;
    r.control = control();
    return r;
}

double abs_diff(double a, double b) { return std::abs(a - b); }

// One unavailable-candidate result: the producing solver status, no
// convergence, and the missing likelihood — NaN, and specifically not the
// number the prior successful fit reported.
void check_failure(const kalos::SarimaEstimationResult& result, double prior_fit_ll,
    const char* entry) {
    assert(result.status == "initial_unavailable");
    assert(!result.converged);
    assert(std::isnan(result.log_likelihood));
    assert(!(result.log_likelihood == prior_fit_ll));
    // The estimate record is the single rejected start evaluation; its own
    // likelihood is missing on the same convention.
    assert(result.passes.size() == 1);
    assert(result.passes[0].stage == "estimate");
    assert(result.passes[0].attempted);
    assert(result.passes[0].optimizer_status == "initial_unavailable");
    assert(result.passes[0].evaluations == 1);
    assert(std::isnan(result.passes[0].log_likelihood));
    (void)entry;
}

} // namespace

int main() {
    // The successful fit runs FIRST, exactly as in the repro: its likelihood
    // is the value a stale slot could carry.
    const auto good = kalos::estimate_sarima_fixed(success_request(), {});
    assert(good.status == "converged");
    assert(good.converged);
    assert(abs_diff(good.log_likelihood, -100.76884714356329) < 5e-4); // Success path unchanged.
    std::printf("  zero_r11: converged, ll %.14g (corpus pin)\n", good.log_likelihood);

    const auto failed = kalos::estimate_sarima_fixed(failing_request(), {});
    check_failure(failed, good.log_likelihood, "estimate_sarima_fixed");
    std::printf("  estimate_sarima_fixed: initial_unavailable, likelihood missing\n");

    std::array<std::vector<double>, 4> start;
    start[1].assign(1, 0.0);
    const auto warm = kalos::optimize_sarima_from(failing_request(), start, {});
    check_failure(warm, good.log_likelihood, "optimize_sarima_from");
    std::printf("  optimize_sarima_from: initial_unavailable, likelihood missing\n");

    std::printf("test_estimation_failure_likelihood: all checks passed\n");
    return 0;
}
