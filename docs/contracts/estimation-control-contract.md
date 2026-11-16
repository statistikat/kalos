# Fixed-order estimation control (21-A.1)

`estimate_sarima_fixed()` is the estimation-**control** layer of the pinned
`jdplus RegSarimaComputer` composed onto the qualified fixed-order
[objective](arma-objective-contract.md) and [optimizer](arma-optimizer-contract.md)
kernels. It decides *where an estimation starts, which restarts the pinned
machine attempts, and which candidate is retained*; it is not automatic model
selection, an ARIMA order search, a RegARIMA fit, or full JD+ estimator
qualification. There is no Rcpp boundary yet; the unit is C++-only.

```cpp
kalos::SarimaEstimationRequest r;              // series, design, blocks,
r.control = {1e-7, 1e-4, true, false, true,    // orders, period, control,
             true, kalos::EstimationStartingPoint::Zero};
auto fit = kalos::estimate_sarima_fixed(r);    // passes + retained fit
```

## Pinned control fields

Every `EstimationControl` field is required request data: the struct has no
default-constructed form and the core applies no implicit value. The pinned
builder defaults (`eps = 1.0E-7`, `feps = 1.0E-4`, `ml = true`, `mt = false`,
`cdf = true`, `fast = true`, starting point `HannanRissanen`, default
Levenberg-Marquardt minimizer) are values the *caller* passes, mirroring the
pinned call sites; promotion of any of them into configuration is held by the
maintainer decision list of [item21-roadmap.md](../item21-roadmap.md)
(items 1–2, stage E).

Numerically active in this unit:

* `eps` — replaces `optimizer.function_tolerance` for every pass (the pinned
  minimizer receives `eps` as its function precision).
* `starting_point` — the Zero/Default policy below.

Recorded but inert at this seam, accepted at either value, because the
qualified Kalos solver owns the corresponding mechanism:

* `feps` — the pinned internal precision of the Multiple-start passes
  (`feps` is only read on paths that start from HannanRissanen/Multiple);
  validated positive and finite.
* `unit_root_search` (`mt`) — a stored, never-read field of the pinned
  `RegArmaProcessor` (bytecode-verified: written by the constructor, no
  `getfield` anywhere).
* `corrected_degrees_of_freedom` (`cdf`) and `fast_derivatives` (`fast`,
  set by the negated `computeExactFinalDerivatives`) — they select the source
  of the pinned final `Point` score/information, which this unit does not
  expose.
* `maximum_likelihood` — the objective form. Only `true` is qualified by the
  existing kernel; `false` is an explicit `invalid_argument`, never a silent
  approximation.

## Starting points and sign convention

`HannanRissanen` and `Multiple` are explicit `invalid_argument` rejections
until item 21-A.2 qualifies the Hannan-Rissanen initializer — not silent
fallbacks to a supported start. The two delivered starts transcribe the
pinned `SarimaModel.Builder.setDefault` fills on the sign convention (probe-
verified against the pinned jars: rjd3 stores the raw coefficients of
`1 + phi(B)`, Kalos uses `1 - phi(B)`; MA is `1 + theta(B)` on both sides):

| pinned fill (raw) | Kalos free-coefficient fill |
| --- | --- |
| `phi = bphi = -0.1` (Default) | `ar = sar = +0.1` |
| `th = bth = -0.2` (Default) | `ma = sma = -0.2` |
| all zero (`setZero`, Zero) | all zero |

The policy overwrites every **free** entry before the first optimization, so
a free block's `initial` never acts as a start; it only templates the map and
must keep it constructible. **Fixed** entries are preserved exactly in every
pass and in the result (the pinned `SarimaFixedMapping` seam). Mean and drift
enter only as explicit regression columns per the
[regression-likelihood contract](regression-likelihood-contract.md); no
implicit mean exists here (the pinned `meanCorrection(true)` constant column
is not reproduced).

## Passes and decision records

`result.passes` records the final pass first, then each pinned unit-root
restart attempt in the pinned order:

1. `estimate` — the fixed-precision pass from the policy start
   (`attempted = gates_passed = accepted = available`).
2. `ur_regular` (pinned `tryUrpCancelling`) — gated on nonempty regular AR
   and MA blocks, `d > 0`, and `|1 + Σ phi_raw| <= 0.1` and
   `|1 + Σ theta| <= 0.1` (the raw polynomials at `B = 1`; with the Kalos
   sign, `phi_raw = -phi`).
3. `ur_seasonal` (pinned `tryUrmCancelling`) — same nonemptiness gates and
   `seasonal_d > 0`, with the same **regular** blocks evaluated at `B = -1`
   (toggle accumulator `-c0 + c1 - c2 + ...`, first entry negated, per the
   pinned toggle).

Both passes deflate only the regular blocks on the pinned arrays and restart
the same fixed-order problem from the deflated point:

* `ur_regular`: `par[i] = -acc; acc += old` (top-down) on AR and MA.
* `ur_seasonal`: `par[i] = acc; acc = old - acc` (top-down) on AR and MA.

A candidate replaces the incumbent **only on a strictly greater adjusted
concentrated likelihood**; equality keeps the incumbent. Every candidate-side
failure keeps the incumbent and never throws — the pinned passes swallow all
exceptions — which the qualified solver reports as an unavailable objective;
a deflated start that the parameter map rejects (non-admissible reflection
block) mirrors the pinned raw-coordinate optimizer throwing: the record keeps
`attempted = true` with an empty candidate. `accepted` is meaningful only
with `attempted`, and each record carries the pass's solver status, objective
evaluation count and candidate likelihood (`NaN` when unavailable).

`status` is `likelihood_only` with `converged = false` and no passes when the
request has no free ARMA coefficient (the pinned `parametersCount == 0`
short circuit: the result is the direct physical objective, bit-identical).
Otherwise `status` is `converged` when the producing (last accepted) pass
converged on the qualified solver's first-order criterion, and that pass's
solver status otherwise; a candidate whose optimizer stopped without an
available objective never becomes the producing pass.

## Qualification and tolerances

The corpus is the 17-case pinned capture under
`validation/evidence/native/estimation-control/` (cases.csv, inputs.csv,
metadata.R, SHA256SUMS.csv), produced by
`validation/reference/capture-estimation-control.R` driving the pinned
`RegSarimaComputer.process` through the public-API observer
`validation/reference/EstimationControlProbe.java` (no Kalos code is an oracle
input). Stored parameters use the Kalos convention `[ar, sar, ma, sma]` with
the AR sign flip applied.

Because the pinned Levenberg-Marquardt and the Kalos BFGS are independent
solvers, agreement is asserted at fit tolerance: **2e-3 per coefficient and
5e-4 in the adjusted log-likelihood**. Both bounds are dominated by the
pinned side's own run-to-run spread between the Zero and Default starts on
identical targets (measured worst 4e-4), not by the Kalos error: the observed
Kalos-vs-pinned agreement on this corpus is worst 3.99e-4 (coefficient) and
3.12e-6 (likelihood). The three likelihood-only guard cases compare at 1e-9
(no solver involved; the test additionally checks bit equality against the
direct physical objective). Solver *status strings* are deliberately not
asserted on the fixture cases: on flat near-unit-root directions the pinned
function-precision stop and the qualified gradient criterion can disagree
while the outcomes agree; the near-threshold `urp_*`/`urm_*` fixtures leave
the gate decision itself to record consistency (`accepted == attempted &&
log_likelihood strictly better`) rather than a boundary-dependent expectation.

`tests/test_estimation_control.cpp` embeds the corpus at 17 significant
digits (generated from the same CSVs); `tools/verify_estimation_control.cpp`
sweeps the on-disk corpus (17 cases, 0 failures; compile against the built
`libkalos.a` with OpenMP and run from the repository root). OMP and serial
builds pass 49/49 ctest; an ASan/UBSan run of `test_estimation_control` is
clean.

## Not delivered or claimed

The HannanRissanen and Multiple starts and the `HannanRissanenInitializer`
(item 21-A.2); the pinned final `Point` score/information (hence the `cdf`
and `fast` arithmetic); the `ml = false` (residual-variance) objective form;
the `PROCESSOR` singleton's `eps = 1e-9` or any defaulting; any R/Rcpp
binding, order search, transformation decision or outlier loop.
