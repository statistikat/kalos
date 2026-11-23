# TRAMO fit assembly (23-F F.4)

`tramo_fit()` is the automatic-TRAMO assembly unit: the living
`jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling` pipeline of
`TramoKernel.process` for a monthly series, assembled from the landed
decisions — it carries ZERO stage logic of its own. Every stage is a call
into an already-qualified unit followed by bit/exact cross-checks against
the captured stream; a desync throws `std::invalid_argument` naming the
site, and out-of-envelope REQUESTS answer `refused:<reason>` status from the
fail-closed matrix (the refusals are decisions, not shape errors). The
`TramoSeatsFit` decomposition assembly stays 24-D; the end-to-end
`TramoKernel.process` observer oracle stays 23-G.

```cpp
kalos::TramoFitRequest rq;   // spec + series + kernel facts + seam inputs + final record
auto fit = kalos::tramo_fit(rq);
if (fit.status == "available") { /* final surface + D9 covariance */ }
```

## Stage order (with the corpus witnesses that pin each edge)

| # | stage | unit (live) | cross-check anchor |
|---|-------|-------------|--------------------|
| 0 | refusal matrix | - (fail-closed) | 10 corpus refusal rows + the d6/i10 synthetic rows |
| 1 | 23-A model build | `build_tramo_model` | `fitbuild`/`prestate orders mean log` lines; orders gate incl. the decoded m011 downgrade shape (fit_gs2/fit_shortfail) |
| 2 | 23-B transform decision | `decide_tramo_transform` | `prestate log` + `fitdec` verdict context; Auto-transform only (rows with a decided transform skip per `isFullySpecified`) |
| 3 | `ami()` branch | `tramo_run_kernel_rounds` (F.3 verbatim) | decoded `isFullySpecified`: no seams iff transform decided && !autoModel && !outliers && !meanTest (fit_fsnone vs fit_nomean) |
| 4 | per-seam live re-runs | `decide_tramo_differencing` / `select_tramo_arma` on the `fitseam`/`fitresid` ENTRY state | seam result/orders/mean vs fact; tier "live"; the zero-drift entry-chain check against the previous witnessed post-state |
| 5 | final surface + D9 | native `covariance(k, true)` algebra | `rfinal`/`fitcl`/`fituc`/`fitcoef` bits (74/74 D9 entries bit-exact) |

The stage-4 modules consume the EVOLVING seasonality context
(`ctx_seasonal`, updated at the `seasonality_controller` seams via
`TramoKernelSeam.seasonal_after`) — the jar factories
(`TramoKernel.differencingModule()/armaModule()`) hand the builder
`seasonal(ctx.seasonal)` at module creation, not the pre-stage value; the
gs2/shortfail pass-3 rows pin it.

## Fail-closed matrix

- `d11-frequency`: annual frequency != 12 (fit_q_air).
- `d10-calendar`: `td_automatic || td_test || easter_test || user_variables`
  (fit_tr2_air, fit_trfull_air, fit_td_air, fit_aic_air, fit_bic_air,
  fit_wald_air, fit_easter_air — the joint 21-I surface; the auto-method
  rows are calendar-bearing in the corpus so they refuse here first).
- `i10-automethod`: FTEST/AIC/BIC/WALD `auto_method` (synthetic row).
- `oc-precorrection`: `outliers_correction` (fit_oc_air — never witnessed
  THROUGH the fit seam).
- `d6-decomposition`: `seats_decomposition_mode != -1` (synthetic row — the
  fit never decomposes; policy belongs to 24-D/24-E consumers).
- `missing-values`: non-finite series (fit_missing — no interpolation plan
  inside the fit; qualified at unit level in 23-A/23-B).

## Live/fact seam tiers

A seam is tier "live" only when its module inputs are fully reconstructible
natively: a differencing seam with an outlier-free entry design, `ml = false`
and a round-1 (initial) call re-runs 23-C; an arma seam with the captured
`olsResiduals` hand-over re-runs 23-D. Everything else rides as "fact" with
a pinned note: scan seams (the outliers machine is qualified in the
tramo-outlier-rounds corpus), differencing seams with outlier columns or in
a re-round (outside the 23-C envelope), arma seams without the hand-over.
Measured census over the 34-row golden: **27 live / 54 fact**; the test
asserts the per-case counts against the golden, so the census can never
silently drift.

Fitted outlier positions are zero-based on the original estimation domain.
The 24-D handoff uses this date when creating each deterministic-effect
column. Differenced full residuals can start later and have a separate domain.

## D9 — the regression-block covariance

`RegSarimaModel.coefficientsCovariance` is `ConcentratedLikelihood
.covariance(k, true)` = `unscaledCovariance().deepClone()` scaled by
`ssq() / (dim() - nx() - k)` — byte-identical algebra to the 20c.1b.2
tramo_residual accurate covariance (pinning SECTION 32.2). The unit emits
the native recomputation (`TramoFitCovariance`, raw-bit multiply) and
compares every upper-triangle entry against the captured `cov` bits:
**74/74 D9 entries bit-exact**, `matches_capture` true everywhere. The
ARMA-tail covariance is the SEPARATE `ParametersEstimation`
(`final_information`) matrix — the two surfaces stay distinct (D9), and
neither is the SEATS transmission variance (24-D); no consumer may
substitute one for the other. Any `cov` divergence is a STOP, never a
widened band.

## Corpus

`validation/evidence/native/tramo-fit/` — the triple capture
(`fit-capture-1.out`, `fit-capture-2.out`, `fit-witness.out`) is
byte-identical across the two captures and the class-load witness run
(SHA256SUMS.csv verified; probe `Scratch23f.java`, dual-driver identity
engine, public `TramoKernel.of(spec, new ModellingContext()).process` path
only). 35 rows: 34 golden cases + the pinned `fit_const` crash-witness row
(JVM NPE, no golden). Witness gate: ZERO `jdplus.spec.x13` loads,
`FinalEstimator` loads only from `tramoseats.tramo`. The golden
`tests/jvm_tramo_fit.h` regenerates byte-identically from BOTH captures
(`tools/gen_tramo_fit_golden.py`, reusing the F.3 kr machinery).

## Debug correctives (dated 2026-11-23; javap re-decodes adjudicated by the corpus)

- **Differenced-response orientation (fit_fct_air).**
  `BackFilter.weightsToArray()` emits `w[i] = weights(lowerBound + i)` with
  `getLowerBound() = -degree`, i.e. the polynomial coefficients REVERSED
  (`lambda$weights$1(lag) = poly.get(-lag)`), and both `IFiniteFilter.apply`
  branches run the forward window `dst[i] = sum_k w[k] src[i+k]`. The fit
  response recompute therefore applies `differencing_filter(...)` REVERSED
  directly; the `(-1)^(d+bd)` parity form (the 23-C ladder-internal pin) is
  entry-wise equivalent except that a cancelled sum lands at JVM `+0.0`
  where negating the ascending result gives `-0.0` (the fit_fct_air index-99
  witness, capture bit `0000000000000000`). Corpus taps are in {±1, ±2}, so
  the products stay bit-exact.
- **Stage-1 orders gate.** A mismatch between the builder registration and
  the captured prestage orders is admitted ONLY in the decoded
  `testSeasonality` downgrade shape (`autoModel && !seasonal &&` prestate
  `== m011 = (0,1,1)(0,0,0)`); everything else must equal the registration.
- **Stage-3 `ami()` branch.** The round driver is skipped iff
  `isFullySpecified` (transform function != Auto && !autoModel &&
  !outliersDetection && !meanTest; the td/easter clauses are vacuous under
  the D10 refusal). The auto-transform template fit_nomean runs the iterate
  loop — the empty-seam stream is NOT the authority; it is cross-checked
  against the decoded verdict.
- **Evolving `ctx_seasonal` (stage 4).** Module factories read
  `ctx.seasonal` at creation; the gs2 seam-18 and shortfail seam-6/17/19
  pass-3 modules are created after the `scontroller res Changed` flip
  (`fitresid spc` values and the comespa/airline gates confirm per seam).
- **23-C unit: Changed-law + airline shape.** `DifferencingModule.process`
  compares the ladder end against the ENTRY orders (javap offsets 293-316)
  — not the initial-round zeroed start (fit_tr3_air/fit_mean_air/fit_pure);
  and `ModelDescription.setAirline(seasonal)` writes `SarimaOrders.airline
  (freq)` = ARIMA(0,1,1)(0,1,1) — the seasonal block carries `bp = 0`, and
  `isAirline` tests `p == 0`/`bp == 0` (fit_shortfail seams 6/17). See the
  dated appendix in tramo-differencing-decision-contract.md.
- **23-D unit: module catch.** `ArmaModelSelector` has no Exception table
  (no per-candidate try); a HannanRissanen pipeline RuntimeException
  escapes into the `ArmaModule.process` catch and answers `Failed` — the
  unit now mirrors the module catch and never throws mid-selection.

## Not delivered or claimed

No interpolation, calendar, or automatic-regression-method path (refused
above); no SEATS decomposition (D6/24-D); the estimation-model/regtest
seams ride as facts of the F.2/F.3 families (their machines are qualified
in their own corpora); quarterly TRAMO stays D11. `tramo_fit` performs no
estimation: the final surface is the captured record replayed through the
native covariance algebra.

## Gates

ctest `test_tramo_fit` replays 34 cases (10 refusals, 74/74 D9 bit-exact,
per-case live/fact tiers, the d6/i10 synthetic refusals) in both lanes; the
sweep `tools/verify_tramo_fit` re-drives the golden surface off the raw
capture. ASan/UBSan clean. Shim-invariance battery 26/26 with
`test_tramo_fit` as the 26th cell.

### 23-G ARMA input preparation

When an ARMA module entry has no registered regressors beyond an optional
mean, the fit reconstructs the selected response and differences, solves
the mean on the differenced domain, and passes those residuals to the
selector. The OLS vector and observation count can be omitted in those
entries. When supplied they must match in length and binary64 bits.

With dated AO/LS variables in `entry_variables` the selector reconstructs
the differenced outlier design and solves the full OLS problem. An existing
request with only the labels and a residual witness continues to use that
witness. Typed variables must follow the recorded order and cover the entry
design. The admitted panel now needs no supplied ARMA residual vectors.

### 23-G outlier module cross-check

The fit also runs the multi-round scan at integrated, complete monthly
entries with no pre-existing regressors beyond the mean, no remaining
outliers on entry, and the full estimation interval. Both default and
explicit maximum-likelihood estimation are qualified. Factory columns map back to their AO/LS/TC/SO types in encounter
order. Both the processing outcome and each dated selection must agree with
the recorded module state. The native detector uses the captured critical
value and the 1e−5 intermediate tolerance of TramoKernel.AmiOptions.
The 40 successful comparisons include changed, unchanged and failed module
outcomes. Post-scan coefficients continue to arrive through the fitting
seams. The 67 live / 14 recorded census supersedes the earlier 23-F census.

### 23-G final estimation from observations

The overload of `tramo_final_estimator_run` accepts finite monthly
model-scale observations and an optional mean. The fit uses it at the 16
final estimation calls immediately after a scan leaves an empty outlier
design. Options come from the model at entry, the kernel pass and the
resolved specification, including EstimateSpec's precision and unit-root
threshold. Return status, changed-parameter count, all six returned orders
and every fitted ARMA coefficient agree exactly with the JVM. The 29 other
final-estimator calls retain their captured results. These calls extend
the fit census to 83 live and 43 recorded across the four modules.

The multi-column extension differences each raw regression column along
with the response. The HR start uses the pinned pivoted OLS solve and
subtracts each fitted column in model order. The fit preserves detector
order for the scan comparison, then stably sorts the selected automatic
outliers by date as ModelDescription does before final estimation. The
first estimation, ARMA hand-over and post-mean-test retry also supply the
model and mean at entry. All 45 final-estimation calls in the admitted
panel reproduce the return, changed-parameter count, orders, dated outlier
order and fitted ARMA coefficient bits. The census is 112 live / 14
recorded. Initial, intermediate and final public fitted-record assembly
are separate parts of the automatic path.

The differencing selector consumes the model-scale response after
subtracting the fitted automatic-outlier effects. The estimate's variable
names, orders and mean must match the module entry. A refit or model
restoration invalidates the cached effects. All 20 selectors agree with
the public path, including the five-variable model after the seasonality
controller and the short-series HR failure. On an unchanged airline
fallback the returned description preserves its entering orders.

The last stationary scan is covered by the monthly regular AR(1)/AR(2)
scanner with a mean and complete input. The LS column is zero-ended, and
its nonzero constant correction enters the scan weights and the initial
sum of squares for a stationary model. Six dedicated public-module and
internal round traces cover AO, LS, TC and unchanged outcomes. Across the
34 fit cases all 126 differencing, ARMA, scan and final-estimation module
checks now run numerically. The recorded loop and final-model data are
still part of the separate fit request.

The integrated replay computes every round-0 and pass-2 statistics row
from the preceding fitted likelihood. The final reporting QR operates in
fixed design order. Its residual tail supplies `cl.e()`; a design-free
fit uses unscaled Kalman innovations. The optimizer uses its own pivoted
QR for derivatives. All 39 round rows and 46 fast mean-test decisions in
the admitted corpus agree bit-for-bit, including each post-test t-vector
and the empty vector after the mean test clears an estimation. The
standalone round-replay input still carries these values for comparison.
