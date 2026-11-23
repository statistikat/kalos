#include "kalos/calendar_detection.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "kalos/detail/special_functions.hpp"

namespace kalos {
namespace {

// JVM Double.compare: NaN is the largest value, -0.0 < 0.0.
int double_compare(double a, double b) {
    if (a < b) return -1;
    if (a > b) return 1;
    if (a == b) {
        if (a == 0.0) {
            const bool a_neg = std::signbit(a);
            const bool b_neg = std::signbit(b);
            if (a_neg == b_neg) return 0;
            return a_neg ? -1 : 1;
        }
        return 0;  // both NaN
    }
    return a != a ? (b != b ? 0 : 1) : -1;  // NaN sorts last
}

// TradingDaysRegressionComparator.comparator(f): the lambda is
// compare(a, b) = a absent ? 1 : b absent ? -1
//                : Double.compare(key(a), key(b))   (nulls sort last).
int comparator_compare(const CalendarModelRow& a, const CalendarModelRow& b,
                       bool selector_aicc) {
    if (!a.present) return 1;
    if (!b.present) return -1;
    return double_compare(selector_aicc ? a.aicc : a.bic,
                          selector_aicc ? b.aicc : b.bic);
}

// The one p-value of the Wald algebra: f_upper_p over ints widened exactly
// like the bytecode's i2d on (dnx, dof). A NaN result IS the jar's
// DStatException (invalid df or a NaN value reaching incBeta; the landed
// wrapper maps that throw surface to NaN) - signalled through `threw`.
double wald_p(int dnx, int dof, double f, bool& threw) {
    const double p = detail::f_upper_p(f, static_cast<double>(dnx),
                                       static_cast<double>(dof));
    if (p != p) {
        threw = true;
        return 0.0;
    }
    return p;
}

}  // namespace

int calendar_best_model(const std::vector<CalendarModelRow>& rows,
                        bool selector_aicc) {
    // The scan loop loads ests[i] before its bounds test, so an all-absent
    // array throws ArrayIndexOutOfBounds in the jar (its -1 return is
    // unreachable). Mirror the throw; machines translate it to Failed.
    std::size_t best = 0;
    while (best < rows.size() && !rows[best].present) ++best;
    if (best == rows.size()) {
        throw std::out_of_range(
            "TradingDaysRegressionComparator.bestModel over an all-null "
            "ladder (jar surface: ArrayIndexOutOfBounds -> Failed)");
    }
    for (std::size_t j = best + 1; j < rows.size(); ++j) {
        if (!rows[j].present) continue;
        if (comparator_compare(rows[best], rows[j], selector_aicc) > 0) {
            best = j;
        }
    }
    return static_cast<int>(best);
}

CalendarWaldTestResult calendar_wald_test(
    const std::vector<CalendarModelRow>& rows, double pmodel,
    double pconstraint) {
    if (rows.empty() || !rows.front().present || !rows.back().present) {
        throw std::invalid_argument(
            "TradingDaysRegressionComparator.waldTest needs present first "
            "and last rows (jar surface: NullPointerException -> Failed)");
    }
    CalendarWaldTestResult out;
    const std::size_t n = rows.size();
    const int last = static_cast<int>(n) - 1;
    const CalendarModelRow& ref = rows.back();
    const int dof = ref.degrees_of_freedom - 2;
    const double s2 = ref.ssq / static_cast<double>(dof);
    std::vector<int> nx(n, 0);
    std::vector<double> ssq(n, 0.0);
    nx[0] = rows[0].nx;
    nx[static_cast<std::size_t>(last)] = ref.nx;
    ssq[0] = rows[0].ssq;
    ssq[static_cast<std::size_t>(last)] = ref.ssq;
    for (std::size_t i = 1; i < static_cast<std::size_t>(last); ++i) {
        if (rows[i].present) {
            nx[i] = rows[i].nx;
            ssq[i] = rows[i].ssq;
        }
    }
    std::vector<double>& p = out.p;
    std::vector<double>& f_echo = out.f;
    p.assign(n, 0.0);
    f_echo.assign(n, 0.0);
    bool threw = false;
    // The p-loop covers i in [1, last] INCLUDING absent rows (their cached
    // nx = 0 / ssq = 0 feed the arithmetic, matching the jar's arrays).
    for (int i = 1; i <= last; ++i) {
        const int dnx = nx[static_cast<std::size_t>(i)] - nx[0];
        if (dnx == 0) {
            p[static_cast<std::size_t>(i)] = 1.0;
            continue;
        }
        const double f = (ssq[0] - ssq[static_cast<std::size_t>(i)]) /
                         (static_cast<double>(dnx) * s2);
        f_echo[static_cast<std::size_t>(i)] = f;
        // ifle arm with dcmpl: f <= 0 (NaN excluded, NaN enters).
        if (!(f <= 0.0)) {
            p[static_cast<std::size_t>(i)] = wald_p(dnx, dof, f, threw);
            if (threw) {
                out.failed = true;
                return out;
            }
        } else {
            p[static_cast<std::size_t>(i)] = 1.0;
        }
    }
    int best = last;
    // Descend: while (best > 0 && compare(dcmpl, p[best], pmodel) > 0).
    // dcmpl pushes 1 for NaN, so a NaN p[best] KEEPS descending; the plain
    // C++ `>` would stop. !(p <= pmodel) is the exact mirror.
    while (best > 0 && !(p[static_cast<std::size_t>(best)] <= pmodel)) --best;
    if (best > 1) {
        for (int j = best - 1; j > 0; --j) {
            const std::size_t ju = static_cast<std::size_t>(j);
            const std::size_t bu = static_cast<std::size_t>(best);
            if (p[ju] >= pmodel) continue;  // dcmpg: NaN enters the arm
            const int dnx = nx[bu] - nx[ju];
            const double f = (ssq[ju] - ssq[bu]) /
                             (static_cast<double>(dnx) * s2);
            f_echo[ju] = f;
            if (f <= 0.0) continue;  // dcmpl: NaN enters the arm
            const double q = wald_p(dnx, dof, f, threw);
            if (threw) {
                out.failed = true;
                return out;
            }
            if (!(q <= pconstraint)) best = j;  // dcmpl: NaN swaps
        }
    }
    out.best = best;
    return out;
}

CalendarFtestInfo calendar_ftest_info(const CalendarModelRow& model0,
                                      const CalendarModelRow& model_wd,
                                      const CalendarModelRow& model_td,
                                      int desc_free_parameters) {
    CalendarFtestInfo info;
    info.ss0 = model0.ssq;
    info.ss1 = model_wd.ssq;
    info.ss6 = model_td.ssq;
    const double df1 = static_cast<double>(model_wd.degrees_of_freedom -
                                           desc_free_parameters);
    const double df6 = static_cast<double>(model_td.degrees_of_freedom -
                                           desc_free_parameters);
    info.ssmc1 = info.ss1 / df1;
    info.ssmc6 = info.ss6 / df6;
    const double f1 = (info.ss0 - info.ss1) / info.ssmc1;
    // dcmpg guard against 0: !(f > 0) -> 0.0 (NaN lands on 0.0).
    if (f1 > 0.0) {
        const double p = detail::f_upper_p(f1, 1.0, df1);
        if (p != p) {
            info.failed = true;
            return info;
        }
        info.pftd1 = 1.0 - p;  // Lower = 1 - Upper, dconst_1 dsub
    }
    const double f6 = (info.ss0 - info.ss6) / (info.ssmc6 * 6.0);
    if (f6 > 0.0) {
        const double p = detail::f_upper_p(f6, 6.0, df6);
        if (p != p) {
            info.failed = true;
            return info;
        }
        info.pftd6 = 1.0 - p;
    }
    return info;
}

namespace {

// ConcentratedLikelihood.tstat(i, nhp, true) through the landed 23-E kernel
// (nhp rides on statistics.free_parameters, the denominator dim-nx-nhp arm).
// out_of_range mirrors the JVM array surface (EMPTYARRAY then [i] ->
// AIOOBE; negative or too-large i -> AIOOBE): the machines translate it to
// Failed.
double tstat_at(const TramoTestStatistics& statistics, int i,
                bool& out_of_range) {
    const int nx = static_cast<int>(statistics.coefficients.size());
    if (nx == 0 || i < 0 || i >= nx) {
        out_of_range = true;
        return 0.0;
    }
    return tramo_test_tstats(statistics, true)[static_cast<std::size_t>(i)];
}

}  // namespace

CalendarTramoUpdateEcho tramo_calendar_update(
    const CalendarTramoUpdateState& state, bool td_selected, bool lp_selected,
    bool has_lp, const CalendarCandidateHandover& candidate) {
    CalendarTramoUpdateEcho echo;
    const bool can_pre = state.adjust && state.log_transformation;
    // The jar adds the td variable but does NOT set changed for it (the
    // change is signalled by the estimation the caller performs; the
    // machine's test() treats update()'s result as authoritative).
    if (td_selected) echo.td_added = true;
    if (state.test_mean) {
        echo.mean_test_ran = true;
        bool oor = false;
        const double t0 = tstat_at(candidate.statistics, 0, oor);
        if (oor) {
            echo.failed = true;
            return echo;
        }
        echo.mean_tstat = t0;
        // keep = (dcmp(Math.abs(tstat(0, nhp, true)), tmean) > 0), dcmpl:
        // NaN -> keep. Math.abs is the IEEE abs.
        const double abs_t0 = std::fabs(t0);
        const bool keep = !(abs_t0 <= state.tmean);
        echo.mean_test_set = keep != state.mean;
        if (keep != state.mean) {
            echo.mean_after = keep;
            echo.changed = true;
        } else {
            echo.mean_after = state.mean;
        }
    } else {
        echo.mean_after = state.mean;
    }
    // The lp arm. AR/Wald gate on the SELECTED lp argument (lp_selected,
    // unguarded position); FTEST gates on (tdVar != null && lp != null) -
    // the caller expresses that through lp_selected too - with the guarded
    // position (pos < 0 -> t = 0.0). state.easter_guarded selects the
    // guarded shape.
    if (lp_selected) {
        double t = 0.0;
        if (candidate.lp_position < 0) {
            if (!state.easter_guarded) {
                echo.failed = true;  // jar: tstat(-1) -> AIOOBE -> Failed
                return echo;
            }
            t = 0.0;
        } else {
            bool oor = false;
            t = tstat_at(candidate.statistics, candidate.lp_position, oor);
            if (oor) {
                echo.failed = true;
                return echo;
            }
        }
        if (!(std::fabs(t) <= state.tlp)) {  // dcmpl: NaN enters
            if (can_pre && !(t <= 0.0)) {    // dcmpl: NaN enters preadjust
                echo.lp_action = 1;
            } else {
                echo.lp_action = 2;
            }
            echo.changed = true;
        }
    }
    (void)has_lp;
    if (state.has_easter) {
        if (candidate.easter_position < 0) {
            if (!state.easter_guarded) {
                echo.failed = true;  // jar: tstat(-1) -> AIOOBE -> Failed
                return echo;
            }
            // FTEST: the guarded arm skips the whole easter block.
        } else {
            bool oor = false;
            const double t =
                tstat_at(candidate.statistics, candidate.easter_position, oor);
            if (oor) {
                echo.failed = true;
                return echo;
            }
            if (!(std::fabs(t) <= state.teaster)) {  // dcmpl: NaN enters
                echo.easter_action = 1;
                echo.changed = true;
            }
        }
    }
    return echo;
}

TramoAutoTdDecision run_tramo_auto_td_test(const TramoAutoTdRequest& request) {
    TramoAutoTdDecision decision;
    int best = -1;
    try {
        if (request.selector == TramoAutoTdRequest::Selector::wald) {
            const CalendarWaldTestResult w = calendar_wald_test(
                request.ladder, request.pmodel, request.pconstraint);
            if (w.failed) return decision;  // DStatException -> Failed
            best = w.best;
        } else {
            best = calendar_best_model(
                request.ladder,
                request.selector == TramoAutoTdRequest::Selector::aicc);
        }
    } catch (const std::exception&) {
        return decision;  // AIOOBE / NPE surfaces -> Failed
    }
    if (best < 0 || static_cast<std::size_t>(best) >= request.candidates.size()
        || static_cast<std::size_t>(best) >= request.ladder.size()) {
        return decision;  // malformed hand-over: fail closed
    }
    decision.best = best;
    decision.selected_name_index = best;
    // tdSel = (best < 2 ? null : td[best - 2]), lpSel = (best < 1 ? null :
    // lp) - identical guards in all four ladder machines.
    const bool td_selected = best >= 2;
    const bool lp_selected = request.has_lp && best >= 1;
    decision.update = tramo_calendar_update(request.update, td_selected,
                                            lp_selected, request.has_lp,
                                            request.candidates[static_cast<std::size_t>(best)]);
    if (decision.update.failed) return decision;
    decision.result = decision.update.changed ? CalendarProcessingResult::changed
                                              : CalendarProcessingResult::unchanged;
    return decision;
}

TramoFtestTdDecision run_tramo_ftest_td_test(
    const TramoFtestTdRequest& request) {
    TramoFtestTdDecision decision;
    const CalendarCandidateHandover* none_cand = &request.candidate_none;
    if (!request.has_td) {
        // Branch q: no td field wired - update on the (null,null) model.
        decision.branch = 'q';
        decision.update =
            tramo_calendar_update(request.update, false, false,
                                  request.has_lp, *none_cand);
        if (!decision.update.failed) {
            decision.result = decision.update.changed
                                  ? CalendarProcessingResult::changed
                                  : CalendarProcessingResult::unchanged;
        }
        return decision;
    }
    decision.info = calendar_ftest_info(request.model0, request.model_wd,
                                        request.model_td,
                                        request.info_free_parameters);
    if (decision.info.failed) return decision;  // DStatException -> Failed
    const double cutoff = 1.0 - request.fpvalue;  // dconst_1 dsub
    // Branch A (both dcmpl, NaN enters): pftd6 > pftd1 && pftd6 > 1 - fp.
    const bool a = !(decision.info.pftd6 <= decision.info.pftd1)
                   && !(decision.info.pftd6 <= cutoff);
    const bool wd =
        !a && decision.info.pftd1 >= cutoff;  // dcmpg ifge: NaN stays out
    const CalendarCandidateHandover* cand = none_cand;
    if (a) {
        decision.branch = 'a';
        cand = &request.candidate_td;
    } else if (wd) {
        decision.branch = 'c';
        cand = &request.candidate_wd;
    } else {
        decision.branch = 'n';
    }
    // Branch A/C pass a non-null tdVar (the WD core under 'c'); the q/n
    // branches pass null. The lp arm gates on (tdVar != null && lp != null).
    const bool td_selected = a || wd;
    const bool lp_selected = td_selected && request.has_lp;
    decision.update = tramo_calendar_update(request.update, td_selected,
                                            lp_selected, request.has_lp, *cand);
    if (decision.update.failed) return decision;
    decision.result = decision.update.changed ? CalendarProcessingResult::changed
                                              : CalendarProcessingResult::unchanged;
    return decision;
}

SaAutoTdDecision run_sa_auto_td_test(const SaAutoTdRequest& request) {
    SaAutoTdDecision decision;
    int best = -1;
    try {
        if (request.selector == SaAutoTdRequest::Selector::wald) {
            const CalendarWaldTestResult w = calendar_wald_test(
                request.ladder, request.pmodel, request.pconstraint);
            if (w.failed) return decision;  // DStatException -> Failed
            best = w.best;
        } else {
            best = calendar_best_model(
                request.ladder,
                request.selector == SaAutoTdRequest::Selector::aicc);
        }
    } catch (const std::exception&) {
        return decision;  // AIOOBE / NPE surfaces -> Failed
    }
    if (best < 0 || static_cast<std::size_t>(best) >= request.candidates.size() ||
        static_cast<std::size_t>(best) >= request.ladder.size()) {
        return decision;  // malformed hand-over: fail closed
    }
    decision.best = best;
    decision.selected_name_index = best;
    // tdSel = (best < 2 ? null : ladder[best - 2]), lpSel = (best < 1 ||
    // lp == null ? null : lp) - the same guards as the TRAMO ladders.
    const bool td_selected = best >= 2;
    const bool lp_selected = request.has_lp && best >= 1;
    // The AIC/BIC machine logs "no trading days" when its td selection is
    // null; the Wald machine has no such constant in its pool.
    decision.no_trading_days_log =
        request.selector != SaAutoTdRequest::Selector::wald && !td_selected;
    const CalendarCandidateHandover& cand =
        request.candidates[static_cast<std::size_t>(best)];
    const SaCalendarDescWitnesses& w = request.witnesses;
    bool changed = false;
    const bool can_pre = request.adjust && request.desc_log_transformation;
    // The td walk on the CURRENT description (desc.variable("td"), core
    // equals witness against the selected ladder variable).
    if (td_selected) {
        if (w.desc_td_present) {
            const std::size_t i = static_cast<std::size_t>(best - 2);
            const bool eq = i < w.td_core_equals.size() && w.td_core_equals[i];
            if (eq) {
                decision.td_kept = true;  // equals -> no-op
            } else {
                decision.td_swapped = true;  // remove("td") + add(tdSel)
                changed = true;
            }
        } else {
            decision.td_added = true;
            changed = true;
        }
    } else if (w.desc_td_present) {
        decision.td_removed = true;
        changed = true;
    }
    // The lp arm: desc.variable("lp") is re-read into the same local after
    // the td walk; the tstat uses the CANDIDATE description's findPosition,
    // UNGUARDED (pos < 0 -> the jar's tstat AIOOBE -> Failed).
    if (lp_selected) {
        bool oor = false;
        const double t = tstat_at(cand.statistics, cand.lp_position, oor);
        if (oor) return decision;
        if (!(std::fabs(t) <= request.tlp)) {  // dcmpl: NaN enters
            if (!w.desc_lp_present) {
                if (can_pre && !(t <= 0.0)) {  // dcmpl: NaN enters
                    if (!w.desc_adjusted) {
                        decision.lp_preadjusted = true;
                        decision.early_return = 1;  // return Changed
                        decision.changed = true;
                        decision.result = CalendarProcessingResult::changed;
                        return decision;
                    }
                    // isAdjusted: 186 ifne 263 - fall through to the tail.
                } else {
                    decision.lp_added = true;
                    decision.early_return = 2;  // return Changed
                    decision.changed = true;
                    decision.result = CalendarProcessingResult::changed;
                    return decision;
                }
            } else {
                if (can_pre && !(t <= 0.0)) {
                    decision.lp_preadjusted = true;
                    decision.lp_removed_with_preadjust = true;
                    decision.early_return = 3;  // return Changed
                    decision.changed = true;
                    decision.result = CalendarProcessingResult::changed;
                    return decision;
                }
                // The pinned quirk (259): this arm RETURNS Unchanged,
                // discarding any change the td walk made.
                decision.early_return = 4;
                decision.result = CalendarProcessingResult::unchanged;
                return decision;
            }
        }
    }
    // The tail (263): remove a stale lp, else reset a stale preadjustment.
    if (w.desc_lp_present) {
        decision.lp_removed = true;
        changed = true;
    } else if (w.desc_adjusted && can_pre) {
        decision.preadjust_reset_none = true;
        changed = true;
    }
    decision.changed = changed;
    decision.result = changed ? CalendarProcessingResult::changed
                              : CalendarProcessingResult::unchanged;
    return decision;
}

}  // namespace kalos
