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
observations). Native replay is the next implementation gate. Nonzero additive
effects, TC/SO, calendar and missing-data surfaces have no witnesses in this
corpus. The SEATS extraction can be tested separately. Restoring deterministic
effects to the final components via `TwoStepsDecomposition.merge` and the
23-G/24-E end-to-end oracles remain separate gates.
