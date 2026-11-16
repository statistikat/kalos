# Complete fixed additive C pass (22b.4 S2)

```cpp
kalos::X11BPass b = kalos::x11_b_pass(series, 12, /*first_period=*/1,
    initial_filters, final_filters, /*trend_length=*/13, kalos::X11SigmaPolicy::legacy);
kalos::X11CPass pass = kalos::x11_c_pass(series, 12, 1,
    initial_filters, final_filters, 13, kalos::X11SigmaPolicy::legacy, b);
```

This is the fixed additive C pass of the default X-11 decomposition
(item 22b.4, slice S2): all public pinned `X11CStep` tables C1..C20 on their
pinned spans plus the final extreme-value analysis on C13 (correction
history). It is composed from the item 22b.4 S1 B pass exactly like the
pinned `X11Kernel`: `X11BStep.process(series, ctx)` followed by
`X11CStep.process(series, bstep.getB20(), ctx, cvFilters)`. The D pass,
automatic filter choice and cross-validation, multiplicative and
non-seasonal modes and ARIMA-regression inputs are not delivered or claimed.

## Public surface and validation

Arguments are validated exactly as in the B pass (`MaskedSeries::validate`,
frequency 4/12, 1-based `first_period`, fixed per-phase requests from
{0,1,3,5,9,15}, odd Henderson length >= 5 with `n >= trend_length` and
`n >= 3*(frequency/2) + frequency`, required sigma policy, bounds defaulting
to the pinned 1.5/2.5). The pinned pipeline composes the C pass only onto the
finished B pass of the same context and series, and the API enforces that
coupling: `b.status` must be `"available"`, `b` must echo the same first
period, seasonal requests and Henderson length, every B round must carry the
same policy, `b.b20` must cover the series and `b.b1` must equal the supplied
series exactly (NaN-aware). A default-constructed or mismatched `b` is
rejected rather than silently recomposed.

## Pinned oracle and exactness

`validation/reference/capture-x11-cpass.R` drives the pinned rjd3 3.9.0
toolkit through the X11Kernel composition over the same 18 fixed-additive
cases as the B fixture and captures all 14 public C tables (17,320 rows)
plus `getC2drop` at 17 significant digits (`SHA256SUMS.csv` under
`validation/evidence/native/x11-cpass`). The capture re-derives the
composition identities from the captured input, the captured B20 and the
captured tables themselves, and re-applies the pinned trend-cycle filter to
C1 as an independent check of the C2 interior. Every delivered table is
bit-exact against the capture for all 18 cases in the Release OpenMP build,
the true serial build and under `KALOS_NUM_THREADS=4`.

## Qualified rules (pinned arithmetic)

The pass runs on the internal pinned-sequence machinery
(`kalos/detail/x11_pinned.hpp`) shared with the B pass; the C-specific
sequence reads off `X11CStep` directly:

* C1 = `ctx.remove(refSeries, B20)` — the original series corrected by the
  final B-round factors; the C tables therefore live on the corrected series,
  and C11/C13 subtract from the original `refSeries`, not from C1.
* C2 = `X11TrendCycleFilterFactory.makeTrendFilter(period)` over the central
  span only (the pinned table length is `n - 2*c2drop`; the filter is the
  same annual trend-cycle filter qualified for B2 in item 22a) and
  C4 = drop(C1) - C2.
* C5a = the pinned seasonal processing of C4 with the initial requests at
  `getPosition(c2drop)`; C5 = `DefaultSeasonalNormalizer.normalize(c5a,
  c2drop, ctx)` — the `iStart` variant that extends the normalized sequence
  back to the full span through `CopyPeriodicEndPoints`; C6 = C1 - C5.
* C7 = the fixed Henderson filter with mmsre asymmetric endpoints over C6 —
  the same qualified pipeline as B7; C9 = C1 - C7.
* C10a = the final requests over C9 at `getPosition(0)`, C10 = the
  `iStart = 0` normalization (no extension), C11 = refSeries - C10,
  C13 = C11 - C7.
* The final round is the pinned `IExtremeValuesCorrector.analyse(c13,
  getPosition(0), ctx)` — qualified items 22b.3a/3b with the explicit policy
  and the original-calendar analysis position; C17 is its observation
  weights and C20 its correction factors (unit-weight rows carry zero
  factors). The fixed requests keep cross-validation and the automatic
  Henderson selection disengaged exactly as in the B pass, so no CV table is
  produced or claimed.

## Fixture and composition-schema notes

The C fixture's `inputs.csv` is **bit-identical to the B fixture's** (the
same 18 series and case ids, verified with `cmp`) — the capture reuses the
B-pass case list — so C cases replay directly on the B fixture's case
loader and no second input loader is warranted. (The D fixture's
`inputs.csv` genuinely differs: its 22 cases include hand-extended
calendars.)

The `first_period` and `reference_series` members were added to
`X11CPass` when slice S3 introduced the D-pass coupling that consumes them.
Treat that growth as what it was: **the pass structs' echo members are part
of the composition contract**, so any struct growth is a schema change for
downstream result views (an R result shape, a serialization), not merely a
new getter. The same rule reads back over the B pass: `first_period` and
`mode` are echo members for exactly this reason — the C and D passes take
no first-period or mode argument and inherit both solely through the
finished companion, so a faithful downstream rebuild of a companion must
carry the echoes.

## Determinism and interruption

Every stage runs through `parallel_for`; the output is bit-identical between
the serial build, `PERSEPHONE_NUM_THREADS=1` and `PERSEPHONE_NUM_THREADS=4`.
Checkpoints precede each pass phase and every `parallel_for` row; throwing
interrupts unwind at every boundary (qualified by a full boundary sweep in
`test_x11_c_pass.cpp`).

## Evidence

`tests/test_x11_c_pass.cpp` embeds five quarterly pinned cases verbatim
(14 tables each, bit-exact, NaN-aware), re-checks the composition identities
C1=Y-B20, C4=crop(C1)-C2, C6=C1-C5, C9=C1-C7, C11=Y-C10 and C13=C11-C7
exactly, the correction-history linkage and counts, the B-pass coupling
validation (status, settings, policy, series equality), argument validation,
default-versus-explicit bounds, serial/OpenMP bit equality and the
interruption boundary sweep; the full 18-case sweep is re-run against the
fixture through the native B+C composition. `ctest` totals: 41/41 Release
OpenMP, 41/41 true serial.
