#ifndef KALOS_TESTS_CALENDAR_DETECTION_REPLAY_H
#define KALOS_TESTS_CALENDAR_DETECTION_REPLAY_H

// Shared replay layer for the item 21-I.3 calendar-detection golden
// (docs/contracts/calendar-detection-contract.md). Both
// tests/test_calendar_detection.cpp (ctest) and
// tools/verify_calendar_detection.cpp (full-corpus sweep) drive every unit
// entry point from the captured hand-overs here and compare bit patterns,
// so the sweep exercises exactly what ctest qualifies. Every golden double
// rides its raw bit pattern; the comparator algebra, the machine decisions,
// the FTEST Info bits, the joint-test quadratic form and the jar's
// post-state lines are compared bit-for-bit.
#include "jvm_calendar_detection.h"

#include "kalos/calendar_detection.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
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

inline void check_str(Reporter& rep, const char* id, const std::string& field,
                      const std::string& got, const std::string& want) {
    ++rep.checks;
    if (got != want)
        rep.fail(std::string(id) + ": " + field + " \"" + got +
                 "\" != \"" + want + "\"");
}

inline std::vector<double> decode(const std::uint64_t* bits, int n) {
    std::vector<double> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out[static_cast<std::size_t>(i)] =
            as_double(bits[static_cast<std::size_t>(i)]);
    return out;
}

inline int result_code(kalos::CalendarProcessingResult r) {
    switch (r) {
    case kalos::CalendarProcessingResult::changed:
        return 1;
    case kalos::CalendarProcessingResult::unchanged:
        return 0;
    case kalos::CalendarProcessingResult::failed:
        return 2;
    }
    return -1;
}

// The probe's echoVars grammar as a mutable list.
inline std::vector<std::string> split_vars(const std::string& s) {
    std::vector<std::string> out;
    std::size_t p = 0;
    while (p <= s.size()) {
        const std::size_t q = s.find(',', p);
        if (q == std::string::npos) {
            if (p < s.size())
                out.push_back(s.substr(p));
            break;
        }
        out.push_back(s.substr(p, q - p));
        p = q + 1;
    }
    return out;
}

inline std::string join_vars(const std::vector<std::string>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i)
            s += ',';
        s += v[i];
    }
    return s;
}

// "td/a:td" -> "td"
inline std::string var_kind(const std::string& entry) {
    return entry.substr(0, entry.find('/'));
}

inline void remove_kind(std::vector<std::string>& v, const std::string& kind) {
    for (std::size_t i = 0; i < v.size();) {
        if (var_kind(v[i]) == kind)
            v.erase(v.begin() + static_cast<std::ptrdiff_t>(i));
        else
            ++i;
    }
}

inline std::vector<kalos::CalendarModelRow> ladder_rows(
    const kalos::test::JvmCdRow* rows, int n) {
    std::vector<kalos::CalendarModelRow> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const kalos::test::JvmCdRow& r = rows[i];
        kalos::CalendarModelRow& m = out[static_cast<std::size_t>(i)];
        m.present = r.present != 0;
        m.aicc = as_double(r.aicc);
        m.bic = as_double(r.bic);
        m.ssq = as_double(r.ssq);
        m.nx = r.nx;
        m.degrees_of_freedom = r.dof;
    }
    return out;
}

inline kalos::TramoTestStatistics stats_from(
    const std::uint64_t* coef, int ncoef, const std::uint64_t* cov, int ncov,
    std::uint64_t ssq, int dim, int k) {
    kalos::TramoTestStatistics s;
    s.coefficients = decode(coef, ncoef);
    s.cov_diagonal = decode(cov, ncov);
    s.ssq = as_double(ssq);
    s.dim = dim;
    s.free_parameters = k;
    return s;
}

inline kalos::CalendarCandidateHandover handover_from(
    const kalos::test::JvmCdCand& c) {
    kalos::CalendarCandidateHandover h;
    h.statistics = stats_from(c.coef, c.ncoef, c.covdiag, c.ncov, c.ssq,
                              c.dim, c.k);
    h.td_position = c.td_pos;
    h.lp_position = c.lp_pos;
    h.easter_position = c.ea_pos;
    return h;
}

// The column-major square matrix (upper half zeroed; lcholesky/solveLx read
// the lower triangle only, exactly like the jar's provider).
inline std::vector<double> cov_matrix(const kalos::test::JvmCdCovRow* rows,
                                      int n) {
    std::vector<double> m(static_cast<std::size_t>(n) *
                              static_cast<std::size_t>(n),
                          0.0);
    for (int i = 0; i < n; ++i) {
        const kalos::test::JvmCdCovRow& r = rows[i];
        for (int j = 0; j < r.n && j < n; ++j) {
            const double v = as_double(r.bits[j]);
            m[static_cast<std::size_t>(j) * n + i] = v;  // column-major (i,j)
        }
    }
    return m;
}

// ---------------------------------------------------------------------------
// CM: the shared comparator algebra.
// ---------------------------------------------------------------------------
inline void replay_cmp(Reporter& rep) {
    using namespace kalos;
    for (const auto& c : kalos::test::kCdCmp) {
        const auto rows = ladder_rows(c.rows, c.nrows);
        check_int(rep, c.id, "bestaicc", calendar_best_model(rows, true),
                  c.bestaicc);
        check_int(rep, c.id, "bestbic", calendar_best_model(rows, false),
                  c.bestbic);
        if (c.wald != -2) {
            const CalendarWaldTestResult w = calendar_wald_test(
                rows, as_double(c.pmodel), as_double(c.pconstraint));
            ++rep.checks;
            if (w.failed)
                rep.fail(std::string(c.id) + ": wald failed unexpectedly");
            check_int(rep, c.id, "wald", w.best, c.wald);
        }
    }
}

// ---------------------------------------------------------------------------
// TA + SA: the ladder machines.
// ---------------------------------------------------------------------------
inline void replay_ladder(Reporter& rep) {
    using namespace kalos;
    for (const auto& c : kalos::test::kCdLadder) {
        const auto rows = ladder_rows(c.rows, c.nrows);
        if (c.sa == 0) {
            TramoAutoTdRequest req;
            req.selector = c.selector == 0
                               ? TramoAutoTdRequest::Selector::aicc
                               : (c.selector == 1
                                      ? TramoAutoTdRequest::Selector::bic
                                      : TramoAutoTdRequest::Selector::wald);
            req.pmodel = as_double(c.pmodel);
            req.pconstraint = as_double(c.pconstraint);
            req.ladder = rows;
            req.has_lp = c.has_lp != 0;
            for (int i = 0; i < c.nrows; ++i)
                req.candidates.push_back(
                    handover_from(c.cands[static_cast<std::size_t>(i)]));
            req.update.mean = c.cands[0].curmean != 0;
            req.update.log_transformation = c.cands[0].log != 0;
            req.update.test_mean = c.testmean != 0;
            req.update.adjust = c.adjust != 0;
            req.update.has_easter = c.has_easter != 0;
            const TramoAutoTdDecision d = run_tramo_auto_td_test(req);
            check_int(rep, c.id, "result", result_code(d.result), c.result);
            const int want_best = c.selector == 0
                                      ? c.bestaicc
                                      : (c.selector == 1 ? c.bestbic : c.wald);
            check_int(rep, c.id, "best", d.best, want_best);
            check_int(rep, c.id, "mean_after", d.update.mean_after ? 1 : 0,
                      c.mean_after);
            // The update echo vs the jar's post-state variables.
            auto vars = split_vars(c.vars_pre);
            if (d.update.td_added)
                vars.push_back("td/a:td");
            if (d.update.easter_action == 1)
                vars.push_back("mh/a:easter");
            // lp arms mutate no variable (preadjustment or a skipped add
            // under the pinned grammar) unless the add arm ran.
            if (d.update.lp_action == 2)
                vars.push_back("lp/a:lp");
            check_str(rep, c.id, "vars_after", join_vars(vars), c.vars_after);
            if (d.result == CalendarProcessingResult::failed)
                check_int(rep, c.id, "update_failed", d.update.failed ? 1 : 0,
                          1);
        } else {
            SaAutoTdRequest req;
            req.selector = c.selector == 0
                               ? SaAutoTdRequest::Selector::aicc
                               : (c.selector == 1
                                      ? SaAutoTdRequest::Selector::bic
                                      : SaAutoTdRequest::Selector::wald);
            req.pmodel = as_double(c.pmodel);
            req.pconstraint = as_double(c.pconstraint);
            req.tlp = as_double(c.tlp);
            req.adjust = c.adjust != 0;
            req.ladder = rows;
            for (int i = 0; i < c.nrows; ++i)
                req.candidates.push_back(
                    handover_from(c.cands[static_cast<std::size_t>(i)]));
            req.has_lp = true;
            req.desc_log_transformation = c.sa_log != 0;
            req.witnesses.desc_td_present = c.sa_td_present != 0;
            req.witnesses.desc_lp_present = c.sa_lp_present != 0;
            req.witnesses.desc_adjusted = c.sa_adj != 0;
            for (int i = 0; i < c.sa_nequals; ++i)
                req.witnesses.td_core_equals.push_back(c.sa_equals[i] != 0);
            const SaAutoTdDecision d = run_sa_auto_td_test(req);
            check_int(rep, c.id, "result", result_code(d.result), c.result);
            const int want_best = c.selector == 0
                                      ? c.bestaicc
                                      : (c.selector == 1 ? c.bestbic : c.wald);
            check_int(rep, c.id, "best", d.best, want_best);
            if (c.det_atd_best >= -1)
                check_int(rep, c.id, "det_atd_best", d.best,
                          c.det_atd_best);
            check_int(rep, c.id, "mean_after", c.mean_after,
                      c.cands[0].curmean);  // sa never tests the mean
            // Rebuild the jar's post-state variables from the decision.
            auto vars = split_vars(c.vars_pre);
            if (d.td_kept) {
                // kept as-is (attributes already carried) - no name change
            } else if (d.td_swapped) {
                remove_kind(vars, "td");
                vars.push_back("td/a:td");  // re-added with calendarAMI
            } else if (d.td_added) {
                vars.push_back("td/a:td");
            } else if (d.td_removed) {
                remove_kind(vars, "td");
            }
            if (d.lp_added)
                vars.push_back("lp/a:lp");
            if (d.lp_removed || d.lp_removed_with_preadjust)
                remove_kind(vars, "lp");
            check_str(rep, c.id, "vars_after", join_vars(vars), c.vars_after);
        }
    }
}

// ---------------------------------------------------------------------------
// FT: the TRAMO FTEST machine (Info math + the branch walk).
// ---------------------------------------------------------------------------
inline void replay_ftest(Reporter& rep) {
    using namespace kalos;
    for (const auto& c : kalos::test::kCdFtest) {
        TramoFtestTdRequest req;
        req.fpvalue = as_double(c.fpvalue);
        req.update.mean = c.info_none.curmean != 0;
        req.update.log_transformation = c.info_none.log != 0;
        req.update.test_mean = c.testmean != 0;
        req.update.adjust = c.adjust != 0;
        req.update.has_easter = c.has_easter != 0;
        req.update.easter_guarded = true;
        req.has_td = req.has_wd = req.has_lp = true;
        const auto mk = [](const test::JvmCdCand& cc) {
            CalendarModelRow r;
            r.present = true;
            r.ssq = as_double(cc.ssq);
            r.nx = cc.nx;
            r.degrees_of_freedom = cc.dof;
            return r;
        };
        req.model0 = mk(c.info_none);
        req.model_wd = mk(c.info_wd);
        req.model_td = mk(c.info_td);
        req.info_free_parameters = c.info_none.nhp;
        req.candidate_none = handover_from(c.info_none);
        req.candidate_td = handover_from(c.upd_td);
        req.candidate_wd = handover_from(c.upd_wd);
        const TramoFtestTdDecision d = run_tramo_ftest_td_test(req);
        check_int(rep, c.id, "result", result_code(d.result), c.result);
        check_int(rep, c.id, "mean_after", d.update.mean_after ? 1 : 0,
                  c.mean_after);
        auto vars = split_vars("");  // desc starts calendar-free
        if (d.update.td_added)
            vars.push_back("td/a:td");
        if (d.update.easter_action == 1)
            vars.push_back("mh/a:easter");
        check_str(rep, c.id, "vars_after", join_vars(vars), c.vars_after);
        if (c.info_seen) {
            check_bits(rep, c.id, "info.ss0", d.info.ss0,
                       as_double(c.ss0));
            check_bits(rep, c.id, "info.ss1", d.info.ss1, as_double(c.ss1));
            check_bits(rep, c.id, "info.ss6", d.info.ss6, as_double(c.ss6));
            check_bits(rep, c.id, "info.ssmc1", d.info.ssmc1,
                       as_double(c.ssmc1));
            check_bits(rep, c.id, "info.ssmc6", d.info.ssmc6,
                       as_double(c.ssmc6));
            check_bits(rep, c.id, "info.pftd1", d.info.pftd1,
                       as_double(c.pftd1));
            check_bits(rep, c.id, "info.pftd6", d.info.pftd6,
                       as_double(c.pftd6));
            check_int(rep, c.id, "info.nhp", d.info.failed ? 1 : 0, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// FS: the FastRegressionTest calendar step (the 23-E seam, activated).
// ---------------------------------------------------------------------------
inline void replay_fast_step(Reporter& rep) {
    using namespace kalos;
    for (const auto& c : kalos::test::kCdFastStep) {
        TramoFastCalendarStepRequest req;
        req.statistics = stats_from(c.clc.coef, c.clc.ncoef, c.clc.covdiag,
                                    c.clc.ncov, c.clc.ssq, c.clc.dim, c.clc.k);
        req.nhp = c.clc.k;
        req.td.present = c.td.pos >= 0;
        req.td.position = c.td.pos;
        req.td.dim = c.td.dim;
        req.lp.present = c.lp.pos >= 0;
        req.lp.position = c.lp.pos;
        req.lp.dim = c.lp.dim;
        req.easter.present = c.mh.pos >= 0;
        req.easter.position = c.mh.pos;
        req.easter.dim = c.mh.dim;
        req.tvar = as_double(c.tvar);    // the captured 1.96 (inputs.csv)
        req.tmean = as_double(c.tmean);  // the captured 1.96 (inputs.csv)
        req.test_mean = c.testmean != 0;
        req.regarima_mean = c.desc_mean != 0;
        req.desc_mean = c.desc_mean != 0;
        const TramoFastCalendarStep s = run_tramo_fast_calendar_step(req);
        check_int(rep, c.id, "result", result_code(s.result), c.result);
        check_int(rep, c.id, "mean_after", s.mean_after ? 1 : 0, c.mean_after);
        check_int(rep, c.id, "estimation_cleared",
                  s.estimation_cleared ? 1 : 0, c.est_set ? 0 : 1);
        auto vars = split_vars(c.vars_pre);
        if (s.td_removed)
            remove_kind(vars, "td");
        if (s.lp_removed)
            remove_kind(vars, "lp");
        if (s.easter_removed)
            remove_kind(vars, "mh");
        check_str(rep, c.id, "vars_after", join_vars(vars), c.vars_after);
    }
}

// ---------------------------------------------------------------------------
// DS: the DefaultRegressionTest calendar step + JT: the JointTest form
// (replayed through the step's td/join arm).
// ---------------------------------------------------------------------------
inline void replay_default_step(Reporter& rep) {
    using namespace kalos;
    for (const auto& c : kalos::test::kCdDefaultStep) {
        TramoDefaultCalendarStepRequest req;
        req.statistics = stats_from(c.dtc.coef, c.dtc.ncoef, c.dtc.covdiag,
                                    c.dtc.ncov, c.dtc.ssq, c.dtc.dim, c.dtc.k);
        req.nhp = c.dtc.k;
        req.td.variable_present = c.td.pos >= 0;
        req.td.auto_identified = c.td.pos >= 0;  // calendarAMI on all fields
        req.td.position = c.td.pos;
        req.td.dim = c.td.dim;
        req.lp.variable_present = c.lp.pos >= 0;
        req.lp.auto_identified = c.lp.pos >= 0;
        req.lp.position = c.lp.pos;
        req.lp.dim = c.lp.dim;
        req.easter.variable_present = c.ea.pos >= 0;
        req.easter.auto_identified = c.ea.pos >= 0;
        req.easter.position = c.ea.pos;
        req.easter.dim = c.ea.dim;
        req.join_test = c.join != 0;
        req.test_mean = c.testmean != 0;
        req.tmean = as_double(c.tmean);  // the captured 1.96 (inputs.csv)
        req.adjust = c.adjust != 0;
        req.desc_test_log = c.desc_test_log != 0;
        req.regarima_mean = c.regarima_mean != 0;
        req.desc_mean = c.desc_mean != 0;
        if (req.join_test && c.td.pos >= 0)
            req.covariance = cov_matrix(c.cov, c.ncov);
        const TramoDefaultCalendarStep s = run_tramo_default_calendar_step(
            req);
        check_int(rep, c.id, "result",
                  s.unprocessed ? 3 : result_code(s.result), c.result);
        check_int(rep, c.id, "unprocessed", s.unprocessed ? 1 : 0,
                  c.result == 3 ? 1 : 0);
        check_int(rep, c.id, "mean_after", s.mean_after ? 1 : 0, c.mean_after);
        check_int(rep, c.id, "estimation_cleared",
                  s.estimation_cleared ? 1 : 0, c.est_set ? 0 : 1);
        if (c.join != 0 && c.td.pos >= 0 && c.td.dim >= 2) {
            // Bytecode (fresh javap, offsets 142-163): dim == 1 takes wdTest
            // EVEN under joinTest - the joint form then never runs inside
            // the step and its jt echo (the probe's direct `new
            // JointTest(tcl)` witness over the dtc hand-over) is consumed
            // by the public joint-test entry below.
            check_int(rep, c.id, "joint_test_ran", s.joint_test_ran ? 1 : 0,
                      1);
            check_bits(rep, c.id, "joint_f", s.joint_f, as_double(c.jt_value));
            check_bits(rep, c.id, "joint_pvalue", s.joint_pvalue,
                       as_double(c.jt_pvalue));
            check_int(rep, c.id, "joint_df1", s.joint_df1, c.jt_q);
            check_int(rep, c.id, "joint_df2", s.joint_df2, c.jt_df);
        } else {
            check_int(rep, c.id, "joint_test_skipped",
                      s.joint_test_ran ? 1 : 0, 0);
        }
        if (c.jt_q > 0) {
            // The jar's public JointTest class, built directly off the dtc
            // hand-over exactly as the probe built it (this is the only
            // route for the dim == 1 witness ds_join, which the machine's
            // wdTest routing above never runs).
            const kalos::CalendarJointTestResult jt = calendar_joint_test(
                req.statistics, cov_matrix(c.cov, c.ncov), c.td.pos, c.jt_q,
                c.dtc.k);
            check_bits(rep, c.id, "jt_f", jt.f, as_double(c.jt_value));
            check_bits(rep, c.id, "jt_pvalue", jt.pvalue,
                       as_double(c.jt_pvalue));
            check_int(rep, c.id, "jt_df1", jt.df1, c.jt_q);
            check_int(rep, c.id, "jt_df2", jt.df2, c.jt_df);
        }
        auto vars = split_vars(c.vars_pre);
        if (s.td_kept)
            vars.push_back("td/a:td");
        if (s.lp_action == 2)
            vars.push_back("lp/a:lp");
        if (s.easter_action == 1)
            vars.push_back("mh/a:easter");
        check_str(rep, c.id, "vars_after", join_vars(vars), c.vars_after);
    }
}

inline void replay_joint(Reporter& rep) {
    using namespace kalos;
    for (const auto& c : kalos::test::kCdJoint) {
        // Exactly the probe's construction: JointTest(cl)
        // .variableSelection(pos, q).hyperParametersCount(k).build().
        const kalos::CalendarJointTestResult jt = calendar_joint_test(
            stats_from(c.clc.coef, c.clc.ncoef, c.clc.covdiag, c.clc.ncov,
                       c.clc.ssq, c.clc.dim, c.clc.k),
            cov_matrix(c.cov, c.ncov), c.pos, c.q, c.k);
        check_bits(rep, c.id, "joint_f", jt.f, as_double(c.value));
        check_bits(rep, c.id, "joint_pvalue", jt.pvalue, as_double(c.pvalue));
        check_int(rep, c.id, "joint_df1", jt.df1, c.q);
        check_int(rep, c.id, "joint_df2", jt.df2, c.clc.dim - c.clc.ncoef - c.k);
    }
}

// ---------------------------------------------------------------------------
// PT/PX: the full-pipeline wiring witnesses.
// ---------------------------------------------------------------------------
inline void replay_pipe(Reporter& rep) {
    for (const auto& c : kalos::test::kCdPipe) {
        ++rep.checks;
        if (c.run_ok != 1)
            rep.fail(std::string(c.id) + ": pipeline run not OK set");
        const std::string line(c.tdspec);
        // The decoder wiring the machines read (pinned jar behaviour).
        ++rep.checks;
        if (line.find("method=") == std::string::npos)
            rep.fail(std::string(c.id) + ": missing tdspec method");
        if (c.px == 0) {
            // TRAMO: automatic, ptd = probabilityForFTest (TRfull default
            // 0.01), adjust false, rtest None.
            check_str(rep, c.id, "tdspec",
                      line, std::string("tdspec ") + c.id + " method=" +
                                c.method +
                                " automatic=true ptd=3f847ae147ae147b "
                                "adjust=false rtest=None");
        } else if (std::strcmp(c.method, "base") == 0) {
            check_str(rep, c.id, "tdspec", line,
                      std::string("x13tdspec ") + c.id +
                          " method=UNUSED automatic=false p1=0 p2=0 "
                          "adjust=true rtest=Remove");
        } else {
            ++rep.checks;
            if (line.find("automatic=true") == std::string::npos)
                rep.fail(std::string(c.id) + ": x13 auto not wired");
        }
    }
}

inline int run_calendar_detection_replay(const char* lane) {
    Reporter rep;
    replay_cmp(rep);
    replay_ladder(rep);
    replay_ftest(rep);
    replay_fast_step(rep);
    replay_default_step(rep);
    replay_joint(rep);
    replay_pipe(rep);
    std::printf("%s calendar-detection replay: %d checks, %d failures\n",
                lane, rep.checks, rep.failures);
    return rep.failures == 0 ? 0 : 1;
}

}  // namespace kalos::replay

#endif  // KALOS_TESTS_CALENDAR_DETECTION_REPLAY_H
