# TRAMO/SEATS roadmap — items 23/24 (M5 slice ledger)

Planning decomposition of items 23 (TRAMO-specific preprocessing and selection)
and 24 (SEATS model decomposition and component uncertainty) into deliverable
slices on top of the qualified Kalos kernels, in the shape of
[item21-roadmap.md](item21-roadmap.md). Everything below is grounded in the
pinned rjd3 3.9.0 tramoseats jar family (`jdplus-tramoseats-base-{api,core,r,
protobuf}-3.9.0.jar`, hashes in the M5 pinning notes) — `javap -p -c` dumps under the
probe tree `tmp_opencode/jdprobe/m5/` and, decisively, LIVE JVM probes of the
pinned 3.9.0 stack run 2026-11-21 (`tmp_opencode/m5probe/`, double-captured
byte-identical across two OpenJDK 21 processes with the G2 intrinsic-disable
recipe; session record `tmp_opencode/kalos-m5-pinning.md`, sections 1–6).
Nothing here is implemented — this is the plan, its dependency order, its
per-slice commit boundaries, and the decision boundaries that need maintainer
sign-off before their slices start. The item-level acceptance rules stay as in
[docs/NUMERICAL_ROADMAP.md](NUMERICAL_ROADMAP.md) §23/§24: TRAMO is NOT X-13
(no routing `perTramo()` through the native X-13 selector), the TRAMO/SEATS
`rsa0`–`rsa5`/`rsafull` are separate expanded specifications, SEATS defaults to
Wiener–Kolmogorov with a Kalman-smoother alternative that must be defined, not
assumed, and acceptance needs rsa3 AND rsafull end to end.

## The pinned master pipelines (probe-verified)

### Full pipeline — `TramoSeatsKernel.of(TramoSeatsSpec, ModellingContext).process(TsData, ProcessingLog)`

(this is the exact entry `jdplus.tramoseats.base.r.TramoSeats.process` drives)
in bytecode + observed-trace order:

1. `preliminary` checks (`PreliminaryChecks$Tool`);
2. `tramo.process(ts, log)` → `RegSarimaModel` (the item-23 machine below);
3. `TramoSeatsKernel.of(RegSarimaModel)` → `SeatsModelSpec` — the bridge is
   probe-decoded: back-transformed, LoP-adjusted interpolated series
   (`interpolatedSeries`/`deterministicEffect`/`backTransform`, `divide`/
   `subtract`), log flag, mean-correction, estimation variance, and the fitted
   SARIMA re-pinned as a `SarimaSpec` with d/bd taken from the FITTED model and
   every coefficient `ParameterType.Fixed` (pinning notes §6.5);
4. `seats.process(SeatsModelSpec, log)` → `SeatsResults` (item 24);
5. `SeatsResults.getFinalComponents` + `TwoStepsDecomposition.merge` → the
   finals `SeriesDecomposition` (SEATS Stdev rows are NOT carried into the
   merged finals — they stay on the decomposition result; extractor decision
   for 24-D);
6. `CholetteProcessor.process` — only when the benchmarking spec is enabled
   (the rsa* templates carry `SaBenchmarkingSpec` defaults OFF; probe fact
   `benchmarking false`);
7. `TramoSeatsDiagnostics.of` + results assembly; on any failure the fallback
   path recomposes components from the preprocessing model alone.

### TRAMO — `TramoKernel.of(TramoSpec, ModellingContext).process(TsData, ProcessingLog)`

Live stage order on the pinned jars (identical in the TR3 and TRfull double-
captured traces; pinning notes §2) — provably DIFFERENT from the X-13
`RegArimaKernel.calc` order:

```
seasonality test  ->  log-level test  [TRfull: + F-based trading-days tests]
  -> round 0  { outliers selection  ->  full estimation }
  -> round 1  { differencing selection  ->  arma selection
                -> outliers selection  ->  full estimation }
```

TRAMO runs a seasonality test FIRST, defers the d/D decision to round 1 AFTER
the round-0 outlier pass, and re-runs outliers AFTER the ARMA search. Its
critical values are runtime-adaptive kernel state (`curva`, `pcr` fields with a
`reduceVa` rule), not X-13's `calcCv(n)`-once policy. Decision units are the
`tramo.internal` trio (`ArmaModule`+`ArmaModelSelector`, `DifferencingModule`,
`OutliersDetectionModule`) plus `LogLevelModule`, `SeasonalityController`/
`SeasonalityTests`/`TramoSeasonalityDetector`, `TradingDaysController`,
`ModelController` (the shared decision-module base: `checkMean`/`estimate`/
`transferInformation`), `ModelStatistics` (its `refStats` compare gates
`pass2`), `ModelEstimator`, `FinalEstimator` — all
class-load-confirmed live (pinning notes §5). `TramoSelection` does not exist
in the pinned jars (ClassNotFoundException) — plans referencing it are void;
the live ARMA decision machine is `internal.ArmaModule` over
`ArmaModelSelector.FastBIC` candidates (an HR-based fast-BIC search — the
selector bytecode references `HannanRissanen$Builder` — NOT X-13's
`RegArmaBic`/`AutoModellingModule` machinery).

### Spec materialization — where the TRAMO templates become settings

`TramoSpec` holds `TR0…TR5/TRfull` as static finals built in `<clinit>`;
`TramoSpec.fromString` resolves exactly `{tr0..tr5,trfull}` (+ uppercase) to
those SAME instances and REJECTS `rsa*`/`Trfull`. `TramoSeatsSpec` holds
`RSA0…RSA5/RSAfull`; its `<clinit>` pins `RSA0 = DEFAULT` (builder defaults)
and `RSA1..RSA5, RSAfull = builder().tramo(TR1..TR5, TRfull)
.seats(DecompositionSpec.DEFAULT)` (bytecode-proven). The R side
(`rjd3tramoseats::tramo_spec`) renames `rsa→tr` BEFORE `fromString`. The
template matrix (live reflective dump, pinning notes §3):

| spec | ARMA start | transform | autoModel | outliers | calendar |
|---|---|---|---|---|---|
| TR0 | airline (0,1,1)(0,1,1) | None | OFF | OFF | OFF |
| TR1 | airline | Auto | OFF | AO/LS/TC | OFF |
| TR2 | = TR1 | Auto | OFF | AO/LS/TC | TD2+LeapYear, Easter test, Separate_T |
| TR3 | airline | Auto | **ON** | AO/LS/TC | OFF |
| TR4 | = TR3 | Auto | ON | AO/LS/TC | TD2+LeapYear |
| TR5 | = TR3 | Auto | ON | AO/LS/TC | TD7+LeapYear |
| TRfull | airline | Auto | ON | AO/LS/TC | TD7+LeapYear, Easter IncludeEaster, FTEST (p=0.01) |

The TRAMO `AutoModelSpec` scalars are cancel 0.05, pc 0.12, pcr 0.95, tsig 1.0,
ub1 0.97, ub2 0.91 — measurably DIFFERENT from the X-13 decoder set (cancel
0.1, ub1 1.0309278350515465, ub2 1.1363636363636365): the roadmap's
"distinct policies" warning is now a spec-layer fact. SEATS
`DecompositionSpec` constants: DEF_XL 0.95, DEF_EPSPHI 2.0, DEF_RMOD 0.5,
DEF_SMOD1 0.8, DEF_SMOD 0.8, DEF_FORECASTS −2, DEF_BACKCASTS 0. Note the
template families do NOT align with X-13 (`rsa2c`/`rsa5c` have no TR
counterpart; `TRfull`/`rsafull` has no X-13 counterpart): the binding-layer
template decode tables must carry a SEPARATE TRAMO list.

## What exists in Kalos and is reusable (probe-cited)

| Kalos unit (item) | Verdict | Why (probed behavior in the TRAMO/SEATS stack) |
| --- | --- | --- |
| `state_space`, `diffuse_state_space`, `regression_likelihood`, `arma_objective` (20a/20b) | reuse-as-is | `tramo.ModelEstimator` builds a toolkit `RegSarimaComputer` (builder→precision→`LevenbergMarquardtMinimizer`) — the same likelihood/estimation machine the 21-* units already reproduce; TRAMO call-site values (tol 1e-7, ML true) arrive as request data |
| `ssq_deviances`, `ssq_levmar`, `estimation_control` (21-A) | reuse-as-is | the pinned-LM footprint IS the TRAMO minimizer (probed above); control fields stay required request data |
| `hannan_rissanen` (21-A.2) | reuse (surface) | `ArmaModelSelector` bytecode references `HannanRissanen$Builder`; exact FastBIC usage is decoded inside 23-D |
| `outlier_scoring` / `outlier_scan_engine` (21-C) | fork-with-TRAMO-policy | same shared `IOutliersDetectionModule.Info` event surface, but TRAMO cv policy is `curva`/`reduceVa`/pcr-adaptive (TramoKernel fields), not `calcCv(n)` once |
| `transformation_decision` (21-B) | fork-with-TRAMO-policy | TRAMO's `LogLevelModule` runs AFTER the seasonality test and reports a "log-preference" statistic (probe event detail), not the X-13 AICC-pair form; threshold source (TransformSpec) decoded in 23-B |
| `differencing_decision` (21-D) | fork-with-TRAMO-policy | TRAMO's `internal.DifferencingModule` (fields `ub1`/`ub2`/`pc`/`cancel`/`maxd`/`maxbd`/`useml`) over the `RegularUnder/SeasonalUnder(+2)` test battery + `SeasonalityController`/`TramoSeasonalityDetector`; those four test classes class-load on the live run, `SeasonalOverDifferencingTest` does not (pinning notes §5) |
| `initial_arma_identification`, `arma_refinement` (21-E/F) | not-applicable | X-13-specific `RegArmaBic`/`AutoModellingModule` machinery; TRAMO's live engine is `internal.ArmaModule` + `ArmaModelSelector.FastBIC` (new 23-D unit) |
| `regression_variable_tests` (21-G) | not-applicable | TRAMO's regression test family is `Default/Fast/Automatic{,F,Wald}RegressionTest` + `TradingDaysController` — live split: `FastRegressionTest` on both runs, `DefaultRegressionTest` on rsa3, `AutomaticFRegressionTest`+controller on rsafull (`Automatic`/`AutomaticWald` not loaded); new 23-E unit |
| `arma_diagnostics`, `special_functions`, `mlj_engine` (21-H slice 1) | reuse-as-is | `ModelStatistics.of(...)` is live-loaded and consumed inside `TramoKernel` itself: `iterate` pins `refStats`, `pass2` compares the current `getLjungBoxPvalue()`/`getSe()` against it (bytecode) — the same Ljung-Box/SE family the 21-H diagnostics units already reproduce |
| `final_estimator`, `final_information` (21-H slice 1/H.0) | fork-candidate | `tramo.FinalEstimator` is live-loaded; whether its stencil/information path coincides with the qualified x13 producer is decided in 23-F after decode |
| `residual_information`/`arma_information` (20c.1b.2, TRAMO covariance contract) | reuse-as-is | already the TRAMO covariance convention (`method="tramo_residual"` residual-Jacobian modes) — the TRAMO-side coefficient uncertainty surface |
| `arima_forecasts` (21-H.4) | not-applicable | X-13 extension machine; SEATS forecasts/backcasts come from the SEATS estimator stack (24-C) |
| `x11_*` (22b), `x13_allocation`/`x13_fit`/`x13_automatic_fit` (22c/21-H) | not-applicable | SEATS replaces the X-11 leg; deterministic restoration is the TramoSeatsKernel bridge/finals (pinning notes §6.5), not `x13_restore` |
| `preparation`, `transforms`, `numerics`, `linear_algebra`, provider boundary | reuse-as-is | engine-agnostic; the G2 bit-exact build semantics (`-ffp-contract=off`, intrinsic-disable capture recipe, double capture) carry over verbatim |

## Item-23 decision units and slices

Slice pattern = the proven 21-* pattern: one decision unit per slice, JVM
corpus golden (double-captured, evidence under
`validation/evidence/native/<unit>/` with SHA256SUMS), replay verifier under
`tools/`, ctest unit + sweep, contract doc under `docs/contracts/`, fail-closed
matrix, both build lanes + sanitizers, manifest/migration checks. All
decision-stage scalars arrive as required request data — no promotion of
pinned values without a maintainer ruling (the 21 precedent).

- **23-A — `tramo_model_builder`** (spec materialization + `TramoModelBuilder.build`):
  the template-expansion table as pinned data plus the ModelDescription
  construction decisions (the `initializeArima` frequency rule — under
  autoModel the start is airline iff the series is NOT annual, plus
  `setMean(true)`; otherwise `spec.getArima().withPeriod(freq)` —,
  `initializeTransformation`, `initializeMean`, `initializeMissing`,
  calendar/outlier/user/ramp/intervention registration with the `regeffect`
  attribute vocabulary the 22c seam already pins). Full spec in the last
  section. This is the TRAMO analogue of the stage-Z decoder contract, moved
  front — TRAMO's estimation spine is already qualified, the builder is the
  first TRAMO-specific machine every later unit consumes. `TramoModelBuilder`
  has a public constructor and public `build(TsData, ProcessingLog)`, so the
  oracle needs no instrumentation.
- **23-B — `decide_tramo_transform` + `scan_tramo_outliers` (seasonality/log-level
  + round-0 outlier scoring)**: the `SeasonalityController`/`SeasonalityTests`
  pre-stage and the `LogLevelModule` decision, then the round-0
  `internal.OutliersDetectionModule` loop with TRAMO's `curva`/`pcr`/`reduceVa`
  critical-value policy (fork of the 21-C scan engine at the policy layer). The
  JVM topology is TWO modules: the spec-driven one (AO/LS/TC/SO + tcrate + ML +
  span) used by `iterate`/`execOutliers`, and a robust one (AO+LS, hard-coded
  precision 1e-3) used inside `testTransformation` for the log-vs-level
  comparison (pinning notes §6.7).
  Sign-off: D3 (outlier iteration semantics) before the second half — CLOSED APPROVED
  2026-11-22 (option A, faithful multi-round port landing in 23-F; see register).
- **LANDED 2026-11-22 (both halves as scoped: Unit A + round-0 Unit B).**
  `kalos::decide_tramo_transform` (Unit A: calcCv grid, the seasonality
  detector machine, the LogLevelModule pair through the CI-FDLIBM-routed
  estimation layer; 26-case bit-exact replay) and
  `kalos::scan_tramo_outliers` (Unit B: the round-0
  `OutliersDetectionModule` observation — make gate, design pre-stage,
  OlsComputer/HR/stabilize/warm-optimize estimateModel, FastKalmanFilter
  initmodel, the SimplifyingTool-cancelled pi weight columns, searchMax and
  the `!(|maxT| < cv)` gate; all 22 numerically observable corpus cases
  bit-exact at IEEE-754 level against the live-JVM microscope dumps).
  D3 untouched: the multi-round driver, `curva`/`reduceVa`/pcr adaptation
  and the robust AO+LS pre-stage stay open; `sc_so`/`sc_spanout` (round-0
  pick later emptied) and `sc_spannone` (collapsed scan) are registered as
  the decision-brief evidence, `sc_pexao47` pins the warm-seam fail-closed
  throw. Contracts `tramo-transform-decision` / `tramo-outlier-scan`.
- **23-C — `decide_tramo_differencing`**: round-1 `internal.DifferencingModule`
  with the `RegularUnder(+2)`/`SeasonalUnder(+2)` test battery (class-load
  confirmed live; `SeasonalOverDifferencingTest` is NOT loaded on the probe path
  and must be forced by corpus cases)
  and the pcr/pc/tsig/ub1/ub2 rule set (all already probe-materialized as spec
  scalars; thresholds decoded from the dumps).
- **LANDED 2026-11-22 (round-1 ladder as scoped; battery stage = 23-C.2).**
  `kalos::decide_tramo_differencing` transcribes the live
  `DifferencingModule.process` ladder — the `comespd`/autlar gate with the
  airline fallback, the step0/nextstep ladder (HR start, the 1.02/1.12
  stabilize rules, warm ML on the 23-B seam with the
  tryUrpC/tryUrmC-cancel fail-closed guard), the `searchur` scans on the
  `FastEigenValuesSolver` closed forms with the scaled `ComplexType.abs`,
  the `FastKalmanFilter.fastFilter` tmean (new
  `pinned_fast_filter_residuals`) and the `isMeanCorrection` table —
  bit-exact on all 120 module fact lines of the 22 replayed cases (25-case
  double-captured corpus), incl. the `(-1)^(d + bd)` filter-orientation pin
  and the fdlibm log channel. The battery/over-side replay stays open as
  23-C.2 (its landing facts pinned inert; gated on the noncentral-beta
  F-CDF prerequisite); the `ml = true` multi-round wiring is 23-F.
  Contract `tramo-differencing-decision`.
- **LANDED 2026-11-22 (23-C.2: F kernel + battery activation).** The
  noncentral-beta F-CDF prerequisite landed first — `detail::incBeta`/
  `incBetaCf`/`f_upper_p` (Lentz continued fraction, the 1000-iteration
  throw as the NaN surface, fdlibm exp/log) replay BIT-exact over the
  double-captured live-JVM golden `validation/reference/fcdf_golden.txt`
  (18489 rows: grid + specials + the 90 corpus F triples; cf 3342 rows with
  1770 continue-throws, beta 11732/51, fprob 3325/1081) inside
  `test_special_functions`. The 23-C battery facts went from inert to
  positive: `kalos::decide_tramo_differencing_battery` transcribes the
  `SeasonalityController` passes (the `hasSeasonality` score, the
  m011+`setBq(1)` swap, the seasonal flips, the provably-Unchanged second
  pass), the `RegularUnder(+2)`/`SeasonalUnder(+2)` ladder (ru1
  mean/quasi-root gates, su1 fixSeasonalRoots + residual ladder, ru2 lbp
  gate with the corrected `dcmpl` NaN reading + the zero-mean-autocovariance
  sign count `pos12 >= freq || pos12 >= 9 || total >= 17`, su2 ost gate),
  the `ModelComparator` (BIC over builder defaults, exact 71/85 branch map),
  the `SpectralPeaks` length dispatch and the per-state
  `SeasonalOverDifferencingTest` codes — bit-exact on the 15-case battery
  corpus `validation/evidence/native/tramo-differencing-battery/` (1175
  fact lines, 90 F p-values through the kernel, 75 decision lines, 26
  candidate snapshots; double capture byte-identical; `-Xlog:class+load`
  witnesses force `SeasonalOverDifferencingTest` + `SpectralPeaks`;
  SHA256SUMS verified; the io = y cases and the `ml = true` rounds stay
  23-F). Contract updated with the battery section.
- **23-D — `select_tramo_arma`**: `internal.ArmaModule` + `ArmaModelSelector.
  FastBIC` — the HR-fast-BIC candidate enumeration, the `CInfo` panel and the
  selection rule, composed on the reusable HR/estimation units (the selector's HR
  call site never executed on the probe series — `HannanRissanen` is not
  class-loaded there, so the corpus must force candidate estimation). (No relation
  to 21-E/F; expected to be a genuinely new engine port.)
- **LANDED 2026-11-22 (23-D: ArmaModule + ArmaModelSelector FastBIC).**
  `kalos::select_tramo_arma` transcribes the live `ArmaModule.process` —
  the `comespa`/`maxInic` gate (monthly pcnt ∈ {3,2,0}, the 68/62 airline
  thresholds, `checkespa`'s bare second-gate autlar sign), the
  `calcmaxspec`/`checkmaxspec` order tables (the real divergence:
  `calcmaxspec` bp = bq = 1 unconditional on bd, `checkmaxspec` bp = 1
  only at bd == 0), the three-stage selector with the `getPreferred` pick,
  the `mergeInto` watermark merge and the eleven-gate `select()` tie-break
  (alpha tiers 0.0625/0.25/1.0, budgets 0.01/0.0025/0.0075, per-accept
  relief) — on the qualified HR seam plus the FastBIC
  `log(ssq/n) + k·log(n)/n` over the pinned `pinned_fast_filter_residuals`
  (fdlibm logs), bit-exact on 14 module-line cases + the D11 quarterly
  refusal of the 16-case double-captured corpus
  `validation/evidence/native/tramo-arma-selection/` (probe in the
  `tramo.internal` split package reading the package-private gates;
  `--repro` 7 files byte-identical; SHA256SUMS verified; the constant
  series pins the upstream `olsResiduals` NPE record-only). The
  `-Xlog:class+load` witness (decoded bytecode corroborated) pins HR +
  fast conditional BIC with ZERO `RegSarimaComputer`/`GlsSarimaComputer`/
  `internal.HannanRissanenInitializer`/`LevenbergMarquardtMinimizer` loads
  — no ML machine. OLS residuals are request data (upstream regression
  stage); the multi-round driver stays 23-F. Sweep
  `tools/verify_tramo_arma_selection.cpp` 14 replayed + 1 refusal + 1
  record-only, 0 failures; shim-invariant (thatan/explog/all + no-AVX2).
  Contract `tramo-arma-selection`.
- **23-E — `run_tramo_regression_tests`**: `TradingDaysController` FTEST/Wald/T
  families plus the `FastRegressionTest`/`Default`/`Automatic{,Wald}` family and
  the `ModelStatistics` measures the `iterate`/`pass2` `refStats` gate consumes —
  GATED by 21-I for the
  TD/Easter machinery; the no-calendar branch (TR0/TR1/TR3) is the
  `ModelStatistics`/mean-rule layer only and is not gated.
- **23-F — `tramo_final_estimation` + kernel round driver**: `FinalEstimator`
  decode (fork-candidate against `final_estimator`/`final_information`), then
  the `TramoKernel.process` driver itself (round/pass/pass3 wiring, refAuto/
  refStats compare) — the `x13_automatic_fit` analogue (`tramo_fit`).
- **23-G — end-to-end TRAMO oracle** against the double-captured
  `TramoKernel.process` corpus (mirrors 21-H.2/H.3: probe observer, decision-
  bit qualification, measured tiers, sweep).

## Item-24 decision units and slices

- **24-A — `validate_tramo_seats_model`** (admissibility + cutoff):
  `DefaultModelValidator` — the XL=0.95 parameter-cutoff machine (already
  exercised live: the probe's boundary airline at exactly ±0.95 reports
  `parametersCutOff=true` with pinned landing −0.95), epsphi/rmod/smod
  admissibility tests, model-change flags. Input is the probe-decoded fixed
  `SeatsModelSpec` shape — no TRAMO needed.
- **24-B — `decompose_ucarima`** (canonical decomposition + approximation):
  `RootDecomposer`/`SimpleModelDecomposer` → `UcarimaModel` (the probe
  captured the canonical 3-factor decomposition of a fitted (0,1,1)(0,1,1)
  with per-factor variances, double-captured), plus
  `DefaultModelApproximator` for non-decomposable models (approximation-mode
  policy D6) and the compact-model reduction (`compactUcarimaModel`).
- **24-C — `seats_extract_wk`**: the Wiener–Kolmogorov path
  (`seats.WienerKolmogorovEstimator` over toolkit `WienerKolmogorovEstimators`,
  `BurmanEstimates`) — initial vs final extraction, component
  forecasts/backcasts (DEF_FORECASTS −2 = auto), bias correction, error
  variances. Kalman/Matrix estimators are NOT loaded by the default path —
  envelope decision D1; parameter-estimation uncertainty vs conditional
  extraction uncertainty stay separate surfaces (roadmap rule).
- **24-D — `seats_pipeline`**: the `SeatsKernel.process` orchestration
  (validation→decomposition→estimation→bias stages), the
  `TramoSeatsKernel.of(RegSarimaModel)` bridge port, `TwoStepsDecomposition.
  merge`, and the result surface (which Stdev rows are exposed — finals merge
  drops them; extractor decision recorded).
- **24-E — end-to-end TRAMO/SEATS oracle + composition**:
  `tramo_seats_fit` driver = 23-F + 24-D; acceptance corpus MUST cover rsa3
  AND rsafull end to end (roadmap acceptance), the latter gated by 21-I for
  the FTEST stage unless the maintainer defers rsafull (register D2).

## Slice sequence, dependencies, and blocking status

Recommended order: **23-A → 24-A → 24-B → 23-B → 23-C → 23-D → 24-C → 23-E
(no-calendar branch) → 23-F → 24-D → 23-G → 24-E**. The SEATS validation/
decomposition units front-load because they consume only the fixed-model input
shape (probe-verified) and de-risk the 24 series while TRAMO decisions settle.

Dependencies on the unfinished X-13 queue (M4 report §6):

| Queue item | Blocking? | Why |
| --- | --- | --- |
| 21-I (TD/Easter/user-variable detection) | **BLOCKING** for 23-E's calendar branch, the TRfull-FTEST part of 23-G, and the rsafull leg of 24-E. **NOT blocking** for 23-A/23-B/23-C/23-D/23-F, 24-A…24-D, or the rsa3-branch parts of 23-G. | TRfull inserts "F-based trading days tests" into the live trace (probe class-load diff rsa3→rsafull); TR2/TR4/TR5 register TD/Easter design columns in the builder |
| 21-A replay-fidelity pass (Multiple alternate-start basins, G2 seam) | **NOT blocking.** | TRAMO lands through the pinned-LM/HR surfaces whose drift bands are already measured; if the TRAMO corpus shows the same basin drift it is banded per 21 precedent, and the 21-A pass (when it lands) re-qualifies both engines |
| Native restricted-fit export (unblocks X-13 rsa0/1/2c) | **NOT blocking.** | TR0 is a builder+estimation path inside 23-A/23-F, not the X-13 fixed-fit seam |
| rkalos horizon mirror | NOT blocking. | product-layer, not core |
| Contract-doc touch-ups, pykalos smoke ref | NOT blocking. | cosmetic |

## 23-A — spec: `tramo_model_builder` (cold-resume ready)

- **Unit**: `kalos::tramo_model_builder` (`src/tramo_model_builder.cpp`,
  header-only public surface like the other units; no R/Rcpp). Inputs: the
  expanded `TramoSpec` field set as required request data (template rows arrive
  as data, never as a name the core resolves), the series + start, the
  `ModellingContext`-equivalent user-variable table (empty in-envelope).
  Outputs: the built ModelDescription mirror — start-rule status (airline iff
  the series is not annual, else the re-periodized fixed spec), initial
  SARIMA spec (airline or override), initial log/None flag + LoP rule,
  estimated-mean flag, missing-value interpolation plan, the registered
  variable list (kind, name, period, `regeffect` attribute) — plus the
  template-expansion table itself as a pinned data function
  `tramo_template(name) -> spec fields` covering TR0…TRfull (names accepted:
  exactly `tr0..tr5,trfull` + uppercase, matching `fromString`; anything else
  including `rsa*` is a fail-closed refusal).
- **JVM oracle entry points** (all live-probed 2026-11-21):
  `TramoSpec.fromString` / the `TR*` statics for the expansion table
  (reflective getter dump = `specs.out`);
  `new TramoModelBuilder(spec, new ModellingContext()).build(ts, log)` for the
  builder corpus — public signatures, no instrumentation needed; the
  ProcessingLog observer records the builder's own messages. Compile with the
  m5probe classpath, run with the G2 recipe
  (`-XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog`), pin the JVM to
  OpenJDK 21 exactly like `validation/reference/capture-x13-automatic.R`.
- **Corpus shape** (`capture-tramo-model-builder.R` +
  `TramoModelBuilderProbe.java` under `validation/reference/`, deterministic
  inputs built with StrictMath only; every case echoes its "req" block):
  the seven templates TR0…TRfull on the 96-point monthly fixed series; a
  non-autoModel fixed-order spec (e.g. (1,1,0)(1,1,1) with coefficients fixed —
  exercises the `initializeArima` `setArimaSpec(withPeriod(freq))` branch,
  reached only with autoModel off);
  forced-log and levels transforms on one case each; mean on/off via MeanSpec;
  a leading/interior/trailing missing
  pattern (initializeMissing); manual AO/LS/TC/SO registration through the
  outlier variable path (the regeffect-vocabulary cross-check against the 22c
  allocation contract; `initializeOutliers` also picks the TC rate and the
  periodic-SO factory by frequency); a user-variable and a ramp case; an ANNUAL
  and a quarterly series
  (the builder branches on `annualFrequency()==1`; annual is the non-airline
  case; the non-monthly cases are captured for the record and refused at the
  kalos unit boundary in the first envelope per D11); one
  non-January start. ~18–22 cases.
- **Fixture plan**: `validation/evidence/native/tramo-model-builder/`
  (`cases.csv`, `inputs.csv`, `builder-facts.csv` — %.17g values, variable
  rows with attributes —, `builder-events.csv`, `metadata.R`,
  `SHA256SUMS.csv`), captured twice from independent JVM processes and
  byte-compared by the `--repro` gate (final-information precedent); the
  generated golden header lands in `tests/jvm_tramo_model_builder.h`.
- **Gates**: new ctest `test_tramo_model_builder` in BOTH lanes (OMP +
  serial) bit-exact; `tools/verify_tramo_model_builder.cpp` sweep over the
  corpus at 0 failures (decision fields exact, numerics %.17g bit-exact —
  this unit is pure bookkeeping + calendar algebra, no transcendental
  estimation, so the whole unit is bit-tier); ASan/UBSan run of unit+sweep;
  the template-expansion table asserted against the pinned `specs.out` rows;
  fail-closed matrix (unknown template names, including `rsa*` and case
  variants such as `Trfull`, non-monthly frequency, non-finite input,
  misaligned user
  variables, outlier types outside the AO/LS/TC/SO set); existing suite
  unchanged (ctest count +N, nine sweeps + oracle unchanged);
  `check-manifest.py` and `verify-migration.py` 406 after staging; contract doc
  `docs/contracts/tramo-model-builder-contract.md` including the
  "Implementation-actual deltas at landing" section from day one.
- **LANDED 2026-11-21.** All decisions bit-exact on the 22-case corpus in
  both lanes; envelope decisions D2/D5/D9/D10/D11 approved by the maintainer
  at slice time (see the dated status entry; D3/D6 stay open). Ledger: unit
  `kalos::tramo_model_builder` closed; calendar design columns, the
  collinearity `check()` pass, automatic outlier identification and the
  log/level decision remain scoped to 21-I/23-B/23-E/23-F as above.

## 24-A — spec: `validate_tramo_seats_model` (admissibility + root handling + decomposability)

- **Scope (delivered).** `DefaultModelValidator` (simplifyModel trim/cutOff,
  `stabilizeMA` vs `fixMaUnitRoots` including the degree-1 −1 quirk), the
  `SeatsToolkit.of` selector chain wired from the DecompositionSpec knobs, the
  `RootDecomposer` linear system (`LULinearSystemSolver2` bit-exact) and the
  `UcarimaModel.setVarianceMax(-1, noisy)` minima-sum decomposability test
  (`Spectrum$Minimizer` by-roots/by-grid), plus the kernel flags/events.
  Bit-exact golden numerics; decisions from the `cases.csv` matrix.
- **Explicitly out.** Everything after a null decomposition
  (`DefaultModelApproximator`, register D6 — stays OPEN, the unit reports
  `approximation_required`, its events are an exact prefix of the captured
  `kevt` frame, and the corpus records the live continuation incl. the two
  approximator `MatrixException` crashes and the Noisy NPE); spectral
  factorization and variance allocation (24-B); estimation uncertainty (24-C).
- **D11 refusals (record cases).** Five corpus rows whose live path enters
  `Polynomial.roots()` of degree > 2 (`d_d2`/`q_airline`/`q_d2` degree-4 and
  `v_cut_bq2` degree-10 minimizer numerators, `v_simplify` stationary-`bphi`
  reconstruction) were refusal-asserted at landing; the QR cascade is now
  ported (next bullet) and all five replay as POSITIVE assertions.
- **24-A.1 extension — the `FastEigenValuesSolver.qr` cascade (LANDED
  2026-11-21).** Bit-exact port of the degree ≥ 3 roots machine (Householder
  Hessenberg, Wilkinson/exceptional shifts, bulge + chase, deflation at
  `Constants.getEpsilon()`, `eigenValues()` + Givens root extraction, the
  `dcmpl`/`dcmpg` comparison flavors; convergence profiles bit-matched against
  JVM solver-state dumps). The two JVM-abortive cascade ends (it_max → null
  roots → consumer NPE; 2×2-tail chase → `B[-1]` AIOOBE) are mirrored as
  internal aborts reproducing the captured null-decomposition decisions —
  measured firing on `q_airline`/`q_d2`; the `new Random()` bulge branch is
  substituted deterministically (the JVM is not bit-reproducible there, the
  captured decisions are — contract doc, pinned-semantics section). Fixed the
  24-A `SymmetricFrequencyResponse` transposition (Ux computes `Uᵀ x`);
  pinned the `SarimaModel.seasonalAR()/seasonalMA()` loop-bound quirk
  verbatim (bp/bq ≥ 2 silently drops the seasonal component). Unit 983
  checks 0 failures, sweep 35/35 + 1026 checks (8 envelope probes), both
  lanes 66/66 fresh trees, ASan/UBSan clean; the csv corpus is byte-unchanged
  (landing `--repro` re-capture byte-identical, SHA256SUMS 5/5).
- **LANDED 2026-11-21.** 30 replayed + 5 refusal cases bit-exact in both
  lanes (66/66 each, fresh trees); sweep 35/35 + determinism + envelope
  probes; ASan/UBSan clean; D6 untouched (observations in the contract doc).
  Ledger: unit `kalos::validate_tramo_seats_model` closed for the
  admissibility/root-handling/decomposability decision surface INCLUDING the
  degree ≥ 3 root cascade (24-A.1); the one-sided MA/variance facts
  (`r c{i} ma|var`, `k.ucar`, `model seasonality`) and the approximation
  policy remain scoped to 24-B/D6 as above.

## 24-B — spec: `decompose_ucarima` (the decomposition seam)

- **Scope (delivered).** The seam between 24-A admissibility and SEATS
  filter construction: the raw selector-chain split ("r"),
  `UcarimaModel.setVarianceMax(-1, false)` ("d") and
  `SeatsModel.compactUcarimaModel(false, false)` ("cmp"), emitted as the
  probe's fact grammar. The one-sided spectral factorization chain
  (`SymmetricFilter.factorize`: degree-0 shortcut → `EigenValuesDecomposer2`
  → `EigenValuesDecomposer`, the z = x + 1/x companion +
  `balance`/`hessenbergQR` (maxIter 120), the real-pair / unit-circle
  classification with faithful AIOOBE bounds traps, `UnitRootsSolver` +
  `IVector.sqrt` quirks, exact LS divide, leja + `fromComplexRoots`), the
  `Spectrum$Minimizer` dispatch on `2*degree+1 <= 6`, and the Java-`%.17g`
  renderer (shortest round-trip digits PADDED to 17). A live JVM chain probe
  over all 150 distinct corpus symmetric filters (124 EV2 / 5 EV1 / 1 fast /
  20 none) settles the tier scope; JVM abort classes are captured as data
  (dangling-prefix `maerr`/`varr`/`sfdr` lines, `d.decompose null`, the
  SeatsException status row).
- **Explicitly out.** `DefaultModelApproximator` (register D6 — refused);
  the fast/robust factorizer tiers (their only corpus filter belongs to a
  validator-refused case); `k.*` kernel flags and post-seam estimation
  statuses (record-only); the WK-only downstream is D1 (24-C re-pins
  WK-vs-sfd when wiring the seam into the pipeline).
- **LANDED 2026-11-21.** 32 replayed cases (1620 fact lines, byte-identical
  incl. the Eco- and SeatsException-status rows) + 17 pinned refusals
  (16 approximation_required, 1 Noisy) bit-exact in both lanes (67/67 each,
  fresh trees); sweep `tools/verify_decompose_ucarima.cpp` 49 cases direct
  from csv, 1620 lines exact + determinism + grammar audits, 0 failures;
  ASan/UBSan unit + sweep clean; existing suite invariant. Transcription
  fixes found on the way (EV1 chain tier + its `column(degree-1)` placement,
  the `2*degree+1` minimizer dispatch, `jvm_decompose_abort` escaping the
  driver's `std::exception` catch, the `balance` shrink-loop `dcmpl` flavor,
  the QR left-update slots and the `sqrt` cart branch) are itemized in the
  contract doc. Ledger: unit `kalos::decompose_ucarima` closed for the
  decomposition-seam surface; D6 CLOSED APPROVED (row below).

## Maintainer decision register (sign-off before the owning slice)

| # | Decision | Slice | Position recorded at M5-open |
|---|---|---|---|
| D1 | SEATS extraction path envelope: WK-only first, Kalman/Matrix estimator later (probe: neither is loaded by the default path) | 24-C | recommend WK-only first, others fail-closed |
| D2 | Spec coverage order: rsa3-first mirroring X-13, rsafull end-to-end deferred to 24-E and gated by 21-I | 23-G/24-E | recommend yes (roadmap acceptance still requires both before item 24 closes) |
| D3 | TRAMO outlier iteration semantics: round/pass3 wiring, `curva`/`reduceVa`/pcr adaptation, maxOutliers/maxIter defaults | 23-B | **CLOSED APPROVED 2026-11-22 (maintainer, option A)**: faithful multi-round port in 23-F — round/pass3 wiring + `curva`/`reduceVa`/pcr adaptation ported, `maxOutliers`/`maxIter` defaults decoded as constants where the jar shows them constant, pass wiring ported only where the live trace shows execution (24-A dead-code discipline); the 23-B `divergent=1` final-state rows (`sc_so`/`sc_spanout` emptied picks, `sc_spannone` collapsed scan — contract `tramo-outlier-scan`) become POSITIVE multi-round assertions in 23-F; round-0 unit stays as-is (correct component); option B (round-0-only + fail-closed) rejected |
| D4 | FastBIC search: reuse HR unit as initializer vs faithful `ArmaModelSelector` port | 23-D | recommend faithful port of the selector with HR reuse inside (21-F lesson: decode before reusing) |
| D5 | TRAMO call-site minimizer: pinned LM at the units, BFGS stays the core default (probed: TRAMO builds an LM minimizer) | 23-B…F | recommend the 21-D.2 precedent verbatim |
| D6 | Non-decomposable model policy: which `ModelApproximationMode` values are in the envelope | 24-B | **CLOSED APPROVED at 24-B landing**: decomposable-only envelope — `approximation_required` decisions and `Noisy` mode refuse fail-closed with the pinned reasons; the crashing `DefaultModelApproximator` is NOT ported (validator classifies, unit refuses; contract `decompose-ucarima`) |
| D7 | Bias correction (Burman) + auto forecast/backcast counts (−2) in the first 24-C envelope | 24-C | recommend include (the live default path uses them) |
| D8 | Benchmarking (Cholette) stays OUT of the first pipeline envelope (rsa* defaults are OFF); requests refuse fail-closed | 24-E | recommend yes |
| D9 | TRAMO-side coefficient uncertainty reuses the 20c.1b.2 `tramo_residual` covariance modes (already TRAMO-pinned) | 23-F | recommend yes, kept distinct from SEATS extraction variances |
| D10 | Calendar/Easter/user-variable detection deferred to the 21-I joint surface (same native modules serve both engines) | 23-E | recommend yes — do NOT build a second TD implementation |
| D11 | Envelope: monthly frequency only at the unit boundary first (annual/quarterly corpus cases captured for the record — the start rule keys off `annualFrequency()==1`, and `TramoFrequency` is not on the live path) | 23-A | recommend yes (X-13 precedent) |

## Non-goals

No `perTramo()` product wiring (product layer = rkalos/persephone3, own
slices after core); no Terror diagnostics, no revision-history tooling, no
benchmarking beyond the fail-closed boundary, no v2/legacy `SeatsSpec` path
(not loaded by the live pipeline), and no reuse of the X-13 automatic driver
as a TRAMO skeleton — the pinned stage order differs and the probe says so.
