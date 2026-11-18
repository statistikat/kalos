# ARMA refinement and comparison — the AutoModellingModule stage-M machine (21-F)

`refine_arma_model()` is the stage-M part-2 refinement unit: a literal
transcription of the pinned `jdplus.x13.base.core.x13.regarima.
AutoModellingModule.process` - the refinement + comparison stage that follows
the initial identification (21-E) in the pinned round-0 pipeline. It maps the
current model state (response, user regressors, mean flag, the current
estimation's concentrated-likelihood regression coefficients, the current
`SarimaOrders`) onto the refined specification, mean flag and changed-flags,
plus the linearized data, the re-differenced select cursor, the differencing
record and the select panel. It is a decision unit feeding - not yet wired
into - the fit seams; the round-structure wiring is stage Z (21-H). There is
no Rcpp boundary; the unit is C++-only.

Scope note: the roadmap's original stage-M part-2 vocabulary (`SarimaComponent`
hill-climb with the `AICcComparator` -0.2/-0.1 margins) is dead or stage-R
code in the pinned 3.9.0 bytecode - `SarimaComponent` has zero users and
`AICcComparator` is referenced only by the calendar/TD (stage-R) modules. The
live refinement + comparison stage is `AutoModellingModule.process`, and that
is what this contract pins.

```cpp
kalos::ArmaRefinementRequest q; // series, regressors, mean, regression_
                                // coefficients, current p/q/bp/bq/d/bd,
                                // period, both module states' scalars
auto m = kalos::refine_arma_model(q); // refined spec + flags + records
```

Everything the pinned module holds arrives as required request data (standing
rule of item 21: pinned values stay transcription constants, never
configuration; roadmap decisions 5 and 6 stay open). The decoder-pinned call
site (`X13SpecDecoder.readAutoModel`) builds the two module states as
`DifferencingModule(2, 1, 0.97, 0.88, cancel = 0.1, precision = 1.0E-7)` and
`ArmaModule.builder().modelsCount(5).maxP(2).maxQ(2).maxBp(1).maxBq(1)
.build()` over the default `AutoModellingSpec` (`balanced = false`,
`mixed = true`, `eps = 1.0E-7`, stored-never-read `acceptWhiteNoise =
false`); the probe re-reads both objects and pins the exact double bits. The
current estimation's `cl.coefficients()` ((mean ? 1 : 0) + k values) is the
only state that 21-E did not need: `RegArimaUtility.linearizedData` reads the
regression coefficients, never the ARMA landing, so the ARMA parameters of
the current estimation provably do not enter the machine.

## Pinned semantics, step by step

`process` guards the whole region `linearizedData .. SarimaOrders.of` in one
`RuntimeException` catch whose handler is the airline fallback (spec
`SarimaOrders.airline(period)`, mean off, remark "default model selected") -
the module survives where `ArmaModule.process` would report Failed.

*Linearized data* (`RegArimaUtility.linearizedData`): `z = y` (missing-data
corrections are upstream preparation, as in every item-21 seam); if the
concentrated likelihood carries coefficients, per user column in ascending
order `z[i] = z[i] - coefficient * column[i]` (the `DataBlock.apply`
element loop with the captured-coefficient lambda). The mean coefficient is
skipped, never removed: the linearized data keeps the mean. Pure double
arithmetic - bit-identical corpus-wide.

*Differencing decision* (`DifferencingModule.process(lin, period)`): the 21-D
unit verbatim on the linearized response (the pinned `== false` remark branch
and the `(1, 1, false)` path are dead - `calc()` always returns true; every
failure throws). A throw here is the airline fallback with stage
"differencing".

*Re-differencing* (the stage's key pinning finding): `if (d != 0 || bd != 0)
z = BackFilter(UnitRoots.D(d).times(UnitRoots.D(period, bd))).apply(lin)`,
else `z = lin`. `UnitRoots.D(n)` is the single tap `1 - B^n`, **not**
`(1 - B)^n`: at `d = 2` the module re-differences with `(1 - B^2)(1 - B^P)`,
genuinely different from the 21-E cursor filter `(1 - B)^d (1 - B^P)^bd`
(transcribed as the local `automod_unitroot_polynomial`, never the 21-E
`pinned_differencing_polynomial`). The corpus exercises it: the `m_i2_d2*`
and `m_i2_s7` cases end at `d = 2` and `q_i2_60` at `d = 1` with period 4.
The filtered sequence is bit-identical corpus-wide.

*Select* (`ArmaModule.select(z, period, d, bd)` + `getPreferedModels()`): the
21-E machine (`select_arma_orders`, the seam 21-E exposes exactly for this
call site) on the filtered sequence - no OLS cursor, design-free candidates,
three-stage grid, BIC2 sort/merge, eleven-rule refinement; `d`/`bd` enter as
refinement polarity. Pinned candidate/panel aborts are `runtime_error`s
landing in the same airline catch (stage "select").

*Comparison*: `SarimaOrders.equals` covers all seven fields; the result
period equals the request period, so the six orders decide
`specification_changed` (`setSpecificationChanged`/`...Unchanged`), then the
separate mean-flag test sets `mean_changed`; `changed =
specification_changed || mean_changed` drives `clearEstimation` +
`ProcessingResult.Changed`.

## Failure and interrupt contract

`validate()` composes the 21-D and 21-E scalar validators plus the new
coefficient-count rule (`(mean ? 1 : 0) + regressors.size()`, all finite) and
throws `std::invalid_argument`: empty/non-finite series, regressor length or
finite violations, negative orders, period outside {4, 12},
`models_count < 1`, negative grid bounds or clamps, non-finite or
non-positive `eps`/`precision`/`ub1`/`ub2`/`cancel`. A linearized response
the differencing unit rejects statically is a request-data error (outside
the pinned machine), not a fallback.

Inside the guarded region every pinned-abort `runtime_error` (differencing
throws, filtered sequence not longer than the filter - the pinned
negative-array candidate path - candidate/panel aborts) becomes the airline
fallback result, never a throw; `fallback_stage` names the stage ("differencing"
or "select") and the records stop at the last completed stage. Interrupt
exceptions always propagate: the first firing of the caller's callback arms
the region, so any later exception is rethrown instead of converted into the
fallback (pinned by the smoke test on both paths).

## Documented deviations

No new deviation classes: the machine reuses the 21-D decision unit and the
21-E select machine bit-for-bit, so the only cross-implementation slack is
the documented HannanRissanen-start/landing basin class of 21-E (and the
DGELSY cursor class, which cannot arise here - `select` takes the sequence
directly). The 21-E annihilated-design skip is likewise unreachable: the
corpus builds no annihilated linearized case.

## Tiers (sweep and smoke test)

Exact: the linearized data and the filtered sequence (pure pinned arithmetic;
measured bit-identity, deviation 0, on all 23 cases including the `d = 2`
one-tap filter), the differencing quadruple `(d, bd, mu)`, `tmean` (measured
bit-identity on all 21 normal cases, both the algebraic stMean and the
nstMean-fit paths - the same machine on the same sequence), the select
orders, all comparison flags (`specification_changed`/`mean_changed/
changed`), the fallback flag and stage, and the panel slot orders. Banded:
`|tmean|` per the 21-D tiers (stMean 1e-12, nstMean 1e-3) with the 21-D
decision-margin audit (never needed - deviations measured 0); panel `BIC2`
and landing coefficients follow the 21-E cell cascade (tight 1e-5 / 2e-3,
documented basin ceilings 2e-1 / 2.0 and drift ceilings 1e-3 / 5e-2 - never
relaxed here). Measured corpus-wide (23 cases, 105 panel cells): 86 exact
cells, 16 tight-band cells, 3 basin-class cells - two drift-class `(2,2,1,0)`
last slots in the `m_i2_d2a`/`m_i2_d2b` pair (dBIC2 7.9e-06, dcoef 1.5e-2)
and one converged basin cell `m_i2_s7/4` (dBIC2 7.8e-06, dcoef 6.2e-03); no
decision, flag or membership is affected in any case. Anything above those
ceilings, or any decision mismatch, is a failure, never a band.

## Corpus and capture

`validation/evidence/native/arma-refinement/` was captured by
`validation/reference/capture-arma-refinement.R` driving
`ArmaRefinementProbe.java` (public pinned API only: real
`RegArimaUtility.processor(...).process`, `RegArimaUtility.linearizedData`,
`DifferencingModule.process` + `getTMean`, `BackFilter(UnitRoots.D(d)
.times(UnitRoots.D(period, bd)))`, `ArmaModule.select` +
`getPreferedModels` - the package-private `Builder` reached by reflection,
the `RegArmaBic` arima field read for observation only - and
`SarimaOrders.of/airline/equals`; no decision rule in the observer) against
the pinned rjd3 3.9.0 jars on the pinned JVM (JAVA_HOME
java-21-openjdk-amd64, `_dexp`/`_dlog` intrinsics disabled per 21-D.2,
guarded by an in-script equality check). 23 cases (white noise, AR,
random-walk, seasonal-walk and doubly-integrated series, n in [16, 144],
period {4, 12}, current specs spanning d up to 1 with the machine reaching
d = 2, mean on/off, zero/one/two user regressors): 21 normal runs plus two
genuine airline-fallback cases (`m_sw20`, `m_sw16`, pinned differencing
failures). Estimation-error rows (`Gls failed` on annihilated regressor
designs) never produce a case; their JVM text is kept in `estimation.csv`
for the record. The corpus was captured twice from independent JVM processes
and all files are byte-identical (double capture), so no nondeterministic
cell had to be dropped; `SHA256SUMS.csv` pins the bytes.

Qualification: ctest (`test_arma_refinement`, both lanes),
`tools/verify_arma_refinement.cpp` (423 checks: 0 failures; 86 exact cells,
19 tight-band, 3 basin/drift-class cells), ASan/UBSan runs of the test, and
the unchanged 21-A/21-B/21-C/21-D/21-E sweeps.
