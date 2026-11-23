# TRAMO fitted-model bridge reference (24-D)

The gate observes the actual `TramoSeatsKernel.of(RegSarimaModel)` result from
the pinned JDemetra 3.9.0 jars. The probe invokes that private method on the
result of a public `TramoKernel.of(template, context).process(series, log)`
call. The five monthly series and their dates come from the frozen 23-F
`tramo-fit/cases.txt`. The probe uses the standard templates listed below.

| 23-F series | Template | Log | Mean | Selected regression effects | Changed observations |
| --- | --- | --- | --- | --- | ---: |
| fit_tr0_air | TR0 | no | yes | none | 0/144 |
| fit_tr3_air | TR3 | yes | no | none | 0/144 |
| fit_tr1_ao | TR1 | yes | yes | AO to irregular | 1/96 |
| fit_ls | TR3 | yes | yes | LS to trend | 60/96 |
| fit_multi | TR3 | yes | no | AO and two LS | 70/96 |

## Replay and evidence

`reference.json` pins the six jars and the source cases by SHA-256. The
capture was produced on macOS ARM64 with Temurin 21.0.11+10 and the HotSpot
exp/log intrinsics disabled. From the Kalos repository root, run:

```sh
python3 validation/reference/capture-tramo-seats-bridge.py \
  --jdk "$JAVA_HOME" \
  --jar-dir "$(Rscript -e 'cat(system.file("java", package="rjd3jars"))')" \
  --jar-dir "$(Rscript -e 'cat(system.file("java", package="rjd3tramoseats"))')"
```

The runner compiles the probe, runs it in two fresh JVMs, and verifies both
outputs against the frozen file. It takes local jars and makes no network
requests. Binary64 numbers are 16 hexadecimal digits encoding their raw bits;
series rows give the first year, one-based month, length, then those bits.
[Bridge facts](../../validation/evidence/native/tramo-seats-bridge/bridge.txt)
· [Reference identity](../../validation/evidence/native/tramo-seats-bridge/reference.json)

## Observed mapping

- The bridge takes fitted d/bd and copies each phi, bphi, theta and btheta
  value into a fixed parameter. Log and mean-correction flags follow the fit.
- It transmits the likelihood statistic `ssqErr / (effectiveObservationsCount
  - estimatedParametersCount)`. Each captured variance matches bit-for-bit.
- It selects variables whose regression effect differs from `Undefined`.
  In this corpus the mean is excluded, the AO is irregular, and the LS is
  trend. Each selected variable has its own contribution row. The bridge
  back-transforms the summed effect with length-of-period correction enabled,
  then divides the original-scale interpolated series on log models or
  subtracts the effect on level models. The capture contains the fitted
  coefficients, each selected contribution, the combined effect, its back
  transformation, and the independently observed SEATS input.

The scope is the reference handoff for these five fitted models (576 monthly
observations). The native replay enters through `make_tramo_seats_model(fit)`
(`kalos/tramo_seats_bridge.hpp`). It uses the available 23-F result on its
complete estimation calendar, the AO/zero-ended LS factories, and the pinned
exponential. Malformed or unsupported fits throw `std::invalid_argument`.
The fixed request can be passed directly to `seats_extract_wk`. Nonzero additive
effects, TC/SO, calendar and missing-data surfaces have no witnesses in this
corpus. The merged product surface and its qualification are described below.

## Deterministic-effects merge gate

Replay the capture command above with `--stage merge`. It drives the actual
`SeatsKernel.process` on each observed fitted-model bridge, then supplies its
bias-corrected components to `TwoStepsDecomposition.merge`. The evidence pairs
each dated WK row with the observed product row, including conditional
standard deviations on the input side. The merge emits value, forecast and
backcast series for the original, trend, seasonally adjusted, seasonal and
irregular components. The mean is outside the assigned effects. AO belongs
to the irregular, and zero-ended LS belongs to trend.
[Merge facts](../../validation/evidence/native/tramo-seats-bridge/merge.txt)

Five default horizon cases (0 backcasts, 24 forecasts) plus a three-backcast,
six-forecast run on the mixed AO/LS model give 65 product rows and 4,005
binary64 observations. The exit gate requires their identities, calendars,
lengths and values to match. On the mixed case the JVM changed the model
during SEATS approximation. Its frozen WK result qualifies the subsequent
merge independently. D6 requires the native extraction route to refuse that
fit. The AO-only case cuts off a parameter and stays in the admitted envelope.

The fitted bridge bytes reproduce unchanged on ARM64 and x86-64 Temurin
21.0.11. Two of the valid fitted models expose architecture differences in
the JVM's WK numerics. Replaying the *identical* fixed inputs on the two JVMs
changed 29 of 40 initial/final rows for TR3 AirPassengers and 30 of 40 for
the LS case. The other two admitted models matched on both. Kalos's existing
host-lineage math targets the pinned x86 JVM. The runner checks the total
capture against the executing JVM's profile. The native composition gate uses
the x86-64 profile on every host. The isolated merge gate uses both.
[x86-64 paired reference](../../validation/evidence/native/tramo-seats-bridge/merge-x86_64.txt)

## Native fitted-model composition

`kalos/tramo_seats_pipeline.hpp` exposes two entry points. Given a fitted
23-F result and the bias-corrected WK components, `merge_tramo_seats_components`
restores the original observed series and applies the fitted AO and LS
effects on the observed and extended domains. It requires the five
component rows on each nonempty interval, checks dates and lengths and
skips zero-length extensions. The observed WK series must equal the bridge
input. The 65 merged rows agree bit for bit on both pinned JVM profiles.

`run_tramo_seats_pipeline(fit, backcast_count, forecast_count)` builds the
fixed handoff, extracts WK components and merges the product. Its extraction
retains the dated initial and bias-corrected rows and standard deviations.
Its finals contain estimates for value, forecast and backcast intervals.
The default is zero backcasts and two years of forecasts. A WK error status
produces empty finals. The mixed AO/LS model requires approximation and
raises the established D6 refusal. Four decomposable fitted models reproduce
the complete x86-64 reference extraction-final and product rows at identical
fitted inputs. Running a raw series and specification without captured fit
witnesses and qualifying the mandatory rsa3 and rsafull end-to-end oracles
remain 23-G/24-E work.

## Original monthly series through RSA0

`estimate_tramo_tr0(series, start_year, start_month)` constructs the TR0
monthly model and estimates the two airline MA parameters and the mean
correction. TR0's fully specified specification skips automatic model
identification. The response must be finite and complete and the fitted
model must leave positive residual degrees of freedom. The result contains
the fixed SEATS request, mean coefficient, log likelihood, residual SSQ,
effective observations and estimated parameter count.

`run_tramo_seats_rsa0(series, start_year, start_month)` exposes this fitted
projection together with the monthly extraction and restored final components.
Backcast and forecast counts are optional, with the same SEATS defaults and
D6 decision as the fitted-model pipeline. At the default horizon, six complete
series matched the public JVM's TR0 estimation and RSA0 finals, including all
3,960 component values. The fully specified TR0 branch requires no estimator
records, selected orders or coefficient inputs from a caller. RSA3's automatic
driver and the TRfull calendar stages still have separate qualification gates.
[Raw-input reference panel](../../validation/evidence/native/tramo-seats-bridge/rsa0-x86_64.txt)
