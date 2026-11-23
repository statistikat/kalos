#include "kalos/calendar_detection.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <stdexcept>
#include <vector>

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

// JVM dcmpl/dcmpg (the landed per-TU pattern).
int dcmpl(double a, double b) {
    if (a > b) return 1;
    if (a < b) return -1;
    if (a == b) return 0;
    return -1;  // NaN
}

int dcmpg(double a, double b) {
    if (a > b) return 1;
    if (a < b) return -1;
    if (a == b) return 0;
    return 1;  // NaN
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

namespace {

// SymmetricMatrix.lcholesky(FastMatrix) through the pinned default SPI
// provider (matrices.lapack.Cholesky, tol = 0) over the compact
// column-major storage of an n x n symmetric matrix (row increment 1,
// column increment n, start 0), in place. The storage indices mirror the
// bytecode walkers (diagonal idx10, row-walker idx11, next-column idx12)
// operation for operation, including the operand order of the update
// `storage[q] -= v * storage[p]` (v read as A[row i, column c]).
// Throw surfaces: MatrixException "m_err_chol" - a pivot d failing the
// strict dcmpg(d, -0.0) > 0 test (negative or zero; a NaN pivot passes and
// propagates through the sqrt) - is std::domain_error here. The d < tol
// zero-pivot branch is unreachable at tol = 0 (any d < 0 throws first).
void lcholesky_std(std::vector<double>& storage, int n) {
    if (n == 0) return;
    if (n == 1) {
        const double d = storage[0];
        if (dcmpl(d, -0.0) <= 0) {
            throw std::domain_error("lcholesky: m_err_chol");
        }
        storage[0] = std::sqrt(d);  // the d < 0 store-zero arm is dead
        return;
    }
    int diag = 0;              // idx10: A[i, i]
    int row_walk = 0;          // idx11: A[i, 0]
    int col_end = n;           // idx12: start of column i + 1
    for (int i = 0; diag != n * (n + 1); ++i) {
        double d = storage[static_cast<std::size_t>(diag)];
        for (int k = row_walk; k != diag; k += n) {
            const double v = storage[static_cast<std::size_t>(k)];
            d -= v * v;
        }
        if (dcmpl(d, -0.0) <= 0) {
            throw std::domain_error("lcholesky: m_err_chol");
        }
        const double l = std::sqrt(d);
        storage[static_cast<std::size_t>(diag)] = l;
        for (int k = row_walk; k != diag; k += n) {
            const double v = storage[static_cast<std::size_t>(k)];
            if (dcmpl(v, 0.0) != 0) {  // NaN enters the elimination
                for (int off = 1; diag + off < col_end; ++off) {
                    storage[static_cast<std::size_t>(diag + off)] -=
                        v * storage[static_cast<std::size_t>(k + off)];
                }
            }
        }
        const double inv = 1.0 / l;
        for (int off = 1; diag + off < col_end; ++off) {
            storage[static_cast<std::size_t>(diag + off)] *= inv;
        }
        ++row_walk;
        diag += n + 1;
        col_end += n;
        (void)i;
    }
}

// LowerTriangularMatrix.solveLx(L, DataBlock) (tol = 0, unit x-increment)
// over the column-major lower triangle `l` and the contiguous vector `x`.
// Operand order x[j] -= s * L[j, i] mirrors the bytecode. Throw surface:
// MatrixException "m_err_sing" (a zero-ish pivot with a non-NaN, not
// strictly-smaller rhs entry; dcmpg/dcmpl polarities pinned): a NaN pivot
// takes the division branch, a NaN rhs entry under a dead pivot zeroes x.
void solve_lx(const std::vector<double>& l, std::vector<double>& x, int n) {
    int diag = 0;
    for (int i = 0; i < n; ++i) {
        const double xi = x[static_cast<std::size_t>(i)];
        const double ld = l[static_cast<std::size_t>(diag)];
        if (dcmpg(std::fabs(ld), 0.0) <= 0) {  // NaN pivots enter division
            if (dcmpl(std::fabs(xi), 0.0) < 0) {
                x[static_cast<std::size_t>(i)] = 0.0;  // NaN rhs arm
            } else {
                throw std::domain_error("solveLx: m_err_sing");
            }
        } else {
            const double s = xi / ld;
            x[static_cast<std::size_t>(i)] = s;
            for (int j = i + 1, p = diag + 1; j < n; ++j, ++p) {
                x[static_cast<std::size_t>(j)] -=
                    s * l[static_cast<std::size_t>(p)];
            }
        }
        diag += n + 1;
    }
}

// DataBlock.ssq(): left fold of d += v * v skipping the non-finite
// entries (Double.isFinite guard in the bytecode).
double datablock_ssq(const std::vector<double>& x) {
    double d = 0.0;
    for (const double v : x) {
        if (std::isfinite(v)) d += v * v;
    }
    return d;
}

struct JointTestOutcome {
    double f = 0.0;
    double pvalue = 0.0;
    int df1 = 0;
    int df2 = 0;
};

// JointTest.build() over the hand-over for variableSelection(first, dim)
// and hyperParametersCount(nhp): the b[S] copy, the bvar[S, S] selection,
// the in-place lcholesky + solveLx, the ssq / |S| quadratic form, then
// f = z / (ssq / df) and the F(|S|, df) UPPER p-value (f_upper_p; a NaN
// out of it IS the jar's DStatException throw - propagated).
JointTestOutcome joint_test_build(const TramoTestStatistics& statistics,
                                  const std::vector<double>& covariance,
                                  int first, int dim, int nhp) {
    const int nx = static_cast<int>(statistics.coefficients.size());
    if (nx == 0 || dim <= 0 || first < 0 || first + dim > nx) {
        throw std::invalid_argument(
            "the JointTest variable window lies outside the hand-over "
            "(jar surface: ArrayIndexOutOfBounds)");
    }
    if (static_cast<int>(covariance.size()) != nx * nx) {
        throw std::invalid_argument(
            "the join_test arm needs the full nx x nx unscaledCovariance "
            "hand-over");
    }
    const bool select_all = first == 0 && dim == nx;  // b.length() == dim
    std::vector<double> bS(static_cast<std::size_t>(dim));
    std::vector<double> w(static_cast<std::size_t>(dim) *
                          static_cast<std::size_t>(dim));
    for (int q = 0; q < dim; ++q) {
        const int jq = select_all ? q : first + q;
        bS[static_cast<std::size_t>(q)] =
            statistics.coefficients[static_cast<std::size_t>(jq)];
        for (int r = 0; r < dim; ++r) {
            const int jr = select_all ? r : first + r;
            w[static_cast<std::size_t>(q) * static_cast<std::size_t>(dim) +
              static_cast<std::size_t>(r)] =
                covariance[static_cast<std::size_t>(jq) *
                               static_cast<std::size_t>(nx) +
                           static_cast<std::size_t>(jr)];
        }
    }
    lcholesky_std(w, dim);
    solve_lx(w, bS, dim);
    JointTestOutcome out;
    out.df1 = dim;
    out.df2 = statistics.dim - nx - nhp;  // blue = true: n - rows - hyper
    const double z = datablock_ssq(bS) / static_cast<double>(dim);
    out.f = z / (statistics.ssq / static_cast<double>(out.df2));
    out.pvalue = detail::f_upper_p(out.f, static_cast<double>(out.df1),
                                   static_cast<double>(out.df2));
    if (out.pvalue != out.pvalue) {
        throw std::domain_error(
            "JointTest F p-value: the jar's DStatException surface");
    }
    return out;
}

// The landed TRegressionTest bucket law through a bounds guard whose throw
// mirrors the jar's array surface (23-E convention: std::invalid_argument).
bool t_accepts_guarded(const TramoTestStatistics& statistics, int first,
                       int dim, double tlow, double thigh, int nsig) {
    return tramo_t_accepts(statistics, first, dim, tlow, thigh, nsig)
        .accepted;
}

}  // namespace

TramoFastCalendarStep run_tramo_fast_calendar_step(
    const TramoFastCalendarStepRequest& request) {
    TramoFastCalendarStep step;
    step.mean_after = request.desc_mean;
    bool changed = false;
    bool td_dropped = false;
    if (request.td.present) {
        // dim == 1 takes wdTest = T(tvar) (nsig 1), otherwise
        // tdTest = T(tvar, tvar) (nsig 2).
        const bool accept =
            t_accepts_guarded(request.statistics, request.td.position,
                              request.td.dim, request.tvar, request.tvar,
                              request.td.dim == 1 ? 1 : 2);
        if (!accept) td_dropped = true;
    }
    if (td_dropped && request.lp.present) {
        step.lp_test_ran = true;
        const bool accept =
            t_accepts_guarded(request.statistics, request.lp.position, 1,
                              request.tvar, request.tvar, 1);
        if (accept) {
            td_dropped = false;
            step.lp_rescued_td = true;
        } else {
            step.lp_removed = true;  // the rescue arm removes lp
            changed = true;
        }
    }
    if (td_dropped) {
        // Removes the td variable and, when wired, the lp variable.
        step.td_removed = true;
        if (request.lp.present) step.lp_removed = true;
        changed = true;
    }
    if (request.easter.present) {
        step.easter_test_ran = true;
        if (!t_accepts_guarded(request.statistics, request.easter.position, 1,
                               request.tvar, request.tvar, 1)) {
            step.easter_removed = true;
            changed = true;
        }
    }
    if (request.test_mean && request.regarima_mean) {
        step.mean_test_ran = true;
        if (!t_accepts_guarded(request.statistics, 0, 1, request.tmean,
                               request.tmean, 1)) {
            step.mean_removed = true;
            step.mean_after = false;
            changed = true;
        }
    }
    if (changed) {
        step.estimation_cleared = true;  // m.set(desc, null)
        step.result = CalendarProcessingResult::changed;
    }
    return step;
}

TramoDefaultCalendarStep run_tramo_default_calendar_step(
    const TramoDefaultCalendarStepRequest& request) {
    TramoDefaultCalendarStep step;
    step.mean_after = request.desc_mean;
    // The jar's Unprocessed guard reads the builder fields (witnessed
    // through variable_present, which a probe sets exactly when the field
    // is wired AND the test model carries the variable); meanTest == null
    // is !test_mean.
    if (!request.td.variable_present && !request.lp.variable_present &&
        !request.easter.variable_present && !request.test_mean) {
        step.unprocessed = true;
        return step;
    }
    bool changed = false;
    bool keep_td = false;
    if (request.td.variable_present && request.td.auto_identified) {
        bool accept = false;
        if (request.td.dim == 1) {
            accept = t_accepts_guarded(request.statistics, request.td.position,
                                       1, request.twd, request.twd, 1);
        } else if (request.join_test) {
            const JointTestOutcome jt =
                joint_test_build(request.statistics, request.covariance,
                                 request.td.position, request.td.dim,
                                 request.nhp);
            step.joint_test_ran = true;
            step.joint_f = jt.f;
            step.joint_pvalue = jt.pvalue;
            step.joint_df1 = jt.df1;
            step.joint_df2 = jt.df2;
            // accept: dcmpg(Math.abs(p), eps) < 0 - a NaN p ACCEPTS.
            accept = dcmpg(std::fabs(jt.pvalue), request.feps) < 0;
        } else {
            accept =
                t_accepts_guarded(request.statistics, request.td.position,
                                  request.td.dim, request.t0td, request.t1td,
                                  2);
        }
        if (accept) {
            keep_td = true;
            step.td_kept = true;
            changed = true;
        }
    }
    if (request.lp.variable_present && request.lp.auto_identified &&
        keep_td) {
        // lpTest = T(t0td) - lp rides on the t0td threshold, nsig 1.
        if (t_accepts_guarded(request.statistics, request.lp.position, 1,
                              request.t0td, request.t0td, 1)) {
            const int pos = request.lp.position;
            if (pos < 0 || static_cast<std::size_t>(pos) >=
                               request.statistics.coefficients.size()) {
                throw std::invalid_argument(
                    "the lp coefficient lies outside the hand-over "
                    "(jar surface: ArrayIndexOutOfBounds)");
            }
            const double coeff =
                request.statistics.coefficients[static_cast<std::size_t>(pos)];
            // setPreadjustment under adjust && descTest.isLogTransformation()
            // && dcmpg(cl.coefficient(pos), 0.0) > 0 (NaN adds instead).
            if (request.adjust && request.desc_test_log &&
                dcmpg(coeff, 0.0) > 0) {
                step.lp_action = 1;
            } else {
                step.lp_action = 2;
            }
            changed = true;
        }
    }
    if (request.easter.variable_present && request.easter.auto_identified) {
        // mhTest = T(teaster), nsig 1; NO keep_td guard.
        if (t_accepts_guarded(request.statistics, request.easter.position, 1,
                              request.teaster, request.teaster, 1)) {
            step.easter_action = 1;
            changed = true;
        }
    }
    if (request.test_mean && request.regarima_mean) {
        step.mean_test_ran = true;
        if (!t_accepts_guarded(request.statistics, 0, 1, request.tmean,
                               request.tmean, 1)) {
            step.mean_removed = true;
            step.mean_after = false;
            changed = true;
        }
    }
    if (changed) {
        step.estimation_cleared = true;  // m.clearEstimation()
        step.result = CalendarProcessingResult::changed;
    }
    return step;
}

CalendarJointTestResult calendar_joint_test(
    const TramoTestStatistics& statistics,
    const std::vector<double>& covariance, int first, int dim, int nhp) {
    const JointTestOutcome o =
        joint_test_build(statistics, covariance, first, dim, nhp);
    CalendarJointTestResult r;
    r.f = o.f;
    r.pvalue = o.pvalue;
    r.df1 = o.df1;
    r.df2 = o.df2;
    return r;
}

}  // namespace kalos
