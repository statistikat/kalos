# Scope of item 22c slice M3c (X-13 deterministic-effect allocation staging)

Item 22c (roadmap "restricted integration") ultimately connects a fixed
preprocessing/X-13 specification end to end. This slice M3c pins **only**
the deterministic-effect allocation staging around X-11 — the two seams of
the pinned `X13Kernel` that surround the qualified X-11 passes:

* the **preadjust** seam (`initialStep`): bucket allocation of the model
  variables into the stored `a`-tables, the `out` series fed to X-11 and the
  `X11Spec` mode forcing; and
* the **restore** seam (`finals`): the deterministic-effect re-addition and
  the `e`-tables.

Inside the slice the inputs are **coefficients given, not estimated**: the
complete input series, the pinned variable order, the pinned regression
columns and one coefficient per column are all inputs, exactly as the pinned
run used them. The capture proves that, under those givens with zero
backcast/forecast horizons and `LengthOfPeriodType.None`, the whole staging
is pure algebra — no ARIMA evaluation, no estimation, no interpolation —
and the fixtures pin it bit-exactly (154 in-pipeline recomposition checks,
plus the test/sweep table checks listed in the contract).

## What the fixtures pin

* Class predicates and bucket routing: `isOutlier`/`isDaysRelated`/
  `isMovingHoliday`/`isUser` with the EXACT `regeffect` attribute match (the
  captured outlier with an `Undefined` attribute contributes to **no**
  table; only user variables carry the `SeasonallyAdjusted`/`Series`/
  `CalendarEffect`/`Undefined` buckets; the days/holiday buckets ignore the
  attribute).
* The `DataBlock.addAY` accumulation rule and `deterministicEffect`'s
  model-order column accumulation (multi-column TD7, six columns; Easter as
  a moving holiday).
* The left-associative fold order of `a8`, `a9` and `v26`; `a9u`'s exclusion
  from `out` and the finals; `out`'s subtract/divide rule keyed on
  `isLogTransformation` (not on the mode).
* `backTransform` = exp under a log model for every stored a-table, while
  `a1` stays the complete input bit-exactly.
* The `X11Spec` mode forcing (`updateSpec`): non-log → Additive; log →
  Multiplicative unless LogAdditive/PseudoAdditive.
* The finals chains with the op/invOp/mean truth table, the log-additive
  subtract-then-multiply asymmetry included, and the `correctSeries`
  replacement rule (weight == 0, negative zero included, NaN kept).

## What later 22c slices must pin before extending

1. **Coefficient estimation** (belongs to items 20b/21): the free-parameter
   cursor order (`Estimation.getCoefficients()`), outlier/auto model
   selection and the `PreliminaryChecks` refusals (no data, fewer than
   `max(8, 3*freq)` observations, >80% identical values, >33% NaN).
2. **Backcast/forecast extension**: the nonzero-horizon route through
   `ExactArimaForecasts` (`linv − v27` residual, `v27 = v26 + a9u`), the
   `a1a`/`a1b` extension tables, the extended `out` calendar and the
   `d11a/d11b/...` split finals.
3. **Missing-value interpolation**: `interpolatedSeries(arima)` and the
   `MissingValueEstimation` semantics (`mv + pred` vs `mv·exp(pred)` under
   log), which make `a1` differ from the input.
4. **Length-of-period transforms**: `backTransform`'s converse LoP step and
   the LoP regressor routing.
5. **ArimaDiffReg/bty** user variables (the ARIMA route that consumes `v27`).
6. **Benchmarking and diagnostics** (item 25): the `CholetteProcessor` hook
   in `X13Kernel.process`, the SI/metadata outputs and the `X13Results`
   export layout, including the M3b-i results-builder export stage this
   slice only reconstructs for verification (its general `BiasCorrection`
   options and `makePositivity` branch are not delivered).

X-11 internals between the seams are the qualified M3a/M3b-i/M3b-ii scope
(`x11-multiplicative-contract.md`, `x11-log-additive-contract.md`,
`x11-pseudo-additive-contract.md`); the selection pass over the preadjusted
series is exercised end to end in this slice's tests but adds no new X-11
claims.

## Explicit non-goals of this surface

No estimation entry point, no spec object, no exception-free error channel:
shape violations throw `std::invalid_argument` (fail closed), missing rows,
non-positive inputs under a log model, non-zero horizons and
`lengthOfPeriod != None` are simply outside the represented domain. The
`a`/`b` extension fields and benchmarking fields are not represented at all,
so no caller can mistake a zero-horizon result for the general one.
