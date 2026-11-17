# Fixed-order estimation control (21-A)

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
             true, kalos::EstimationStartingPoint::Multiple};
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

* `eps` — replaces `optimizer.function_tolerance` for the final pass (the
  pinned minimizer receives `eps` as its function precision).
* `feps` — the same override for the two internal-precision passes of the
  Multiple starting policy (the pinned `DEF_INTERNAL_EPS = 1.0E-4`); read
  only on Multiple paths, still validated positive and finite.
* `starting_point` — all four pinned policies, below.

Recorded but inert at this seam, accepted at either value, because the
qualified Kalos solver owns the corresponding mechanism:

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

rjd3 stores the raw coefficients of `1 + phi(B)`; Kalos uses `1 - phi(B)`
with MA `1 + theta(B)` on both sides (probe-verified against the pinned
jars). The `Zero` and `Default` policies transcribe the pinned
`SarimaModel.Builder` fills:

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

`HannanRissanen` seeds the final pass from the transcribed
`HannanRissanenInitializer` (next section). `Multiple` is the pinned two-pass
internal-precision policy: run `estimate(feps)` from the initialized start,
build the pinned alternate start from its winner (`ma[0]` replaced by `+0.9`
when the fitted `ma[0]` is negative else `-0.9`, every regular AR entry zeroed,
`ma[i >= 1]` zeroed, seasonal blocks carried over), run `estimate(feps)` again,
and retain as the final seed whichever of the two has the strictly greater
likelihood (the pinned `dcmpg` keeps the first estimate on equality and on any
NaN). The winner then seeds the final `eps` pass.

Scope limits of the Hannan-Rissanen-dependent starts (documented fail-closed
deviations, explicit `invalid_argument`, never a silent fallback): the pinned
initializer consumes the differenced `LinearModel` with all columns estimated
and no missing positions, so the `HannanRissanen` and `Multiple` starts reject
masked series, fixed regression coefficients, and responses not longer than
the differencing warmup `d + seasonal_d * period`. Where the pinned flow would
rebuild the final start from an unavailable winner, Kalos keeps the
best-available candidate (second estimate, else first, else the initialized
start) instead of the pinned `null` propagation.

## Hannan-Rissanen initializer

`hannan_rissanen_start()` (item 21-A.2, `kalos/hannan_rissanen.hpp`) is the
bytecode transcription of `HannanRissanenInitializer.initialize` and the
`HannanRissanen` three-stage pipeline: OLS residuals of the differenced linear
model (or the response itself without regressors), the pinned rms < 1e-9
variance guard (`setDefault(0, 0)` — every block zero), `AbsMean` normalization,
the Levinson AR prefilter of the pinned `npi()` length, the dense two-sided
regression on response-lag and prefiltered-residual-lag columns (warmup is the
pinned `max(p + P*bp, q + P*bq)` largest design lag), the pinned bias
correction (ea/es sensitivity recursions, `|v| > 1e16` abort keeps `m_pi`), and
the final correction (filter by the full estimated AR polynomial, refit the MA
orders, overwrite the MA tail). Block stabilization inverts roots inside the
unit circle per `SarimaMapping.stabilize` (reciprocal-root reconstruction via
`fromComplexRoots`, constant normalized to one). Only the first entry of each
seasonal block reaches the model, exactly as the pinned `updatemodel`
extraction. Builder defaults `biascorrection = finalcorrection = true` and
initializer settings `stabilize = true`, `useDefaultIfFailed = true`,
`failifunstable = false` are the pinned call-site values and are not exposed.

The result carries the provenance: `Fitted` (with `stabilized` when a block
was rewritten), `ZeroVariance` (guard fired, zero start) or `Defaulted` (any
pinned catch-all: the `setDefault()` fill, Kalos `ar = sar = +0.1`,
`ma = sma = -0.2`). Numerical failure is never an exception; interrupt
exceptions always propagate. The pinned `LevinsonAlgorithm`,
`QRLeastSquaresSolver` (full-column-rank equivalent via DGELSY),
`Polynomial.roots`/`fromComplexRoots` (via DGEEV through the qualified
`polynomial_roots` adapter) and `BackFilter` semantics were each
bytecode-pinned before transcription.

## Passes and decision records

`result.passes` is the **chronological** decision record of one call:

1. `initialize` (HannanRissanen starts only) — the initializer outcome;
   `detail` is the source (`fitted`, `zerovariance`, `defaulted`) suffixed
   `,stabilized` when stabilization rewrote a block; `accepted` marks the
   start retained as the final seed.
2. `multiple_first.*`, `multiple_second.*` (Multiple only) — each internal-
   precision chain: `estimate`, `ur_regular`, `ur_seasonal`. Exactly one of
   `initialize`, `multiple_first.estimate` and `multiple_second.estimate`
   carries `accepted = true` (the pinned selection).
3. The final `eps` chain: `estimate`, `ur_regular` (pinned
   `tryUrpCancelling`), `ur_seasonal` (pinned `tryUrmCancelling`). The two
   restarts are gated on nonempty regular AR and MA blocks, `d > 0`
   (regular) or `seasonal_d > 0` (seasonal), and `|1 + Σ phi_raw| <= 0.1`
   and `|1 + Σ theta| <= 0.1` on the raw polynomials at `B = 1`
   (regular) or the toggle accumulator `-c0 + c1 - c2 + ...` (first entry
   negated) at `B = -1` (seasonal). Both passes deflate only the regular
   blocks on the pinned arrays and restart the same fixed-order problem from
   the deflated point (`ur_regular`: `par[i] = -acc; acc += old`;
   `ur_seasonal`: `par[i] = acc; acc = old - acc`, both top-down).

A restart replaces the incumbent **only on a strictly greater adjusted
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

Two pinned corpora under `validation/evidence/native/`, each with cases.csv,
inputs.csv, metadata.R, SHA256SUMS.csv, produced by
`validation/reference/capture-estimation-control.R` (17 cases, Zero/Default)
and `validation/reference/capture-estimation-control-hr.R` (10 cases,
HannanRissanen/Multiple, per-case `eps`/`feps` including the pinned
`PROCESSOR` singleton at `eps = 1e-9`) driving the pinned
`RegSarimaComputer.process` through the public-API observer
`validation/reference/EstimationControlProbe.java` (no Kalos code is an oracle
input). Stored parameters use the Kalos convention `[ar, sar, ma, sma]` with
the AR sign flip applied. The HR corpus was captured twice in independent
JVM processes and is byte-identical (reproducibility check).

Because the pinned Levenberg-Marquardt and the Kalos BFGS are independent
solvers, agreement is asserted at fit tolerance: **2e-3 per coefficient and
5e-4 in the adjusted log-likelihood**. Both bounds are dominated by the
pinned side's own run-to-run spread between the Zero and Default starts on
identical targets (measured worst 4e-4), not by the Kalos error: the observed
Kalos-vs-pinned agreement over the 27 cases is worst 6.2e-4 (coefficient) and
7.25e-6 (likelihood). The three likelihood-only guard cases compare at 1e-9
(no solver involved; the test additionally checks bit equality against the
direct physical objective). Solver *status strings* are deliberately not
asserted on the fixture cases: on flat near-unit-root directions the pinned
function-precision stop and the qualified gradient criterion can disagree
while the outcomes agree; the near-threshold `urp_*`/`urm_*`/`hrs_near_unit`
fixtures leave the gate decision itself to record consistency (`accepted ==
attempted && log_likelihood strictly better than the chain's own estimate`)
rather than a boundary-dependent expectation. The record vocabulary, chain
shape and the Multiple selection bit are asserted structurally per starting
point (3 records for Zero/Default, 4 for HannanRissanen, 10 for Multiple),
plus bit-exact reuse of the full record vector at each thread count.

`tests/test_estimation_control.cpp` embeds both corpora at 17 significant
digits (generated from the same CSVs by
`validation/reference/generate-estimation-control-test.py`);
`tools/verify_estimation_control.cpp` sweeps both on-disk corpora (27 cases,
0 failures; compile against the built `libkalos.a` with OpenMP and run from
the repository root). `tests/test_hannan_rissanen.cpp` qualifies the
initializer branches the corpora cannot reach at pinned level: AR(1)/MA(1)
recovery, explosive-root stabilization (`stabilized`, start inverted to
`1/1.6`), the rms variance guard (`ZeroVariance`, zero blocks), the
`Defaulted` fill, design-residual recovery, interrupt propagation and the
input contract. OMP and serial builds pass 50/50 ctest; ASan/UBSan runs of
`test_estimation_control`, `test_hannan_rissanen` and `test_linear_algebra`
are clean.

Known divergence (measured, deliberate non-qualification): the rms < 1e-9
variance guard fires only when the differenced design explains the response,
which makes the ML innovation variance zero and the likelihood unbounded; the
pinned optimizer's landing point there is *process-dependent* (the same input
in two pinned JVM processes produced final parameters `[-0, -0.9]` with
log-likelihood 1869.854 and `[-9.7e-10, -3.6e-9]` with 1841.836). A corpus
case exercising the guard is therefore not pinned evidence; the branch is
qualified at the unit level (source and returned start), and the pinned
`Multiple` selection semantics stay qualified by the fitted-fixture cases.

## Not delivered or claimed

The pinned final `Point` score/information (hence the `cdf` and `fast`
arithmetic); the `ml = false` (residual-variance) objective form; any
defaulting (`eps = 1e-9` is qualified as data, never as a default); the
pinned `HannanRissanenInitializer` settings other than the call-site defaults
(`failifunstable = true`, `useDefaultIfFailed = false` and `stabilize = false`
are outside this unit); masked-series or fixed-coefficient HannanRissanen
starts (rejected per the scope limits above); pinned behaviour on degenerate
unbounded-likelihood inputs (per the divergence note); any R/Rcpp binding,
order search, transformation decision or outlier loop.
