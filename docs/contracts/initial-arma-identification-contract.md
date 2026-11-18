# Initial ARMA identification — the ArmaModule stage-M machine (21-E)

`identify_initial_arma()` is the stage-M part-1 identification unit: a literal
transcription of the pinned `jdplus.x13.base.core.x13.regarima.ArmaModule`
initial-identification machine (the `IArmaAutoModellingModule` behind
`ArmaAutoModelling`, minus the AutoModellingModule refinement hill-climb, which
is stage M part 2) composed onto the qualified estimation seams of items
21-A/21-C/21-D. It maps the upstream model (response, pre-differencing user
regressors, mean flag, differencing orders, period) onto the selected
stationary orders plus the OLS-residual cursor, every candidate evaluation of
the three-stage grid, the merged `estimations[]` panel and the refinement
trace. It is a decision unit feeding - not yet wired into - the fit seams.
There is no Rcpp boundary; the unit is C++-only.

```cpp
kalos::InitialArmaIdentificationRequest q; // series, regressors, mean, period,
                                           // d, seasonal_d, models_count,
                                           // max_p/max_q/max_bp/max_bq,
                                           // balanced, mixed, accept_white_
                                           // noise, eps: all required data
auto id = kalos::identify_initial_arma(q); // selected orders, residuals
                                           // (cursor), evaluations, preferred
                                           // models, refinement steps
```

Everything the pinned module holds arrives as required request data (standing
rule of item 21: pinned values stay transcription constants, never
configuration defaults; roadmap decision 6 stays open). The decoder-pinned
call site (`X13SpecDecoder.readAutoModel`) builds the module as
`ArmaModule.builder().modelsCount(5).maxP(2).maxQ(2).maxBp(1).maxBq(1)
.build()` over the default `AutoModellingSpec`: `balanced = false`,
`mixed = true`, `estimationPrecision = 1.0E-7`, `acceptWhiteNoise = false`.
`acceptWhiteNoise` is a stored, never-read field of the pinned 3.9.0 bytecode
(constructor `putfield`, no `getfield` anywhere): the unit accepts either
value with no effect, and the qualified smoke test pins bit-identity across
the two settings. The stage-3 `bp` clamp keys on the seasonal differencing
order (`bd == 1` collapses `m1` to 0), not on that flag, exactly as pinned.

## Pinned semantics, step by step

`ArmaAutoModelling.process` computes `cursor = RegArimaUtility
.olsResiduals(desc.regarima())` and calls `select(cursor, period, d, bd)`;
any exception (including the null-`best` NPE) means Failed, never a silent
fallback. All pinned-abort equivalences here are `std::runtime_error`.

*Cursor* (`olsResiduals`): the response is the unit-root BackFilter
(`pinned_backfilter_apply`, 21-D.2) applied to the series and user columns
(`w = d + bd * period` observations lost; `n = obs - w`, `n <= 0` throws the
"Not enough observations" EcoException equivalence); the design is a *fresh*
ones column (only under the mean flag - not a differenced constant) followed
by the filtered user columns. `variablesCount == 0` returns the response
unchanged. Otherwise `OlsComputer` guards `y.norm2() < 1.1102230246251565e-16`
(the pinned `Constants.getEpsilon()`; a null result NPEs in the pinned code)
and solves `b = QRLeastSquaresSolver.robustLeastSquares(y, X)` - pivoted
Householder with rcond `1.0E-9` - then accumulates
`r[i] -= b[j] * col_j[i]` per column in ascending column order (the pinned
`DataBlock.addAY` sweep, not per-row dot products). Kalos reproduces the
single-column case bit-exactly via `detail::pinned_ols_1col` and the
multi-column case through the qualified `qr_least_squares` (DGELSY, rcond
`1.0E-9`, the 21-A transcription of the same pivoted-Householder solve; see
"Deviations").

*select(cursor, period, d, bd)* (`period == 1` shortcuts are outside the
qualified period domain {4, 12}): `estimations` is a fixed `nmod` array.
Stage 1 evaluates the `(bp, bq)` grid `bp in [0..maxBp] x bq in [0..maxBq]`
(`bp` outer) on the regular part `(3, 0)` (the `period == 2 ? 1 : 3` preset,
`q = 0`), admitting a candidate iff `mixed || bp == 0 || bq == 0`; the stable
sort head (first minimum) becomes the working `best`. Stage 2 evaluates the
`(p, q)` grid `p in [0..maxP] x q in [0..maxQ]` (`p` outer, `q` inner, same
mixed gate) on that `(bp, bq)`; its head is adopted and the sorted list is
merged into the fresh `estimations`. Stage 3 re-grids `(bp, bq)` on the stage-2
winner with `m1 = (bd == 1 ? 0 : maxBp)`, `m2 = maxBq`, and merges again. The
stage-3 winner is never re-adopted directly; the stage-1 winner's regular part
(p = 3) is never merged. The result is then `select(cursor, d, bd)` over the
merged panel.

*Candidate evaluation* (`RegArmaBic` constructor):
`RegArimaUtility.processor(true, eps)` - `GlsSarimaComputer` with
`HannanRissanenInitializer(stabilize = true, useDefaultIfFailed = true)`,
the default LM minimizer, `precision = eps`, `useMaximumLikelihood = true` -
over `RegArimaModel.builder().y(cursor).arima(SarimaModel
.builder(orders).setDefault().build()).build()`. The builder's `mean` field
defaults to false (bytecode: `Builder.<init>` never stores it), so every
candidate is a *design-free* ARMA on the cursor - the mean correction already
lives in the cursor - and `statistics()` counts
`k = (p + q + bp + bq) + variablesCount(0) + 1(variance)`. Kalos runs the
same chain as `estimate_sarima_fixed` with `design.columns = 0`,
`d = seasonal_d = 0`, `starting_point = HannanRissanen`,
`solver = PinnedLevenbergMarquardt`, `ml = true` (21-C: this chain has no
starting-point policy and no unit-root restarts; the minimizer landing is
adopted whatever the stop status). `BIC2 = ((-2 * logLikelihood) +
k * log(n)) / n` over the cursor length `n`, with the FdLibm log (21-D.2
ground truth; the capture JVM pins the intrinsics off) and the raw
`logLikelihood` - verified bit-exactly against `statistics().getBIC2()` on
every corpus row (the probe aborts on any mismatch). `sort()` keeps every
candidate whose BIC `!= 99999` (NaN kept), ordered by `Double.compare`
(`-0.0 < +0.0`, NaN greatest; stable on bit-equal ties), evaluated in grid
order.

*mergeInto* (ascending list into the fixed panel): first-fit scan per incoming
candidate - equal specifications are dropped, an entry with a worse BIC is
inserted with the ascending tail shifted one slot right (the last entry is
evicted), otherwise the scan continues. The first merge degenerates to a copy
because the array is fresh.

*select(cursor, d, bd)* (refinement over the merged panel): the pinned
99999-tail trim can reach an unfilled slot - all `models_count` slots must be
filled or the pinned code NPEs, so an unfilled panel is a `runtime_error`
equivalence here. With `dpq = |p + d - q|`, `dbpq = |bp + bd - bq|` (polarity
measures counting the implied unit roots as AR factors),
`dif = bic[n-1] - bic[0]` mapped through `(< 0.003 ? 0.0625 : < 0.03 ? 0.25 :
1.0)`, and `lim1 = 0.01 * dif`, `lim2 = 0.0025 * dif`, `lim3 = 0.0075 * dif`,
the loop over `i = 1..n-1` tests eleven sequential accept rules against the
incumbent-relative `curdif = bic[i] - bic[id]` (a NaN `curdif` fails every
comparison, as pinned): rules 1-7 additionally require `balanced`; rules 8-11
never do. On acceptance the limits are re-anchored by
`bic[i] - bic[0]`, the incumbent and its counters update. After the loop: an
empty-parameter incumbent with a following entry returns `estimations[id + 1]`
(the pinned white-noise rescue - this is where `acceptWhiteNoise` is *not*
consulted), else the incumbent.

## Failure and interrupt contract

`validate()` rejects with `std::invalid_argument`: empty/non-finite series,
regressor length or finite violations, negative `d`/`seasonal_d`, period
outside the qualified {4, 12} domain (the pinned JVM accepts any period;
Kalos qualifies 4 and 12 only, like every earlier fit seam),
`models_count < 1`, negative grid bounds, non-finite or non-positive `eps`.

Pinned-abort equivalences throw `std::runtime_error` (the
`ProcessingResult.Failed` paths, never a silent fallback): no observations
after differencing; a degenerate OLS response below the pinned norm2 epsilon;
a candidate whose start or landing the pinned machine rejects (the pre-
minimize `RegArmaSsqFunction` throw or the landing `DEFAULT_COMPUTER.compute`
throw); a merged panel that cannot fill all `models_count` slots. Interrupt
exceptions always propagate (the smoke test pins it).

## Documented deviations

- Multi-column OLS cursors solve through the qualified DGELSY transcription
  instead of the pinned Java pivoted-Householder (same rcond 1.0E-9, same
  pivoted-Householder family): agreement is at the 1e-16..1e-14 level on the
  corpus. The extreme ar120 trend+sine case at `d = bd = 1` is annihilated by
  the differencing polynomial (the trend dies under `(1 - B)(1 - B^12)`, the
  period-12 sine under `(1 - B^12)`): the projection is conditioned on
  1e-15-magnitude pivots and is only path-stable inside one implementation.
  The sweep skips that case with a counted documented skip.
- The 21-A.2 HannanRissanen start is qualified with tolerance, not bit
  identity; near its 1-ulp deltas the small-n likelihood surfaces carry
  distinct local maxima separated by ridges, so the pinned chain and the
  Kalos chain can converge - both to certified converged landings - in
  different basins. This drives the landing tiers below; it never moved a
  decision in the corpus (288/288 selected orders match).

## Tiers (sweep and smoke test)

Exact: cursor (one-column cases), evaluation enumeration (stage, slot,
orders), grid admission, merged-panel membership as a set, selected orders,
refinement outcome, the one-column landing cells the JVM reproduces bit-stably.
Banded: multi-column cursors at 1e-9; candidate `logLikelihood`/`BIC2`/
coefficients at 1e-5 / 5e-4 * n_eff / 2e-3 ("tight"), above that the measured
documented classes - converged basin flips (measured ceilings: dBIC2 9.7e-02,
dlogLik 1.7, dcoef 1.7 on one corpus cell) and iteration-limit drift cells
(dBIC2 4.5e-04) - and panel BICs likewise; a panel whose set matches but
whose value order permuted at basin cells counts as a documented reorder.
Anything above those ceilings, or any decision mismatch, is a failure, never
a band.

## Corpus and capture

`validation/evidence/native/initial-arma-identification/` was captured by
`validation/reference/capture-initial-arma-identification.R` driving
`InitialArmaIdentificationProbe.java` (public pinned API only: real
`RegArimaUtility.olsResiduals`, `RegArmaBic`, `ArmaModule.select`) against the
pinned rjd3 3.9.0 jars on the pinned JVM (JAVA_HOME java-21-openjdk-amd64,
`_dexp`/`_dlog` intrinsics disabled per 21-D.2, guarded by an in-script
equality check). 72 cursor cases (white noise, AR, random-walk and
seasonal-walk series, n in [36, 144], period {4, 12}, d/bd in {0, 1}^2, mean
on/off, zero/one/two user regressors) x {mixed, non-mixed} grids give 1908
candidate rows (each cross-checked BIC2 == statistics BIC2 in-process) and
288 select rows (decoder, balanced, non-mixed and nmod-3 variants). The corpus
was captured twice from independent JVM processes and the six CSVs are
byte-identical (`--repro`), so no nondeterministic cell had to be dropped;
`SHA256SUMS.csv` pins the bytes.

Qualification: ctest #54 (`test_initial_arma_identification`, both lanes),
`tools/verify_initial_arma_identification.cpp` (4368 checks: 0 failures;
1858 exact, 29 documented basin/drift/reorder classes, 1 documented skip),
ASan/UBSan runs of the test, and the unchanged 21-A/21-B/21-C/21-D sweeps.
