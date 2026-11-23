#ifndef KALOS_CALENDAR_DETECTION_HPP
#define KALOS_CALENDAR_DETECTION_HPP

// Transcription of the automatic calendar-detection layer of the pinned
// jdplus 3.9.0 stack (item 21-I.3 of docs/calendar-roadmap.md): the shared
// ladder machine TradingDaysRegressionComparator (test layout as a pure
// selection over hand-over rows, waldTest, bestModel), the TRAMO machines
// AutomaticRegressionTest (AIC/BIC), AutomaticWaldRegressionTest,
// AutomaticFRegressionTest (the FTEST Info math and its three-branch walk)
// and the calendar branches of FastRegressionTest / DefaultRegressionTest
// (the 23-E seam rows, activated here), and the X-13 side machines
// AutomaticTradingDaysRegressionTest (AIC/BIC) and
// AutomaticTradingDaysWaldTest.

// Fork boundary (docs/contracts/calendar-detection-contract.md): the TRAMO
// machines read TramoModelBuilder.calendarAMI variables while the sa
// machines read ModelBuilder.calendarAMI ones; the two families mutate the
// description under test with DIFFERENT grammars (TRAMO: unconditional
// addVariable of the selected td, a mean test and an easter test; sa: a
// keep/swap/add/remove walk over the existing "td" variable with
// core-equality, an lp branch with early returns, no mean test and no
// easter test). Both families are transcribed here, no fork is merged and
// no second trading-day column implementation is added (register I-4): the
// columns of every variable come from calendar_generation.hpp, the
// estimation results feeding every decision are REQUIRED REQUEST DATA
// (21-G precedent) and only the decision grammar plus the pure algebra
// (info math, Wald ratios, t/F statistics over hand-overs, the JointTest
// quadratic form) runs here.

// Wiring the machines run with is itself pinned (fresh javap of the
// 3.9.0 decoder chain, tmp_opencode/jdprobe/i21c):
// - TRAMO (TramoKernel.regressionModule, auto branch):
//   FTEST: tradingDays = TD7 contrast, workingDays = TD2 contrast,
//   leapYear, easter from spec, adjust = tdSpec.isAutoAdjust,
//   fPValue = tdSpec.probabilityForFTest, testMean = AmiOptions.checkMu,
//   estimationPrecision = AmiOptions.intermediatePrecision, thresholds
//   tmean 1.96 / tlp 2.0 / teaster 2.2 (builder defaults, never moved by
//   the wiring).
//   AIC/BIC: tradingDays = alltd = [TD2, TD2c, TD3, TD3c, TD4, TD7],
//   aic()/bic(), same testMean/precision/adjust.
//   WALD: same alltd ladder, pconstraint = 0.1 (decoder literal),
//   pmodel = tdSpec.probabilityForFTest.
// - sa (X13SpecDecoder.calendarTest): AIC/BIC machines run
//   alltd = [TD2c, TD2, TD3, TD3c, TD4, TD7] with aic()/bic(),
//   tlp 2.0, adjust = spec.isAutoAdjust; the WALD machine runs
//   nestedtd = [TD2, TD3, TD4, TD7] with fpvalue = autoPvalue1 (0.01),
//   pconstraint = autoPvalue2 (0.1), tlp 2.0.
// Ladder layout (TradingDaysRegressionComparator.test, mirrored as the
// row-vector convention below): row 0 = base (copyOf(desc) with "td"/"lp"
// removed, setAirline(true), setMean(true)); row 1 = base + lp and is
// ABSENT (present = false) unless lp != null && !base.isAdjusted();
// row i+2 = row-1 base (with lp if row 1 was built, sharing the mutated
// base description) + td_i. Names for the log line are
// modelNames(td) = ["no td", "lp", td descriptions...].

// Numeric notes (pinned by tmp_opencode/kalos-21i-pinning.md SECTION 9 and
// the fresh javap dumps):
// - No new distribution kernel: every p-value routes through
//   detail::f_upper_p (23-C.2). The FTEST Info math is the Lower tail of
//   FIXED-df1 F tests (F(1, dof_wd - nhp) for the WD comparison, F(6,
//   dof_td - nhp) for the TD comparison; the Info constructor receives its
//   likelihood arguments SWAPPED relative to their field names: Info(cl0,
//   WD-row, TD-row, nhp) stores field ll1 = 2nd argument and field ll6 =
//   3rd argument; the mirror names the arguments by their wiring role).
//   Lower = 1 - f_upper_p, evaluated as dconst_1 dsub inside the getter.
// - All comparisons mirror the exact bytecode comparator polarity
//   (dcmpg/dcmpl and the ifle/ifge/iflt/if_icmp* arm), so NaN operands
//   decide exactly as the JVM does; Math.abs is the IEEE abs,
//   Math.sqrt/std::sqrt coincide bit-for-bit.
// - The Wald algebra is the landed pinned shape: s2 = ssqRef / (dofRef - 2)
//   over the FIRST-present reference row, per-model p from
//   f_upper_p(deltaNx, dof8, f), the descend-while loop and the j-swap
//   loop with the pconstraint constraint (see the contract doc table).
// - tramo_test_tstats (23-E) is the tstat(i, nhp, addmu=true) kernel:
//   coef / sqrt((ucovDiag * ssq) / (dim - nx - nhp)) with the JVM
//   ucov==0 -> NaN and coef==0 -> +-0 short circuits; the unguarded call
//   sites (position < 0 or >= nx) mirror the JVM exception surface as
//   result = failed (fail closed, no invented statistic).

#include <vector>

#include "kalos/tramo_regression_tests.hpp"

namespace kalos {

// ---------------------------------------------------------------------------
// Result surface shared by the machines. `failed` is the pinned
// catch-surface (FTEST/AR/Wald test() catch Exception ->
// ProcessingResult.Failed; the sa family catches RuntimeException ->
// remark "automatic trading days selection failed" then Failed).
// ---------------------------------------------------------------------------
enum class CalendarProcessingResult { changed, unchanged, failed };

// ---------------------------------------------------------------------------
// TradingDaysRegressionComparator layer
// ---------------------------------------------------------------------------

// One candidate-model hand-over row. `present` is false exactly where the
// jar leaves a null array entry (row 1 with no lp arm; a probe must never
// set present = false elsewhere, bestModel/waldTest null semantics cover
// the jar surface). The keys ride on the row's LikelihoodStatistics; the
// ssq/nx/degrees_of_freedom are the concentrated likelihood reads of the
// same estimation (waldTest and the FTEST Info math). Statistics for
// absent rows are never read. Keys verified against the fresh bytecode:
// aiccComparator reads LikelihoodStatistics.getAICC(), bicComparator reads
// getBIC(); the comparator lambda is
// compare(a, b) = (a == null ? 1 : b == null ? -1
//                            : Double.compare(key(a), key(b)))
// (nulls sort last).
struct CalendarModelRow {
    bool present = true;
    double aicc = 0.0;
    double bic = 0.0;
    double ssq = 0.0;
    int nx = 0;
    int degrees_of_freedom = 0;
};

// TradingDaysRegressionComparator.bestModel: skip the leading absent rows,
// then replace while comparator.compare(ests[best], ests[candidate]) > 0
// over present rows only (lower key wins, ties keep the earlier index).
// The jar's `-1` return (all rows absent) is UNREACHABLE bytecode: the
// all-absent array throws ArrayIndexOutOfBounds in the scan loop (the
// bounds test follows the element load), which the machines catch as
// Failed. The mirror throws std::out_of_range on all-absent; the machine
// functions translate it to result = failed.
// `selector_aicc` picks the aicc key, otherwise the bic key.
int calendar_best_model(const std::vector<CalendarModelRow>& rows,
                        bool selector_aicc);

// TradingDaysRegressionComparator.waldTest over the rows. The reference is
// the LAST row: dof = rows.back().degrees_of_freedom - 2,
// s2 = rows.back().ssq / dof. Caches: nx[0]/ssq[0] from the first row,
// nx[last]/ssq[last] from the last, per-inner-row reads only where present
// (an absent inner row keeps nx = 0 / ssq = 0 and is NOT skipped by the
// p-loop). p-loop over i in [1, last]: deltaNx = nx[i] - nx[0];
// p[i] = 1.0 when deltaNx == 0; else with
// f = (ssq[0] - ssq[i]) / ((double)deltaNx * s2),
// p[i] = f > 0.0 (dcmpl, NaN enters) ? f_upper_p(deltaNx, dof, f) : 1.0.
// Then descend: while (best > 0 && p[best] > pmodel) --best (dcmpl, NaN
// descends). If best <= 1 return it; else the j-loop over j in [best-1, 1]
// descending: when p[j] < pmodel (dcmpg, NaN enters), recompute
// deltaNx = nx[best] - nx[j],
// f = (ssq[j] - ssq[best]) / ((double)deltaNx * s2),
// and when f > 0.0 (dcmpl, NaN enters) take
// q = f_upper_p(deltaNx, dof, f); best = j when q > pconstraint (dcmpl,
// NaN swaps). The returned `f` echoes carry the recomputed pairwise f per
// row for golden cross-checks (0.0 where never computed).
// The jar dereferences rows[0] and rows[last] outside any present check:
// a request with an absent first or last row is malformed (the JVM
// NullPointerException surface) - calendar_wald_test throws
// std::invalid_argument and the machine functions translate it to
// result = failed.
struct CalendarWaldTestResult {
    int best = 0;
    std::vector<double> p;  // per-row p as left by the loops (1.0 default)
    std::vector<double> f;  // per-row last f value (0.0 default)
    // The jar's F.getProbability(DStatException) surface (df <= 0 or a NaN
    // value reaching incBeta - the iteration bound throws): the machine
    // catches Exception -> Failed. Set when a needed p-value cannot be
    // produced; best/p/f are then meaningless.
    bool failed = false;
};

CalendarWaldTestResult calendar_wald_test(
    const std::vector<CalendarModelRow>& rows, double pmodel,
    double pconstraint);

// ---------------------------------------------------------------------------
// AutomaticFRegressionTest$Info math
// ---------------------------------------------------------------------------

// The Info constructor arithmetic (fresh bytecode; the pre-existing
// pinning had this wrong). Call-site argument order is
// new Info(cl0, cl_wd, cl_td, nhp) - the fields are
// ll1 = cl_wd (2nd argument), ll6 = cl_td (3rd argument):
// ss0/ss1/ss6 = the three cl.ssq() reads;
// ssmc1 = ss1 / (double)(dof_wd - nhp),
// ssmc6 = ss6 / (double)(dof_td - nhp),
// f1 = (ss0 - ss1) / ssmc1, then (dcmpg guard, NaN or f1 <= 0 -> pftd1 =
// 0.0) pftd1 = F(1, dof_wd - nhp).getProbability(f1, Lower)
//                                    = 1 - f_upper_p(f1, 1, dof_wd - nhp),
// f6 = (ss0 - ss6) / (ssmc6 * 6.0), then the same guard
// pftd6 = F(6, dof_td - nhp).getProbability(f6, Lower)
//                                    = 1 - f_upper_p(f6, 6, dof_td - nhp).
// "Lower" is 1 - Upper on the same F (the landed f_upper_p covers both);
// the 1 - x subtraction mirrors the JVM operand order inside the getter.
// Only the degrees_of_freedom/ssq of the rows are read; nx feeds the
// update()/info consumers elsewhere.
struct CalendarFtestInfo {
    double ss0 = 0.0;
    double ss1 = 0.0;   // ssq of the 2nd argument (WD model)
    double ss6 = 0.0;   // ssq of the 3rd argument (TD model)
    double ssmc1 = 0.0;
    double ssmc6 = 0.0;
    double pftd1 = 0.0;
    double pftd6 = 0.0;
    // DStatException surface of either F call (invalid df or a NaN value
    // reaching incBeta): the machine catches it as Failed.
    bool failed = false;
};

CalendarFtestInfo calendar_ftest_info(const CalendarModelRow& model0,
                                      const CalendarModelRow& model_wd,
                                      const CalendarModelRow& model_td,
                                      int desc_free_parameters);

// ---------------------------------------------------------------------------
// Shared machine inputs
// ---------------------------------------------------------------------------

// One candidate-model (descTest) estimation hand-over plus the lookups
// the machine performs on the candidate description: td_position =
// descTest.findPosition(selected td core), lp_position =
// descTest.findPosition(lp), easter_position =
// descTest.findPosition(easter), all -1 when the core is not a variable
// of descTest. statistics.free_parameters carries the nhp the accept
// sites pass (SarimaSpec.freeParametersCount() of the description each
// machine reads - pinned per machine in the contract table).
struct CalendarCandidateHandover {
    TramoTestStatistics statistics;
    int td_position = -1;
    int lp_position = -1;
    int easter_position = -1;
};

// The update() layer state a TRAMO machine reads on the CURRENT
// description (desc, not descTest) plus its wired thresholds.
struct CalendarTramoUpdateState {
    bool mean = true;              // desc.isMean()
    bool log_transformation = false;
    bool test_mean = true;         // AmiOptions.checkMu wiring
    double tmean = 1.96;
    double tlp = 2.0;
    double teaster = 2.2;
    bool adjust = true;            // tdSpec.isAutoAdjust wiring
    bool has_easter = false;       // machine easter field != null
    bool easter_guarded = false;   // FTEST update(): position < 0 forces t 0
};

// Echo of the update() layer shared by the TRAMO machines.
struct CalendarTramoUpdateEcho {
    bool mean_test_ran = false;
    bool mean_test_set = false;    // setMean() executed
    bool mean_after = true;        // desc mean state after the test
    double mean_tstat = 0.0;       // raw tstat(0, nhp, true) (pre-abs)
    bool td_added = false;         // addVariable("td", tdVar, calendarAMI)
    int lp_action = 0;             // 0 none, 1 setPreadjustment, 2 addVariable
    int easter_action = 0;         // 0 none, 1 addVariable
    bool changed = false;
    bool failed = false;           // JVM tstat index surface (unguarded sites)
};

// The TRAMO update() grammar. td_selected selects the tdVar argument
// branch; lp_selected the lpSel argument (AR/Wald) - FTEST instead gates
// its lp arm on (td_selected && has_lp), which the caller expresses by
// passing lp_selected accordingly; has_lp/has_easter carry the machine
// field nullity. Returns failed (echo.failed) when an unguarded tstat
// index lies outside [0, nx).
CalendarTramoUpdateEcho tramo_calendar_update(
    const CalendarTramoUpdateState& state, bool td_selected, bool lp_selected,
    bool has_lp, const CalendarCandidateHandover& candidate);

// ---------------------------------------------------------------------------
// The TRAMO ladder machines (AutomaticRegressionTest,
// AutomaticWaldRegressionTest)
// ---------------------------------------------------------------------------

// AutomaticRegressionTest.test / AutomaticWaldRegressionTest.test as one
// function (the two differ only in how `best` is selected and share
// createTestModel/update byte-for-byte). The re-estimation of the
// selected candidate is request data carried FULLY: candidates[i] is the
// createTestModel(est, tdSel(i), lpSel(i)) + processor(true, precision)
// hand-over the machine would build for ladder row i (airline/mean forced
// on inside createTestModel, witnessed by the probe; tdSel = null for
// rows 0/1 and lpSel = null for row 0, so candidates[0] carries no td and
// no lp positions). The machine's selection picks candidates[best]; the
// probe captures every row so no caller has to know `best` first.
// `nhp` is desc.getArimaSpec().freeParametersCount() (the CURRENT
// description). bestModel returning -1 (all rows absent) is the JVM
// ArrayIndexOutOfBounds -> catch -> Failed surface. The sa-style
// names[best] / Info log is echoed as selected_name_index.
struct TramoAutoTdRequest {
    enum class Selector { aicc, bic, wald } selector = Selector::aicc;
    double pmodel = 0.01;       // wald selector: tdSpec.probabilityForFTest
    double pconstraint = 0.1;   // wald selector: decoder literal
    std::vector<CalendarModelRow> ladder;
    std::vector<CalendarCandidateHandover> candidates;  // same length
    CalendarTramoUpdateState update;
    bool has_lp = true;   // machine lp field != null (leapYear wiring)
};

struct TramoAutoTdDecision {
    CalendarProcessingResult result = CalendarProcessingResult::failed;
    int best = -1;
    int selected_name_index = -1;  // modelNames ladder index
    CalendarTramoUpdateEcho update;
};

TramoAutoTdDecision run_tramo_auto_td_test(const TramoAutoTdRequest& request);

// ---------------------------------------------------------------------------
// The TRAMO FTEST machine (AutomaticFRegressionTest)
// ---------------------------------------------------------------------------

// AutomaticFRegressionTest.test. The FTEST machine's OWN createTestModel
// does not force airline/mean (unlike the AR/Wald/sa families - fresh
// bytecode erratum): every candidate is copyOf(desc) + the requested
// variables. The Info likelihood rows come from the NO-lp models
// (null,null), (wd,null), (td,null); the update() layer consumes a
// separate estimation per branch: the q/none branches use the (null,null)
// model, the TD branch its (td,lp) model, the WD branch its (wd,lp)
// model. Branch walk (exact bytecode polarity):
//   A: pftd6 > pftd1 (dcmpl) && pftd6 > 1 - fpvalue (dcmpl)  [NaN -> true]
//      -> branch 'a': update(tdVar=td,  candidate_td)
//   else pftd1 >= 1 - fpvalue (dcmpg)                        [NaN -> false]
//      -> branch 'c': update(tdVar=wd,  candidate_wd)  (the WD swap)
//   else -> branch 'n': update(tdVar=null, candidate_none)
// Branch 'q' is the no-td-field early exit (update on candidate_none);
// 'e' is the catch-Exception -> Failed surface. The update()/Info nhp is
// candidate_none's spec free-parameter count (bytecode local 7).
struct TramoFtestTdRequest {
    double fpvalue = 0.01;
    CalendarTramoUpdateState update;  // easter_guarded = true
    bool has_td = true;               // TD7 wiring
    bool has_wd = true;               // TD2 wiring
    bool has_lp = true;               // leapYear wiring
    // Info rows: the NO-lp models (the jar swaps args 2/3 into fields
    // ll1/ll6): model0 from (null,null), model_wd from (wd,null),
    // model_td from (td,null).
    CalendarModelRow model0;
    CalendarModelRow model_wd;
    CalendarModelRow model_td;
    int info_free_parameters = 0;  // candidate_none spec free-parameter count
                                   // (the single bytecode local feeding both
                                   // the Info nhp and every update() nhp)
    // Update-model hand-overs: candidate_none = (null,null),
    // candidate_td = (td,lp), candidate_wd = (wd,lp).
    CalendarCandidateHandover candidate_none;
    CalendarCandidateHandover candidate_td;
    CalendarCandidateHandover candidate_wd;
};

struct TramoFtestTdDecision {
    CalendarProcessingResult result = CalendarProcessingResult::failed;
    char branch = 'e';
    CalendarFtestInfo info;
    CalendarTramoUpdateEcho update;
};

TramoFtestTdDecision run_tramo_ftest_td_test(const TramoFtestTdRequest& request);

// ---------------------------------------------------------------------------
// The sa machines (AutomaticTradingDaysRegressionTest,
// AutomaticTradingDaysWaldTest)
// ---------------------------------------------------------------------------

// The sa update() grammar reads the CURRENT description's variables by
// name ("td"/"lp") and compares cores; the witnesses below are the
// captured lookup results (no core-equality is re-implemented here).
struct SaCalendarDescWitnesses {
    bool desc_td_present = false;     // desc.variable("td") != null
    bool desc_lp_present = false;     // desc.variable("lp") != null
    bool desc_adjusted = false;       // desc.isAdjusted() (None-reset arm)
    // [i] = desc td core equals(t_i) - only read when desc_td_present and
    // the ladder selected td_i (or by the keep/swap branch when td
    // selection lands on i + 2). Ladder-sized.
    std::vector<bool> td_core_equals;
};

// AutomaticTradingDaysRegressionTest.test (selector aicc/bic) and
// AutomaticTradingDaysWaldTest.test (selector wald) as one function: the
// two share the update() grammar byte-for-byte (verified by bytecode
// diff) and differ in the selection and in the log message (the AIC/BIC
// machine logs "no trading days" when the td selection is null, the
// Wald machine always logs "selected trading days: " + names[best]).
// tlp is the builder default 2.0 in both wirings; adjust is
// spec.isAutoAdjust. candidates[i] is the createTestModel(est, tdSel(i),
// lpSel(i)) + processor(true, precision) hand-over for ladder row i
// (airline + mean forced on, no easter - the sa machine has none); the
// selection picks candidates[best].
struct SaAutoTdRequest {
    enum class Selector { aicc, bic, wald } selector = Selector::aicc;
    double pmodel = 0.01;      // wald selector: autoPvalue1
    double pconstraint = 0.1;  // wald selector: autoPvalue2
    double tlp = 2.0;
    bool adjust = true;
    std::vector<CalendarModelRow> ladder;
    std::vector<CalendarCandidateHandover> candidates;  // same length
    bool has_lp = true;        // leapYear wiring (always wired in 3.9.0)
    bool desc_log_transformation = false;
    SaCalendarDescWitnesses witnesses;
};

struct SaAutoTdDecision {
    CalendarProcessingResult result = CalendarProcessingResult::failed;
    int best = -1;
    int selected_name_index = -1;
    bool no_trading_days_log = false;  // AIC/BIC machine, selection null
    // update() echo: exactly one td_* flag may be set, lp flags follow
    // the pinned branch (early_return marks the arm that returned
    // directly, discarding the changed flag).
    bool td_kept = false;
    bool td_swapped = false;
    bool td_added = false;
    bool td_removed = false;
    bool lp_added = false;
    bool lp_preadjusted = false;
    bool lp_removed_with_preadjust = false;  // swap arm: preadjust + remove
    bool lp_removed = false;                 // the "263" tail removal
    bool preadjust_reset_none = false;       // the None-reset tail
    int early_return = 0;  // 0 none; 1 Changed(preadjust), 2 Changed(add),
                           // 3 Changed(preadjust+remove), 4 Unchanged(drops
                           // the changed flag: the pinned quirk)
    bool changed = false;
};

SaAutoTdDecision run_sa_auto_td_test(const SaAutoTdRequest& request);

// ---------------------------------------------------------------------------
// The seam calendar steps (23-E activation, "TRAMO gate" corpus)
// ---------------------------------------------------------------------------

// FastRegressionTest.test with automatically identified calendar
// variables (the filter is isTradingDays/isLengthOfPeriod/isEaster AND
// isAutomaticallyIdentified; the easter variable is tested by the mhTest
// slot exactly as the jar wires it). statistics is the LIVE estimation's
// concentrated likelihood (est.getEstimation()), nhp the copied
// description's spec free-parameter count, tvar the builder field feeding
// every calendar test. Fresh-ctor bytecode: tdTest = TRegressionTest(tvar,
// tvar) - the TWO-arg constructor with nsig = 2; wd/lp/mh tests are the
// ONE-arg TRegressionTest(tvar) with nsig = 1 (all arms tlow = thigh =
// tvar; the landed 23-E bucket law applies).
struct TramoFastCalendarStepRequest {
    struct Slot {
        bool present = false;
        int position = -1;   // descTest.findPosition(core)
        int dim = 0;         // core.dim()
    };
    TramoTestStatistics statistics;
    int nhp = 0;
    Slot td;       // first auto-identified trading-days variable
    Slot lp;       // first auto-identified length-of-period variable
    Slot easter;   // first auto-identified easter variable (mhTest slot)
    double tvar = 1.96;
    bool test_mean = true;
    double tmean = 1.96;
    bool regarima_mean = true;   // regarima().isMean()
    bool desc_mean = true;       // result echo start state
};

struct TramoFastCalendarStep {
    CalendarProcessingResult result = CalendarProcessingResult::unchanged;
    bool td_dropped = false;     // the T-accept failed (drives the removals)
    bool lp_test_ran = false;
    bool lp_rescued_td = false;  // lp accepted, td spared
    bool lp_removed = false;     // the rescue-arm removal
    bool td_removed = false;
    bool easter_test_ran = false;
    bool easter_removed = false;
    bool mean_test_ran = false;
    bool mean_removed = false;   // setMean(false)
    bool mean_after = true;
    bool estimation_cleared = false;  // est.set(desc, null)
};

TramoFastCalendarStep run_tramo_fast_calendar_step(
    const TramoFastCalendarStepRequest& request);

// DefaultRegressionTest.test over its TEST-MODEL estimation. Every
// calendar arm first looks the machine variable up on descTest
// (variable(core) != null && isAutomaticallyIdentified) and the lp arm
// additionally requires the td arm to have been kept. join_test selects
// the tdTest implementation: the T arm - tdTest = TRegressionTest(t0td,
// t1td) with nsig = 2, wdTest = TRegressionTest(twd) nsig = 1, lpTest =
// TRegressionTest(t0td) nsig = 1 (lp rides on t0td, not twd), mhTest =
// TRegressionTest(teaster) nsig = 1 - or FRegressionTest(0.05) (hardcoded
// eps in the ctor bytecode) with the JointTest quadratic form (the
// |p| < eps dcmpg law, NaN p ACCEPTS). The joint-test statistic runs here
// on the hand-over: z = ssq(solveLx(lcholesky(bvar[S,S]), b[S])) / |S|,
// f = z / (ssq / (dim - nx - nhp)), p = f_upper_p(f, |S|,
// dim - nx - nhp); the Cholesky/solveLx/ssq operand order mirrors
// SymmetricMatrix.lcholesky (lapack.Cholesky provider, tol = 0) /
// LowerTriangularMatrix.solveLx / DataBlock.ssq bit-for-bit over the
// column-major storage of the covariance hand-over. select_all =
// (first == 0 && nx == dim) is the jar's no-selection short circuit
// (S = all coefficients, b copied through toArray()). The covariance
// hand-over (column-major, nx = coefficient count) is REQUIRED whenever
// the td arm runs with join_test; its absence is a malformed request
// (std::invalid_argument, fail closed). lcholesky/solveLx throw surfaces
// (m_err_chol on a negative or zero pivot, m_err_sing on a zero solve
// pivot; NaN pivots take the arithmetic branch, NOT the throw) propagate
// as std::domain_error - the jar lets the MatrixException leave the
// machine.
struct TramoDefaultCalendarStepRequest {
    struct Lookup {
        bool variable_present = false;
        bool auto_identified = false;
        int position = -1;   // descTest.findPosition(core)
        int dim = 0;         // core.dim()
    };
    TramoTestStatistics statistics;  // the descTest estimation's cl
    int nhp = 0;                     // descTest.getArimaSpec().freeParametersCount()
    // join_test arm: the unscaledCovariance of the same estimation as a
    // column-major square matrix (nx x nx, nx = the coefficient count of
    // the hand-over above). Required when the td arm runs with join_test.
    std::vector<double> covariance;
    Lookup td;
    Lookup lp;
    Lookup easter;
    bool join_test = false;
    double t0td = 2.0;
    double t1td = 2.6;
    double twd = 2.2;
    double teaster = 1.96;
    double tmean = 1.96;
    double feps = 0.05;
    bool test_mean = true;
    bool adjust = false;             // isAutoAdjust wiring
    bool desc_test_log = false;      // descTest.isLogTransformation()
    bool regarima_mean = true;
    // Result-echo start state (desc, the machine adds onto a copy).
    bool desc_mean = true;
};

struct TramoDefaultCalendarStep {
    CalendarProcessingResult result = CalendarProcessingResult::unchanged;
    bool unprocessed = false;  // no calendar field and no mean test
    bool td_kept = false;      // addVariable("td", ..., calendarAMI)
    int lp_action = 0;         // 0 none, 1 setPreadjustment, 2 addVariable
    int easter_action = 0;     // 0 none, 1 addVariable
    bool mean_test_ran = false;
    bool mean_removed = false;
    bool mean_after = true;
    // Joint-test witnesses (echoed only when the td arm ran with
    // join_test): the statistic, its degrees of freedom and p-value.
    bool joint_test_ran = false;
    double joint_f = 0.0;
    double joint_pvalue = 0.0;
    int joint_df1 = 0;
    int joint_df2 = 0;
    bool estimation_cleared = false;
};

TramoDefaultCalendarStep run_tramo_default_calendar_step(
    const TramoDefaultCalendarStepRequest& request);

// The public mirror of the jar's public class
// JointTest(cl).variableSelection(first, dim).hyperParametersCount(nhp)
// .build() - the quadratic form documented on
// TramoDefaultCalendarStepRequest (the step machine above calls exactly
// this when the td arm runs with join_test and dim >= 2; the corpus also
// witnesses forms built directly off an estimation, dim == 1 included,
// which the step's dim == 1 -> wdTest routing never reaches). covariance
// is the column-major nx x nx unscaledCovariance (nx = the coefficient
// count of `statistics`). Throw surfaces: std::invalid_argument when the
// selection window lies outside the hand-over or the covariance is not
// the full nx x nx matrix (jar surface ArrayIndexOutOfBounds / NPE);
// std::domain_error for m_err_chol / m_err_sing and for a NaN p-value out
// of f_upper_p (the jar's MatrixException / DStatException leave the
// class unchecked).
struct CalendarJointTestResult {
    double f = 0.0;
    double pvalue = 0.0;
    int df1 = 0;  // |S| = dim
    int df2 = 0;  // statistics.dim - nx - nhp
};

CalendarJointTestResult calendar_joint_test(
    const TramoTestStatistics& statistics,
    const std::vector<double>& covariance, int first, int dim, int nhp);

}  // namespace kalos

#endif  // KALOS_CALENDAR_DETECTION_HPP
