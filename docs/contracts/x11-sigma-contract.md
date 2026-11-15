# Moving five-year dispersion and two-pass sigma weights (22b.3a)

```cpp
kalos::X11SigmaWeights w = kalos::x11_sigma_weights(
    series, 12, 1, kalos::X11SigmaPolicy::legacy,
    /*backcasts=*/0, /*forecasts=*/0, /*exclude_projections=*/false,
    /*lower_sigma=*/1.5, /*upper_sigma=*/2.5, /*multiplicative=*/false);
```

This is the default X-11 extreme-value *weighting* stage: per-calendar-year
moving five-year dispersion windows over the current irregular-type sequence
(B13 semantics) and the two-pass sigma detection that produces observation
weights. Replacement values, correction factors and the B/C/D placement are
item 22b.3b work and are not produced or claimed here.

`series` is a validated `MaskedSeries` (finite storage everywhere, explicit
binary mask), `frequency` is 4 or 12, `start_period` is the 1-based calendar
period of the first row and must lie in `[1, frequency]`. Requires
`n >= frequency` and `upper_sigma > lower_sigma > 0.5`, matching the pinned
sigma-limit setter. Below one full calendar period the pinned reference has no
defined behavior (its first-year boundary walk runs off the array), and
undefined behavior is not a result to reproduce; the call is rejected. With
`exclude_projections` the dispersion span drops exactly `backcasts` leading
and `forecasts` trailing rows (`backcasts + forecasts <= n`); weights are
always reported for every row of the full series.

## Pinned oracle and exactness

The oracle is `DefaultExtremeValuesCorrector` from the pinned rjd3 3.9.0
artifacts — the class implementing the JDemetra+ default rules — driven
through its public `analyse`/`getObservationWeights` API via rJava, with its
pass primitives re-run for per-pass provenance
(`validation/reference/capture-x11-sigma.R`). The fixtures
(`validation/fixtures/native/x11-sigma`, 125 cases) store pass-1 and final
weights, pass-1 and final dispersion arrays, and pass counts through
17-significant-digit round-trip exact CSV files. The native legacy policy
reproduces every captured value with exact double equality: the implementation
performs the same IEEE-binary64 operations in the same order (left-fold
squared-deviation sums, `sqrt(sum/(length-missing))`, the ramp quotient), so no
tolerance is involved anywhere. A throwaway harness verified all 125 cases;
`test_x11_sigma.cpp` embeds six of them (spike, revival, near-zero,
projection-excluded, masked and multiplicative) plus closed-form controls.

## Qualified rules (both policies)

Window construction (span `[lo, hi)`, `f = frequency`, `p = (start+lo) mod f`,
`head = p ? f-p : 0`, complete years `c` after dropping partial head/tail
years):

1. `c < 5`: one global window over the whole span for every year.
2. Otherwise the centered window of calendar year `y` starts at span row
   `head + (y - y0) * f` and covers `5f` rows, for years
   `y0 = first_year+2+(head>0)` through `y1 = y0+c-5`; the
   leading window `[lo, lo+head+5f)` fills earlier years and the trailing
   window from the last centered start to `hi` fills later years; with
   `head = 0`/`tail = 0` the adjacent centered value is reused.
3. Dispersion is `sqrt(sum((x-center)^2)/(length - missing))`, `center` 1 for
   multiplicative and 0 for additive, masked rows skipped in numerator and
   denominator. A window with no usable row has unavailable (NaN) dispersion.

Detection iterates calendar-year blocks starting at `f - start` (0-based
`start`) and uses `window[min(year, last)]`:

- `X11SigmaPolicy::legacy` — the pinned default rules exactly: a year with
  `|dispersion| < 1e-5` keeps every weight at 1; comparisons use a `1e-15`
  absolute slack (`dev - upper > 1e-15` gives weight 0, `dev - lower > 1e-15`
  gives the ramp `(upper-dev)/(upper-lower)`, otherwise 1). The slack can make
  a ramp weight slightly negative; such a row reports state `zero`.
- `X11SigmaPolicy::accurate` — the scale-limit rule: no dispersion cutoff, no
  slack (`dev > upper` gives 0, `dev > lower` gives the same ramp), and a zero
  dispersion grants weight 1 only to exactly zero deviations and 0 to any
  nonzero deviation.

Both policies: unavailable (NaN) dispersion or a masked row keeps weight 1;
an infinite dispersion (accumulation beyond binary64) keeps weight 1 because
every finite deviation stays below the infinite thresholds — the pinned IEEE
accumulation is reproduced instead of range-rejected, per the
[extended-precision range contract](extended-precision-range-contract.md).
If any first-pass weight is `< 1`, a second pass removes rows with weight
exactly 0 (as missing), recomputes the windows and re-detects **on the
original values**; the fixture pass-2 arrays confirm this exclusion rule
bit-exactly. Year indexing of the window array is absolute over the full
series, including excluded-projection years, so detection flags the full
series even when the windows were trimmed.

## The reserved default decision

The maintainer decision recorded in the 22b.3a scope note — spike behavior
after a zero/tiny dispersion year versus the limiting formula — remains open
and **no default is promoted**: the policy is a required argument of
`x11_sigma_weights`. The captured evidence separates the two rules on exactly
seven of the 125 fixture cases, all tiny-scale families:

- `revive-*`: a spike is zeroed in pass 1; excluding it makes the year
  dispersion `< 1e-5`, so the legacy rule revives every weight to 1 while the
  accurate limit rule keeps the spike at 0.
- `tiny-*`/`zeroish-*`: the legacy cutoff is unit-dependent (`1e-5` on the
  values' own scale), so scaling the input changes the outcome; the accurate
  rule is scale-equivariant.

The same separation appears in the embedded cases
(`revive-4`/`zeroish-4`) where the test asserts the two policies differ, and
on every other embedded or fixture case they agree exactly.

## Determinism and interruption

The centered windows of one dispersion array are computed through
`parallel_for` with an internally serial accumulation order, so the serial and
OpenMP builds produce bit-identical weights, windows and states (asserted in
`test_x11_sigma`). The `Interrupt` callback is honored before window
construction, every 1024 detections and before returning. Results are reused,
never cached, by the core.
