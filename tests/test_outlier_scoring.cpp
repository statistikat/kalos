// Unit-level qualification of the 21-C outlier-scoring layer: the pinned
// critical-value chain (Utility.intProbability / intProbabilityInverse,
// CriticalValueComputer, X13Utility.calcCv), the outlier factories, the
// ConcentratedLikelihood.tstats(0, false) recipe, the ExactSingleOutlierDetector
// scan unit and the ExactOutliersDetector driver loop. The pinned bytecode is
// qualified in docs/contracts/outlier-scoring-contract.md; the corpus sweep
// (tools/verify_outlier_scoring.cpp over validation/evidence/native/
// outlier-scoring) carries the bit-level pinning of the JVM-facing values.
#include "kalos/outlier_scoring.hpp"
#include "kalos/regression_likelihood.hpp"
#include "kalos/detail/hostlineage.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using kalos::OutlierScanRequest;
using kalos::OutlierScanResult;
using kalos::OutlierScoringRequest;
using kalos::OutlierScoringResult;
using kalos::OutlierType;
using kalos::SarimaEstimationRequest;

constexpr double kInf = std::numeric_limits<double>::infinity();

std::array<std::vector<double>, 4> empty_blocks() {
    std::array<std::vector<double>, 4> blocks;
    return blocks;
}

bool close(double got, double want, double rel) {
    return std::fabs(got - want) <= rel * std::max(1.0, std::fabs(want));
}

// ---- Critical values ---------------------------------------------------------

void check_cdf() {
    assert(kalos::standard_normal_lower_cdf(0.0) == 0.5); // +0.5 return exactly.
    struct Anchor {
        double x, p;
    };
    const Anchor anchors[] = {
        {0.5, 0.6914624612740131},      // region 1
        {0.66291, 0.74630590161026},    // region 1 at THRSH
        {1.0, 0.8413447460685429},
        {-2.0, 0.02275013194817922},
        {3.0, 0.9986501019683699},      // region 2
        {5.656854249492381, 0.999999992291371}, // region 2 at ROOT32
        {6.0, 0.9999999990134123},      // region 3
        {-8.0, 6.220960574271819e-16},  // region 3 lower tail
        {-10.0, 7.619853024160593e-24},
    };
    for (const auto& a : anchors) {
        const double got = kalos::standard_normal_lower_cdf(a.x);
        const double tol = a.x < 0.0 && std::fabs(a.p) < 1e-6 ? 1e-9 : 1e-12;
        assert(std::fabs(got - a.p) <= tol * std::fabs(a.p));
    }
    for (double x = -8.0; x <= 8.0; x += 0.25) {
        assert(kalos::standard_normal_lower_cdf(x) <= kalos::standard_normal_lower_cdf(x + 0.25));
    }
    assert(std::isnan(kalos::standard_normal_lower_cdf(std::nan(""))));
    // The region-3 exp split stays finite; the tail 1.8e-33 simply rounds the
    // complement to 1.0 in double precision.
    assert(kalos::standard_normal_lower_cdf(12.0) == 1.0);
    assert(kalos::standard_normal_lower_cdf(-12.0) > 0.0);
    std::printf("  cdf: ok\n");
}

void check_quantile() {
    struct Anchor {
        double p, x;
    };
    const Anchor anchors[] = {
        {0.025, -1.959963984540054}, {0.1, -1.2815515655446004},
        {0.75, 0.6744897501960817},  {0.9, 1.2815515655446004},
        {0.975, 1.959963984540054},  {0.999, 3.090232306167813},
        {1e-6, -4.7534243088228987}, {0.5 + 1e-9, 2.5066281778151362e-9},
    };
    for (const auto& a : anchors) {
        const double got = kalos::standard_normal_lower_quantile(a.p);
        assert(close(got, a.x, 1e-9));
        // The pinned Newton terminates at a relative step of 1e-13.
        assert(close(kalos::standard_normal_lower_cdf(got), a.p, 1e-11));
    }
    // The pinned path solves for t = 1 - p in binary arithmetic, so the
    // mirror pair differs by the rounding of 1 - 0.975 (one ulp here).
    assert(std::fabs(kalos::standard_normal_lower_quantile(0.975) +
                kalos::standard_normal_lower_quantile(0.025)) < 1e-15);
    for (const double bad : {0.0, 1.0, -0.5, 2.0, 1e-16, 1.0 - 1e-16}) {
        try {
            kalos::standard_normal_lower_quantile(bad);
            assert(false);
        } catch (const std::invalid_argument&) {}
    }
    // A NaN passes the pinned guard (dcmpg on both bounds) and can only
    // escape through the 200-iteration loop, the pinned DStatException home.
    try {
        kalos::standard_normal_lower_quantile(std::nan(""));
        assert(false);
    } catch (const std::overflow_error&) {}
    std::printf("  quantile: ok\n");
}

void check_critical_value() {
    assert(kalos::default_critical_value(1) == 2.8); // max(2.8, 1.9599...)
    double worst_below = 0.0, worst_above = 0.0;
    for (std::size_t n = 1; n <= 512; ++n) {
        const double cv = kalos::default_critical_value(n);
        assert(cv >= 2.8);
        worst_below = std::max(worst_below, 2.8 - cv);
        worst_above = std::max(worst_above, cv - 2.8);
    }
    std::printf("  critical value: ok (max rise above the 2.8 floor: %.6g)\n", worst_above);
    (void)worst_below;
    try {
        kalos::default_critical_value(0);
        assert(false);
    } catch (const std::invalid_argument&) {}
}

// ---- Factories ---------------------------------------------------------------

void check_transitory_rate() {
    assert(kalos::transitory_change_rate(12, 0.7) == 0.7);
    assert(kalos::transitory_change_rate(4, 0.7) == kalos::detail::glibc_pow(0.7, 3));
    assert(kalos::transitory_change_rate(6, 0.7) == kalos::detail::glibc_pow(0.7, 2));
    assert(kalos::transitory_change_rate(3, 0.7) == kalos::detail::glibc_pow(0.7, 4));
    assert(kalos::transitory_change_rate(1, 0.7) == kalos::detail::glibc_pow(0.7, 12));
    assert(kalos::transitory_change_rate(2, 0.7) == kalos::detail::glibc_pow(0.7, 6));
    for (const int bad : {0, -4, 5, 7, 24}) {
        try {
            kalos::transitory_change_rate(bad, 0.7);
            assert(false);
        } catch (const std::invalid_argument& e) {
            assert(std::string(e.what()).find("frequnecy") != std::string::npos); // sic
        }
    }
}

void check_zones() {
    using kalos::OutlierZone;
    const auto zone = [](OutlierType t, int p) { return kalos::outlier_excluding_zone(t, p); };
    assert(zone(OutlierType::Additive, 4).start == 0 && zone(OutlierType::Additive, 4).end == 0);
    assert(zone(OutlierType::LevelShift, 4).start == 1 && zone(OutlierType::LevelShift, 4).end == 1);
    assert(zone(OutlierType::TransitoryChange, 4).start == 0 &&
           zone(OutlierType::TransitoryChange, 4).end == 1);
    assert(zone(OutlierType::SeasonalOutlier, 4).start == 8 &&
           zone(OutlierType::SeasonalOutlier, 4).end == 8);
    try {
        zone(OutlierType::SeasonalOutlier, 1);
        assert(false);
    } catch (const std::invalid_argument&) {}
}

void check_fills() {
    auto column = [](std::size_t n) { return std::vector<double>(n, 0.0); };
    {
        auto c = column(5);
        kalos::fill_outlier_column(OutlierType::Additive, 2, 4, 0.7, c);
        assert((c == std::vector<double>{0, 0, 1, 0, 0}));
    }
    {
        auto c = column(5);
        kalos::fill_outlier_column(OutlierType::LevelShift, 2, 4, 0.7, c);
        assert((c == std::vector<double>{-1, -1, 0, 0, 0}));
        auto d = column(5);
        kalos::fill_outlier_column(OutlierType::LevelShift, 0, 4, 0.7, d);
        assert((d == std::vector<double>{0, 0, 0, 0, 0}));
    }
    {
        // PeriodicOutlierFactory (zeroEnded): backward runs of P-1 entries of
        // -1/(P-1) then one 1.0; the position itself keeps the zero.
        const double n3 = -1.0 / 3.0;
        auto c = column(12);
        kalos::fill_outlier_column(OutlierType::SeasonalOutlier, 7, 4, 0.7, c);
        assert((c == std::vector<double>{n3, n3, n3, 1.0, n3, n3, n3, 0, 0, 0, 0, 0}));
        auto d = column(12);
        kalos::fill_outlier_column(OutlierType::SeasonalOutlier, 1, 4, 0.7, d);
        assert((d == std::vector<double>{n3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
        try {
            kalos::fill_outlier_column(OutlierType::SeasonalOutlier, 5, 1, 0.7, d);
            assert(false);
        } catch (const std::invalid_argument&) {}
    }
    {
        auto c = column(6);
        kalos::fill_outlier_column(OutlierType::TransitoryChange, 2, 4, 0.5, c);
        assert((c == std::vector<double>{0, 0, 1, 0.5, 0.25, 0.125}));
        // The ZERO 1e-15 cutoff: 2^-50 < 1e-15 stops the write after index 49.
        auto d = column(200);
        kalos::fill_outlier_column(OutlierType::TransitoryChange, 0, 4, 0.5, d);
        for (int i = 0; i <= 49; ++i) assert(d[static_cast<std::size_t>(i)] == kalos::detail::glibc_pow(0.5, i));
        for (int i = 50; i < 200; ++i) assert(d[static_cast<std::size_t>(i)] == 0.0);
        // A rate of zero writes the single 1.0 and stops.
        auto e = column(4);
        kalos::fill_outlier_column(OutlierType::TransitoryChange, 1, 4, 0.0, e);
        assert((e == std::vector<double>{0, 1, 0, 0}));
    }
    {
        auto c = column(3);
        try {
            kalos::fill_outlier_column(OutlierType::Additive, 3, 4, 0.7, c);
            assert(false);
        } catch (const std::invalid_argument&) {}
    }
    std::printf("  factories: ok\n");
}

// ---- t statistics --------------------------------------------------------------

void check_t_statistics() {
    kalos::RegressionLikelihood fit;
    fit.status = "available";
    fit.coefficients = {2.0, 3.0};
    fit.fixed = {0, 0};
    fit.unscaled_covariance = {4.0, 0.0, 0.0, 16.0}; // free 2x2, column-major
    fit.sum_squared_standardized = 1.0;
    const auto t = kalos::regression_t_statistics(fit, 1);
    assert((t == std::vector<double>{1.0, 0.75})); // coef / sqrt(covii * ssq / dim)
    // dim scaling: stdev grows as sqrt(1/dim).
    const auto t4 = kalos::regression_t_statistics(fit, 4);
    assert(t4[0] == 2.0 && t4[1] == 1.5);
    // Zero and NaN propagate through the division without special cases.
    fit.unscaled_covariance = {0.0, 0.0, 0.0, 0.0};
    fit.coefficients = {2.0, 0.0};
    const auto z = kalos::regression_t_statistics(fit, 4);
    assert(z[0] == kInf && std::isnan(z[1]));
    fit.unscaled_covariance.clear();
    const auto nan = kalos::regression_t_statistics(fit, 4);
    assert(nan.size() == 2 && std::isnan(nan[0]) && std::isnan(nan[1]));
    fit.unscaled_covariance = {4.0, 0.0, 0.0, 16.0};
    fit.status = "unavailable";
    try {
        kalos::regression_t_statistics(fit, 4);
        assert(false);
    } catch (const std::invalid_argument&) {}
    std::printf("  t statistics: ok\n");
}

// ---- Scan unit -------------------------------------------------------------------

// A 100-point response with a +100 spike at position 40 over small
// deterministic noise (the pinned Mad(50) scale needs non-degenerate
// residuals): the differenced response concentrates +100/-100 at 39/40 and the
// AO column differenced at 40 has the same shape, so the pinned scan must
// find (40, AO) with a huge |T| against O(1) noise cells.
std::vector<double> spike_series() {
    std::uint64_t state = 99887766ULL;
    std::vector<double> y(100, 0.0);
    for (std::size_t i = 0; i < 100; ++i) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        const double u = static_cast<double>(state >> 11) / 9007199254740992.0;
        y[i] = (u - 0.5) * 0.02;
    }
    y[40] = 100.0;
    return y;
}

OutlierScanRequest spike_scan(std::vector<OutlierType> types = {OutlierType::Additive}) {
    OutlierScanRequest scan;
    scan.series.values = spike_series();
    scan.series.missing.assign(100, 0);
    scan.design.columns = 0;
    scan.arma = empty_blocks(); // white noise (unit innovation variance)
    scan.period = 4;
    scan.d = 1;
    scan.seasonal_d = 0;
    scan.lower_bound = 2;
    scan.upper_bound = 98;
    scan.types = std::move(types);
    scan.transitory_rate = 0.7;
    scan.excluded.assign(100 * scan.types.size(), 0);
    return scan;
}

void check_scan() {
    { // The pinned winner: (40, AO), positive T, no new exclusions.
        const auto found = kalos::scan_outlier_candidates(spike_scan());
        assert(found.available && found.status == "ok");
        assert(found.rows == 100);
        assert(found.max_type == 0 && found.max_position == 40);
        assert(found.max_t_stat > 100.0);
        assert(found.t_table.size() == 100 && found.coefficient_table.size() == 100);
        assert(found.t_table[40] == found.max_t_stat);
        assert(found.coefficient_table[40] > 0.0);
        assert(found.excluded_after.size() == 100);
        assert(std::all_of(found.excluded_after.begin(), found.excluded_after.end(),
                [](std::uint8_t f) { return f == 0; }));
        // Out-of-bounds cells are untouched zeros; noise cells stay small.
        for (std::size_t p = 0; p < 2; ++p) assert(found.t_table[p] == 0.0);
        for (std::size_t p = 98; p < 100; ++p) assert(found.t_table[p] == 0.0);
        for (std::size_t p = 3; p < 38; ++p) assert(std::fabs(found.t_table[p]) < 10.0);
    }
    { // All four factories: the AO spike still wins (factory index 0).
        const auto found = kalos::scan_outlier_candidates(
            spike_scan({OutlierType::Additive, OutlierType::LevelShift,
                OutlierType::TransitoryChange, OutlierType::SeasonalOutlier}));
        assert(found.available && found.status == "ok");
        assert(found.max_type == 0 && found.max_position == 40);
        assert(found.t_table.size() == 400);
        // LS/TC columns see the spike too: their |T| is positive at 39/40.
        assert(std::fabs(found.t_table[40 + 100]) > 0.0);
        assert(std::fabs(found.t_table[40 + 200]) > 0.0);
    }
    { // Requested exclusions persist and the winner moves off the cell.
        auto scan = spike_scan();
        scan.excluded[40] = 1;
        const auto found = kalos::scan_outlier_candidates(scan);
        assert(found.available);
        assert(found.max_position != 40);
        assert(found.excluded_after[40] == 1);
        // The excluded cell keeps NaN in T (never recomputed).
        assert(std::isnan(found.t_table[40]));
        assert(std::isnan(found.coefficient_table[40]));
    }
    { // All cells uncomputable (Mad(50) of an exactly constant series is zero,
      // so every T is NaN): the pinned searchMax never promotes a NaN and the
      // oMax == -1 escape reports the sentinel with max_t_stat 0.
        auto scan = spike_scan();
        scan.series.values.assign(100, 5.0);
        const auto found = kalos::scan_outlier_candidates(scan);
        assert(found.available && found.status == "ok");
        assert(found.max_type == OutlierScanResult::max_type_sentinel);
        assert(found.max_position == OutlierScanResult::max_type_sentinel);
        assert(found.max_t_stat == 0.0);
    }
    { // ubound <= lbound: the pinned calc() early return.
        auto scan = spike_scan();
        scan.upper_bound = scan.lower_bound;
        const auto found = kalos::scan_outlier_candidates(scan);
        assert(!found.available && found.status == "empty_bounds");
        assert(found.t_table.size() == 100 && found.t_table[40] == 0.0);
    }
    { // warmup >= length: the pinned "Not enough observations" false.
        auto scan = spike_scan();
        scan.series.values.assign(10, 0.0);
        scan.series.missing.assign(10, 0);
        scan.period = 12;
        scan.d = 1;
        scan.seasonal_d = 1; // warmup 13 on 10 points
        scan.lower_bound = 1;
        scan.upper_bound = 9;
        scan.excluded.assign(10, 0);
        const auto found = kalos::scan_outlier_candidates(scan);
        assert(!found.available && found.status == "too_short");
    }
    { // Duplicate design columns: the pivoted QR rank test fails the scan.
        auto scan = spike_scan();
        scan.design.columns = 2;
        scan.design.values.assign(200, 1.0);
        scan.design.coefficients.assign(2, 0.0);
        scan.design.fixed.assign(2, 0);
        scan.d = 0;
        scan.lower_bound = 0;
        scan.upper_bound = 100;
        const auto found = kalos::scan_outlier_candidates(scan);
        assert(!found.available && found.status == "rank_deficient");
    }
    { // Request problems stay errors (fail-closed), not statuses.
        try {
            kalos::scan_outlier_candidates(spike_scan({}));
            assert(false);
        } catch (const std::invalid_argument&) {}
        try {
            kalos::scan_outlier_candidates(spike_scan({OutlierType::Additive, OutlierType::Additive}));
            assert(false);
        } catch (const std::invalid_argument&) {}
        {
            auto scan = spike_scan({OutlierType::SeasonalOutlier});
            scan.period = 1;
            try {
                kalos::scan_outlier_candidates(scan);
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
        {
            auto scan = spike_scan();
            scan.series.missing[7] = 1;
            try {
                kalos::scan_outlier_candidates(scan);
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
        {
            auto scan = spike_scan();
            scan.upper_bound = 101;
            try {
                kalos::scan_outlier_candidates(scan);
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
        {
            auto scan = spike_scan();
            scan.excluded.pop_back();
            try {
                kalos::scan_outlier_candidates(scan);
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
    }
    std::printf("  scan: ok\n");
}

// ---- Driver ----------------------------------------------------------------------

struct DriverFixture {
    OutlierScoringRequest request;
    // Canned per-call estimate/optimize answers.
    std::vector<std::vector<double>> estimate_t; // per call; empty vector = unavailable
    std::vector<bool> estimate_available;
    std::size_t estimate_calls = 0;
    std::size_t optimize_calls = 0;
    std::vector<std::size_t> estimate_design_columns;
    std::vector<std::size_t> optimize_design_columns;
    std::vector<double> last_optimize_start_first_block;
    std::vector<double> optimize_t;      // t returned by the warm calls
    std::vector<bool> optimize_available;
};

OutlierScoringRequest fixture_request(const DriverFixture& f) {
    OutlierScoringRequest r;
    r.base.series.values = spike_series();
    r.base.series.missing.assign(100, 0);
    r.base.design.columns = 0;
    for (auto& block : r.base.blocks) {
        block.initial.clear();
        block.fixed.clear();
    }
    r.base.period = 4;
    r.base.d = 1;
    r.base.seasonal_d = 0;
    r.base.control.eps = 1e-07;
    r.base.control.feps = 0.0001;
    r.base.control.maximum_likelihood = true;
    r.base.control.unit_root_search = false;
    r.base.control.corrected_degrees_of_freedom = true;
    r.base.control.fast_derivatives = true;
    r.base.control.starting_point = kalos::EstimationStartingPoint::Zero;
    r.initial_coefficients = empty_blocks();
    r.types = {OutlierType::Additive};
    r.critical_value = 3.0;
    r.transitory_rate = 0.7;
    r.lower_bound = 2;
    r.upper_bound = 98;
    r.max_round = 8;
    r.max_outliers = 4;
    (void)f;
    return r;
}

OutlierScoringResult run_driver(DriverFixture& f) {
    auto estimate = [&](const SarimaEstimationRequest& candidate, const kalos::Interrupt&) {
        const std::size_t call = f.estimate_calls++;
        f.estimate_design_columns.push_back(candidate.design.columns);
        kalos::OutlierEstimation est;
        est.available = f.estimate_available[std::min(call, f.estimate_available.size() - 1)];
        est.status = "stub";
        est.coefficients = empty_blocks();
        if (est.available) {
            est.t_statistics =
                f.estimate_t[std::min(call, f.estimate_t.size() - 1)];
            est.t_statistics.resize(candidate.design.columns, 4.0);
        }
        return est;
    };
    auto optimize = [&](const SarimaEstimationRequest& candidate,
                        const std::array<std::vector<double>, 4>& start, const kalos::Interrupt&) {
        ++f.optimize_calls;
        f.optimize_design_columns.push_back(candidate.design.columns);
        f.last_optimize_start_first_block = start[0];
        kalos::OutlierEstimation est;
        est.available = f.optimize_available.empty() ? true : f.optimize_available.back();
        est.status = "stub";
        est.coefficients = empty_blocks();
        if (est.available) {
            est.t_statistics = f.optimize_t;
            est.t_statistics.resize(candidate.design.columns, 4.0);
        }
        return est;
    };
    return kalos::detect_outliers(f.request, estimate, optimize, {});
}

void check_driver() {
    { // Add, keep at the outlier limit, verifymodel keeps (|t| = 4 > cv).
        DriverFixture f;
        f.request = fixture_request(f);
        f.request.max_outliers = 1;
        f.estimate_available = {true, true};
        f.estimate_t = {{}, {4.0}};
        const auto result = run_driver(f);
        assert(f.estimate_calls == 2);
        assert((f.estimate_design_columns == std::vector<std::size_t>{0, 1}));
        assert(f.optimize_calls == 0);
        assert(result.changed);
        assert(result.rounds == 1);
        assert(result.stop_reason == "max_outliers");
        assert(result.outliers.size() == 1);
        assert(result.outliers[0].position == 40);
        assert(result.outliers[0].type == OutlierType::Additive);
        assert(result.outliers[0].t_statistic > 3.0);
        assert(result.outliers[0].coefficient > 0.0);
        assert(result.rejections.empty());
    }
    { // Add rollback: the post-add estimate fails; the column is removed, the
      // warm reoptimization runs (discarded) and the run stops.
        DriverFixture f;
        f.request = fixture_request(f);
        f.estimate_available = {true, false};
        f.estimate_t = {{}, {}};
        const auto result = run_driver(f);
        assert(f.optimize_calls == 1);
        assert((f.optimize_design_columns == std::vector<std::size_t>{0}));
        assert(result.changed);
        assert(result.rounds == 1);
        assert(result.stop_reason == "estimate_failed");
        assert(result.outliers.empty());
        assert(result.rejections.size() == 1);
        assert(result.rejections[0].origin == kalos::OutlierRejection::Origin::AddRollback);
        assert(result.rejections[0].position == 40);
        assert(result.rejections[0].round == 1);
        assert(result.rejections[0].abs_t_statistic > 3.0);
    }
    { // The opening estimate failing keeps the incoming model (no adoption).
        DriverFixture f;
        f.request = fixture_request(f);
        f.request.critical_value = 1e300; // nothing passes
        f.estimate_available = {false};
        f.estimate_t = {{}};
        const auto result = run_driver(f);
        assert(result.rounds == 0);
        assert(result.stop_reason == "below_cv");
        assert(!result.changed);
        assert(f.optimize_calls == 0);
    }
    { // scan_failed: the empty bounds stop before any estimation decision.
        DriverFixture f;
        f.request = fixture_request(f);
        f.request.upper_bound = f.request.lower_bound;
        f.estimate_available = {true};
        f.estimate_t = {{}};
        const auto result = run_driver(f);
        assert(result.rounds == 0);
        assert(result.stop_reason == "scan_failed");
        assert(!result.changed);
        assert(f.estimate_calls == 1);
    }
    { // verifymodel drop: the kept outlier comes back weak (|t| = 1 < cv) and
      // is removed with the allow() and a warm re-estimation.
        DriverFixture f;
        f.request = fixture_request(f);
        f.request.max_outliers = 1;
        f.estimate_available = {true, true};
        f.estimate_t = {{}, {1.0}};
        f.optimize_available = {true};
        f.optimize_t = {};
        const auto result = run_driver(f);
        assert(result.stop_reason == "max_outliers");
        assert(result.outliers.empty());
        assert(result.rejections.size() == 1);
        assert(result.rejections[0].origin == kalos::OutlierRejection::Origin::VerifyModel);
        assert(result.rejections[0].position == 40);
        assert(result.rejections[0].abs_t_statistic == 1.0);
        assert(result.rejections[0].round == 1);
        assert(f.optimize_calls == 1);
        assert((f.optimize_design_columns == std::vector<std::size_t>{0}));
    }
    { // The added column is the factory fill at the winner (AO unit vector).
        DriverFixture f;
        f.request = fixture_request(f);
        f.request.max_outliers = 1;
        std::vector<double> seen_column;
        auto estimate = [&](const SarimaEstimationRequest& candidate, const kalos::Interrupt&) {
            if (candidate.design.columns == 1) seen_column = candidate.design.values;
            kalos::OutlierEstimation est;
            est.available = true;
            est.status = "stub";
            est.coefficients = empty_blocks();
            est.t_statistics.assign(candidate.design.columns, 4.0);
            return est;
        };
        auto optimize = [&](const SarimaEstimationRequest&,
                            const std::array<std::vector<double>, 4>&, const kalos::Interrupt&) {
            kalos::OutlierEstimation est;
            est.available = true;
            return est;
        };
        const auto result =
            kalos::detect_outliers(f.request, estimate, optimize, {});
        assert(result.outliers.size() == 1);
        assert(seen_column.size() == 100);
        assert(seen_column[40] == 1.0);
        long ones = 0;
        for (const double v : seen_column) ones += v == 1.0 ? 1 : 0;
        assert(ones == 1); // a pure AO unit column
    }
    { // Request validation: every rejection is explicit.
        DriverFixture base;
        base.request = fixture_request(base);
        base.estimate_available = {true};
        base.estimate_t = {{}};
        {
            auto r = base.request;
            r.types.clear();
            try {
                kalos::detect_outliers(r, [](const SarimaEstimationRequest&,
                                              const kalos::Interrupt&) {
                    return kalos::OutlierEstimation{};
                }, [](const SarimaEstimationRequest&, const std::array<std::vector<double>, 4>&,
                        const kalos::Interrupt&) { return kalos::OutlierEstimation{}; }, {});
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
        {
            auto r = base.request;
            r.types = {OutlierType::Additive, OutlierType::Additive};
            try {
                kalos::detect_outliers(r, [](const SarimaEstimationRequest&,
                                              const kalos::Interrupt&) {
                    return kalos::OutlierEstimation{};
                }, [](const SarimaEstimationRequest&, const std::array<std::vector<double>, 4>&,
                        const kalos::Interrupt&) { return kalos::OutlierEstimation{}; }, {});
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
        {
            auto r = base.request;
            r.base.design.columns = 1;
            r.base.design.values.assign(100, 1.0);
            r.base.design.coefficients.assign(1, 0.0);
            r.base.design.fixed.assign(1, 1);
            try {
                kalos::detect_outliers(r, [](const SarimaEstimationRequest&,
                                              const kalos::Interrupt&) {
                    return kalos::OutlierEstimation{};
                }, [](const SarimaEstimationRequest&, const std::array<std::vector<double>, 4>&,
                        const kalos::Interrupt&) { return kalos::OutlierEstimation{}; }, {});
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
        {
            auto r = base.request;
            r.max_round = 0;
            try {
                kalos::detect_outliers(r, [](const SarimaEstimationRequest&,
                                              const kalos::Interrupt&) {
                    return kalos::OutlierEstimation{};
                }, [](const SarimaEstimationRequest&, const std::array<std::vector<double>, 4>&,
                        const kalos::Interrupt&) { return kalos::OutlierEstimation{}; }, {});
                assert(false);
            } catch (const std::invalid_argument&) {}
        }
        { // A broken callback contract (t shorter than the design) is a logic_error.
            auto estimate = [](const SarimaEstimationRequest& candidate,
                                const kalos::Interrupt&) {
                kalos::OutlierEstimation est;
                est.available = true;
                est.coefficients = empty_blocks();
                if (candidate.design.columns > 0) est.t_statistics = {}; // wrong size
                return est;
            };
            auto optimize = [](const SarimaEstimationRequest&,
                               const std::array<std::vector<double>, 4>&,
                               const kalos::Interrupt&) { return kalos::OutlierEstimation{}; };
            try {
                kalos::detect_outliers(base.request, estimate, optimize, {});
                assert(false);
            } catch (const std::logic_error&) {}
        }
    }
    std::printf("  driver: ok\n");
}

// ---- Warm-start entry (optimize_sarima_from) ---------------------------------------

std::vector<double> lcg_series(std::size_t n) {
    std::uint64_t state = 20261117ULL;
    std::vector<double> values;
    values.reserve(n);
    double x = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        const double u = static_cast<double>(state >> 11) / 9007199254740992.0;
        const double e = (u - 0.5) * 2.0;
        x = 0.5 * x + e;
        values.push_back(x);
    }
    return values;
}

SarimaEstimationRequest warm_request() {
    SarimaEstimationRequest r;
    r.series.values = lcg_series(96);
    r.series.missing.assign(96, 0);
    r.design.columns = 1;
    r.design.values.assign(96, 1.0); // constant column
    r.design.coefficients.assign(1, 0.0);
    r.design.fixed.assign(1, 0);
    r.blocks[0].initial = {0.0};
    r.blocks[0].fixed = {0};
    for (std::size_t b = 1; b < 4; ++b) {
        r.blocks[b].initial.clear();
        r.blocks[b].fixed.clear();
    }
    r.period = 4;
    r.d = 0;
    r.seasonal_d = 0;
    r.control.eps = 1e-07;
    r.control.feps = 0.0001;
    r.control.maximum_likelihood = true;
    r.control.unit_root_search = false;
    r.control.corrected_degrees_of_freedom = true;
    r.control.fast_derivatives = true;
    r.control.starting_point = kalos::EstimationStartingPoint::Zero;
    return r;
}

std::array<std::vector<double>, 4> single_block(double phi) {
    std::array<std::vector<double>, 4> blocks = empty_blocks();
    blocks[0] = {phi};
    return blocks;
}

void check_warm_start() {
    const auto request = warm_request();
    const auto cold = kalos::estimate_sarima_fixed(request, {});
    assert(cold.status == "converged" && cold.converged);
    const auto warm = kalos::optimize_sarima_from(request, cold.coefficients, {});
    assert(warm.status == "converged" && warm.converged);
    assert(warm.passes.size() == 1);
    assert(warm.passes[0].stage == "estimate" && warm.passes[0].accepted);
    assert(std::fabs(warm.log_likelihood - cold.log_likelihood) < 1e-6);
    assert(std::fabs(warm.coefficients[0][0] - cold.coefficients[0][0]) < 1e-6);

    // A start with the wrong shape or an inadmissible value is rejected.
    try {
        auto start = cold.coefficients;
        start[0].push_back(0.0);
        kalos::optimize_sarima_from(request, start, {});
        assert(false);
    } catch (const std::invalid_argument&) {}
    try {
        kalos::optimize_sarima_from(request, single_block(1.5), {});
        assert(false);
    } catch (const std::invalid_argument&) {}

    // A fully fixed model reports the likelihood-only result, no passes.
    auto fixed_model = request;
    fixed_model.blocks[0].fixed = {1};
    fixed_model.blocks[0].initial = {0.3};
    const auto fixed = kalos::optimize_sarima_from(fixed_model, single_block(0.0), {});
    assert(fixed.status == "likelihood_only" && !fixed.converged);
    assert(fixed.passes.empty());
    assert(fixed.coefficients[0][0] == 0.3); // the fixed value is preserved
    assert(std::isfinite(fixed.log_likelihood));
    std::printf("  warm start: ok\n");
}

} // namespace

int main() {
    check_cdf();
    check_quantile();
    check_critical_value();
    check_transitory_rate();
    check_zones();
    check_fills();
    check_t_statistics();
    check_scan();
    check_driver();
    check_warm_start();
    std::printf("test_outlier_scoring: ok\n");
    return 0;
}
