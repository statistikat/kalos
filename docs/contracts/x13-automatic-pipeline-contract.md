# Automatic-spec X-13 pipeline (21-H, stage Z) — integration contract

**Status: design contract at the slice boundary — the composition code
follows the slices scheduled at the bottom.** This document is the durable
integration record for the 21-H pipeline: the pinned module order, the
chained decision-record vocabulary, the estimation adapter (call-site map
from the JVM processors to the qualified Kalos seams), the scope envelope
and fail-closed matrix, and the two producer gaps that gate the code
slices. Bytecode sources of record: the current-jar dumps under
`/tmp/opencode/jdprobe/cur/` (jdplus 3.9.0, re-pinned 2026-11-19; the old
`h1/h2` orchestration dumps are superseded per the pinning notes SECTION
12), the per-unit contracts of items 21-A…21-G and 21-H slice 1, and
[x13-fit-contract.md](x13-fit-contract.md) for the downstream composition.

## The seam

The pipeline lands as an **`automatic` spec kind alongside the restricted
fixed fit**, additive behind the kind:

```cpp
enum class X13FitSpecKind { fixed, automatic }; // default fixed = today

struct X13AutomaticRequest {
    std::vector<double> input;       // complete, finite, monthly (freq 12)
    int first_period = 1;
    // ---- decoder-equivalent module state, ALL required request data ----
    double aicc_log_correction;      // TransformSpec aicDiff (-2.0 in corpus)
    // DifferencingModule + ArmaModule states (the 21-D/21-E/21-F unit
    // fields: max_d, max_bd, ub1, ub2, cancel, precision; models_count,
    // max_p/max_q/max_bp/max_bq, balanced, mixed, arma eps)
    // OutliersDetectionModule: type set, span, max_round(30), tcrate
    double ljung_box_limit, ur_limit, reduce_va, va, precision, tsig;
    bool check_mu, mixed_model, accept_airline;
    // RegressionVariablesTest: mean_test_initial(1.96)/mean_test_final(1.0),
    // derived flag; tsig cutoffs arrive, never literals at the seam
    // X-11 block as in X13FitRequest: seasonal requests (Msr allowed),
    // trend_length (0 = automatic allowed), REQUIRED sigma policy,
    // sigma cuttings, mode, horizons {0,0}
    ...
};

struct X13AutomaticFitResult {
    std::string status;              // "available" | "unsupported:<...>"
    X13FitResult fit;                // preadjust → selection → finals
    // Final model description: log, mean, orders {p,q,bp,bq,d,bd}, the
    // estimated ARMA coefficients, the selected regression variables with
    // columns, coefficients and {ami, regeffect} attributes
    // Decision chain (below), in pinned execution order
    ...
};

X13AutomaticFitResult x13_automatic_fit(const X13AutomaticRequest&);
```

`X13FitRequest`/`X13FitResult` are not grown (the schema-change note in
the fit contract applies): the automatic kind is a distinct request/result
pair that *reuses* `X13FitResult` verbatim as its downstream composition
block. The fixed path is untouched by construction — the guards of the
fixed contract (fixed seasonal requests, fixed Henderson length) stay
where they are; the automatic kind passes its own, explicitly decoder
shaped X-11 request block (Msr seasonal requests, Henderson length 0,
calendarSigma None) straight into the qualified `x11_selection_pass`,
which already delivers those surfaces (items 22b.3c/22b.3d/22b.4/22b.5).
`EstimationSolver::Bfgs` remains the core default; the automatic driver
selects `PinnedLevenbergMarquardt` only inside the qualified units that
already pin it internally (21-D/E/F machines, 21-C callback recipe). The
sigma policy stays a required explicit field on both kinds.

## Pinned composition order (RegArimaKernel.calc, current jar)

Every arrow is a qualified unit or a transcription of the pinned kernel
code in `cur/RegArimaKernel.txt` (lines 303-1073, pinning notes 12.2-12.4):

```
build   initial ModelDescription: log=false, mean=false,
        airline(freq) spec (0,1)(0,1)(0,1)(0,1) [setAirline, freq!=1],
        no variables                              X13ModelBuilder 12.11
  |     curva = va>0 ? va : default_critical_value(n)   (va0, needOutliers)
calc:   log&&any(v<=0) -> error status
        decide_transformation (21-B; level+log ML/Multiple 1e-7 branches,
          aicc compare against aiccLog + aicc_log_correction; the CHOSEN
          branch estimation stays in the modelling — 12.8)
        regAIC                                     no-op: calendar/easter
                                                   tests null in scope
        checkMu(m, 1.96)         run_mean_controller (21-G; mean-added
                                 estimation = adapter row 3)
        if needOutliers && outliers.process(m, curva)==Changed:
            [m.estimate]? ; regressionTest0.process(m) (21-G, mean@1.96)
        isAutoModelling()==false branch is dead for this kind
        [m.estimate]? ; reference = copyOf(m)
        ModelController(0.95).accept(m)            model_controller_accept
        plbox0=1-p, rvr0, rtval0; acceptAirline==false -> rounds
   ┌─ LOOP (round<5):
   │   if needAutoModelling:
   │     refine_arma_model (21-F = AutoModellingModule: linearizedData,
   │       decide_differencing 21-D re-entry, re-difference,
   │       select_arma_orders 21-E, spec/mean compare, airline fallback)
   │     if Changed && desc has outliers: removeVariable(isOutlier(v,true))
   │       + clearEstimation + needOutliers=true     (round vocabulary 12.9)
   │     [m.estimate]?; if stable && loop<=1: regAIC
   │   if needOutliers: outliers.process(m, curva) [detect_outliers 21-C
   │     + forward-add as Variables + fixing, §Vocabulary] + [estimate]?
   │   if outliers!=null && loop<=2: pass2(stable,m) else accept-refresh
   │   if regressionTest1.process(m)==Changed: loop=max(loop,3),
   │     [estimate]?, accept-refresh
   │   do { ok = checkUnitRoots(m); ok &= checkMA(m) } while(!ok)
   │   final_mean_rule_adds_mean(auto,mean,checkMu,rtval0)  (21-G)
   │   FinalEstimator.estimate(m) loop: estimate(adapter row 4) →
   │     final_estimator_test (slice-1 unit) → reduced re-estimate,
   │     ≤5 rounds, status>1 → round failure
   │   if cascade ok -> END else if loop<=2: curva=max(2.8,curva*(1-
   │     reduceVa)); needOutliers=true; needAutoModelling=true
   └─ round++ (pass2/lastSolution may rewrite loop/curva/needAutoModelling)
END:    final description+estimation -> x13_preadjust -> x11_selection_pass
        (decoder X-11 surface, forced mode) -> results-builder -> x13_restore
```

`pass2` is the pinned seven-filter reference-restore gate (notes 12.4:
filters 0-6 on n0/nOut, spec/mean equality, `plbox`/`plbox0` at 0.95/0.75,
`rvr` vs `FCT=1.0126582278481011`, the phi(1)<-0.82 and bphi(1)<-0.65
shape filters), the pcr accumulation (0.95 base + 0.025/0.015 per loop)
and its `plbox <= pcr` continue rule, the loop==2 curva reduction, the
`lastSolution` airline-of-last-resort (p=period-clamp, bp, q=mixedModel?1:0,
bq=1) and the reference bookkeeping — all transcription with the decoder
values as request data. `checkUnitRoots` fires `searchur` (|im|<=0.05,
re>0, |z|>ur_limit) over the mirrored AR roots, `checkMA` the
|theta(1)|<1e-3 demotion with setMean(true); both re-enter through
`redoEstimation` (12.4), whose checkMu(1.6)/outlier-strip/accept-refresh
sequence and `reference=copyOf(m)` are part of the transcription.

## Estimation adapter (call-site map)

| # | JVM call site | JVM processor | Kalos seam |
|---|---|---|---|
| 1 | `m.estimate(1e-7)` (kernel rounds, redoEstimation) | `RegSarimaComputer` builder, precision=eps, startingPoint HannanRissanen, default minimizer | `estimate_sarima_fixed`, control {eps=request, feps 1e-4, ml=true, starting_point HannanRissanen, solver **Bfgs** (product default, maintainer-ruling #2)} |
| 2 | `transformation.process` | `DemetraUtility.processor(true,1e-7)` | inside the 21-B unit (unchanged) |
| 3 | mean-added estimation (`MeanController`, eps 1e-5) and the ODM `estimateModel` chain (`RegArimaUtility.processor(true,1e-7/1e-5)`) | `GlsSarimaComputer` + LM + HR init | 21-C callback recipe: `estimate_sarima_fixed`/`optimize_sarima_from` + `sarima_regression_likelihood` + `regression_t_statistics` (solver `PinnedLevenbergMarquardt` + HR — the recipe the 21-C sweep qualifies) |
| 4 | `FinalEstimator.estimate` computer | `RegSarimaComputer` + LM minimizer + Multiple start + `computeExactFinalDerivatives(true)` | adapter row 1 shape **plus the final-information producer — open gap G1 below** |
| 5 | differencing trial fits / ARMA candidate grids | pinned SSR/LM chains | inside the 21-D/21-E/21-F units (unchanged) |

Modelling-state semantics (`RegSarimaModelling`, 12.12): `desc` = {log,
mean, specification (SarimaOrders), variables (name, kind, ami attribute,
regeffect, columns, coefficient)}, estimation = null | {coefficients[4],
cl statistics (coefficients, unscaledCovariance, ssq, dim), neff, aicc,
passes}; `needEstimation` ⇔ estimation null; `clearEstimation` nulls and
frees; `copyOf` deep-copies; `set(desc,est)`/`setSpecification` per 12.12.
The cl statistics are the 21-C-sweep composition at the landing (row 3),
JVM-qualified at the basin tiers, never bit-claimed (see Envelope).

## Decision-record chain (provenance)

The result exposes the **full chain in execution order**; every entry is
stage-tagged and carries the unit's own qualified record verbatim:

| stage tag | payload (qualified type) |
|---|---|
| `init` | echo: request scalars, `va0`/`curva` rule (`spec` \| `calcCv(n)`), airline start |
| `transformation` | `TransformationDecisionResult` (outcome, aiccs, chosen branch) |
| `meantest0` | `MeanControllerResult` (tstat, flag flip) |
| `outliers0` | `OutlierScoringResult` (decisions, rejections, rounds) |
| `regtest0` | `RegressionVariableTestResult` (removed, tstats, audits) |
| `accept0` | `ModelControllerResult` (plbox0, rvr0, rtval0, accepted) |
| `round<k>.refine` | `ArmaRefinement` (differencing record, panel, refinement steps, spec/mean delta, fallback) |
| `round<k>.outliers` | `OutlierScoringResult` |
| `round<k>.pass2` | filter id 0-6, pcr after, loop after, restore?, curva after |
| `round<k>.regtest` | `RegressionVariableTestResult` |
| `round<k>.unitroots` | searchur counts per block, order changes |
| `round<k>.matest` | \|theta(1)\|, order change, mean set |
| `round<k>.meanrule` | rtval0, decision (`final_mean_rule_adds_mean`) |
| `round<k>.cascade` | per round: status, reduced orders, asy-diag inputs |
| `final` | final description echo + downstream fit composition |

Chain invariants: one entry per machine execution (accept-refreshes
included), never reordered, never merged; the downstream `X13FitResult`
keeps the `X11SelectionPass` stage records. The final description plus the
chain is the full regeneration recipe of the fit.

## Outlier round vocabulary (isOutlier(v, true))

The pinned round vocabulary (notes "ModellingUtility -- PINNED", 12.6,
12.9): machine-added outliers carry attribute `ami="x13"` and
`regeffect=<SaVariable.defaultComponentTypeOf>`; user-fixed outliers
carry no `ami` attribute and are never removed by the round code, only
excluded from the scan. `removeVariable(isOutlier(v,true))` in
`calc`/`redoEstimation`/`ODM.process` removes **all** ami outliers. The
driver adds each selected outlier through the qualified
`fill_outlier_column` factory (AO/LS-zero-ended/TC-rate/SO, pinned zone
set) with name `IOutlier.defaultName(code, period)`; `defaultComponentTypeOf`
maps AO/TC/SO → Irregular (capture-proven, 12.6) and LS → LevelShift
(pin-verify against the first LS-selected capture before enabling that
pairing at the seam — recorded below).

## Scope envelope and fail-closed matrix

Delivered: frequency 12 only; horizons {0, 0}; the four decomposition
modes through the pinned forcing; a complete, finite input (missing-value
interpolation is not part of the automatic description build); outlier
span = full estimation domain, no exclusions; `transform.function=Auto`
(the decoder builds the LogLevelModule only in that case); calendar,
easter (test=Add semantics only), automatic-TD and user-variable detection
**deferred/fail-closed** (21-I), `SeasonalityDetector` and the
`FRegressionTest` joint branch dead in the current jar and fail-closed at
their unit surfaces; every module scalar required request data (nothing
promoted — decisions 1-9 stay open per the roadmap ledger).

| request | status |
|---|---|
| frequency != 12 | `unsupported:x13 automatic fit monthly series only` |
| missing/non-finite observations | `unsupported:x13 automatic fit requires a complete finite series` |
| backcast/forecast != 0 | `unsupported:x13 fit requires zero horizons` (shared with the fit) |
| sigma policy unset | `unsupported:x13 fit requires an explicit sigma policy` (shared) |
| calendar/easter-detection/automatic-TD/user detection requested | `unsupported:x13 automatic fit defers calendar/easter/TD/user detection (21-I)` |
| outlier span narrower than the series | `unsupported:x13 automatic fit spans the full series only` |
| selection-pass refusal | verbatim `unsupported:<stage>` (fit passthrough) |
| JVM-throw chain outcomes (kernel `return null`) | `failed:<stage>` with the chain kept |

Shape violations throw `std::invalid_argument` exactly as the seams below
do. Estimation non-convergence is not a refusal: the pinned kernel drives
on whatever the processor retained (the 21-C "available" semantics).

## Non-determinism and tier envelope (slice-3 policy)

* JVM `Polynomial.roots()` is `Random`-seeded for degree ≥ 3 (documented,
  regression-tests + final-estimator contracts). `checkUnitRoots` reads it
  through `mirror().roots()`: the regular-AR mirror is degree ≤ 2 (closed
  form, deterministic — `kalos::detail` port); the monthly seasonal-AR
  mirror is degree 12 — deterministically solved natively, and the JVM
  verdict is stable only because the |im| ≤ 0.05 window is far from the
  rotation-symmetric root angles of `1 + bphi(B^12)`. Slice 3 pins the
  count equality per case; a case landing within 0.01 of the |im| or
  modulus boundaries is removed from the corpus (21-A removal precedent)
  and the unit stays qualified structurally.
* FinalEstimator `checkRoots` degree ≥ 3 blocks are guarded fail-closed
  already (slice-1 contract envelope); the automatic corpus must stay in
  degrees 0-2, verified per case.
* Landing tiers follow the 21-B/21-C/21-F basin precedent: decisions
  bit-exact, landing quantities measured with per-quantity tiers and a
  pass-record root cause; the double-JVM-capture byte-identity gate and
  the nondeterministic-case removal rule are as specified in the roadmap
  stage-Z entry. No tolerance is ever applied to a decision bit.

## Open seams gating the code slices

* **G1 (hard gap, blocks the driver): final-information producer.** The
  FinalEstimator test cascade consumes the `asymptoticCovariance` diagonal
  of the final point (`LLPoint`, exact-final-derivatives path: the
  `NumericalDerivatives` stencil → information → `SymmetricMatrix.inverse`
  — pinned in `cur/NumericalDerivatives.txt`, `cur/LLPoint.txt`,
  `item21h/G2_LLPoint`, `G2_RegArmaProcessor`, `h3_NumericalDerivatives`).
  No qualified producer exists in the tree (recorded "remaining gap" in
  the pinning notes; `src/final_information.cpp` was never created). It
  needs its own JVM golden before the cascade can run natively.
* **G2 (decision recorded, precedent accepts): landing CL-statistics
  producer.** The adapter row-3 composition (`sarima_regression_likelihood`
  + `regression_t_statistics` at the landing) is the 21-C-sweep recipe,
  JVM-qualified at the basin tiers, not bit-exact (the pinned CLWM
  Householder/QR arithmetic differs from the 20b DGELSY path; the
  `detail::ssq_deviances` bit-exact CLWM covers only the design-free /
  single-column machines). Slice 3 measures whether any decision bit of
  the corpus lands near a cutoff where band drift could flip it; the
  maintainer keeps the alternative (a general bit-exact CLWM unit) on the
  table — the driver does not invent a third recipe.
* **G3 (pin-verify): LS `regeffect`.** AO capture-pinned to Irregular;
  the LS pairing (LevelShift expected) unverified — first LS-selected
  capture settles it, no LS case ships before then.
* **G4 (naming): X-11 decoder surface echo.** The automatic X-11 request
  block mirrors the 12.13 corpus facts (filters=[Msr], cvTables B3, sigma
  1.5/2.5, calendarSigma None, Henderson 0, horizons 0/0, mode Undefined
  → forced); calendarSigma None is the only qualified value.

## Slice plan (stage Z, remainder) and resume checklist

1. **21-H.0 — final-information unit (G1).** Transcribe the
   NumericalDerivatives stencil + information assembly + pinned inverse
   behind `final_estimator_test`'s input surface; golden-capture
   qualification (double JVM capture, intrinsic-disabled convention);
   ctest + sweep + sanitizer gates as usual.
2. **21-H.1 — driver (this contract).** `x13_automatic.{hpp,cpp}` +
   modelling state + adapter + decision chain + fail-closed matrix +
   `test_x13_automatic.cpp`; restricted-fit parity re-proven bit-exact
   (the fixed corpus through the untouched path); spec kind wired at the
   fit seam.
3. **21-H.2 — end-to-end oracle.** JVM automatic-modelling captures
   (`validation/reference` probe conventions, `jdprobe`/`jarcache` infra,
   double-JVM byte-identity) on realistic monthly series; decisions
   bit-exact, series at the measured tiers with per-case root causes;
   evidence under `validation/evidence/native/` only; corpus decisions,
   tier tables and removals recorded in this contract's qualification
   section at landing.

Resume checklist: `git log -3` (slice-1 unit commit first, then this
boundary commit); this contract; the pinning notes SECTION 12 and the
tail sections; the slice-1 contracts; the roadmap stage-Z ledger. The two
untracked files (`src/kalos_write_probe.cpp`, `cpp-out.txt`) are
user-pending, not part of the manifest, never `git add`.

## Implementation-actual deltas at landing (21-H.1)

What the driver shipped against, recorded where the transcription above
and the bytecode/probe evidence differ or sharpen it (the pinning notes
SECTION 15 carries the probe evidence):

* **G3 CLOSED by JVM probe (jshell, `SaVariable.defaultComponentTypeOf`
  bytecode + live call).** AO/TransitoryChange/SwitchOutlier →
  `Irregular`, **LevelShift → `Trend`**, **PeriodicOutlier(SO) →
  `Seasonal`**. The "LS pairing (LevelShift expected)" note above was
  wrong twice over: `LevelShift` is not a `ComponentType`, and SO is not
  Irregular. `AutoVariableRecord.regeffect` carries the probed names; no
  LS fail-closed gate was needed. The suite pins AO → Irregular; LS/SO
  ride on the probe.
* **The estimation-domain and concentrated-deviance laws are
  byte-verified** (`cur/RegArimaKernel_full.txt`, `FinalEstimator`):
  the estimator's effective count is `domainLength − xcols − armaDim`
  (cdf default true counts the innovation variance), sigma2 rides the raw
  estimation-domain length, and the cascade objective is the concentrated
  deviance `ssq · exp(Σ log var / dim)` (= `LLPoint`/`cl.ssq() ·
  exp(ldet/dim)`).
* **`desc.mapping()` is `SarimaMapping2.of(orders)` — ARMA-only getDim.**
  The cascade's `dim == 0` early-true therefore fires on a zero ARMA
  parameter count, never on an empty regression design.
* **Pass-2 NaN flavours** (the pinning note 12.4 pseudocode was corrected
  by the full bytecode): the continue rule is `plbox <= pcr` under
  `dcmpg` (a NaN operand takes the *false* path, i.e. the round body
  continues), the `curva > 2.8` reduction guard is `dcmpl`-based (a NaN
  curva skips the reduction) and the floor uses `Math.max` NaN
  propagation (`java_max`); the tail `needOutliers = true` escalation is
  unconditional at bytecode 640.
* **The refinement `Changed` branch clears the retained candidate**
  before calc's removal branch runs (21-F semantics: specification-or-
  mean change drives clearEstimation; a stale candidate at old orders
  must never reach the round's `estimate_if_needed`).
* **Chain vocabulary extensions** (beyond the record-kind table): the
  redoEstimation sub-machines re-enter the mean controller, the outlier
  machine and the accept refresh as repeated base tags with round
  context and the note `"redo"`; plain accept refreshes carry the note
  `"regtest"` (regtest-Changed branch) or nothing; the pass-2 record
  notes are `"last chance model"` and `"curva reduction"`; a refinement
  fallback notes `"fallback:<stage>"`; outlier removal after a failed
  refinement annotates the previous entry with `"outliers removed"`; the
  refusal path ends the chain with a `"final"` entry noting `"downstream
  refused" + status`, the success path with `"final"` noting `"downstream
  composed", "available"`, and the caught-throw path ends with a
  `"failed"` entry whose first note is the machine message.
* **Entry-point gate order** (pinned by the suite): the empty-series
  `invalid_argument` throw first, then the monthly-status refusal (it
  answers for every non-monthly request before anything interprets its
  shape), then the shape throws (`first_period`, one-seasonal-entry-per-
  period, `size <= 13`, outliers-without-type), then the status gates in
  order finite-data, zero-horizons, explicit-sigma, deferred-modules,
  full-series-span. A refused request carries an empty chain; a caught
  machine throw maps to `"failed:<stage>"` with the chain kept plus the
  `"failed"` tail entry.
* **ML-window bounds (new trap, slice-found).** The JVM
  `MaLjungBox.calcg` indexes `psi[m_n − j]` for every `j < q` with
  `psi = RationalFunction(1, ma).coefficients(m_n)`: `q ≥ m_n + 2` raises
  an `ArrayIndexOutOfBoundsException` (a RuntimeException — the JVM run
  fails the same shape natively). The transcription reproduces the trap
  as a `std::runtime_error` from `MaLjungBox::prepare` (and the
  `ModifiedLjungBox` window-below-AR-degree case) instead of performing
  the out-of-range read, so the driver answers `failed:<stage>` exactly
  where the JVM returns null. No corpus-qualified case sits in-range of
  the boundary (all eight sweeps unchanged).
* **The landing estimator's status string passes through verbatim**
  (`"converged"` for the corpus shapes) — `estimation_status` is the row-4
  landing recipe's status, not the fit status; AICC/log-likelihood are
  the G2 landing statistics (never bit-claimed).
* `X13FitSpecKind` lives in `x13_automatic.hpp` (the fixed header stays
  byte-identical; the fit contract's schema-change note).

Deltas added at 21-H.3 (oracle-found, pinning notes SECTION 18):

* **The specification start is the AIRLINE.** `X13ModelBuilder` builds
  the (0,1,1)(0,1,1) start — `q` and `bq` are 1 from the beginning (the
  ODM entry facts echo the spec as `0 1 1 0 1 1`). The driver's
  `Description` initialized `q = bq = 0`; the ODM whitening and the
  refinement search size themselves from these orders, so the entry
  state diverged from the JVM's before the first scan (this was the
  ao/noout/multi decision residual, not an ODM-arithmetic gap).
* **`X13AutomaticFitResult.coefficients` exports the JVM surface's
  sign convention**: `SarimaSpec.parameters()` carries the all-plus
  polynomial signs, so the AR and SAR blocks are the negated driver
  φ-terms (MA/SMA keep the θ-convention). The internal estimation
  keeps φ; only the export negates blocks 0 and 2.
* **The landing AICC rides the ADJUSTED log-likelihood**
  (`LikelihoodStatistics.calc` reads `adjustedLogLikelihood`; the
  Builder adds the transformation Jacobian): in log mode the JVM
  subtracts `sum(log y_t)` over the estimation domain — the last
  `n − d − 12·bd` original values (pinned by the oracle stats facts:
  ao −333.0415626 at d=0, bd=1; rw −450.5425654 at d=1, bd=0; level
  cases carry no adjustment). The AICC is `−2·(adjLl − k·neff/(neff −
  k − 1))` with `neff` the stats effective count (`n − d − 12·bd`) and
  `k = armaDim + xcols + 1`.
* **`IOutliersDetectionModule.Info` pick payloads carry the FACTORY
  index** into the case's requested type list, not the global AO/LS/
  TC/SO code: the golden generator translates through the event's
  type header (the full AO,LS,TC,SO list hid this until the SO-only
  case exercised the gate).

## Oracle qualification (21-H.2)

The end-to-end JVM oracle observes the pinned automatic pipeline
(`validation/reference/X13AutomaticProbe.java` running
`X13Kernel.of(spec, ctx).process(ts.cleanExtremities(), log)` against the
rjd3/rjd3x13 3.9.0 jars, captured by
`validation/reference/capture-x13-automatic.R` under the
intrinsic-disable `_JAVA_OPTIONS` convention). Evidence:
`validation/evidence/native/x13-automatic/` (cases, inputs,
oracle-events, oracle-facts, metadata, SHA256SUMS); the double-capture
byte-identity gate passed (two fresh JVM runs, the four data files
byte-identical). `tools/gen_x13_automatic_golden.py` renders the golden
header `tests/jvm_x13_automatic.h`;
`tests/test_x13_automatic_oracle.cpp` diffs the driver's chain
vocabulary and landing surfaces case-by-case (argv selects case ids;
since 21-H.3 ctest registers all thirteen);
`tools/verify_x13_automatic.cpp`
sweeps the full corpus with the family report.

Corpus: 13 cases (monthly n=96 start 1990-1; single-AO, LS,
multi-outlier, all-reject, SO-set, detection-off, d=2,
random-walk-with-drift, seasonal-only and the three decomposition
forcings, all four modes effective on the mode series). The planned
14th case ("plain") was removed pre-capture under the slice-3 policy -
its round-0 ODM sits on the TC@69-versus-AO@63 knife edge inside the
documented G2 basin seam.

Tiers (decisions are always bit-exact; no tier is ever applied to a
decision bit); final column updated with the 21-H.3 measured values
(qualifying runs: oracle 4637 checks 0 failures, full sweep 71401
checks 0 bad, all thirteen cases):

| family | band | basis |
|---|---|---|
| decision bits, chain counts/scalars, model facts | bit-exact | this contract |
| diff-search tmean | 1e-3 | 21-D trial-fit class (measured 7.4e-4) |
| panel BIC2 | 1e-2 | measured 8.1e-3 at the automatic landing: the panel trial fits run on the ODM-altered design; membership and selections stay bit-exact |
| AICC display pair (observer `%.6g`) | 1e-5 | display round-trip (measured 3.4e-6) |
| landing regression coefficients, loglik, AICC | 2e-3 | adapter row-4 basin tier (measured 4.5e-7) |
| ARMA landing coefficient block | 1e-2 | measured 9.8e-3 — row-4 landing-basin depth from the driver's start values (G2 seam) |
| downstream tables (out, d11final, e11) | 1e-6 rel | measured 5.0e-7 over all thirteen cases |
| c17 composite column (full sweep only) | 1e-4 rel | measured 8.3e-5 — the composite filter recursion propagates the landing drift; the golden subset the gate carries excludes c17 |
| refine linearizedSeries (full sweep) | 1e-6 rel | measured 2.4e-7 — the linearization rides the landing regression coefficients (G2 seam); the decisions it feeds are the bit-exact diff-decisions |
| ODM scan-replay match (full sweep) | informational (the `|t|` cells at 1e-3) | probe replay artifacts — see the 21-H.3 resolution |

Landing verdict (H.2's final sweep; the two open residuals were
resolved by 21-H.3, recorded below): ten cases carry
bit-exact decision chains end to end (quick, seas, ls, so, ao_nodet,
d2, rw and the three forcing cases); quick and seas are bit-identical
across every family and form the ctest-registered pair. The four
display-class deviations of those ten are recorded with their
mechanism in the pinning notes 16.4/16.5 (selection-coefficient sign,
panel-BIC and AICC display scales, the 2e-7 a8 drift class in so) -
none of them carries a decision bit. Two cases (ao, noout) keep
decision-bit divergences at the round-0 ODM's first-entry state: the
driver's scan accepts candidates the JVM rejects. The JVM's
rejected-candidate t is not in the capture schema, so the follow-up's
first move - extend the probe's odm event with the best rejected
abs(t) against cv, recapture, re-pin the 21-C entry state with the
measured delta - is pinned in the pinning notes 16.5. Multi's residual
is the 21-E stage-grid (bp,bq) selection on its four-variable
linearization; its ODM machine itself reproduces the JVM's four-pick
rounds exactly.

Driver bugs found by the oracle and fixed with this slice: the ODM
retained-coefficient carrier, the Ansley p=0 loop underflow (segfault
on pure-MA models), the checkMu mean-on design mismatch (the estimation
carrier now records whether it was fitted with the mean column), and
the ODM estimate-before-detect seeding. The restricted fit
(`test_x13_fit`) and the 21-C corpus (`test_outlier_scoring`) stay
green unchanged throughout - the shared-engine fix is exercised by
both suites.

### 21-H.3 full qualification (2026-11-20)

**All thirteen cases are decision-bit qualified end to end and ctest
registers the full set** (the case-id filter argument was removed from
`add_test`; timeout raised to 3600 s, the full in-lane run takes
390 s). Both H.2 residuals — the ao/noout round-0 ODM over-accept and
the multi (bp,bq) stage-grid — had the one root cause: the
non-airline specification start (delta addendum above). The H.3 probe
instrumentation (the `odmscan`/`odmw`/`odmscanpost`/`refine`-state
facts, added to `X13AutomaticProbe.java` for exactly this diagnosis)
showed the driver whitening at orders the JVM entry spec (`0 1 1
0 1 1`) does not carry; with the init fixed the round-0 scan stops
accepting what the JVM rejects and multi's stage grid selects the JVM
pair. Post-fix decision-bit status: every decision family 0-bad over
all thirteen cases — odm-decisions 62/62 bit, diff-decisions, chain
counts/scalars, arma selections, panel membership, model facts,
variables, chain-count families all exact; oracle gate 4637 checks 0
failures; full sweep 71401 checks 0 bad, 13/13 cases ok. The recapture
that accompanies this slice is double-JVM byte-identical (`oracle-facts
.csv` sha256 `798c81ac…`, 4 data files; the event stream and every
golden-relevant fact are byte-identical to the H.2 capture — the
instrumentation is purely additive).

**Scan-replay policy (the one family no tier can honestly carry).**
The odmscan/odmscanpost facts are FRESH `ExactOutliersDetector`
re-runs on the captured whitening snapshot, not the pipeline's own
scans: their internal accept/verify round structure, stop-scan argmax
and `|t|` legitimately diverge from the in-pipeline call wherever a
candidate `|t|` sits near the cv or near an argmax tie (the so replay
finds nothing where the pipeline accepted two SO picks; ao's
(48,AO)/(85,SO) pair is a 0.0011 `|t|` tie). They are therefore an
informational sweep family — reported with the worst deviations,
excluded from verdicts — while the JVM's real in-pipeline selections
stay bit-qualified through odm-decisions. The `refine-x`
(linearizedSeries) family became a measured-tier family once the
G2-seam root cause was recorded (band table above).

**Unit-suite re-pin.** `test_x13_automatic` T08 (the driver-internal
worked search on the unit's own spiked series, not a corpus case)
re-pinned to the fixed trajectory: landed (0,0,0)(0,1,1) (was the
pre-fix (3,0,0)(0,1,1)), 20-entry chain (was 24), and the single
pass-2 gate now fires filter 5 and restores the reference (was a
keep). The JVM-verified landing surface is the oracle corpus; these
pins guard driver-trajectory regression only (comment recorded at the
test).

**Debug scaffolding disposition.** All `KALOS_DUMP_ODM` stderr dumps
added during the diagnosis were stripped from `src/`
(`outlier_scan_engine.cpp` is back at its pre-slice state, hot loop
included); the functional `ScanRecord` trajectory carrier added to
`OutlierScoringResult` stays (the sweep tool's diagnostic input).
The sweep tool keeps its `KALOS_DUMP_ODM`/`KALOS_DUMP_ARMA` switches
(tools are triage instruments, not shipped surface).
