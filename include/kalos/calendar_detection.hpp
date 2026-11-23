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
//   detail::f_upper_p (23-C.2). The FTEST Info math computes
//   pftd1 = 1 - F(dof1, dof6).cumulate(ssmc1/ssmc6) and
//   pftd6 = 1 - F(dof6, dof1).cumulate(ssmc6/ssmc1) (the Info constructor
//   receives its likelihood arguments SWAPPED relative to their field
//   names: Info(ll0, WD-row, TD-row, nhp) stores field ll1 = 2nd argument
//   and field ll6 = 3rd argument; the mirror names the arguments by their
//   wiring role). 1 - x is evaluated as dconst_1 dsub after cumulate.
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
// the jar surface). aic/bic keys are the comparator keys of the row's
// LikelihoodStatistics: aiccComparator reads getAicc(), bicComparator
// reads getBic(). ssq/nx/degrees_of_freedom are the concentrated
// likelihood reads of the same estimation (waldTest and the FTEST Info
// math). Statistics for absent rows are never read.
struct CalendarModelRow {
    bool present = true;
    double aicc = 0.0;
    double bic = 0.0;
    double ssq = 0.0;
    int nx = 0;
    int degrees_of_freedom = 0;
};

// TradingDaysRegressionComparator.bestModel: skip the leading absent rows
// (all absent -> -1, the jar's ArrayIndexOutOfBounds surface is the
// caller's failure, see the machine functions), then replace while
// comparator.compare(current, candidate) > 0 over present rows only;
// comparator.compare = Double.compare(key(candidate), key(current)) for
// aicc/bic (lower key wins, ties keep the earlier index).
// `selector_aicc` picks the aicc key, otherwise the bic key.
int calendar_best_model(const std::vector<CalendarModelRow>& rows,
                        bool selector_aicc);

// TradingDaysRegressionComparator.waldTest over the rows. dof =
// ref.degreesOfFreedom - 2 (the FIRST-present row is the reference),
// s2 = ref.ssq / dof; p[k] = 1.0 when deltaNx == 0 or f <= 0 or f is NaN,
// else f_upper_p(deltaNx, dof, f) with
// f = (ssq[best] - ssq[k]) / ((double)deltaNx * s2); descend best while
// p[best] > pmodel (dcmpl arm); then the j-swap loop over
// p[j] < pmodel (dcmpg arm) with the pconstraint constraint on the
// recomputed pairwise f (the exact operand order of the bytecode, see the
// contract table). The jar dereferences rows[0] and rows[last] outside
// their present checks: a request with an absent first or last row makes
// the machine fail (the JVM NullPointerException surface) - encoded as
// result = failed inside the machine functions, calendar_wald_test itself
// throws std::invalid_argument for the malformed shape.
struct CalendarWaldTestResult {
    int best = 0;
    std::vector<double> p;  // per-row p as left by the loops (1.0 default)
    std::vector<double> f;  // per-row last f value (0.0 default)
};

CalendarWaldTestResult calendar_wald_test(
    const std::vector<CalendarModelRow>& rows, double pmodel,
    double pconstraint);

// ---------------------------------------------------------------------------
// AutomaticFRegressionTest$Info math
// ---------------------------------------------------------------------------

// The Info constructor arithmetic. Arguments are named by their wiring
// role and passed in the bytecode order of the call site
// new Info(ll0, WD-model, TD-model, descFreeParameters):
// ssmc1 = (ssq0 - ssqWd) / (nxDenominator... (nx_wd - nx0)),
// ssmc6 = (ssq0 - ssqTd) / (nx_td - nx0),
// pftd1 = 1 - F(dof_wd - nhp, dof_td - nhp).cumulate(ssmc1 / ssmc6),
// pftd6 = 1 - F(dof_td - nhp, dof_wd - nhp).cumulate(ssmc6 / ssmc1).
// Field names follow the jar (ss1/ssmc1/pftd1 read the 2nd argument).
struct CalendarFtestInfo {
    double ss0 = 0.0;
    double ss1 = 0.0;   // ssq of the 2nd argument (WD model)
    double ss6 = 0.0;   // ssq of the 3rd argument (TD model)
    double ssmc1 = 0.0;
    double ssmc6 = 0.0;
    double pftd1 = 0.0;
    double pftd6 = 0.0;
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
// selected candidate is request data: the caller supplies the candidate
// hand-over produced by createTestModel(est, tdSel, lpSel) +
// processor(true, precision) (airline/mean forced on inside
// createTestModel, witnessed by the probe). `nhp` is
// desc.getArimaSpec().freeParametersCount() (the CURRENT description).
// bestModel returning -1 (all rows absent) is the JVM
// ArrayIndexOutOfBounds -> catch -> Failed surface. The sa-style
// names[best] / Info log is echoed as selected_name_index.
struct TramoAutoTdRequest {
    enum class Selector { aicc, bic, wald } selector = Selector::aicc;
    double pmodel = 0.01;       // wald selector: tdSpec.probabilityForFTest
    double pconstraint = 0.1;   // wald selector: decoder literal
    std::vector<CalendarModelRow> ladder;
    CalendarTramoUpdateState update;
    bool has_lp = true;   // machine lp field != null (leapYear wiring)
    CalendarCandidateHandover candidate;
    int nhp = 0;
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

// AutomaticFRegressionTest.test. The three candidate hand-overs are the
// createTestModel estimations for (td, lp), (wd, lp) and (null, null);
// the branch decides which one the update() layer consumes. The update()
// nhp argument is the descTest[0] spec free-parameter count (equal to the
// current desc value; carried separately to keep the mirror honest).
// Branch letters follow the contract table: 'q' = no td field wired
// (quarterly), 'a' = TD kept, 'b' = no td, 'c' = WD (TD7) swap,
// 'e' = JVM exception surface.
struct TramoFtestTdRequest {
    double fpvalue = 0.01;
    CalendarTramoUpdateState update;  // easter_guarded = true
    bool has_td = true;               // TD7 wiring
    bool has_wd = true;               // TD2 wiring
    bool has_lp = true;               // leapYear wiring
    // Ladder rows in Info order (the jar swaps them into the fields).
    CalendarModelRow model0;
    CalendarModelRow model_wd;
    CalendarModelRow model_td;
    int info_free_parameters = 0;  // desc.getArimaSpec().freeParametersCount()
    int nhp = 0;                   // descTest0 spec free-parameter count
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
// spec.isAutoAdjust. The candidate hand-over is the re-estimated
// createTestModel(descTest with airline + mean forced) row.
struct SaAutoTdRequest {
    enum class Selector { aicc, bic, wald } selector = Selector::aicc;
    double pmodel = 0.01;      // wald selector: autoPvalue1
    double pconstraint = 0.1;  // wald selector: autoPvalue2
    double tlp = 2.0;
    bool adjust = true;
    std::vector<CalendarModelRow> ladder;
    bool has_lp = true;        // leapYear wiring (always wired in 3.9.0)
    CalendarCandidateHandover candidate;
    int nhp = 0;               // desc.getArimaSpec().freeParametersCount()
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
// every calendar test (TRegressionTest(tvar) / (tvar, tvar) arms
// identical thresholds; the landed 23-E bucket law applies).
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
// the tdTest implementation: the T arm (t0td 2.0 / t1td 2.6 / nsig 1,
// twd 2.2) or FRegressionTest(0.05) with the JointTest quadratic form
// (the |p| < 0.05 dcmpg law). The joint-test statistic runs here on the
// hand-over: z = ssq(solveLx(lcholesky(bvar[S,S]), b[S])) / |S|,
// f = z / (ssq / (dim - nx - nhp)), p = f_upper_p(|S|, dim - nx - nhp, f);
// the Cholesky/solveLx/ssq operand order mirrors
// SymmetricMatrix.lcholesky / LowerTriangularMatrix.solveLx /
// DataBlock.ssq bit-for-bit. select_all = (first == 0 && dim == nx) is
// the jar's no-selection short circuit (S = all coefficients).
struct TramoDefaultCalendarStepRequest {
    struct Lookup {
        bool variable_present = false;
        bool auto_identified = false;
        int position = -1;   // descTest.findPosition(core)
        int dim = 0;         // core.dim()
    };
    TramoTestStatistics statistics;  // the descTest estimation's cl
    int nhp = 0;                     // descTest.getArimaSpec().freeParametersCount()
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

}  // namespace kalos

#endif  // KALOS_CALENDAR_DETECTION_HPP
