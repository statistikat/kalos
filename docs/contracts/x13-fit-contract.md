# Restricted fixed-model X-13 fit (22c, M3-finale)

```cpp
kalos::X13FitRequest rq;
rq.input = series; rq.log = false; rq.frequency = 12; rq.first_period = 1;
rq.variables = spec;                       // the allocation vocabulary
rq.seasonal.assign(12, kalos::X11SeasonalRequest::s3x9);
rq.trend_length = 13;
rq.sigma = kalos::X11SigmaPolicy::legacy;  // required — no default
rq.mode = kalos::X11DecompositionMode::additive;
kalos::X13FitResult fit = kalos::x13_fit(rq);
// fit.finalSA / finalTrend / finalIrregular, fit.finals.*, fit.preadjustment,
// fit.selection (full b/c/d accessibility), fit.d10..d13/c17 (as consumed).
```

This slice delivers the **restricted fixed-model X-13 fit**: one entry point
composing the qualified pieces in the pinned `X13Kernel.process` order —
the fixed-coefficient `initialStep` seam (item 22c M3c,
[x13-allocation-contract.md](x13-allocation-contract.md)), the X-11 run over
`out` with the `updateSpec`-forced mode (items 22b.4 S4 and 22b.5 M3a/
M3b-i/M3b-ii), the `X11Results` tables exactly as `finals` consumes them,
and the `finals` seam (M3c). The fit adds exactly three things of its own,
each independently pinned: the mode-forcing echo, the log-additive
results-builder stage, and the final-components triplet.

## What the fit composes

* `x13_preadjust` over `(input, variables, log)` — the stored `a`-tables and
  the preadjusted `out`.
* `x11_selection_pass` over `out` with the requested fixed seasonal filters,
  fixed Henderson length, required sigma policy, sigma cuttings, MSR
  iteration policy (inert while every request is fixed) and horizons `0/0`,
  under the **forced** mode: non-log → additive; log → multiplicative
  unless `log_additive`/`pseudo_additive` was requested (the pinned
  `updateSpec` rule, echoed as `mode_effective`).
* Under `log_additive` only, the pinned `X11Kernel` results-builder stage
  (`finalResults` → `ratioBiasCorrection`, the `X11Spec` default bias):
  with `A=exp(d10step)`, `B=exp(d12step)`, `C=exp(d13step)` and
  `v9=average(A)`, `v13=v9*average(C)` over the whole run calendar, the
  stored tables are `D10=A/v9`, `D12=makePositivity(B·v13)` (the identity —
  every factor here is a strictly positive `exp`), `D11=out/D10`,
  `D13=(out/D10)/D12`. This delivers the stage as fit composition for the
  `Ratio` default; the other `BiasCorrection` options stay outside
  (scope memo item 6). Outside log-additive the results tables are the raw
  step tables (bit-exact, as pinned by M3c).
* `x13_restore` over those tables — the pinned finals
  `d11final/d12final/d13final/d16/d18/e1/e2/e3/e11` with the
  op/invOp/mean truth table and the `correctSeries` rule.

## Final components

The pinned engine stores no separate final-trend or final-irregular table:
the final SI decomposition **is** the `X13Finals` triplet, exported through
the `x13_fast` serialization as `D11final/D12final/D13final`
(`jdplus.x13.base.core.x13.X13Finals`; no `finalTrend`/`finalIrregular`
symbol exists anywhere in the pinned jars). The fit exposes them as
`finalSA`, `finalTrend`, `finalIrregular`, bit-identical to
`finals.d11final/d12final/d13final`.

## Forecast-domain finding: the fixed path supports {0, 0} only

`X13Kernel.process` takes the horizons from the X-11 spec (negative values
become `|h| * annual frequency`) and passes them to `initialStep`;
`updateSpec` rewrites the spec with the same absolute values before the
X-11 run. Inside `initialStep` the extension branch engages **iff**
`back > 0 || forecast > 0` and runs the ARIMA machinery:
`r = linv − v27`, `ExactArimaForecasts` back/forecasts (a1b/a1a), the
extended `out = concat(a1b, inv, a1a) ⊖ v26` calendar and the split finals
`d11a/d12a/d16a/d18a` and `d11b/...`. There is no fixed-coefficient route
to a nonzero horizon: the pinned fixed-path horizon domain is exactly
`{backcast 0, forecast 0}`, and the fit fails closed on every other
request (`unsupported:x13 fit requires zero horizons`) — including negative
values, which for the pinned kernel are not "auto" but `|h| · freq`. The
capture corpus pins this boundary by executing the pinned engine on the
same fixed specification with nonzero horizons and recording the route
taken (`horizons.csv`, see the qualification section).

## Fail-closed boundary

The fit answers an explicit status (never throws) for requests outside the
represented domain, with the full provenance echo still filled:

| request | status |
|---|---|
| `x11default` or `msr` seasonal request | `unsupported:x13 fit fixed seasonal requests only` |
| `trend_length == 0` (automatic Henderson) | `unsupported:x13 fit fixed Henderson trend length` |
| `backcast != 0 \|\| forecast != 0` | `unsupported:x13 fit requires zero horizons` |
| sigma policy unset | `unsupported:x13 fit requires an explicit sigma policy` |
| selection run refusal | the pass's verbatim `unsupported:<stage>` |

Shape violations throw `std::invalid_argument` exactly as the seams below
the fit do: seasonal requests not matching the frequency, missing or
non-finite observations, non-positive input under a log model, ragged
columns, non-finite coefficients, an even or too-short Henderson length, a
calendar too short for the pinned passes. Estimation (free coefficients,
outlier detection, automatic model selection), missing-row interpolation,
length-of-period transforms, `arimadiffreg`/`bty` variables, quarterly and
other frequencies beyond the qualified selection passes, benchmarking and
diagnostics all stay outside (scope memo).

## Qualification

`test_x13_fit.cpp` (ctest `#48`) drives every case of the pinned corpus
through `x13_fit` and checks per case: the `available` status, the full
provenance echo (request fields plus the requested and forced modes), the
13 stored a-tables and `out` (bit-exact; the exp'd log-model buckets at
1e-15), the four results tables as consumed (bit-exact outside
log-additive, across the results-builder boundary at 1e-12 inside), `c17`
(bit-exact in every mode), the nine finals tables (bit-exact outside
log-additive, at 1e-9 inside) and the final-components triplet identity —
30 checks per case per thread leg at pinned thread settings 2, 1 and 8.
The boundary table above, the throw shapes and the `updateSpec` forcing of
pseudo-additive-over-non-log run per executable.

Fixtures: `validation/fixtures/native/x13-allocation` (the M3c seam corpus,
all four modes, quarterly and monthly) plus
`validation/fixtures/native/x13-fit`, captured end to end through the
`x13_fast` entry point (`jdplus.x13.base.r.X13.process` —
`X13Kernel.of(spec, ctx).process(ts.cleanExtremities(), null)`, exactly the
pinned R fast path) on fully fixed monthly specifications, with the
capture-side identity guards of `X13FitProbe.java` (all 27 stored tables
recomposed from the pinned pieces at bit precision, refusal on any
disagreement, fixed-coefficient and zero-horizon domain assertions, plus
the nonzero-horizon route evidence). See
[x13-allocation-scope-22c.md](x13-allocation-scope-22c.md) for what later
slices must pin.
