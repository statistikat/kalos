// Item 21-G: unit qualification of the transcribed stage-R regression
// tests and mean rules (RegressionVariablesTest.process, MeanController.
// test, the RegArimaKernel final mean rule). The corpus sweep
// (tools/verify_regression_variable_tests.cpp against
// validation/evidence/native/regression-tests) carries the full
// JVM-qualified machine over the 18 double-captured cases; this unit covers
// the parts the corpus cannot pin at the fast-lane level: request
// validation including the fail-closed rejection of the dead tdJointTest
// path, the pinned bit-exact replays of the m_allfail statistics machine
// (tstats(0, false), the derived joint statistic, the process decision) and
// of the m_meanadd mean-added t-statistic, the zero-coefficient and
// zero-covariance guards of the scalar tstat form, the strict boundary of
// the MeanController cutoff, the b/c gate and preadjustment-reset branches
// the corpus reaches only in combination, the NaN ordering of the final
// rule, and determinism. See docs/contracts/regression-tests-contract.md.
#include <kalos/regression_variable_tests.hpp>

#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

bool bit_eq(double a, double b) {
    if (std::isnan(a) && std::isnan(b)) {
        return true;
    }
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

// The m_allfail case of the corpus: white noise n = 120, mean on, TD4
// (dim 3, columns 1-3), LoP (column 4), Easter +/- 15 (column 5); the
// captured ConcentratedLikelihoodWithMissing statistics (%.17g).
kalos::RegressionTestStatistics m_allfail_stats() {
    kalos::RegressionTestStatistics s;
    s.ssq = 112.50525283570157;
    s.dim = 120;
    s.coefficients = {
        -0.010049417539263320, -0.030705460815634175, -0.055631494162720050,
        0.38883889170734390, 0.44528053983275256, 1.6397821016084348,
    };
    s.unscaled_covariance = {
        0.0092957481109528600, -5.7404930653796330e-05, 0.00043483745481641160,
        -0.00039395107552731780, -0.0021649444827234747, 9.5619291323091170e-05,
        -5.7404930653796330e-05, 0.0020414968612193986, -0.0051050513119177790,
        0.0020568922758666384, 0.0030088732747256394, 0.0014116544180045824,
        0.00043483745481641160, -0.0051050513119177790, 0.040940962503650840,
        -0.033193955309311990, -0.017996697549337654, 0.011033628370444040,
        -0.00039395107552731780, 0.0020568922758666384, -0.033193955309311990,
        0.060268934585368230, 0.024535412572890235, -0.0036960703888687906,
        -0.0021649444827234747, 0.0030088732747256394, -0.017996697549337654,
        0.024535412572890235, 0.48206451414939955, 0.00033145448024045847,
        9.5619291323091170e-05, 0.0014116544180045824, 0.011033628370444040,
        -0.0036960703888687906, 0.00033145448024045847, 0.27944152942037390,
    };
    return s;
}

std::vector<kalos::RegressionVariableLayout> m_allfail_vars() {
    using kalos::RegressionVariableKind;
    return {
        {RegressionVariableKind::trading_days, 1, 3},
        {RegressionVariableKind::length_of_period, 4, 1},
        {RegressionVariableKind::moving_holiday, 5, 1},
    };
}

kalos::RegressionVariableTestRequest make_request(
        const kalos::RegressionTestStatistics& stats,
        const std::vector<kalos::RegressionVariableLayout>& vars,
        double tmu, double ttd, double tmh, bool derived, bool mean,
        bool adjusted = false, int preadj = 0) {
    kalos::RegressionVariableTestRequest r;
    r.statistics = stats;
    r.variables = vars;
    r.mean_test = tmu;
    r.td_test = ttd;
    r.mh_test = tmh;
    r.derived_test = derived;
    r.td_joint = 0.0;
    r.mean = mean;
    r.adjusted = adjusted;
    r.preadjustment = preadj;
    return r;
}

} // namespace

int main() {
    using kalos::RegressionVariableTestRequest;

    // 1. The statistics machine, replayed bit-exactly against the m_allfail
    // capture: tstats(0, false), the mean tstat(0, 0, false) and the
    // derived joint value/statistic (the OBS w=0 row of configurations
    // 0/1).
    const auto stats = m_allfail_stats();
    const auto vars = m_allfail_vars();
    {
        const auto t = stats.tstats();
        const double want[6] = {
            -0.10764725019868254, -0.70185190696368570, -0.28395264812413057,
            1.6357885446545422, 0.66234666952779530, 3.2036487223663910,
        };
        assert(t.size() == 6);
        for (int j = 0; j < 6; ++j) {
            assert(bit_eq(t[j], want[j]));
        }
        assert(bit_eq(stats.mean_tstat(), -0.10764725019868254));
        const double tcur = kalos::derived_test_tstat(stats, 1, 3);
        assert(bit_eq(tcur, -1.7811013811016239));
        double sum = 0.0;
        for (int j = 1; j < 4; ++j) {
            sum += stats.coefficients[static_cast<std::size_t>(j)];
        }
        assert(bit_eq(-sum, -0.30250193672898973));
    }

    // 2. RegressionVariablesTest.process on m_allfail, replayed against the
    // corpus: the all-fail batch (TD fails individually and jointly, LoP
    // fails) removes td + lp, the Easter test passes and keeps its
    // variable, and the failing meanTest drops the mean - Changed with the
    // mean off.
    {
        const auto res = kalos::run_regression_variable_tests(
            make_request(stats, vars, 1.96, 1.96, 1.96, true, true));
        assert(res.changed);
        assert(!res.mean);
        assert(res.preadjustment == 0);
        assert(res.removed.size() == 3);
        assert(res.removed[0] && res.removed[1] && !res.removed[2]);
        assert(res.derived_tested[0]);
        assert(bit_eq(res.derived_tstats[0], -1.7811013811016239));
        assert(!res.derived_tested[1] && !res.derived_tested[2]);
        assert(res.accepted[2] && !res.accepted[0] && !res.accepted[1]);
    }
    // The mean-only configuration (builder without td/mh tests): only the
    // mean test runs; the calendar variables are untouched.
    {
        const auto res = kalos::run_regression_variable_tests(
            make_request(stats, vars, 1.96, 0.0, 0.0, false, true));
        assert(res.changed);
        assert(!res.mean);
        assert(!res.removed[0] && !res.removed[1] && !res.removed[2]);
        for (const bool tested : res.tested) {
            assert(!tested);
        }
    }
    // Mean off with a failing mean test: nothing happens (the machine never
    // adds a mean).
    {
        const auto res = kalos::run_regression_variable_tests(
            make_request(stats, vars, 1.96, 1.96, 1.96, true, false));
        assert(res.changed); // still changed through the calendar batch
        assert(!res.mean);
        assert(res.removed[0] && res.removed[1] && !res.removed[2]);
    }

    // 3. MeanController replay, bit-exact on the m_meanadd mean-added
    // statistics (tstat(0, 0, false) = 459.00782145713690): a significant
    // mean on a mean-off description flips the flag and changes.
    {
        kalos::RegressionTestStatistics add;
        add.coefficients = {50.022671209829270};
        add.unscaled_covariance = {0.0097536055620844170};
        add.ssq = 116.89599478780055;
        add.dim = 96;
        kalos::MeanControllerRequest mreq;
        mreq.statistics = add;
        mreq.mean = false;
        mreq.cval = 1.96;
        const auto mres = kalos::run_mean_controller(mreq);
        assert(bit_eq(mres.tstat, 459.00782145713690));
        assert(mres.changed);
        assert(mres.mean);
        // Same statistics on a mean-ON description: significant and already
        // on -> Unchanged, no flag touch.
        mreq.mean = true;
        const auto keep = kalos::run_mean_controller(mreq);
        assert(!keep.changed && keep.mean);
    }
    // The m_allfail current statistics: |t| = 0.108 -> mean removed for
    // both pinned cutoffs.
    {
        for (const double cval : {1.96, 1.6}) {
            kalos::MeanControllerRequest mreq;
            mreq.statistics = stats;
            mreq.mean = true;
            mreq.cval = cval;
            const auto mres = kalos::run_mean_controller(mreq);
            assert(mres.changed);
            assert(!mres.mean);
            assert(bit_eq(mres.tstat, -0.10764725019868254));
        }
    }
    // Strict cutoff: |t| exactly 1.96 is NOT significant (dcmpl ifle). With
    // ssq = 1, u00 = 0.25, dim = 1 the standard error is 0.5 and 0.98/0.5
    // is the exact double 1.96.
    {
        kalos::RegressionTestStatistics s;
        s.coefficients = {0.98};
        s.unscaled_covariance = {0.25};
        s.ssq = 1.0;
        s.dim = 1;
        kalos::MeanControllerRequest mreq;
        mreq.statistics = s;
        mreq.mean = true;
        mreq.cval = 1.96;
        const auto mres = kalos::run_mean_controller(mreq);
        assert(bit_eq(mres.tstat, 1.96));
        assert(mres.changed && !mres.mean); // 1.96 > 1.96 is false
    }
    // The pinned scalar-ser guards: a zero covariance entry makes ser()
    // NaN; a zero coefficient makes ser() 0 so tstat() is 0/0 - both give
    // a non-significant mean (removed from the on-state).
    {
        kalos::RegressionTestStatistics s;
        s.coefficients = {1.0, 2.0};
        s.unscaled_covariance = {0.0, 0.0, 0.0, 1.0};
        s.ssq = 1.0;
        s.dim = 10;
        kalos::MeanControllerRequest mreq;
        mreq.statistics = s;
        mreq.mean = true;
        mreq.cval = 1.96;
        const auto zero_cov = kalos::run_mean_controller(mreq);
        assert(std::isnan(zero_cov.tstat) && zero_cov.changed && !zero_cov.mean);
        s.unscaled_covariance[0] = 1.0;
        s.coefficients[0] = 0.0;
        const auto zero_coef = kalos::run_mean_controller(mreq);
        assert(std::isnan(zero_coef.tstat) && zero_coef.changed
               && !zero_coef.mean);
        // The vector form has NO guard: a zero coefficient gives t = 0, a
        // zero covariance entry gives coef / 0 = Inf, which the t test
        // counts as significant.
        s.unscaled_covariance = {1.0, 0.0, 0.0, 0.0};
        const auto t = s.tstats();
        assert(t[0] == 0.0);
        assert(std::isinf(t[1]) && t[1] > 0.0);
        assert(kalos::t_test_accepts(t, 1.96, 1.96, 1, 1, 1));
        assert(!kalos::t_test_accepts(t, 1.96, 1.96, 1, 0, 1));
    }

    // 4. The b/c gates and the adjusted preadjustment reset, isolated from
    // the corpus combinations.
    {
        // m_tdpass shape (synthetic statistics): the TD variable passes ->
        // b; the LoP variable fails; a failing moving holiday is removed
        // immediately and the mean drops - nothing else may go.
        const auto t = stats.tstats();
        assert(kalos::t_test_accepts(t, 1.96, 1.96, 1, 5, 1));

        // b-gate: td fails individually but is rescued by the derived
        // joint statistic -> no batch; lp fails and stays.
        kalos::RegressionTestStatistics s;
        s.coefficients = {0.1, 0.1, 0.05};
        s.ssq = 1.0;
        s.dim = 100;
        s.unscaled_covariance = {
            4.0, -3.999, 0.0,
            -3.999, 4.0, 0.0,
            0.0, 0.0, 1.0,
        };
        using kalos::RegressionVariableKind;
        std::vector<kalos::RegressionVariableLayout> v {
            {RegressionVariableKind::trading_days, 0, 2},
            {RegressionVariableKind::length_of_period, 2, 1},
        };
        const auto rescued = kalos::run_regression_variable_tests(
            make_request(s, v, 0.0, 1.96, 0.0, true, false));
        assert(rescued.derived_tested[0]);
        assert(std::fabs(rescued.derived_tstats[0]) > 1.96);
        assert(!rescued.changed);
        assert(!rescued.removed[0] && !rescued.removed[1]); // the b-gate holds

        // c-gate: lp passes (c), the TD variable fails with the derived
        // test off -> batch blocked, nothing removed, Unchanged.
        s.coefficients[2] = 0.5; // LoP t = 5, passes
        const auto blocked = kalos::run_regression_variable_tests(
            make_request(s, v, 0.0, 1.96, 0.0, false, false));
        assert(blocked.accepted[1]); // c was set
        assert(blocked.removed[0] == false && blocked.changed == false);

        // All fail with the adjusted description: batch fires and the
        // preadjustment resets to None.
        kalos::RegressionTestStatistics f;
        f.coefficients = {0.05, 0.05, 0.05};
        f.ssq = 1.0;
        f.dim = 100;
        f.unscaled_covariance.assign(9, 1.0);
        std::vector<kalos::RegressionVariableLayout> vf {
            {RegressionVariableKind::trading_days, 0, 2},
            {RegressionVariableKind::length_of_period, 2, 1},
        };
        const auto allfail = kalos::run_regression_variable_tests(
            make_request(f, vf, 0.0, 1.96, 0.0, true, false, true, 1));
        assert(allfail.changed);
        assert(allfail.removed[0] && allfail.removed[1]);
        assert(allfail.preadjustment == 0);
        // Not adjusted: the preadjustment survives the same batch.
        const auto unadjusted = kalos::run_regression_variable_tests(
            make_request(f, vf, 0.0, 1.96, 0.0, true, false, false, 1));
        assert(unadjusted.preadjustment == 1);
    }

    // 5. The final mean rule (calc 648-695): strict > 2.5 with dcmpl NaN
    // ordering, gated on automodelling, mean-off and checkMu.
    {
        assert(kalos::kMeanFinalCutoff == 2.5);
        assert(kalos::final_mean_rule_adds_mean(true, false, true, 2.6));
        assert(!kalos::final_mean_rule_adds_mean(true, false, true, 2.5));
        assert(!kalos::final_mean_rule_adds_mean(true, false, true,
            std::numeric_limits<double>::quiet_NaN()));
        assert(!kalos::final_mean_rule_adds_mean(false, false, true, 3.0));
        assert(!kalos::final_mean_rule_adds_mean(true, true, true, 3.0));
        assert(!kalos::final_mean_rule_adds_mean(true, false, false, 3.0));
        // The m_meanadd captured rtval (0.084) does not add the mean.
        assert(!kalos::final_mean_rule_adds_mean(true, false, true,
            0.084090848095986650));
    }

    // 6. Request validation, including the fail-closed dead paths.
    {
        bool threw = false;
        try {
            make_request(stats, vars, 1.96, 1.96, 1.96, true, true)
                .validate();
        } catch (...) {
            threw = true;
        }
        assert(!threw);

        auto bad = make_request(stats, vars, 1.96, 1.96, 1.96, true, true);
        bad.td_joint = 2.0; // dead decoder path (tdJointTest)
        threw = false;
        try {
            bad.validate();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        bad = make_request(stats, vars, 1.96, 1.96, 1.96, true, true);
        bad.variables[2].position = 6; // beyond nx
        threw = false;
        try {
            bad.validate();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        bad = make_request(stats, vars, 1.96, 1.96, 1.96, true, true);
        bad.preadjustment = 3;
        threw = false;
        try {
            bad.validate();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        bad = make_request(stats, vars, 1.96, 1.96, 1.96, true, true);
        bad.statistics.unscaled_covariance.pop_back();
        threw = false;
        try {
            bad.validate();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        kalos::MeanControllerRequest mreq;
        mreq.statistics = stats;
        mreq.mean = true;
        mreq.cval = 0.0;
        threw = false;
        try {
            mreq.validate();
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }

    // 7. Determinism (the corpus itself is double-captured across JVM
    // processes; this pins the native side).
    {
        const auto req =
            make_request(stats, vars, 1.0, 1.96, 1.96, true, true, true, 1);
        const auto a = kalos::run_regression_variable_tests(req);
        const auto b = kalos::run_regression_variable_tests(req);
        assert(a.changed == b.changed && a.mean == b.mean
               && a.preadjustment == b.preadjustment
               && a.removed == b.removed && a.accepted == b.accepted
               && a.derived_tested == b.derived_tested
               && a.tstats.size() == b.tstats.size());
        for (std::size_t j = 0; j < a.tstats.size(); ++j) {
            assert(bit_eq(a.tstats[j], b.tstats[j]));
        }
        assert(a.derived_tstats.size() == b.derived_tstats.size());
        for (std::size_t j = 0; j < a.derived_tstats.size(); ++j) {
            assert(bit_eq(a.derived_tstats[j], b.derived_tstats[j]));
        }
    }

    return 0;
}
