# Differencing decision — d/D selection and the mean test (21-D)

`decide_differencing()` is the stage-F differencing-decision unit: a literal
transcription of the pinned `jdplus.x13.base.core.x13.regarima.
DifferencingModule` (the `IDifferencingModule` behind `IDifferencingModule$Info`)
composed onto the qualified [estimation-control](estimation-control-contract.md)
layer. It maps a (linearized) response and frequency onto the differencing
orders `(d, bd)`, the mean-correction flag and its t-statistic — exactly the
quadruple `IDifferencingModule$Info` records — and additionally returns the
per-iteration trace the machine keeps in its private fields. There is no Rcpp
boundary; the unit is C++-only.

```cpp
kalos::DifferencingDecisionRequest q;   // series, period, max_d, max_seasonal_d,
                                        // ub1, ub2, cancel, precision: all
                                        // required request data
auto d = kalos::decide_differencing(q); // d, seasonal_d, mean_correction,
                                        // t_mean, ml_used, trace records
```

Everything the pinned module holds arrives as required request data (standing
rule of item 21: pinned values stay transcription constants, never
configuration defaults; roadmap decision 5 stays open). The decoder-pinned
call site (`X13SpecDecoder.readAutoModel`) builds the module as
`DifferencingModule.builder().cancel(spec.getCancel()).ub1(1.0 / spec.getUb1())
.ub2(1.0 / spec.getUb2()).precision(1.0E-7).build()` over the default
`AutoModelSpec`: `DEF_CANCEL = 0.1`, `DEF_UB1 = 1.0309278350515465`,
`DEF_UB2 = 1.1363636363636365` — the module stores the reciprocals (the JVM
divisions produce the exact doubles `0.96999999999999990` and
`0.87999999999999990`), `maxd/maxbd` keep the builder defaults 2/1, and the
module `ml` flag is never set, so the pinned ML branch condition transcribes
as `(failure || useml)`.

## Pinned semantics, step by step

`process(DoubleSeq, int)` = clear + `calc()`, which always returns true; the
`airline()/remark` path is reachable only through an exception in the pinned
3.9.0 bytecode. `calc()`: `c_ = cancel`, `useml = mlused = false`,
`rmax = rsmax = tmean = 0`, `mu = false`, then `step0()`; loop `nextstep()`
until it returns false or the iteration guard `iter >= 5` trips; `calcMean()`.

1. *initstep(arfirst)*: `d == bd == 0 && arfirst` selects `p = period == 2 ? 1
   : 2, q = bq = 0, bp = 1 (if hasSeas)`; otherwise `p = q = bp = bq = 1`
   (the seasonal pair when `period > 1`). The response is filtered through
   `(1 − B)^d (1 − B^P)^bd` (pinned `BackFilter`: reversed weights, `k`
   ascending, zero weights skipped), centered by the naive finite-only block
   mean, and handed to the **raw** Hannan-Rissanen pipeline
   (`detail::hannan_rissanen_pipeline`: no initializer RMS guard, no
   stabilization, no default fill — the pinned `HannanRissanen.calc()` has no
   soft-failure path, so numerical problems always throw). The blocks stay in
   the JVM convention (polynomials `1 + phi(B)` stored raw); the two estimator
   seams flip the AR signs.
2. *unstable gate* (only with `arfirst`): a model failing
   `FilterUtility.checkStability` (Schur deflation, sign-equivariant) on any
   of the four blocks fails outright when `p > 1` or `|phi1| > 1.02`, or when
   `bp == 1 && |bphi1| > 1.02`; otherwise the whole blocks are stabilized
   (`SarimaMapping.stabilize`, root inversion, `stabilize_coefficient_block`).
   Failure replaces the model with the `setDefault` fill (JVM `phi = bphi =
   −0.1, th = bth = −0.2`).
3. *trial fit* (when `failure || useml`): the stabilized model warm-re-enters
   `optimize_sarima_from` with `EstimationSolver::PinnedLevenbergMarquardt`
   over the **unit-root-filtered, uncentered** response, no regressors
   (pinned `RegArimaModel(y = x, arima = lastModel)` — `mean = false`, so the
   machine is the design-free `sfilter` branch); `eps = precision`. A
   non-finite landing reproduces the pinned `RegArimaException("Non
   convergence in IDDIF")` as `std::runtime_error`; `mlused = true` records
   the trial fit ran.
4. *step0 root scan*: after `initstep(true)`, `searchur` counts mirrored AR
   roots (`Polynomial.mirror`, degree ≤ 2 here, so the pinned QR branch is
   unreachable) with `|im| ≤ 0.05 && mod ≥ ub1 && re > 0` and keeps the
   maximum modulus over `|im| ≤ 0.02 && re > 0` into `rmax`/`rsmax`; `d` and
   `bd` absorb the counts. `mod` is the pinned `ComplexType.abs` scaled
   max/min `sqrt` — not `Math.hypot`.
5. *maincondition*: `c_ −= 0.002` each round; `uc = (!mlused && ub2 >= 0.869)
   ? 0.136 : 1.005 − ub2`. Regular: `|phi + 1| <= uc` → `useml` when `−phi >
   1.02`, else `d++` when `|phi − theta| > c_`; else `|phi| > 1.12` → `useml`.
   Seasonal: the mirrored battery with `|bphi + 1| <= 0.19`, the `bd == 0`
   guard and the `useml` refund (`res--, useml = false`). NaN comparisons
   reproduce the pinned `dcmp/dcmpl/dcmpg` branch directions exactly.
6. *cond1* (round 0 only, when `d + bd == 0`): cancellation entry
   `|phi − theta| < c_ || |bphi − btheta| < c_` increments only with
   `rmax >= 0.9 || rsmax >= 0.9` and **falls through** to the plain tests
   otherwise (pinned fallthrough, not a return); near tests
   `|phi + 1| <= 0.15 / |bphi + 1| <= 0.16` with `rmax >= 0.9 || rsmax >=
   0.88`; far tests `!phi-near(0.16) && !bphi-near(0.17)` return, otherwise
   `rmax >= 0.91 || rsmax >= 0.89` increments. The increment consumes
   `useml` when `n == 1` and picks `d` vs `bd` by `rmax > rsmax`.
7. *allcond* (when `d + bd == 0`): non-seasonal `|phi + 1| <= 0.15 → d++`;
   seasonal guard `|phi + 1| > 0.15 && |bphi + 1| > 0.25 → return`, else
   `rmax = −phi, rsmax = −bphi` and `phi < bphi ? d++ : bd++`.
8. *finalcond*: `n == 2` rolls both orders back one and re-picks via
   `mlused ? phi1 < bphi1 : (rmax > rsmax ? rmax > 0 : rsmax > 0)`; then the
   clamps `d = min(d, maxd)`, `bd = min(bd, maxbd)` (a clamp forces the return
   0, ending the loop).
9. *calcMean*: with `d + P·bd == 0` the algebraic `stMean`: `tmean = Σx /
   sqrt(Σx²)`, thresholds `n > 200 ? 2.55 : n > 80 ? 2.0 : 1.96`. Otherwise
   `nstMean`: one `estimate_sarima_fixed` with the `Multiple` start and
   `PinnedLevenbergMarquardt` over the unit-root-filtered response carrying a
   constant regressor (the pinned `meanCorrection` lives at ARIMA level, so
   `RegArmaModel.differencedModel()` presents the filtered response with one
   constant column; the spec orders the cancellation gates read travel as
   `spec_d`/`spec_bd`), then the pinned t-statistic
   `coefficient(0) / sqrt((uc00 · ssqErr) / n)` of the same machine's
   `ConcentratedLikelihoodWithMissing` — `uc == 0` gives NaN (mu false), a
   rank-0 whitened regressor reproduces the pinned out-of-bounds access as
   `std::runtime_error`, and a machine failure mirrors the pinned catch that
   leaves the mean untested. Thresholds `n ≤ 80 ? 1.96 : n ≤ 155 ? 1.98 :
   n ≤ 230 ? 2.1 : n ≤ 350 ? 2.3 : 2.5`.

## The estimation seams (21-D.2)

Both seams run the transcribed pinned deviances machine
(`detail::pinned_deviances_evaluate`): `AutoCovarianceFunction` (Yule-Walker
with the `LULinearSystemSolver2` column-pivoting kernel and the
`RationalFunction` psi series at 1e-13) feeding the internal `KalmanFilter`
(`sfilter` design-free, `mfilter` + one-column
`HouseholderWithPivoting`/`QRLeastSquaresSolver(1e-12)` with the constant
regressor), the scaled `DeterminantalTerm` log-determinant and the
`ConcentratedLikelihoodWithMissing` assembly; the LM residual vector is the
`Evaluation.getE()` full-length deviances `residual · sqrt(exp(ldet/n))` and
the target `ssqErr · factor`. The transcription is operation-by-operation
(including the NaN comparison flavours) and was qualified bit for bit against
the pinned JVM at both seam classes; the minimizer is the transcribed
`LevenbergMarquardtMinimizer` (QR variant) with the pinned builder constants
(`eps1 = eps2 = 1e-15, tau = 1e-3, maxIter = 200, nu = 4`, acceptance floor
1e-4, mu floor 0.3333333333333333, ≤ 100 damping retries, `nu <<= 2`) and the
`SarimaMapping2` domain (ar limit 0.99999, ma limit 1.0,
`epsilon = ±sqrt(2.220446E-16)` toward zero), stepping through the pinned
`fastLeastSquares` transcription. Three environment facts pin the bit level:

* `Math.exp`/`Math.log` are the `FdLibm` bytecode kernels, not the C library
  (ulp disagreement at scattered arguments steers the LM trajectory into a
  different basin): `detail::fdlibm_exp`/`fdlibm_log` transcribe
  `FdLibm.Exp.compute`/`Log.compute` and were validated bit-exact against the
  same JVM on a 230k-input fuzz. The capture JVM runs with the build-specific
  `StubRoutines::dexp/dlog` intrinsics disabled (`-XX:DisableIntrinsic=`,
  guard-verified in process), which fixes the corpus ground truth to the
  bytecode semantics the C++ kernels reproduce.
* The JVM runs the machine in the default floating-point environment; BLAS
  back ends and glibc `exp/log` leave flush-to-zero/denormals-are-zero set,
  which moves denormal scaling steps by an ulp. The machine therefore clears
  FTZ/DAZ (MXCSR bits 6/1) for the duration of an evaluation and restores the
  caller's state (`default_fp_environment`).
* The landing likelihood of a machine-backed pass is the machine's
  `ConcentratedLikelihoodWithMissing.logLikelihood`, not the Kalos surface
  profile (which differs by ~1 ulp of rounding); the pinned LM stop-status
  names map onto the pass `optimizer_status` strings
  (`converged_function`/`converged_gradient`/`attempts_exhausted`/
  `step_overflow`/`mu_overflow`/`converged_scale`/`non_finite`, plus
  `start_not_evaluable` for the pinned start-point throw).

## Trace and record vocabulary

`DifferencingIterationRecord` (one per `step0`/`nextstep` call) exposes the
private state the JVM observer reads reflectively: `iteration`, the `initstep`
orders `p, q, bp, bq`, `hr_source` (`"fitted"` or `"defaultfill"`),
`hr_stabilized`, `failure`, `trial_fit` with the warm-re-entry
`trial_status` name, the JVM-convention coefficients `phi, theta, bphi,
btheta` the decision tests consumed, `rmax, rsmax`, the post-decrement `c_`
the round's tests used, `maincondition`, the carried result (`cond1` return on
round 0), `useml`, `mlused`, and the spec `d, seasonal_d` leaving the
iteration. The JVM `iter` counter is `nextstep_calls − 1`.

## Evidence

* Corpus `validation/evidence/native/differencing-decision/` (39 cases):
  oracle = the real `DifferencingModule`
  built through the decoder-pinned scalars and driven via the public
  `process(DoubleSeq, int)` by the observer
  `validation/reference/DifferencingDecisionProbe.java` (it implements no
  decision rule and transcribes no threshold; the private end-state fields
  `iter, mlused, useml, rmax, rsmax, c_` are read reflectively for observation
  only, following the `OutlierScoringProbe` precedent), captured by
  `validation/reference/capture-differencing-decision.R`. **Double capture**:
  the second independent JVM process (OpenJDK 21.0.11, R 4.5.3, rjd3jars
  0.0.6, dexp/dlog intrinsics disabled with the in-process guard) reproduced
  `inputs.csv` and `cases.csv` byte-identically (metadata.R records the
  cross-check; `SHA256SUMS.csv` pins the files).
* Coverage: stationary (stMean bands 1.96/2.0/2.55 incl. the tiny-variance
  normalizer case), quarterly batteries, drift/no-drift random walks, seasonal
  random walks down to `n = 15`, explosive responses (default fill + trial
  fit), near-unit-root AR and seasonal AR, regular and seasonal cancellation
  pairs, the double-integrated stress case (`d → 2`, maxd clamp), level
  shifts, trend + seasonality, and the nstMean threshold boundaries `n = 80/
  81/156/231/360`. Three short-series cases (`m_n25_sw`, `m_n20_sw`,
  `m_n15_sw`) are pinned-error rows: the pinned JVM dies with
  `NegativeArraySizeException`/`ArrayIndexOutOfBoundsException` in the
  short-window Hannan-Rissanen design; the qualified seam requires an explicit
  throw there (no value comparison).
* `tools/verify_differencing_decision.cpp` sweeps the corpus end to end:
  `(d, bd, mu)`, `mlused`, `useml`, the nextstep count and the `c_`
  chain are compared exactly, the final orders and final JVM-convention
  parameters and `rmax/rsmax` per the tier table below, `tmean` at 1e-12 on
  the algebraic `stMean` path and banded on `nstMean` with a decision-margin
  audit (`|tmean|` must stay > 10 band-widths from the sample threshold).
  Result: **39 cases, 0 failures; every banded quantity — all `tmean`s and
  all final parameters — measured bit-identical to the pinned corpus.**
* `tests/test_differencing_decision.cpp` (ctest suite 52→53): Schur
  `checkStability` cases incl. the NaN flavour, monic/quadratic mirrored roots
  incl. the conjugate pair and the `ComplexType.abs` bit pattern, `searchur`
  counting, `removeMean`, the stMean/nstMean threshold tables, the `finalcond`
  clamps, the `cond1` A-then-B fallthrough, `calcMean` replay against the
  pinned clamped module (`maxd = maxbd = 0` spike series solved for exact
  target t-values), the JVM-invalid spike case (pinned `m_err_sing` →
  `runtime_error`), every validation rejection, the pinned corpus replays of
  `m_n80_rw` and `m_trend_seasonal`, and back-to-back determinism.

### Landing qualification tiers

The corpus replay is bit-exact wherever the machine is bit-exact; the tier
table records the measured residual drift (bands are stated as
`abs + rel·|pinned|` floors):

* *Exact tier* (everything): the decisions, flags, counters, `c_`,
  `useml`/`mlused` — exact; all 39 `tmean`s and all final-round JVM-
  convention parameters — measured 0.0 drift; pure-HR `rmax/rsmax`
  (no trial fit) qualified at 1e-9 (measured 0.0).
* *Step0 root-scan tier* (trial-fit cases only): `rmax/rsmax` are computed
  from the step0 model landing when a step0 trial fit ran from the default
  fill. Six cases take this tier (`m_i2_144`, `qtr_rw`, `m_ar9995`,
  `m_double_rw`, `m_trend_seasonal`, `m_n231_rw`); the largest drifts are
  `m_i2_144` at 3.2e-5 on `rmax` and 1.31e-4 on the small `rsmax` (the
  pinned LM and the replay leave the same basin at a ulp offset through the
  near-rank-deficient interior steps), the other five measure ≤1.5e-6 at
  -O2; the tier uses a
  2e-4 absolute floor with the 5e-5 relative term, and the decisions
  downstream of these two quantities are still compared exactly. The sweep
  classifies a run into this tier when any round recorded a trial fit (the
  step0 pass qualifies, as the tier text says); since 21-F the iteration
  records are default-initialized, so the step0 `trial_fit` flag is
  deterministic — earlier sweep runs relied on an indeterminate value here
  (disclosed with item 21-F).
* Measurement caveat: the bands are defined for the pinned -O3 provider
  build. Sanitizer-instrumented compilation shifts the root-algebra ulps by
  up to ~2.1e-6 (observed on five no-trial cases under ASan/UBSan); the
  sweep still reproduces all 39 decisions exactly there.

## Not delivered or claimed

No promotion of any threshold (roadmap decision 5 stays open — every value
above is a transcription constant and the request carries no defaults), no
`SeasonalityDetector` (the decoder's `estimateSeasonality` override path
always hands a concrete period to this seam in the qualified scope), no
missing-value staging (responses are complete and finite; preparation is
upstream), no period outside {4, 12} (the estimation seams are qualified for
the estimation-period domain only — the pinned JVM accepts any period), no
`ml = true` module variant (never set by the decoder; the condition
transcribes as `failure || useml`), no reach of the unreachable `airline()`/
`remark` path, no wiring into the 21-H round structure, and no claim that the
step0 trial-fit landing reproduces the pinned optimizer on arbitrary data —
the measured tier above bounds the corpus only.
