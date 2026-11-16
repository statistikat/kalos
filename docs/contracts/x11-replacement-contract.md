# Same-phase replacements, endpoints and correction factors (22b.3b)

```cpp
kalos::X11ReplacementPlan plan = kalos::x11_replacements(
    series, 12, sigma.weights, /*backcasts=*/0, /*forecasts=*/0,
    /*exclude_projections=*/false, /*multiplicative=*/false);
```

This is the replacement stage of the default X-11 extreme-value treatment:
given the final observation weights of item 22b.3a
([x11-sigma-contract.md](x11-sigma-contract.md)), it produces the same-phase
replacement values, their neighbor provenance, the insufficient-neighbor
period-average fallback, the projection-mask endpoint handling, the pinned
correction factors and the apply-corrections identity. B17/B20 table
placement, the CV correction variant (`computeCVCorrections`) and
period/group-specific alternatives are not delivered or claimed.

## Public surface and validation

`weights` must be the same length as the series and finite (the legacy policy
may store a slightly negative slack weight; it is kept). `frequency` is 4 or
12. Negative horizons are rejected. With `exclude_projections` the
replacement span drops `backcasts` leading and `forecasts` trailing rows;
`backcasts + forecasts > n` is rejected because the pinned masking loop then
writes outside its output array (undefined behavior is not a result to
reproduce), `== n` is defined and marks every row `outside_span`. Without
`exclude_projections` the horizons are inert, exactly like the pinned
two-argument call. The walk never consults a calendar start: same-phase means
row distance `frequency`, matching the pinned implementation.

## Qualified rules (pinned oracle, single policy)

Per flagged row (final weight `w != 1`) inside the span:

1. Neighbor walk (pinned discovery order): step `-frequency` collecting up to
   two rows with weight exactly 1, then `+frequency` collecting up to two; if
   the left side found fewer than two, continue right up to four in total; if
   the right side found fewer than two, continue left up to four. Non-unit
   weights (zero, ramp, or legacy slack) are skipped, not blocking. Positions
   are recorded in discovery order (`neighbors`, `neighbor_counts`).
2. Four neighbors: `replacement = (w*x + s1 + s2 + s3 + s4) * (1/(4+w))` — the
   reciprocal multiply is the pinned arithmetic, not a division.
3. Fewer than four: `kind = period_average`, replacement is the calendar-phase
   average of the original series, the phase rows `p, p+f, ...` summed
   ascending and divided by the phase row count (`period_averages` is always
   reported).
4. `w == 1`: `kind = none`, replacement NaN; outside the projection span:
   `kind = outside_span`, replacement NaN.

Masked rows carry weight one and an effective NaN value, so they are selected
as fully weighted neighbors and poison a blend to NaN, and a period containing
them averages to NaN — the pinned behavior, reproduced operation for
operation. `corrected` substitutes non-NaN replacements over the effective
values (the public apply-corrections identity). Correction factors follow the
public getter: additive `x*(1-w)`, multiplicative `x/(1+w*(x-1))` for `w < 1`,
and the mode default (`0`/`1`) elsewhere, including outside the span; the
zero multiplicative denominator reproduces the pinned IEEE result (±Inf).

**Cancellation.** Rows with weight exactly one receive no replacement, the
default factor and the original value: the correction cancels completely.
Rows whose flagged phase has no fully weighted neighbor replace a value `x`
by that phase's average, which equals `x` itself whenever the whole phase is
equally flagged (qualified by the `allramp`/`phaseflag` fixture cases).

## Evidence

`validation/reference/capture-x11-replacement.R` drives the pinned
`DefaultExtremeValuesCorrector` (rjd3 3.9.0): public `analyse` →
`getObservationWeights` → `computeCorrections` with both projection-masking
booleans → `getCorrectionFactors` → `applyCorrections`, plus public static
`periodAverages`; the private neighbor walk is re-run through JNI purely for
provenance. The capture itself re-derives every factor and kind from the
recorded weights and refuses to write on disagreement. The 22-case fixture
(`validation/evidence/native/x11-replacement`, 17-significant-digit
round-trip exact) covers endpoint extension, flagged-neighbor skipping,
whole-phase fallbacks, NaN-poisoned blends and averages, both masking modes
(including `outside_span`), multiplicative division factors and the legacy
revival no-op. A throwaway harness compared every artifact of every case
(weights through 22b.3a, both replacement modes, kinds, all neighbor slots,
factors, corrected values, averages) with exact double equality, 22/22.
`test_x11_replacement.cpp` embeds six pinned cases plus closed-form walk,
fallback, poisoning, validation, interruption and serial/OpenMP-equality
controls.

Serial and OpenMP builds agree bit for bit: rows are processed through
`parallel_for` with per-row serial walks and a serially precomputed
`period_averages`, so results are row-order independent. The `Interrupt`
callback is honored before the pass, every 1024 factor rows and before
returning (the row pass itself checks between chunks).
