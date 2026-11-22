#ifndef KALOS_TESTS_TRAMO_REGRESSION_REPLAY_H
#define KALOS_TESTS_TRAMO_REGRESSION_REPLAY_H

// Shared replay layer for the item 23-E tramo regression-tests golden
// (docs/contracts/tramo-regression-tests-contract.md). Both
// tests/test_tramo_regression_tests.cpp (ctest) and
// tools/verify_tramo_regression_tests.cpp (full-corpus sweep) drive every
// unit entry point from the captured hand-overs here and compare bit
// patterns, so the sweep exercises exactly what ctest qualifies.
#include "jvm_tramo_regression_tests.h"

#include "kalos/tramo_regression_tests.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace kalos::replay {

inline double as_double(std::uint64_t bits) {
    double v;
    std::memcpy(&v, &bits, sizeof(double));
    return v;
}

inline std::uint64_t as_bits(double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, sizeof(double));
    return b;
}

inline std::vector<double> decode(const kalos::test::JvmRtVec& v) {
    std::vector<double> out(static_cast<std::size_t>(v.n));
    for (int i = 0; i < v.n; ++i)
        out[static_cast<std::size_t>(i)] =
            as_double(v.bits[static_cast<std::size_t>(i)]);
    return out;
}

// Bit-pattern equality (NaNs compare equal when the payloads match, which
// they do: every NaN riding here is the canonical quiet NaN both sides
// produce, and the JVM echoes carry the raw bits through the generator).
inline bool bits_equal(double a, double b) {
    return as_bits(a) == as_bits(b);
}

struct Reporter {
    int failures = 0;
    int checks = 0;
    void fail(const std::string& what) {
        ++failures;
        std::puts(("FAIL " + what).c_str());
    }
};

inline void check_bits(Reporter& rep, const char* id, const std::string& field,
                       double got, double want) {
    ++rep.checks;
    if (!bits_equal(got, want)) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s: %s bits %016llx != %016llx", id,
                      field.c_str(),
                      static_cast<unsigned long long>(as_bits(got)),
                      static_cast<unsigned long long>(as_bits(want)));
        rep.fail(buf);
    }
}

inline void check_int(Reporter& rep, const char* id, const std::string& field,
                      long long got, long long want) {
    ++rep.checks;
    if (got != want) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s: %s %lld != %lld", id,
                      field.c_str(), got, want);
        rep.fail(buf);
    }
}

inline kalos::TramoTestStatistics hand_over(
    const std::vector<double>& coefficients,
    const std::vector<double>& cov_diagonal, double ssq, int dim,
    int free_parameters) {
    kalos::TramoTestStatistics s;
    s.coefficients = coefficients;
    s.cov_diagonal = cov_diagonal;
    s.ssq = ssq;
    s.dim = dim;
    s.free_parameters = free_parameters;
    return s;
}

inline kalos::TramoTestStatistics hand_over(const kalos::test::JvmRtRun& run) {
    return hand_over(decode(run.coefficients), decode(run.cov_diagonal),
                     as_double(run.ssq), run.dim, run.free_parameters);
}

// Both cl.tstats arms against the echoed vectors.
inline void check_tstats(Reporter& rep, const char* id,
                         const std::string& tag,
                         const kalos::TramoTestStatistics& s,
                         const kalos::test::JvmRtVec& addmu,
                         const kalos::test::JvmRtVec& plain) {
    if (s.coefficients.empty() && addmu.n == 0 && plain.n == 0) return;
    const std::vector<double> ta = kalos::tramo_test_tstats(s, true);
    const std::vector<double> tp = kalos::tramo_test_tstats(s, false);
    check_int(rep, id, tag + ".addmu.n",
              static_cast<long long>(ta.size()), addmu.n);
    check_int(rep, id, tag + ".plain.n",
              static_cast<long long>(tp.size()), plain.n);
    for (int i = 0; i < addmu.n && static_cast<std::size_t>(i) < ta.size();
         ++i)
        check_bits(rep, id, tag + ".addmu[" + std::to_string(i) + "]",
                   ta[static_cast<std::size_t>(i)],
                   as_double(addmu.bits[static_cast<std::size_t>(i)]));
    for (int i = 0; i < plain.n && static_cast<std::size_t>(i) < tp.size();
         ++i)
        check_bits(rep, id, tag + ".plain[" + std::to_string(i) + "]",
                   tp[static_cast<std::size_t>(i)],
                   as_double(plain.bits[static_cast<std::size_t>(i)]));
}

inline void replay_ms(Reporter& rep, const kalos::test::JvmRtMsCase& c) {
    kalos::TramoModelStatisticsRequest r;
    r.residuals = decode(c.residuals);
    r.coefficients = decode(c.coefficients);
    r.cov_diagonal = decode(c.cov_diagonal);
    r.ssq = as_double(c.ssq);
    r.dim = c.dim;
    r.degrees_of_freedom = c.dof;
    r.sigma2 = as_double(c.sigma2);
    r.annual_frequency = c.freq;
    r.free_parameters = c.free_parameters;
    r.estimation_domain = c.estimation_domain;
    r.ami_outliers = c.ami_outliers;
    if (c.refuse) {
        // D11 envelope: the unit refuses non-monthly rows (the JVM itself
        // computes them - the corpus records the quarterly row as inert
        // ground truth).
        bool threw = false;
        try {
            (void)kalos::tramo_model_statistics(r);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        ++rep.checks;
        if (!threw) rep.fail(std::string(c.id) + ": expected D11 refusal");
        return;
    }
    const kalos::TramoModelStatistics s = kalos::tramo_model_statistics(r);
    check_int(rep, c.id, "built", s.built ? 1 : 0, c.built);
    check_bits(rep, c.id, "lb", s.ljung_box, as_double(c.lb));
    check_bits(rep, c.id, "lbp", s.ljung_box_pvalue, as_double(c.lbp));
    check_bits(rep, c.id, "se", s.se, as_double(c.se));
    check_bits(rep, c.id, "bic", s.bic, as_double(c.bic));
    check_bits(rep, c.id, "slb", s.seasonal_ljung_box, as_double(c.slb));
    check_bits(rep, c.id, "slbp", s.seasonal_ljung_box_pvalue,
               as_double(c.slbp));
    check_int(rep, c.id, "lb_length", s.lb_length, c.lb_length);
    if (c.outliers_count >= 0)
        check_int(rep, c.id, "outliers_count", s.outliers_count,
                  c.outliers_count);
    if (c.observations_count >= 0)
        check_int(rep, c.id, "observations_count", s.observations_count,
                  c.observations_count);
    if (c.effective_observations_count >= 0)
        check_int(rep, c.id, "effective_observations_count",
                  s.effective_observations_count,
                  c.effective_observations_count);
    ++rep.checks;
    if (!s.built && !(std::isnan(s.ljung_box) &&
                      std::isnan(s.ljung_box_pvalue) && std::isnan(s.se) &&
                      std::isnan(s.bic) && std::isnan(s.seasonal_ljung_box) &&
                      std::isnan(s.seasonal_ljung_box_pvalue)))
        rep.fail(std::string(c.id) + ": un-built row must be all-NaN");
    check_tstats(rep, c.id, "ms",
                 hand_over(decode(c.coefficients), decode(c.cov_diagonal),
                           as_double(c.ssq), c.dim, c.free_parameters),
                 c.tstats_addmu, c.tstats_plain);
}

inline void replay_machines(Reporter& rep,
                            const kalos::test::JvmRtMachineCase& c) {
    kalos::TramoRegressionTestRequest req;
    req.kind = c.kind == 0 ? kalos::TramoRegressionTestKind::fast
                           : kalos::TramoRegressionTestKind::def;
    req.mean = c.mean != 0;
    req.period = 12;
    req.plain_trading_days = c.plain_td != 0;
    req.plain_length_of_period = c.plain_lp != 0;
    req.plain_easter = c.plain_easter != 0;
    req.auto_trading_days = c.auto_td != 0;
    req.auto_length_of_period = c.auto_lp != 0;
    req.auto_easter = c.auto_easter != 0;
    req.has_trading_days = c.has_td != 0;
    req.has_length_of_period = c.has_lp != 0;
    req.has_easter = c.has_easter != 0;
    if (c.refuse) {
        // The seam: the pinned machine runs its calendar kernel on this
        // request; the unit must refuse it naming item 21-I.3.
        for (int i = 0; i < c.nruns; ++i) {
            const kalos::test::JvmRtRun& run = c.runs[i];
            if (run.result < 0) continue;
            req.statistics = hand_over(run);
            req.mean_threshold = as_double(run.tmean);
            bool threw = false;
            try {
                (void)kalos::run_tramo_regression_tests(req);
            } catch (const std::invalid_argument& ex) {
                threw = std::string(ex.what()).find("21-I.3") !=
                        std::string::npos;
            }
            ++rep.checks;
            if (!threw)
                rep.fail(std::string(c.id) +
                         ": expected a 21-I.3 seam refusal (fail closed)");
        }
        return;
    }
    for (int i = 0; i < c.nruns; ++i) {
        const kalos::test::JvmRtRun& run = c.runs[i];
        if (run.result < 0) continue;
        req.statistics = hand_over(run);
        req.mean_threshold = as_double(run.tmean);
        req.test_mean = run.test_mean != 0;
        req.adjust = run.adjust != 0;
        req.join_test = run.join_test != 0;
        const kalos::TramoRegressionTestDecision d =
            kalos::run_tramo_regression_tests(req);
        check_int(rep, c.id, "run" + std::to_string(i) + ".result",
                  static_cast<long long>(d.result), run.result);
        check_int(rep, c.id, "run" + std::to_string(i) + ".mean",
                  d.mean ? 1 : 0, run.mean_after);
        if (run.est_after != 0)
            check_int(rep, c.id, "run" + std::to_string(i) + ".est",
                      d.estimation_cleared ? 2 : 1, run.est_after);
        if (req.test_mean && req.mean && run.tstats_addmu.n > 0) {
            check_bits(rep, c.id, "run" + std::to_string(i) + ".mean_tstat",
                       d.mean_tstat, as_double(run.tstats_addmu.bits[0]));
            check_int(rep, c.id, "run" + std::to_string(i) + ".mean_test_ran",
                      d.mean_test_ran ? 1 : 0, 1);
            // The decision follows the pinned bucket rule on coefficient 0.
            const double a = std::fabs(d.mean_tstat);
            const double th = as_double(run.tmean);
            check_int(rep, c.id, "run" + std::to_string(i) + ".accept",
                      d.mean_test_accepted ? 1 : 0, a >= th ? 1 : 0);
        }
        check_tstats(rep, c.id, "run" + std::to_string(i), req.statistics,
                     run.tstats_addmu, run.tstats_plain);
    }
}

inline void replay_pg(Reporter& rep, const kalos::test::JvmRtPgCase& c) {
    kalos::TramoRefStatsGateRequest r;
    r.pass = c.pass;
    r.round = c.round;
    r.pcr = as_double(c.pcr);
    r.changed = c.changed != 0;
    r.seasonal = c.seasonal != 0;
    r.curva = as_double(c.curva);
    r.reduce_va = as_double(c.reduce_va);
    r.outliers_detection = c.outliers_detection != 0;
    r.orders = {c.p, c.d, c.q, c.bp, c.bd, c.bq};
    r.mean = c.mean != 0;
    r.phi1 = as_double(c.phi1);
    r.bphi1 = as_double(c.bphi1);
    r.ljung_box_complement = as_double(c.lbp);
    r.standard_error = as_double(c.se);
    r.current_outliers = c.nb;
    r.reference_orders = {c.rp, c.rd, c.rq, c.rbp, c.rbd, c.rbq};
    r.reference_mean = c.rmean != 0;
    r.reference_outliers = c.nr;
    r.reference_ljung_box_complement = as_double(c.rlbp);
    r.reference_standard_error = as_double(c.rse);
    const kalos::TramoRefStatsGateResult g = kalos::tramo_ref_stats_gate(r);
    check_int(rep, c.id, "ret", g.accepted ? 1 : 0, c.ret);
    check_int(rep, c.id, "pass_after", g.pass, c.pass_after);
    check_int(rep, c.id, "round_after", g.round, c.round_after);
    check_bits(rep, c.id, "pcr_after", g.pcr, as_double(c.pcr_after));
    check_bits(rep, c.id, "curva_after", g.curva, as_double(c.curva_after));
    check_int(rep, c.id, "need_auto", g.need_auto_modelling ? 1 : 0,
              c.need_auto);
    check_int(rep, c.id, "need_outliers", g.need_outliers ? 1 : 0,
              c.need_outliers);
    check_int(rep, c.id, "orders_after.p", g.orders_after.p, c.ap);
    check_int(rep, c.id, "orders_after.d", g.orders_after.d, c.ad);
    check_int(rep, c.id, "orders_after.q", g.orders_after.q, c.aq);
    check_int(rep, c.id, "orders_after.bp", g.orders_after.bp, c.abp);
    check_int(rep, c.id, "orders_after.bd", g.orders_after.bd, c.abd);
    check_int(rep, c.id, "orders_after.bq", g.orders_after.bq, c.abq);
    check_int(rep, c.id, "mean_after", g.mean_after ? 1 : 0, c.mean_after);
    check_int(rep, c.id, "outliers_after", g.outliers_after, c.nb_after);
    check_int(rep, c.id, "remark", g.remark ? 1 : 0, c.remark_event);
    check_int(rep, c.id, "last_solution", g.last_solution ? 1 : 0,
              c.warn_event);
    if (c.nr < 0) {
        // refAuto null: every revert rule is skipped (nr > nb path), the
        // stored reference row becomes the current one.
        check_int(rep, c.id, "null_ref_not_reverted", g.reverted ? 1 : 0, 0);
    }
    // The pg refLbp/refSe echo is the surviving stored ModelStatistics row:
    // the current row when the reference was replaced (keep/null-entry),
    // the stored one when reverted.
    check_bits(rep, c.id, "ref_lbp_after",
               g.reverted ? as_double(c.ref_pvalue) : as_double(c.cur_pvalue),
               as_double(c.ref_lbp_after));
    check_bits(rep, c.id, "ref_se_after",
               g.reverted ? as_double(c.ref_se) : as_double(c.cur_se),
               as_double(c.ref_se_after));
}

// The checkMu inputs of the pinned TramoSpec constants (jshell witness
// against jdplus-tramoseats-base-api-3.9.0.jar: isUsingAutoModel() true
// exactly for TR3, TR4, TR5, TRfull; RegressionSpec.getMean().isTest()
// false for every constant, the mean-test side of the disjunction comes
// from the user request).
inline bool tramo_spec_using_auto_model(const char* spec) {
    const std::string s(spec);
    return s == "TR3" || s == "TR4" || s == "TR5" || s == "TRfull";
}

inline void replay_am(Reporter& rep, const kalos::test::JvmRtAmCase& c) {
    check_int(rep, c.spec, "checkMu",
              kalos::tramo_ami_check_mu(tramo_spec_using_auto_model(c.spec),
                                        false)
                  ? 1
                  : 0,
              c.check_mu);
    // The mean-test side of the disjunction, qualified on its own.
    check_int(rep, c.spec, "checkMu.meanTest",
              kalos::tramo_ami_check_mu(false, true) ? 1 : 0, 1);
}

// The non-corpus envelope/decision units (retry ladder, round-0 rule,
// reduceVa/last-solution helpers, T bucket rule, fail-closed boundaries).
inline void replay_synthetic(Reporter& rep) {
    using namespace kalos;

    // tramo_round0_accepts: acceptAirline && dcmpg(1 - p, limit) < 0; the
    // dcmpg ordering means a NaN p-value does NOT accept.
    ++rep.checks;
    if (!tramo_round0_accepts(0.9, 0.4, true))
        rep.fail("round0 accept");                       // 1 - 0.9 < 0.4
    if (tramo_round0_accepts(0.1, 0.4, true))
        rep.fail("round0 keep side");                    // 1 - 0.1 > 0.4
    if (tramo_round0_accepts(0.5, 0.4, false))
        rep.fail("round0 gate off");
    if (tramo_round0_accepts(std::nan(""), 0.4, true))
        rep.fail("round0 NaN must not accept");

    // reduceVa arms (Math.max propagates NaN; floor at 2).
    {
        const TramoReduceVa r = tramo_reduce_va(2.0, 0.12);
        if (r.changed || !bits_equal(r.curva, 2.0)) rep.fail("reduceVa 2.0");
        const TramoReduceVa r2 = tramo_reduce_va(3.5, 0.12);
        if (!r2.changed || !bits_equal(r2.curva, 3.5 * (1.0 - 0.12)))
            rep.fail("reduceVa 3.5");
        const TramoReduceVa r3 = tramo_reduce_va(std::nan(""), 0.12);
        if (!r3.changed || !std::isnan(r3.curva))
            rep.fail("reduceVa NaN propagates");
        const TramoReduceVa r4 = tramo_reduce_va(2.0000001, 0.9);
        if (!r4.changed || !bits_equal(r4.curva, 2.0))
            rep.fail("reduceVa floor 2.0");
    }

    // lastSolution orders (p := 3; bp := 0 if bd > 0; q := 1;
    // bq := 1 if seasonal; d, bd pass through).
    {
        const TramoOrders o{1, 1, 1, 1, 1, 1};
        const TramoOrders s = tramo_last_solution_orders(o, true);
        if (s.p != 3 || s.d != 1 || s.q != 1 || s.bp != 0 || s.bd != 1 ||
            s.bq != 1)
            rep.fail("lastSolution seasonal bd>0");
        const TramoOrders ns = tramo_last_solution_orders(o, false);
        if (ns.p != 3 || ns.q != 1 || ns.bp != 0 || ns.bq != 1)
            rep.fail("lastSolution non-seasonal");
        const TramoOrders nbd{0, 0, 0, 2, 0, 0};
        const TramoOrders s2 = tramo_last_solution_orders(nbd, true);
        if (s2.p != 3 || s2.bp != 2 || s2.bq != 1)
            rep.fail("lastSolution bd==0 keeps bp");
    }

    // estimateModel retry ladder over request sequences (the loop shape;
    // see the contract for its partial qualification).
    {
        TramoEstimateModelRetryRequest q;
        q.pass = 1;
        q.need_outliers_desc = true;
        q.need_auto_desc = true;
        q.curva = 3.5;
        q.reduce_va = 0.12;
        q.estimation_results = {false};
        const TramoEstimateModelRetry f = tramo_estimate_model_retry(q);
        if (f.completed || f.estimations_consumed != 1 || f.tests_consumed != 0 ||
            !f.reduce.changed || !bits_equal(f.reduce.curva, 3.5 * (1.0 - 0.12)))
            rep.fail("retry failure reduceVa arm");
        q.pass = 0;
        const TramoEstimateModelRetry g = tramo_estimate_model_retry(q);
        if (g.completed || g.reduce.changed)
            rep.fail("retry failure without pass==1");
        TramoEstimateModelRetryRequest p0;
        p0.pass = 0;
        p0.estimation_results = std::vector<bool>(6, true);
        p0.test_results = std::vector<bool>(5, false);
        const TramoEstimateModelRetry w = tramo_estimate_model_retry(p0);
        if (!w.completed || w.tests_consumed != 5 ||
            w.estimations_consumed != 6)
            rep.fail("retry five-failure completion");
        p0.test_results = {false, true};
        p0.estimation_results = std::vector<bool>(3, true);
        const TramoEstimateModelRetry h = tramo_estimate_model_retry(p0);
        if (!h.completed || h.tests_consumed != 2 || h.estimations_consumed != 2)
            rep.fail("retry test success short-circuit");
        TramoEstimateModelRetryRequest p2;
        p2.pass = 2;
        p2.estimation_results = {true};
        const TramoEstimateModelRetry k = tramo_estimate_model_retry(p2);
        if (!k.completed || k.tests_consumed != 0)
            rep.fail("retry pass!=0 first success");
        TramoEstimateModelRetryRequest ex;
        ex.pass = 0;
        ex.estimation_results = {true};
        ex.test_results = {false};
        bool threw = false;
        try {
            (void)tramo_estimate_model_retry(ex);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) rep.fail("retry exhaustion must throw");
    }

    // Fail-closed calendar boundary on both machines.
    {
        TramoRegressionTestRequest fast;
        fast.auto_trading_days = true;
        bool threw = false;
        try {
            (void)run_tramo_regression_tests(fast);
        } catch (const std::invalid_argument& ex) {
            threw = std::string(ex.what()).find("21-I.3") != std::string::npos;
        }
        if (!threw) rep.fail("fast auto-td refusal");
        fast.auto_trading_days = false;
        fast.auto_easter = true;
        threw = false;
        try {
            (void)run_tramo_regression_tests(fast);
        } catch (const std::invalid_argument& ex) {
            threw = std::string(ex.what()).find("21-I.3") != std::string::npos;
        }
        if (!threw) rep.fail("fast auto-easter refusal");
        fast.auto_easter = false;
        fast.auto_moving_holiday = true;
        threw = false;
        try {
            (void)run_tramo_regression_tests(fast);
        } catch (const std::invalid_argument& ex) {
            threw = std::string(ex.what()).find("fail closed") !=
                    std::string::npos;
        }
        if (!threw) rep.fail("fast auto-moving-holiday refusal");
        fast.auto_moving_holiday = false;
        TramoRegressionTestRequest def;
        def.kind = TramoRegressionTestKind::def;
        def.has_length_of_period = true;
        threw = false;
        try {
            (void)run_tramo_regression_tests(def);
        } catch (const std::invalid_argument& ex) {
            threw = std::string(ex.what()).find("21-I.3") != std::string::npos;
        }
        if (!threw) rep.fail("default calendar-field refusal");
        def.has_length_of_period = false;
        def.test_mean = false;
        const TramoRegressionTestDecision inert =
            run_tramo_regression_tests(def);
        if (inert.result != TramoRegressionTestResult::unprocessed)
            rep.fail("default without mean test: Unprocessed");
    }

    // Envelope refusals.
    {
        TramoModelStatisticsRequest r;
        r.annual_frequency = 4;
        r.residuals = {1.0, 2.0};
        bool threw = false;
        try {
            (void)tramo_model_statistics(r);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) rep.fail("quarterly D11 refusal");
        r.annual_frequency = 12;
        r.residuals.clear();
        threw = false;
        try {
            (void)tramo_model_statistics(r);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) rep.fail("empty residuals refusal");
        r.residuals = {std::nan("")};
        threw = false;
        try {
            (void)tramo_model_statistics(r);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) rep.fail("non-finite residuals refusal");
    }
    {
        TramoRegressionTestRequest r;
        r.period = 4;
        bool threw = false;
        try {
            (void)run_tramo_regression_tests(r);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) rep.fail("non-monthly machine refusal");
    }

    // The T-accept bucket rule: |t| == thigh counts high (accept); a
    // threshold one ulp above the value counts in NEITHER bucket; NaN |t|
    // counts in NEITHER bucket (dcmpl semantics).
    {
        TramoTestStatistics s;
        s.coefficients = {0.5, 0.0};
        s.cov_diagonal = {1.0, 1.0};
        s.ssq = 1.0;
        s.dim = 21;  // addmu arm n = dim - nx - k = 21 - 2 - 0 = 19
        s.free_parameters = 0;
        const std::vector<double> t = tramo_test_tstats(s, true);
        const double expect = 0.5 / std::sqrt((1.0 * 1.0) / 19.0);
        if (!bits_equal(t[0], expect)) rep.fail("tstats addmu denominator");
        const std::vector<double> tp = tramo_test_tstats(s, false);
        if (!bits_equal(tp[0], 0.5 / std::sqrt(1.0 / 21.0)))
            rep.fail("tstats plain denominator");
        if (!tramo_t_accepts(s, 0, 1, expect, expect, 1).accepted)
            rep.fail("T(t) boundary accepts at equality");
        if (tramo_t_accepts(s, 0, 1, std::nextafter(expect, 2.0 * expect),
                            std::nextafter(expect, 2.0 * expect), 1)
                .accepted)
            rep.fail("T(t) just above counts in neither bucket");
        s.coefficients[0] = 0.0;  // t == 0, below both thresholds
        if (tramo_t_accepts(s, 0, 1, 1.96, 1.96, 1).accepted)
            rep.fail("t below thresholds does not accept");
        s.coefficients = {std::nan("")};
        s.cov_diagonal = {1.0};
        const TramoTAcceptance nan_acc = tramo_t_accepts(s, 0, 1, 1.0, 1.96, 1);
        if (nan_acc.accepted || nan_acc.high != 0 || nan_acc.low != 0)
            rep.fail("NaN t must count in neither bucket");
    }
}

inline void replay_all(Reporter& rep) {
    for (std::size_t i = 0; i < kalos::test::kRtMsN; ++i)
        replay_ms(rep, kalos::test::kRtMs[i]);
    for (std::size_t i = 0; i < kalos::test::kRtMachinesN; ++i)
        replay_machines(rep, kalos::test::kRtMachines[i]);
    for (std::size_t i = 0; i < kalos::test::kRtPgN; ++i)
        replay_pg(rep, kalos::test::kRtPg[i]);
    for (std::size_t i = 0; i < kalos::test::kRtAmN; ++i)
        replay_am(rep, kalos::test::kRtAm[i]);
    replay_synthetic(rep);
}

}  // namespace kalos::replay

#endif
