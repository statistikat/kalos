# Item 21 roadmap — automatic X-13 preprocessing and selection

Planning decomposition of item 21 (automatic preprocessing: transformation
decision, outlier detection, differencing, ARMA identification, estimation
control, regression-variable tests) into deliverable slices on top of the
qualified Kalos kernels. Every stage below is grounded in the pinned rjd3
3.9.0 bytecode (`jdplus-x13-base-core`, `jdplus-toolkit-base-core`,
`jdplus-sa-base-core` 3.9.0, `javap -p -c` dumps under the analysis tree);
evidence is cited as class::method. Nothing here is implemented — this is
the plan, its dependency order, its per-slice commit boundaries, and the
decision boundaries that need maintainer sign-off before their slices
start. The pinned reference for end-to-end checks remains the fast entry
`jdplus.x13.base.r.X13.process` already used by the 22c fit captures.

## The pinned master pipeline

`jdplus.x13.base.core.x13.regarima.RegArimaKernel.process(TsData,
ProcessingLog)` is the master sequence:

1. `IModelBuilder.build` (`X13ModelBuilder.build`): spec decoding into a
   `ModelDescription` — `initializeArima` (d/D from spec), `initializeMean`,
   `initializeTradingDays`/`initializeHolidays`/`initializeEaster`/
   `initializeOutliers`/`initializeUsers`/`initializeInterventions`/
   `initializeRamps` (AO/LS/TC/SO factories registered with the `regeffect`
   attributes the 22c M3c seam already consumes), `AverageInterpolator`
   missing handling, `initializeTransformation` (Log/None + LoP adjust).
   A null result is the log's `initialization failed`.
2. Critical value defaulting (`process`): `AmiOptions.getVa() == 0` →
   `X13Utility.calcCv(n)` = `max(2.8, CriticalValueComputer.
   advancedComputer()(n))`; stored as `va0`/`curva`.
3. `calc(RegSarimaModelling)`, the automatic-identification driver
   (log `regarima` → `automatic model identification` → `round 0`):
   - log-transform sanity: `isLogTransformation` and any value ≤ 0 →
     error `can't apply log transformation 'some obs. are <= 0'`;
   - **fully-specified short-circuit** (`isFullySpecified`: no
     transformation, outliers, auto-model, checkMu or calendar/easter test
     modules) → `FinalEstimator.estimate` + `build` only — this is the path
     the restricted fit (22c) qualified around;
   - `ILogLevelModule.process` (log/level decision, stage T below);
   - `regAIC` record, `checkMu(m, 1.96)` (mean test);
   - `IOutliersDetectionModule.process(m, va)` (stage O),
     `RegSarimaModelling.estimate(precision)`;
   - `RegressionVariablesTest.process(m)` (stage R);
   - auto-modelling branch: `ModelController(0.95)` snapshot compare,
     `IAutoModellingModule.process` (stage M), `removeVariable` +
     `clearEstimation` + re-estimate, second `regAIC` and second
     `outliers.process`;
   - `pass2` gate (ModelController measures: `getLjungBoxTest().
     getPvalue()`, `getRvr()`, `getRTval()`; bytecode threshold constants
     0.95, 0.75, 1.0126582278481011, -0.82 on `phi(1)`), `redoEstimation`,
     `checkUnitRoots`, `checkMA`;
   - final mean rule (log `2.5` branch on `isMean`) and the `2.8` floor
     re-entering the outlier critical value (`Math.max(2.8, ...)` before
     the final `FinalEstimator.estimate`);
   - `lastSolution` + `RegSarimaModelling.build()` → `RegSarimaModel`.

Modules are assembled by `X13SpecDecoder` from the `RegArimaSpec`
(LogLevelModule/DifferencingModule/ArmaModule/AutoModellingModule/
OutliersDetectionModule/MeanController/FinalEstimator + the sa-core
calendar/easter/automatic-TD modules). Kalos' job for item 21 is to
reproduce this decision pipeline natively. The 22c seams
(`x13_preadjust`/`x13_restore`) and `x13_fit` already sit at its output:
whatever `calc` produces as the final `ModelDescription` + estimation
feeds the qualified allocation and X-11 stages unchanged.

## Stages

Stage ids are stable names for the slice sequence below. "Consumes" lists
qualified Kalos units; "new" lists primitives this stage needs that do not
exist yet.

### T — transformation decision (log vs level)

Pinned: `jdplus.sa.base.core.regarima.LogLevelModule.process` (+ its
generic twin `jdplus.toolkit.base.core.regsarima.ami.
GenericLogLevelModule`). Inputs: series + current `ModelDescription`.
Sequence: any value ≤ 0 → levels (`negative values, levels are chosen`);
optional outlier preadjustment through `RobustOutliersDetector` and LoP
`setPreadjustment` (Builder `preadjust`, `outliersCorrection`, precision
default 1.0E-7); estimate the level model (`ModelDescription.estimate`
with `DemetraUtility.processor(false, precision)`), then the log model
(`copyOf` + `setLogTransformation(true)`), remove `lp` when adjusting;
record `LogLevelModule$Info(aiccLevel, aiccLog, diff)`; choose log iff
`aiccLog − aiccLevel ≤ thr` with Builder default `aiccdiff = -2.0`
(`isChoosingLog` compares the stored `aiccDiff`); failure path `log/level
failed` keeps levels. Outputs: log flag + both estimations (the chosen one
stays in the modelling).
Consumes: `sarima_regression_likelihood`, `sarima_innovations`/
`diffuse_innovations`/`stationary_innovations` (fixed likelihoods),
`optimize_arma_objective`, `preparation` (windows/transform). New: AICC
derivation from the concentrated likelihood statistics (the
`LikelihoodStatistics.getAICC` form), a level-vs-log compare driver, and
(if the preadjust variant is in scope) `RobustOutliersDetector` staging.
Sign-off: promote −2.0 as the comparison threshold; whether the default
`rsa*` product behavior pre-adjusts before the comparison.

### O — outlier detection scoring and the TsOO loop

Pinned: `jdplus.x13.base.core.x13.regarima.OutliersDetectionModule.process`
(log `outliers selection`, `make(desc, cv)` →
`jdplus.toolkit.base.core.regsarima.ami.ExactOutliersDetector`; selected
outliers re-added as `Variable`s with `attributes()` — the `regeffect`
vocabulary the M3c seam already pins — then `clearEstimation`). The loop
(`ExactOutliersDetector.process` = `estimateModel` + `execute`):
`SingleOutlierDetector.process` scans candidates; `|getMaxTStat()| > cv` →
`addNewOutlier(getMaxOutlierType(), getMaxOutlierPosition())` +
re-estimate; else drop the last added outlier and re-estimate; bounded by
`round < maxRound` and `outliers.size() < maxOutliers`; `verifymodel`
failure forces an extra re-estimate. The scanner is
`jdplus.toolkit.base.core.regarima.outlier.ExactSingleOutlierDetector
(RobustStandardDeviationComputer, ArmaFilter, ResidualsComputer)` over
`PeriodicOutlierFactory` (AO/LS/SO) + `TransitoryChangeFactory(rate)`
(TC; `OutlierSpec.DEF_TCRATE` — the 0.7 seen in the api bytecode);
`setBounds`/`exclude` implement the span window; `OutlierSpec` carries
`lsRun`, `method`, `defaultCriticalValue`, `maxIter`. Critical values:
`CriticalValueComputer.advancedComputer` (constants 1.96, 2.0,
12.566370614359172, −0.5, 0.05, 3.3, 0.0025, 4.3) clamped by `calcCv =
max(2.8, ·)`.
Consumes: `sarima_regression_likelihood`, `arma_residuals` /
`sarima_complete_residuals`, the state-space likelihoods,
`optimize_arma_objective` (re-estimation inside the loop). New: robust
standard deviation computer; residual `ArmaFilter` staging (exact-residual
t-statistic machinery); outlier column factories for AO/LS/TC/SO (the
22c fit vocabulary, generator side); single-outlier scan with candidate
t-values; the TsOO loop driver; both critical-value computer forms.
Sign-off: `method` (RegARIMA vs simplified), TC rate default, `lsRun`,
`maxIter`/max-outliers policy, and the default critical-value rule
(spec `defaultCriticalValue` vs `calcCv(n)` floor 2.8).

### E — estimation control

Pinned: `jdplus.toolkit.base.core.regsarima.RegSarimaComputer` (implements
`IRegArimaComputer`; `PROCESSOR` constant, `DEF_EPS`, `DEF_INTERNAL_EPS`,
Builder defaults `eps = 1.0E-7`, `feps = 1.0E-4`, `ml`/`mt`/`cdf`/`fast`
flags, `StartingPoint`, `SsqFunctionMinimizer`; `tryUrpCancelling`/
`tryUrmCancelling` unit-root-cancellation passes with `UR_LIMIT`;
`finalProcessing`). Initializations:
`jdplus.toolkit.base.core.sarima.estimation.HannanRissanen` (+
`internal.HannanRissanenInitializer`, 1.0E-9 guard) and the Sarima
mappings (`SarimaMapping`, `SarimaMapping2`, `MultiPeriodicAirlineMapping`
for balanced/mixed options).
Consumes: the qualified fixed/diffuse likelihoods, `arma_objective` +
`optimize_arma_objective` (bounded fixed-order optimizer),
`arma_identification` (rank/degeneracy screening),
`arma_information`. New: the Sarima parameter mapping (free/balanced/
mixed factor sets), the SSQ minimizer driver with the two precisions, the
UR-cancellation passes, convergence/status records.
Sign-off: `ml`/`mt`/`cdf`/`fast` promotion (the promoted product
combination), `eps`/`feps` promotion, starting-point policy.

### F — differencing decision (d/D) and seasonality

Pinned: `jdplus.x13.base.core.x13.regarima.DifferencingModule` (Builder
`cancel`; unit-root tests via `Polynomial.mirror`/`roots` and
`Complex.abs`, `IRegArimaComputer.optimize/process` trial fits; threshold
constants 0.9, 0.16, 0.88, 0.17, 0.91, 0.89, 0.869, 0.136, 1.02, 0.002),
supported by `SeasonalityDetector`. `AutoModellingModule` re-enters this
stage through `IDifferencingModule$Info` (cancellation afterwards).
Consumes: `preparation` (differencing), the likelihoods (trial fits),
`arma_optimizer`. New: `SeasonalityDetector` reproduction, the d/D test
battery with its thresholds, cancellation rules.
Sign-off: this is a dense decision-threshold stage — the whole constant
set above is a spec-comparison-threshold sign-off before the slice.

### M — ARMA identification and refinement

Pinned: initial identification `ArmaModule` (`RegArimaUtility.
olsResiduals` + candidate enumeration through `ArmaModule$RegArmaBic`
(`getBIC`/`getSpecification`/`mergeInto`/`sort`), regularity/significance
constants 0.003, 0.0025, 0.0075, 0.01, 0.03, 0.0625, 0.25, sentinel
99999.0; Builder `balanced`/`mixed`/`estimationPrecision`) and refinement
`AutoModellingModule` (`SarimaComponent` add/remove steps with the −0.2/
−0.1 concentrated-likelihood thresholds, `UnitRoots.D`/`BackFilter`
residual recomputation, `RegArimaUtility.linearizedData`, comparison via
`jdplus.toolkit.base.core.regsarima.ami.AICcComparator`).
Consumes: stage E's estimation control, `arma_identification`,
`sarima_complete_residuals`, Hannan-Rissanen (stage E). New: the BIC
candidate search, the hill-climbing step engine (`SarimaComponent` form),
AICc model comparator, `ModelController` measure set (`LjungBox` with
`autoCorrelationsCount`/`hyperParametersCount`, `PeriodicLjungBox`
(0.5), rvr, rTval) feeding `RegArimaKernel.pass2`/`redoEstimation`/
`checkUnitRoots` (−0.82 `phi(1)`, 1.0126582278481011, 0.95/0.75)/`checkMA`.
Sign-off: the BIC/likelihood threshold sets and the pass2/checkUnitRoots
gates are decision boundaries; balanced/mixed promotion.

### R — regression-variable tests, mean, calendar

Pinned: `jdplus.toolkit.base.core.regsarima.regular.
RegressionVariablesTest` (Builder fields `tmu`, `ttd`, `tmh`, `ftd`,
`derived` — t-statistic and F-test cutoffs over the final estimation),
`MeanController.test` (1.0E-5 guard; `RegArimaKernel.checkMu` at 1.96 in
round 0; the final mean rule at 2.5 before `FinalEstimator`), and the
sa-core calendar modules (`CalendarEffectsDetectionModule`,
`EasterDetectionModule`, `AutomaticTradingRegressionModule` with
`AutomaticTradingDaysRegressionTest`/`...WaldTest`, the x13
`UserVariablesDetectionModule`).
Consumes: `arma_information` / asymptotic covariance (the
`LogLikelihoodFunction$Point.asymptoticCovariance` form the final
estimator already exposes), the qualified variable-column algebra of the
22c allocation contract. New: t/F decision records from covariance; the
trading-day/easter comparison drivers (later sub-slices).
Sign-off: p-value/t-statistic cutoffs (`RegressionTestSpec` defaults),
the 1.96/2.5 mean rules, whether calendar-detection modules are in
item-21 scope or a separate item.

### Z — integration and qualification

Pinned: the `calc` round structure itself (round steps, `pass2`,
`lastSolution`), `X13SpecDecoder` module assembly, and the seam into the
qualified downstream: final `ModelDescription` → `x13_preadjust` →
`x11_selection_pass`/`x13_fit` → `x13_restore`. The forecast side is
already boundary-pinned (the {0,0} fixed-path domain and the
`ExactArimaForecasts` route of the fit contract): item 21 is exactly what
makes nonzero horizons reachable natively, and the M3-finale evidence rows
(`horizons.csv`) become the first cross-checks.
Deliverable: end-to-end captures replacing the fit's fixed coefficients
with the automatic decisions (full rsa0–rsa5c spec corpus), decision-trace
fixtures (stage records like the selection pass's), and the OLS/final
estimation through `FinalEstimator` (exact final derivatives +
asymptotic covariance + `FilterUtility.checkRoots`).

## Dependency order and recommended slice sequence

E is the numerical spine (T, O, F, M, R all re-estimate), so the
recommended sequence front-loads it even though the pinned pipeline *runs*
T/O first. One slice = one commit pair at most, following the standing
house gates (ctest growth, sweeps, OMP/serial/sanitized, manifest, status):

1. **21-A — estimation control core (stage E)**: Sarima mappings, Hannan-
   Rissanen initializer, SSQ driver with both precisions, UR-cancellation
   passes, status records. Largest single reuse of existing kernels.
2. **21-B — transformation decision (stage T)**: AICC forms + level/log
   driver (no preadjust variant yet). Needs E; sign-off list is small.
3. **21-C — outlier scoring (stage O)**: robust sd, residual t-statistic
   scan, factories, critical values; TsOO loop driver over a *given*
   estimation. Fixture: pinned `OutliersDetectionModule` outcomes.
4. **21-D — differencing decision (stage F)**: seasonality + d/D tests +
   cancellation.
5. **21-E — initial ARMA identification (stage M part 1)**: OLS-residual
   BIC candidates (ArmaModule).
6. **21-F — refinement and model comparison (stage M part 2)**:
   `AutoModellingModule.process` (linearizedData, differencing re-entry,
   one-tap re-differencing, ArmaModule.select, spec/mean comparison, airline
   fallback) — the roadmap's `SarimaComponent`/`AICcComparator` vocabulary is
   dead or stage-R code in the pinned 3.9.0 bytecode (see 21-F ledger).
7. **21-G — regression tests + mean rules (stage R)**.
8. **21-H — integration (stage Z)**: round structure, decoder-equivalent
   spec plumbing, final estimator, end-to-end qualification against
   `x13_fast` captures, then the nonzero-horizon fit extension as the
   first consumer.

Calendar/easter/automatic-TD detection modules and user-variable
detection (`UserVariablesDetectionModule`) are deferred behind 21-H as an
optional 21-I: they are separable modules in the pinned decoder and do
not block the preprocessing spine.

## Maintainer decision list (sign-off before the owning slice)

| # | Decision | Stage |
|---|---|---|
| 1 | `RegSarimaComputer` flag promotion (`ml`/`mt`/`cdf`/`fast`) and `eps`/`feps` promotion | E |
| 2 | Starting-point policy (Hannan-Rissanen vs defaults) per call site | E |
| 3 | Log/level AICC threshold −2.0; pre-adjustment on/off for the decision | T |
| 4 | Outlier method (RegARIMA vs simplified), type set, TC rate 0.7, `lsRun`, `maxIter`/max-outliers, critical-value default (spec vs `calcCv` floor 2.8) | O |
| 5 | d/D test thresholds (0.9/0.88/0.91/0.869 + 0.16/0.17/0.136 + 1.02/0.002) promotion | F |
| 6 | ARMA BIC thresholds (0.0025…0.25) and balanced/mixed promotion | M |
| 7 | pass2/checkUnitRoots gates (0.95, 0.75, 1.0126582278481011, −0.82) and Ljung-Box lag policy | M |
| 8 | t/F cutoffs for regression tests; mean rules 1.96/2.5 | R |
| 9 | Scope: calendar-detection and user-variable modules inside item 21 or separate | R/21-I |

The resolved 22b.3a sigma default (`legacy`, 2026-11-17) applies to every
re-estimation path that reaches the X-11 stages; no open default remains
there.

Stage E position recorded with 21-A.1 (2026-11-17): decisions 1 and 2 stay
pinned-behavior — no `ml`/`mt`/`cdf`/`fast` or `eps`/`feps` promotion and no
starting-point default. `estimate_sarima_fixed` keeps every control field a
required argument; pinned values (builder defaults 1e-7/1e-4, ml true, mt
false, cdf true, fast true) arrive from the call site, and the
HannanRissanen-dependent starts are explicit rejections until 21-A.2. The
PROCESSOR singleton (`eps = 1e-9`) is not adopted anywhere. No promotion
without a maintainer ruling.

Stage E completed with 21-A.2 (2026-11-17): all four starting policies are
delivered, still strictly as request data — `feps` is now numerically active
(the Multiple chains) but has no core default, the HannanRissanen builder
default is a supported value rather than a Kalos default, and the
initializer's `stabilize`/`useDefaultIfFailed`/`failifunstable`,
`biascorrection`/`finalcorrection` and the 1e-9 guard constant remain pinned
transcription constants, not configuration. The PROCESSOR `eps = 1e-9` is
qualified as a captured case value, still not adopted. Stage E is closed;
21-B consumes this seam.

Stage T position recorded with 21-B (2026-11-17): the log/level decision is
delivered as the qualified unit `decide_transformation`
(contracts/transformation-decision-contract.md). Decision 3 stays open — the
AICC comparison value arrives as required request data (the decoder-pinned
`-2.0` is a captured call-site value, never a core default), and the default
`rsa*` pre-adjustment-on-decision stays undecided: this slice transcribes
only the pinned `preadjust = None, outliersCorrection = false` call site
(the preadjust/outlier pre-fit staging belongs to stage O's slice). No
fit seam is wired. Two corpus fixtures (`reflected_level`, `seasgrowth_log`)
show the pinned LM and the qualified replay selecting different local
basins through the discrete Multiple alternate-start sign; the decision bit
stays pinned there and the landing is qualified — a future 21-A replay-
fidelity pass should start from those fixtures.

Stage O position recorded with 21-C (2026-11-17): the scoring/selection layer
is delivered as the qualified units `scan_outlier_candidates` and
`detect_outliers` over the new warm re-entry `optimize_sarima_from`
(contracts/outlier-scoring-contract.md). The slice stays inside the "score
and select over a given estimation" boundary: outlier fixing into specs and
the preadjust staging remain with 21-H. The pinned call-site values
(`X13Utility.processor(true, 1e-7)`, cv via `calcCv(n)`, monthly tcrate 0.7,
decoder maxRound/maxOutliers 50/30) are captured corpus facts, still not
promoted to configuration. One behavioral deviation is documented and swept
(the `getMaxTStat` sentinel escape); driver final fits are qualified inside
a flat-valley basin band whose noise was verified as basin, not surface,
difference.

Stage F position recorded with 21-D (2026-11-18): the d/D decision is
delivered as the qualified unit `decide_differencing`
(contracts/differencing-decision-contract.md). Decision 5 stays open — the
whole threshold set (0.9/0.88/0.91/0.89/0.869 + 0.136/0.15/0.16/0.17/0.19/
0.25 + 1.02/1.12/1.005/0.002 and the two t-tables) stays pinned transcription
constants and every module scalar is required request data (the decoder
call-site values, including the JVM-computed reciprocals of `DEF_UB1/DEF_UB2`,
arrive from the call site). The slice consumed two new pinned seams: the raw
Hannan-Rissanen pipeline (whose 21-A.2 public wrapper is unchanged in shape)
and the `EstimationSolver::PinnedLevenbergMarquardt` solver seam over the
bit-exact deviances machine — BFGS stays the default and the 21-A/B/C corpora
are untouched by it. Evidence for the sign-off: the corpus replay reproduces
all 39 decisions exactly and every banded landing quantity bit-identically;
the only measured drift is the step0 root-scan pair in `m_i2_144` (3.2e-5 /
1.31e-4, banded at 2e-4). `SeasonalityDetector` and the fit-seam wiring stay
with 21-H.

Stage M part 1 position recorded with 21-E (2026-11-18): the initial ARMA
identification is delivered as the qualified unit `identify_initial_arma`
(contracts/initial-arma-identification-contract.md) — the OLS-residual cursor,
the three-stage (bp, bq)/(p, q)/(bp, bq) RegArmaBic enumeration with the mixed
gate and the bd clamp, the pinned sort/merge, and the eleven-rule BIC-margin
refinement, composed on the 21-C candidate chain (design-free HR +
PinnedLevenbergMarquardt, BIC2 with k = parameters + 1, no mean: the pinned
`RegArimaModel$Builder` never stores its mean field). Decision 6 stays open —
the decoder call-site module state (modelsCount 5, maxP/maxQ 2, maxBp/maxBq 1,
balanced false, mixed true, eps 1.0E-7, the stored-never-read
acceptWhiteNoise) stays transcription constants and required request data.
Evidence for the sign-off: the double-captured JVM corpus (72 cursors, 1908
candidate cells, 288 select runs, byte-identical across two JVM processes)
replays with 0 failures — every decision, enumeration slot, cursor and panel
set exact; 1858 checks bit-exact; the 29 banded checks are the documented
basin/drift classes around the 21-A.2 HR 1-ulp start band, none of which
moved a decision. The refinement hill-climb, SarimaComponent/AICc comparison
and the round-structure wiring stay with 21-F/21-H.

Stage M part 2 position recorded with 21-F (2026-11-18): the refinement and
comparison stage is delivered as the qualified unit `refine_arma_model`
(contracts/arma-refinement-contract.md). Bytecode resolution first: the
planned `SarimaComponent` hill-climb + `AICcComparator` -0.2/-0.1 margins are
dead or stage-R code in the pinned 3.9.0 jars (`SarimaComponent` has zero
users; `AICcComparator` is used only by the calendar/TD modules) - the live
stage is `x13.regarima.AutoModellingModule.process`, transcribed here on the
21-D decision unit and the 21-E `select_arma_orders` seam (21-E refactored to
expose it). Key pinning finding: `UnitRoots.D(n)` is the single tap
`1 - B^n`, not `(1 - B)^n`, so the module re-differences the linearized data
with `(1 - B^d)(1 - B^P)^bd` - genuinely different from the 21-E cursor
filter at d = 2 (exercised by the `m_i2_d2*` corpus cases). Decision 6 stays
open: both module states arrive as required request data (decoder
`readAutoModel` values). Evidence for the sign-off: the double-captured JVM
corpus (23 cases - 21 normal, 2 genuine airline-fallback runs - byte-
identical across two JVM processes) replays with 0 failures: linearized data,
re-differencing filter, differencing quadruple and all 21 t-statistics bit-
identical, every spec/mean/changed decision and panel slot exact, 86/105
panel cells bit-exact with the 22 remaining in the documented 21-E
tight/basin/drift bands (worst dBIC2 7.9e-06, none moving a decision). The
round-structure wiring stays with 21-H.

## Non-goals

No TRAMO/SEATS (items 23/24), no benchmarking/diagnostics (item 25), no
exact `rsa3` promotion until its full pipeline exists (roadmap acceptance
rule), and no unilateral change to any pinned verbatim surface — the two
coordinated-change-needed items recorded in
contracts/x11-selection-pass-contract.md stay coordinated.
