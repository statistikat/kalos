# Restricted fixed-model X-13 fit (22c, M3-finale)

```cpp
kalos::X13FitRequest rq;
rq.input = series; rq.log = false; rq.frequency = 12; rq.first_period = 1;
rq.variables = spec;                       // the allocation vocabulary
rq.seasonal.assign(12, kalos::X11SeasonalRequest::s3x9);
rq.trend_length = 13;
rq.sigma = kalos::X11SigmaPolicy::legacy;  // required at this API; the
                                           // product default is legacy
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
extension route itself landed with item 21-H.4 on the AUTOMATIC driver
composition, where the landing model is estimated in-run (see
[x13-automatic-pipeline-contract.md](x13-automatic-pipeline-contract.md)
"Horizon extension" and [arima-forecasts-contract.md](arima-forecasts-contract.md));
the fixed fit keeps the refusal because its design can carry regression
variables whose extended columns are not representable at that seam. The
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

`test_x13_fit.cpp` (ctest `#48`) drives every fixed-route case of both
pinned corpora through `x13_fit` and checks per case: the `available`
status, the full provenance echo (request fields plus the requested and
forced modes), the 13 stored a-tables and `out`, the four results tables as
consumed, `c17`, the nine finals tables and the final-components triplet
identity (finalSA/finalTrend/finalIrregular are the stored
`finals.d11final/d12final/d13final`) — 30 checks per case per thread leg at
pinned thread settings 2, 1 and 8. Outside a log model every table except
the raw pass inputs is bit-exact (the pass runs on bit-identical input);
under a log model `out` itself carries `backTransform`'s exp — Java's
`Math.exp` and the native libm agree on every M3c allocation argument but
can flip a final ulp (the fit corpus shows one row of `f_log` at 1 ulp) —
so under a log model everything downstream of that exp is checked at the
transcendental boundary: a-buckets and `out` at 1e-15, the results tables
and `c17` at 1e-12, the finals at 1e-9 (the same policy the log-additive
results-builder stage already carries). The boundary table above, the throw
shapes and the `updateSpec` forcing of pseudo-additive-over-non-log run per
executable: 16 cases (7 allocation-seam + 9 fit-corpus fixed-route), 480
checks per leg, 1440 total; `tools/verify_x13_fit.cpp` sweeps both corpora
with per-table diff reporting (528 comparisons, worst boundary deviation
4.5e-16, identical at `KALOS_NUM_THREADS` 1/2/8). OMP, serial and
ASan/UBSan builds all pass 48/48.

Corpora: `validation/evidence/native/x13-allocation` (the M3c seam cases,
all four modes, quarterly and monthly, driven here with the uniform fixed
S3X9/Henderson-5/legacy/1.5–2.5 specification) and
`validation/evidence/native/x13-fit`, captured end to end through the
`x13_fast` entry point (`jdplus.x13.base.r.X13.process` —
`X13Kernel.of(spec, ctx).process(ts.cleanExtremities(), null)`, exactly the
pinned R fast path) on fully fixed monthly specifications: 11 cases — 9
fixed-route (all four forced modes, including the forced `Multiplicative`
from a non-log `Multiplicative` request and the log-model forcing; fixed
seasonal requests Stable/S3X1/S3X9/S3X15, Henderson lengths 5 and 13, sigma
cuttings 1.5/2.5 and 2/3; models from empty through TD7-only and user-only
to the full 11-variable TD7 + Easter + AO/LS + five-bucket user mix) and 2
nonzero-horizon evidence cases. The capture-side identity guards of
`X13FitProbe.java` recompose all 22 stored tables per fixed case from the
pinned pieces at bit precision (198 checks, refusal on any disagreement,
on any free coefficient — the estimation cursor is never read — and on any
fixed-path/horizon-domain violation) and the runs are byte-identical across
reruns. The `f_empty` case is additionally re-executed through the R
`x13_fast` result path (R ts marshalling, the pinned `process` call and the
proto result transform) and compared bit-exactly against the probe tables
(`x13fast.csv`, 14 tables), proving the R entry point and the probe driver
see the same fixed run. The evidence cases (`f_h6`, `f_b3h3`) execute the
same fixed specs with nonzero horizons and record only the deterministic
route facts (`horizons.csv`: `a1a_len`/`a1b_len` equal the requested
horizons, `out_len = n + backcast + forecast`, the split finals present,
`d11final` stays on the input calendar) — the pinned engine leaves the
fixed path through `ExactArimaForecasts` for every nonzero horizon, exactly
where the fit answers `unsupported:x13 fit requires zero horizons`. See
[x13-allocation-scope-22c.md](x13-allocation-scope-22c.md) for what later
slices must pin.

## Integration notes

**Per-corpus comparison policy for the shared `tables.csv` `out` column.**
Both corpora carry an `out` column in `tables.csv`, but they pin different
expectations, and the difference is real, not a tolerance accident: in the
allocation-seam corpus the captured `out` is pinned **bit-exact in every
mode** (its `backTransform(v26, true)` arguments happen to contain no
libm-flip points, so exact equality is a qualified fact of that corpus),
while in the fit corpus `out` is pinned at the transcendental boundary
under a log model — the `f_log` case has one row where the native `exp`
and Java's `Math.exp` genuinely disagree by 1 ulp, and the captured value
is what the pinned kernel executed. Consequences for integrators: reuse
the seam corpus's exact `out` assertions for non-log fits in any corpus,
and under a log model judge `out` (and everything downstream of it —
`d10..d13`, `c17`, the finals) at the corpus's own documented boundary
(1e-15 relative for `out`), never by exact equality; extending the seam
corpus's bit-exact expectation to the fit corpus produces a surprise
1-ulp failure on `f_log`, and the boundary policy is sound on both
corpora.

**Proposed regression-side fingerprint for `X13FitResult`.** The result
echoes the complete X-11 spec but consumes the regression side
(`variables`) without an echo, so after the fact there is no way to audit
which variable set a fit actually used. The agreed direction (documented
here; **implementation deferred** to a coordinated slice, since adding
members to `X13FitResult` is a struct growth = schema change for
downstream result views): a cheap, deterministic, order-sensitive summary
in the provenance — the variable count, per variable the
(class, effect, coefficient count, column length), and one FNV-1a 64-bit
checksum over the IEEE-754 bit patterns of every coefficient and every
column value in pinned consumption order (variables in model order,
coefficient *j* before its column, coefficients/columns ascending). No
table contents, so the result stays auditable without carrying the
design; the checksum exists to detect substituted or truncated variable
sets at downstream pinning, not to identify them.
