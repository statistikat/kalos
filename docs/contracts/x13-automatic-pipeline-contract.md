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
