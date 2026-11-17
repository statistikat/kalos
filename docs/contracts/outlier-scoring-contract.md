# Outlier scoring and forward selection (21-C)

Stage-O units of the pinned `jdplus` 3.9.0 outlier machine: the critical-value
chain (`Utility.intProbability` / `getProbabilityInverse` / `X13Utility.calcCv`),
the outlier factories, the `ExactSingleOutlierDetector` exact-whitening scan
and the `ExactOutliersDetector` forward-selection driver, composed onto the
qualified 21-A [estimation control](estimation-control-contract.md) layer and
the qualified [regression likelihood](regression-likelihood-contract.md).
This slice scores and *selects* outliers over a *given* fixed-order estimation;
it never fixes outliers into a spec, never rebuilds the design for a later
stage and never touches the preadjust staging (stage Z / 21-H owns those).
There is no Rcpp boundary; the units are C++-only and every pinned constant
below is a transcription constant, not configuration.

```cpp
auto found = kalos::scan_outlier_candidates(scan_request);   // T tables + winner
auto res   = kalos::detect_outliers(request, estimate, optimize); // selection
```

Public surface: `include/kalos/outlier_scoring.hpp`
(`standard_normal_lower_cdf`, `standard_normal_lower_quantile`,
`default_critical_value`, `transitory_change_rate`, `outlier_excluding_zone`,
`fill_outlier_column`, `regression_t_statistics`, `scan_outlier_candidates`,
`detect_outliers`); `include/kalos/estimation_control.hpp` gains the warm
re-entry entry point `optimize_sarima_from`. Implementation:
`src/outlier_scoring.cpp` (scoring + driver), `src/outlier_scan_engine.cpp` +
`src/outlier_detail.hpp` (Ansley / ModifiedLjungBox / MaLjungBox / Mad(50) /
pivoted-QR scan machine).

## Critical values

`standard_normal_lower_cdf` is the pinned Cody/branch `intProbability`
transcription (THRSH = 0.66291, the 0/1/2 region series, the signed
`floor(1.6*x)` split, the two-factor `exp` split) — operation for operation,
including the `dcmpl`/`dcmpg` NaN directions per the class-file spec (`dcmpl`
pushes −1 on NaN, `dcmpg` pushes +1). `standard_normal_lower_quantile` is the
`stnVal` seed + Newton refinement at TOL 1e-13 over at most 200 iterations;
the pinned `DStatException` guards (p or 1−p strictly below 1e-15; NaN passes
the guard and lands in the "Too many iterations" escape) map to
`std::invalid_argument` and `std::overflow_error`.

`default_critical_value(n)` is `max(2.8, advancedComputer(0.05).apply(n))`:
three-anchor interpolation on the nodes {2, 100, 200} with the anchor values
built by `calcVAL`, solved with the pinned `LULinearSystemSolver2` at 1e-13.
The `calcVAL` transcription follows the JVM operand-stack order — the final
statement is `d11/d7 + (d7 − d9)` because `ddiv` pops the *last-pushed*
value as the divisor (an earlier Kalos reading had the ratio inverted).
`n == 0` throws: the pinned chain has no defined value there and the module
never asks. The corpus grids qualify all of the above at bit level (1e-14
relative band for the documented 1-ulp JVM/libm deltas) plus the seven pinned
exception families on the error rows.

## Factories

The pinned x13 factory order (AO, LS, TC, SO = ordinals 0..3) indexes the
T-table columns and the detected (position, type) pairs. `fill_outlier_column`
transcribes the four `IOutlierFactory.fill` bytecodes over a fresh zero
column: AO `db.set(n, 1)` on the detector's `2·len` buffer, the zero-ended LS
`−1` prefix, the zero-ended SO periodic backward pattern, and the TC ramp
`coefficient^(j−position)` whose write loop stops at the first magnitude
below the pinned `ZERO` cutoff 1e-15 (the cutoff value itself is still
written; only values ≥ ZERO are ever stored). Excluding zones: AO (0,0),
LS (1,1), SO (2P,2P), TC (0,1).

`transitory_change_rate(freq, monthly)` is the pinned
`TransitoryChangeFactory.rate(freq, monthly)`: the monthly rate itself at
`freq == 12`, `pow(monthly, 12/freq)` when `12 % freq == 0`, rejected
otherwise (the pinned "Invalid frequnecy" `TsException`). The scan and driver
inputs in the corpus store the *monthly* decoder rate; call sites (the sweep,
future decoder wiring) must derive the factory rate from the frequency — the
period-4 cases run at 0.343, not the stored 0.7.

## t-statistics

`regression_t_statistics` is `ConcentratedLikelihood.tstats(0, false)`'s
vector path: `coefficient[i] / sqrt(covdiag[i] * ssq / dim)` in that
arithmetic order for every estimated coefficient, no per-entry zero/NaN
special cases (zeros and NaNs propagate through the division). The guard
requires the fit's status `"available"` — the `RegressionLikelihood` success
string; any other status throws. The concentrated dimension at the pinned
call sites is `n − p − q − nx` after differencing; consumers reconstruct it
as `llround(sum_squared_standardized / profiled_variance)` (the profiled
`ssq/var` pair the pinned module hands to `tstats`), falling back to
`finite_count`.

## Scan engine (`scan_outlier_candidates`)

The request carries the whole pinned detector state: response (no masked
values — the qualified scan reproduces the no-missing differenced model),
current design (base columns first), stationary ARMA blocks in the Kalos
convention, bounds, factory list, factory rate and the exclusion table. The
unit builds the pinned `differencedModel()` and runs
`ExactSingleOutlierDetector.calc()` operation for operation. Bytecode-verified
details that are *not* visible from the class-level semantics:

* **Polynomial degree normalization.** `Polynomial` strips trailing zeros,
  so the expanded stationary `ar`/`ma` polynomials lose the zero-tail orders
  a caller block of padded zeros would suggest (a `[0.0]` seasonal-AR block
  gives stationary p = 1, not 13). Kalos strips trailing zeros after the
  polynomial products; without it the MLJ/Ansley dimensions are wrong.
* **Ansley `lcholesky` update bound.** The pinned banded update loop is
  `while (s < k + cnt)` starting at `s = k + 1` — that is `cnt − 1`
  subtractions, not `cnt`. Running the extra iteration corrupts the whole
  factor downstream (wrong T tables, spurious `m_err_chol` on seasonal
  models).
* **`calcg(int)` (the MaLjungBox G matrix).** The second loop *overwrites*
  each cell from the previous column: `col_i[j] = col_{i−1}[j−1] −
  psi[n−j]·psi[n−i]`. The pinned stack has no `dup2` read of the current
  cell — a `-=` transcription (natural from the disassembly's leading
  `aload/set` pair) is wrong and produced non-PD `X` matrices (the
  `m_err_chol` throw in `MaLjungBox.prepare`) exactly when fitted MA
  coefficients first exercised the path. The corrected loop reproduces the
  pinned `m_G` bit for bit. (`calcg(double[])`, the data path, *does* carry
  the genuine `dup2` decrement.)
* **`mlComputer()` is `ArmaFilter.ljungBox(true)`**: `prepare` returns
  `n + q`, `apply` writes the warmup `u` entries at `[0..q)` and the
  innovations at `[q..n+q)` (`drop(q, 0)` semantics), so the ML residuals
  are `block[q..n+q)`. The Kalos block layout is identical.
* **Robust scale.** `mad(false)` is `1.4826 · median(|residuals|)`; the scan
  uses the MLJ residuals over the differenced response (or the AR-only
  residual path the pinned code takes for `p == 0`).
* **processOutlier slice.** The outlier column is filled into a `2·len`
  buffer, filtered by the non-stationary `BackFilter` (`dst[i] =
  Σ ns[k]·src[i+w−k]`) into a `2·len − w` buffer, and the detector consumes
  `DataBlock.of(xd, len − pos, 2len − w − pos)`. Kalos differsencing is
  globally sign-flipped against the pinned filter; the sign cancels in the
  T ratio and is deliberately left as-is.
* **Exclusion rule.** A non-positive residual variance (`dcmpg(ssqr, 0) ≤ 0`
  path) excludes the cell permanently (`excluded_after`, T stays NaN there);
  the driver feeds the table back into the next scan.
* **searchMax / sentinel.** The winner scan is column-major with unit
  weights; if no cell beats zero the pinned `oMax` stays −1. The pinned
  `getMaxTStat` then indexes `T(−1,−1)` and throws; Kalos reports
  `max_type_sentinel` with `max_t_stat = 0` instead. This is the one
  documented deviation and the sweep asserts it (the corpus stores the JVM
  error row).

Invalid requests throw (`fail closed`): empty type list, duplicate types,
fixed design columns, non-finite design values, bad bounds, differencing
orders, NaN factory rate. Whitening failures inside the pinned machine
(`m_err_chol`, "acgf of non stationary model") map to
`available = false, status = "whitening_failed"` — the pinned `calc()` return
of `false` — with the tables left cleared, as after the pinned `clear(false)`.

## Driver (`detect_outliers`)

`detect_outliers` transcribes `OutliersDetectionModule.run` →
`ExactOutliersDetector`: `process()` (opening `estimateModel(map, true)`),
`execute()` (the forward-add loop), `verifymodel()` (the closing drain).
Bytecode-pinned wiring decisions:

* `estimateModel(mapping, full)` calls `processor.process` when `full` and
  `processor.optimize` otherwise, and **returns false only for a null
  `RegArimaEstimation`**. A stalled-but-finite fit is adopted and its
  tstats drive the next decision — convergence is *not* the gate. The
  `OutlierEstimation.available` callback contract mirrors exactly that:
  availability = the retained coefficients produce an available regression
  likelihood, never `est.converged`.
* `process()` and every add in `execute()` use the full chain (`process`);
  only the failed-add rollback and the `verifymodel` drain re-enter warm
  (`optimize`) from the incumbent coefficients. The warm re-entry is the new
  `optimize_sarima_from` core entry: the qualified single-chain estimator
  seeded from caller-owned coefficients, no starting-point policy, no
  unit-root restarts.
* `addNewOutlier` appends the factory column and excludes nothing; the
  pinned `changed` flag is set by every add/remove and never reset — a
  rolled-back add still reports `changed = true`.
* Stops: `|maxT| > cv` (pinned `dcmpl`/`iflt` semantics: a NaN `maxT` stops
  the loop), `round < maxRound`, `|outliers| < maxOutliers`; the exit is
  reported as `stop_reason` ∈ `"scan_failed" | "below_cv" | "max_rounds" |
  "max_outliers" | "estimate_failed"`. The verifymodel drain afterwards can
  only remove outliers (recorded in `rejections`).
* Every add triggers a scan of the augmented design, and the pinned
  exclusion table is carried between scans (`excluded_after` → next
  `excluded`), so the `processOutlier` exclusions persist across rounds.

The callback contract keeps the estimation policy outside the driver: the
sweep (and future decoder) callbacks wrap `estimate_sarima_fixed` /
`optimize_sarima_from` + `sarima_regression_likelihood` +
`regression_t_statistics` and report the `OutlierEstimation` struct.

## Qualification

Corpus: `validation/evidence/native/outlier-scoring/` (7 files), captured
through `validation/reference/OutlierScoringProbe.java` +
`capture-outlier-scoring.R` against the pinned jars. Everything here touches
estimation, so the corpus is **double captured**: the script was run twice
into independent JVM processes and all case files compared byte for byte
(`--repro`); `metadata.R` records both probe script and observer sha256. The
selection outcomes themselves are JVM-reproducible across processes (verified
across the two captures; the pinned driver reaches identical
(position, type) sequences in both).

`tools/verify_outlier_scoring.cpp` tiers:

* grids: bit level (77 exact values, 1e-14 band rows) + 7 pinned throws.
* scans (6 cases: AO-only, all-four, exclusion table, bounds window,
  period-4 + TC rate, sentinel): pinned availability, exact winner (position,
  factory, `maxT`) and exclusion state; T/coefficient cells at a 1e-8
  relative band (the pivoted Householder QR transcription is operation for
  operation; BLAS order deltas aside); the sentinel case asserts the
  documented deviation.
* drivers (8 cases: two-AO, AO+LS, all-four 7-outlier, clean below-cv,
  maxOutliers limit, cv 4.0 + AO/TC, quarterly, bounds span): `changed`,
  kept (position, factory) pairs, design column count, stop reason and
  rounds **exactly**, final tstats inside the basin band
  `1e−2 + 1e−4·|t|` and final ARMA parameters inside `2e−3` absolute.

Basin band rationale (pinned warm-vs-full behavior, not a Kalos deviation):
with a near-unit-root seasonal MA the likelihood valley is flat, and the
full-chain LM run that produced the pinned final fit stops ~1e-3 in the
parameters from the Kalos run along the same valley (worst observed: 1.2e-3
parameter delta, 4.4e-3 t delta). The band qualifies basin agreement, not
bit identity; the noise was *verified* as flat-valley, not surface,
difference: warm re-entry from the JVM's own final parameters converges back
to the Kalos fixed point at an identical log-likelihood. Worst observed
sweep ratios: driver t 0.37 of band, parameter 0.61 of band.

Unit-level qualification covers the branches the corpus cannot pin
deterministically at unit scope: the t-stats guard/zero/NaN propagation
paths, factory fill edge cases, zone math, request validation, the driver's
rollback/rejection mechanics over scripted callbacks (`warm start` and
`driver` blocks of `tests/test_outlier_scoring.cpp`).

## Deviations and held decisions

1. Sentinel: `getMaxTStat` behavior on the no-winner escape (above) — the
   only behavioral deviation, documented, swept, and unreachable for any
   non-degenerate scan.
2. The scan exposes no defaults: response, blocks, bounds, types, factory
   rate, cv, maxRound/maxOutliers (pinned decoder defaults 50/30 are
   *call-site* values, e.g. the sweep passes them explicitly) and the
   `ZERO`/THRSH/cv-floor 2.8 constants all stay transcription constants or
   required request data. Promotion (e.g. exposing `calcCv` as a config
   default, module-level exclusion-zone switches) stays on the
   [roadmap](../item21-roadmap.md) maintainer decision list; stage O's
   module call-site values (`X13Utility.processor(true, 1.0E-7)`, cv via
   `X13Utility.calcCv(n)`, tcrate 0.7 monthly) are captured call-site facts
   for the 21-H integration to pass in.
