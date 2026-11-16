# Complete fixed additive D pass (22b.4 S3)

```cpp
kalos::X11BPass b = kalos::x11_b_pass(series, 12, /*first_period=*/1,
    initial_filters, final_filters, /*trend_length=*/13, kalos::X11SigmaPolicy::legacy);
kalos::X11CPass cp = kalos::x11_c_pass(series, 12, 1,
    initial_filters, final_filters, 13, kalos::X11SigmaPolicy::legacy, b);
kalos::X11DPass pass = kalos::x11_d_pass(series, 12, 1,
    initial_filters, final_filters, 13, kalos::X11SigmaPolicy::legacy, cp);
```

This is the fixed additive D pass of the default X-11 decomposition
(item 22b.4, slice S3): all 16 public pinned `X11DStep` tables on their
pinned spans, the final seasonal decomposition (D11 is the seasonally
adjusted series over the reference series, D12 the final Henderson trend and
D13 = D11 - D12) and the D9 MSRE diagnostic table. It is composed from the
item 22b.4 S2 C pass exactly like the pinned `X11Kernel`:
`X11BStep.process(series, ctx)`, `X11CStep.process(series, B20, ctx, cv)`
followed by `X11DStep.process(series, C20, ctx, cvSeasonalFilter)`. The MSRE
filter selection, the automatic Henderson choice (the pinned `getICRatio`
scalar is selector machinery, inert in fixed mode), cross-validation, the
period-specific corrector (the pinned `D9g` is null in the default corrector
mode), multiplicative and non-seasonal modes and ARIMA-regression inputs are
not delivered or claimed.

## Public surface and validation

Arguments are validated exactly as in the B and C passes (`MaskedSeries::validate`,
frequency 4/12, 1-based `first_period`, fixed per-phase requests from
{0,1,3,5,9,15}, odd Henderson length >= 5 with `n >= trend_length` and
`n >= 3*(frequency/2) + frequency`, required sigma policy). The pinned
pipeline composes the D pass only onto the finished C pass of the same
context and series, and the API enforces that coupling: `c.status` must be
`"available"`, `c` must echo the same first period, seasonal requests and
Henderson length, `c.round.policy` must equal the required policy, `c.c20`
must cover the series and the supplied series must equal the series the C
pass consumed exactly (`c.reference_series`, NaN- and flag-aware). The D
pass itself performs no extreme-value analysis — its correction round is the
C-pass round carried by `c.c20` — so no sigma bounds are accepted here.
`backcast`/`forecast` are the pinned context horizons (non-negative, with at
least four cycles of original calendar left for the MSRE stride blocks). A
default-constructed or mismatched `c` is rejected rather than silently
recomposed.

## Pinned oracle and exactness

`validation/reference/capture-x11-dpass.R` drives the pinned rjd3 3.9.0
toolkit through the full X11Kernel composition over the 18 fixed-additive
B/C cases at zero horizon and 4 of them additionally hand-extended
(deterministic linear back/forecast rows, context horizons and shifted first
periods), capturing all 16 public D tables (24,800 rows) plus the D9 MSRE
table at 17 significant digits (`SHA256SUMS.csv` under
`validation/evidence/native/x11-dpass`). The capture re-derives every
additive identity from the captured input, the captured C20 and the captured
tables themselves, re-applies the pinned trend-cycle filter to D1 for the D2
interior, recomposes D7 and D12 through the pinned mmsre endpoint pipeline
and reproduces the MSRE diagnostic through the pinned
`X11Utility.defaultMsrTable` factory; for the zero-horizon cases the
consumed C13/C17/C20 must equal the installed x11-cpass fixture. Every
delivered table and diagnostic is bit-exact against the capture for all 22
cases in the Release OpenMP build, the true serial build and under
`KALOS_NUM_THREADS=4`.

## Qualified rules (pinned arithmetic)

The pass runs on the internal pinned-sequence machinery
(`kalos/detail/x11_pinned.hpp`) shared with the B and C passes; the
D-specific sequence reads off `X11DStep` directly:

* D1 = `ctx.remove(refSeries, C20)` — the original series corrected by the
  final C-round factors; D11/D8 subtract the final seasonal/trend from the
  original `refSeries`, D11bis/D9bis from the corrected series.
* D2 = `X11TrendCycleFilterFactory.makeTrendFilter(period)` over the central
  span only (the pinned table length is `n - 2*d2drop`, `d2drop = period/2`)
  and D4 = drop(D1) - D2.
* D5 = the `iStart = d2drop` normalization (extension back to the span) of
  the pinned seasonal processing of D4 with the INITIAL requests at
  `getPosition(d2drop)`; D6 = D1 - D5.
* D7 = the fixed Henderson filter with mmsre asymmetric endpoints over D6;
  D8 = refSeries - D7; D9bis = D1 - D7; the pinned `D9_g_bis` equals D9bis
  in the fixed corrector mode (`D9g` null, `d9filter` null, `d9msriter` 0).
* D9 = the pinned mask pair with `dcmpg` semantics: `|D9bis - D8| >= 1e-9`
  keeps D9bis, a NaN difference keeps D9bis, everything else is missing.
  The non-missing count equals the correction count of the consumed C round.
* The D-stage seasonal requests (`getSeasFilter`) equal the final requests:
  the MSRE selection engages only when every final option is `Msr`
  (`X11Context.isMSR`), so it never engages for the fixed requests.
* D10bis = the final requests over D9bis at `getPosition(0)`, D10 = the
  `iStart = 0` normalization, D11bis = D1 - D10, D12 = the final fixed
  Henderson with mmsre endpoints over D11bis, D11 = refSeries - D10,
  D13 = D11 - D12.
* The D9 MSRE diagnostic is `X11Utility.defaultMsrTable` over the DROPPED
  original calendar at `getPosition(backcast)`: per calendar phase the
  irregular evolution accumulates the stride differences of the residual
  `x - mean` (the additive mode takes the pinned plain-subtract branch — no
  +1 offset, that belongs to the other decomposition modes and would
  quantize tiny residuals around 1.0) scaled by the pinned `fis` lookup, the
  seasonal evolution accumulates the stride differences of the 7-MA
  filtered mean itself scaled by `cs`; the stride start is
  `(phase - position + period) % period` and the global ratio is
  `sum(ri*n)/sum(rs*n)`. The 7-MA is `X11FilterFactory.makeSymmetricFilter(7)`
  with the zero outer tap trimmed by `SymmetricFilter` (radius 3), padded on
  both sides by `FilteredMeanEndPoints` with the plain (not NaN-skipping)
  averages of the first/last stride-block values.

## Decomposition identities and calendars

The pinned `X11DStep` never extends the series internally: it computes on
the `refSeries` and the horizons its caller provides (the `X11Kernel` wires
the spec `backcastHorizon`/`forecastHorizon` into the context). The only
calendar-sensitive part of the D pass is the MSRE diagnostic, which drops
the horizon rows and indexes them at `getPosition(backcastHorizon)`; every
table is horizon-independent. Extended-calendar support therefore means
running the pass over the extended reference series with explicit
`backcast`/`forecast` arguments — the original calendar is the row span
`[backcast, n - forecast)` with first-period `(first_period - 1 - backcast)
% frequency + 1` — exactly like the pinned context. The four extended
fixture cases pin both calendars against the pinned kernel on the same
inputs, including the MSRE table computed on the dropped original calendar.
The fixed-mode probe found no pinned path where the D pass itself invents
horizon extensions, and none is delivered; the selector stages that would
drive them are item 22b.4 S4 scope.

## Determinism and interruption

Every stage runs through `parallel_for`; the output is bit-identical between
the serial build, `PERSEPHONE_NUM_THREADS=1` and `PERSEPHONE_NUM_THREADS=4`.
Checkpoints precede each pass phase and every `parallel_for` row; throwing
interrupts unwind at every boundary (qualified by a full boundary sweep in
`test_x11_d_pass.cpp`).

## Evidence

`tests/test_x11_d_pass.cpp` embeds five quarterly and one hand-extended
pinned cases verbatim (16 tables each, bit-exact, NaN-aware, the fixture
`D9_g_bis` rows checked against the delivered aliased member), re-checks the
decomposition identities D1=Y-C20, D4=crop(D1)-D2, D6=D1-D5, D8=Y-D7,
D9bis=D1-D7, the D9 mask, D11=Y-D10, D11bis=D1-D10 and D13=D11-D12 exactly,
the MSRE per-phase evolutions, ratios, counts and global ratio, the D9 count
against the consumed correction round, the C-pass coupling validation
(status, settings, policy, series equality, horizons), the extended-calendar
horizon selection, argument validation, serial/OpenMP bit equality and the
interruption boundary sweep; the full 22-case sweep is re-run against the
fixture through the native B+C+D composition. `ctest` totals: 42/42 Release
OpenMP, 42/42 true serial.
