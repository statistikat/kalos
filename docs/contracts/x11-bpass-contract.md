# Complete fixed additive B pass (22b.4 S1)

```cpp
kalos::X11BPass pass = kalos::x11_b_pass(
    series, 12, /*first_period=*/1, initial_filters, final_filters,
    /*trend_length=*/13, kalos::X11SigmaPolicy::legacy);
```

This is the fixed additive B pass of the default X-11 decomposition
(item 22b.4, slice S1): all public pinned `X11BStep` tables B1..B20 on their
pinned spans, the three extreme-value correction rounds with their detection
series and analysis positions (correction history), and the fixed per-phase
seasonal requests and Henderson length used. The C/D passes, automatic filter
choice and cross-validation, multiplicative and non-seasonal modes,
ARIMA-regression inputs and the pinned mean-endpoint fallback for series
shorter than the trend filter are not delivered or claimed.

## Public surface and validation

`series` is validated first (`MaskedSeries::validate`). `frequency` is 4 or
12 and `first_period` is the 1-based calendar position of the first row,
1..frequency. `initial_filters` and `final_filters` carry exactly one fixed
request per calendar period — 0 (stable) or 1, 3, 5, 9, 15 (3xN) — matching
the pinned per-phase `SeasonalFilterOption` arrays; no request is resolved or
substituted. `trend_length` is an odd Henderson length >= 5 with
`n >= trend_length` (below that the pinned `AsymmetricEndPoints` replaces the
endpoint region with a mean fallback that is not delivered) and
`n >= 3*(frequency/2) + frequency`, which keeps every normalization span a
full cycle plus the annual radius. The sigma policy is a required argument
and nothing is promoted; the fixture was qualified with
`X11SigmaPolicy::legacy` and the bounds default to the pinned 1.5/2.5.

## Pinned oracle and exactness

`validation/reference/capture-x11-bpass.R` drives the pinned rjd3 3.9.0
toolkit (JVM 21, `X11BStep.process`, additive mode, fixed option arrays,
zero horizons) over 18 monthly and quarterly cases and captures all 20
public tables (24,352 rows, counting the B9d round-2 detection
intermediate),
the per-case metadata and the replacement counts of the three rounds at
17 significant digits (`SHA256SUMS.csv` in `validation/fixtures/native/
x11-bpass`). Every delivered table is bit-exact against the capture for all
18 cases in the Release OpenMP build, the true serial build and under
`KALOS_NUM_THREADS=4`, with NaN matched to NaN; the capture-side identity
assertions re-verify the pinned getters against each other.

## Qualified rules (pinned arithmetic)

The B pass reproduces the pinned arithmetic row for row. Where the qualified
primitives of items 22a/22b.2 deliberately document independent conventions
(seasonal endpoint and mean handling), the pinned-order computation lives in
the internal `kalos/detail/x11_pinned.hpp` machinery shared with the C pass
(item 22b.4 S2) instead of calling them:

* Seasonal processing per phase (`X11SeasonalFilterProcessor.process`):
  `StableFilter` (finite-mean fill) when the request is stable, the phase
  span has fewer than five complete cycles, or a 3x15 span covers fewer than
  twenty calendar years; otherwise `DefaultFilter` zeros plus symmetric
  interior and `AsymmetricEndPoints` with the pinned classic decimal endpoint
  tables (FC1/FC3/FC5/FC9/FC15). The normalization subtraction restores the
  pinned stable/short-span phase set after the central band and periodic
  endpoint copy.
* Fixed trend filter (`X11Context.trendFilter`): the weights are
  `DiscreteKernel.henderson(p)` passed through `LocalPolynomialFilters.of2_3`
  in pinned double arithmetic — the classic closed form differs by up to 16
  ULP — applied as the `IFiniteFilter` slow-path ascending anchored dot.
* Trend endpoints (`X11Context.asymmetricTrendFilters` with the
  `MusgraveFilterFactory.findR` ratio): the pinned filter is
  `AsymmetricFiltersFactory.mmsreFilter2(tf, side, 0, {2/(sqrt(PI)*ic)}, null)`,
  not the classic Musgrave weights (the qualified `x11_musgrave_*` primitive
  documents that independent rule and is deliberately not used here). With
  `dzlen = 1` the mmsre system reduces to a closed scalar pipeline; the b21
  moment is `dot(z12, x*1)` sign-flipped plus the whole `dot(z13, w_tail)`
  added once (`DataBlock.chs`/`addProduct` order — interleaved accumulation
  differs by 1–2 ULP). `AsymmetricEndPoints` consumes the array at index
  `p-1-side` per side.
* The three correction rounds run the qualified items 22b.3a/3b primitives
  on the pinned detection series (B4d, B8−B9c, B13) with the pinned analysis
  positions: `(first_period-1+frequency/2) mod frequency` for the first round
  and `first_period-1` for the last two. `B17`/`B20` are exactly the final
  round's weights and correction factors; unit-weight rows carry zero
  factors.

## Table mapping and validation order

The 20 *public* pinned tables B1..B20 are not 20 struct members: `X11BPass`
holds 19 `MaskedSeries` and **B9d is not one of them**. It is the second
round's detection series — `rounds[1].detection` — because `rounds` is
pinned in the order of its detection series (B4d, B9d, B13) and round 2's
detection is never exported as a table of its own. A downstream public view
therefore exposes 19 member tables plus the three rounds (detection,
analysis position, policy, sigma weights, replacements with their
correction factors and replacement counts), which together carry all 20
pinned tables without duplication.

Read the round linkage in both directions: the final round's sigma weights
are bit-identical to **B17** and its `replacements.correction_factors` are
bit-identical to **B20**, so those two public tables are duplicates of
round-3 content; and **B20 holds correction *factors*, not replacements** —
the replacement values themselves exist only in the round history
(`rounds[i].replacements`). Reading B20 as a replacement series yields zeros
on unflagged rows instead of the replaced observations.

Validation is ordered and the order is observable in the messages. The trend
gate `series.values.size() >= trend_length` ("B pass requires at least the
trend filter length") is checked *before* the normalization floor
`n >= 3*(frequency/2) + frequency` ("B pass requires three annual radii plus
one cycle"), so for quarterly requests the floor's own threshold (n >= 10)
is **shadowed** by the trend gate whenever `trend_length > 10` — including
this fixture family's default of 13. The radii message is therefore only
reachable for quarterly input with `trend_length <= 9`; monthly (n >= 30
against a minimum odd length of 5) surfaces it naturally. Downstream
error-expectation tables should be written against the ordering, not against
the arithmetic of either bound.

## Determinism and interruption

Every stage runs through `parallel_for`; the output is bit-identical between
the serial build, `PERSEPHONE_NUM_THREADS=1` and `PERSEPHONE_NUM_THREADS=4`.
Checkpoints precede each pass phase and each `parallel_for` row, so throwing
interrupts unwind at every boundary (qualified by a full boundary sweep on a
pinned case in `test_x11_b_pass.cpp`).

## Evidence

`tests/test_x11_b_pass.cpp` embeds five quarterly pinned cases verbatim
(20 tables each, bit-exact, NaN-aware), re-checks the additive composition
identities B8=B1-B7, B13=B11-B7, B6=B1-B5, B11=B1-B10, B4d=B3-B4anorm and
B3=crop(B1)-B2 exactly, the correction-history linkage and counts, the
argument validation, serial/OpenMP bit equality and the interruption
boundary sweep; the full 18-case sweep is re-run against the fixture with
the capture. `ctest` totals: 41/41 Release OpenMP, 41/41 true serial.
