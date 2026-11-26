# Item 29 — steady-state path inside the exact diffuse likelihood: probe slice (2026-11-26)

Live-JVM probe + measurement dossier for roadmap item 29, in the
[`tramo-seats-roadmap.md`](tramo-seats-roadmap.md) probe-ledger style. This
slice ships **no production code**: it dissects the pinned JVM filter machine,
measures the kalos exact path at `ec17543`, freezes the regime corpora with
bit-exact decision patterns under
`validation/evidence/native/diffuse-steady-state-probe/`, characterizes what
"steady state" can mean inside the exact path, and sets the acceptance-budget
cost-factor target the implementation slice must hit. Any implementation is
gated on maintainer acceptance of the design in §6 below.

Everything JVM-side is `javap -p -c` on the pinned rjd3 3.9.0 toolkit jars
(`jdplus-toolkit-base-core-3.9.0.jar`
`1880e23dc1665be2822c30c4da11ea0f2482a8da5c3e9c80b372ff6b8375dd71`,
`jdplus-toolkit-base-api-3.9.0.jar`
`fed5fe554c1e64e371d40a1a55d449323cc2630a0d5ea33a3a34e47f7926c2c8`, SHA-matched
against the probe jar cache) plus live probes on Temurin OpenJDK 21.0.11
(`-XX:ActiveProcessorCount=1`), double-processed byte-identical on every case
(`jvm-determinism.txt`). Everything kalos-side runs against a fresh Release
build of `main` at `ec175430643caa293e5db24ad3f2ca62e3afc44d` (GCC 13.3.0,
`-O3 -DNDEBUG -ffp-contract=off`, OpenBLAS 0.3.34, one thread), re-ran
bit-identical. Timed phases bracket `/proc/loadavg` (2.7–4.0 on 8 cores
throughout; none discarded); the driver bytes live under the evidence
directory's `drivers/` and their console output under `tables/`.

## 1. The JVM machine, bytecode-verified

The TRAMO/X-13 ARMA likelihood does **not** run a Kalman filter on the
integrated model. `RegArimaModel.differencedModel()`
(`core.regarima.RegArimaModel`, offsets 0–43) lazily builds a
`RegArmaModel` through `RegArmaModel.of(RegArimaModel)`: the response (and
each regressor) is differenced up front, and the model is pinned to the
stationary SARMA part. The objective the estimator walks is therefore the
stationary differenced likelihood; the diffuse normalization survives only as
a parameter-free constant. `ConcentratedLikelihoodComputer.compute(RegArmaModel)`
(`core.regarima.estimation`) then, **per evaluation**:

- constructs a fresh `internal.toolkit.base.core.arima.KalmanFilter`
  (offsets 26–46; `filter` field null in `DEFAULT_COMPUTER` /
  `DEFAULT_FULL_COMPUTER`) with `multiUse = X.getColumnsCount() > 0`;
- `prepare(model, n)` once (offsets 53–65);
- `apply()` on y and every regressor column (whitening), then Householder
  QR + `QRLeastSquaresSolver` GLS concentration, `ssq` + `getLogDeterminant`.

`KalmanFilter.prepare` (offsets 0–106) is a scalar predictor-form machine, not
a state-space one: `var = innovationVariance`, `ldet = NaN`, `phi = AR array`,
`dim = max(arOrder, maOrder+1)`, `C0 = AutoCovarianceFunction.values(dim)` —
the **analytic** autocovariance prime of the stationary model — `h0 = C0[0]`,
then `tx(C0)` and `calcC()` **only if** `multiUse`. The gain history sweep
`calcC` and both apply paths (`mfilter`, `sfilter`) run a Durbin-type
two-row sweep in which the innovation variance `h` is floored at the
innovation variance `var` (`calcC` offsets 335–348) and the position
`steadyPos = t+1` is latched when the swept row has **every** entry
`|elem| ≤ 1.0E-15` (absolute) — `calcC` offsets 349–394, `mfilter` offsets
236–278, `sfilter` offsets 409–457; after `steadyPos` the frozen last row is
reused. Two consequences matter for item 29:

**(a) The steady-state switch almost never fires on the workloads that
matter.** Measured `steadyPos` vs differenced length `n` (frozen captures,
`jvm/jvm-*.txt`):

| JVM case | model | steadyPos/n | kalos τ=1e-14 t\*/n |
| --- | --- | --- | --- |
| jvm-airline-noreg / -x | (0,1,1)(0,1,1,12) | 130/131 | 143/144 |
| jvm-slow (sma=0.999) | (0,1,1)(0,1,1,12) | 142/143 | 153/156 |
| jvm-nearur (ar→1) | (1,1,1)(0,1,1,12) | 130/131 | 143/144 |
| jvm-nearni (ma→−1) | (0,1,1)(0,1,1,12) | 130/131 | 143/144 |
| jvm-stat-sarma11 | (1,0,1)(1,0,1,12) | 239/240 | 108/240 |
| jvm-airline4 / jvm-mixed4 | quarterly | 90/91 | 95/96, 89/96 |
| jvm-d2 | (0,2,1)(0,0,0) | 49/142 | 25/144 |
| jvm-stat-arma21n | (2,0,1)(0,0,0) | 37/240 | 17/240 |

On every seasonal/integrated shape the JVM switch latches at the last or
second-to-last step: the JVM does not actually use a steady-state tail on the
airline workload either. The two machines' transient boundaries agree
structurally — JVM's absolute 1e-15 row test fires *later* than a relative
1e-14 band on `F_t` would (d2: 49 vs 25; arma21: 37 vs 17).

**(b) The JVM speed is the representation, not the switch.** Warm per-
evaluation medians over evals 1..5 (`tables/cross-validation.txt`):

| JVM case | n | prep µs | apply µs | total µs |
| --- | --- | --- | --- | --- |
| jvm-airline-noreg | 131 | 6.2 | 189.6 | 236.9 |
| jvm-airline-x | 131 | 157.4 | 142.1 | 399.3 |
| jvm-airline4 | 91 | 6.9 | 103.1 | 136.1 |
| jvm-stat-sarma11 | 240 | 19.9 | 342.0 | 405.2 |
| jvm-d2 | 142 | 6.7 | 74.4 | 126.3 |

(single-use filters sweep inside `apply`, hence `prep ≈ 6 µs` with `nx = 0`;
`gaps-x` runs 934 µs because differencing turns the 1-column design into
nx = 6 profiled columns — see §5.) The whole differenced ML pass — sweep,
whitening, GLS — costs 0.24–0.40 ms. That is ~1.5 µs per whitened observation
at dim 14: the scalar Durbin-form sweep is O(dim) per step, and the frozen
gain row is not what makes it cheap.

Coefficient-convention datum (calibrated, not assumed): the JVM `phi/bphi`
values are the **negated** kalos `ar/sar` lists (the JVM prints the
polynomial verbatim); `theta/btheta` map verbatim. `jvm-stat-arma21.txt` vs
`jvm-stat-arma21n.txt` freezes both branches; the calibrated run matches the
kalos golden ssq bit-for-bit.

## 2. Cross-validation: the exact path already equals the JVM machine

At pinned coefficients on the frozen corpus, kalos's exact diffuse likelihood
(verified at variance 1; profiled ll) against the JVM concentrated likelihood
(`tables/cross-validation.txt`; ssq relative gap):

| pair | kalos ssq | JVM ssq | rel | kalos profiled ll / JVM ll |
| --- | --- | --- | --- | --- |
| airline-noreg | 45.070688246188148 | 45.070688246187990 | 3.5e-15 | −118.26941324010129 / −118.2694132401 |
| airline-x (GLS) | 42.904065688632038 | 42.904065688631880 | 3.6e-15 | −115.04252204354874 / −115.0425220435 |
| stat-arma21 | 1588.4169259666944 | 1588.4169259666944 | 0 | −567.38726964354123 / −567.3872696435 |
| stat-sarma11 | 1309.5796057786285 | 1309.5796057786288 | 1.7e-16 | −544.20908472569363 / −544.2090847257 |
| nearur | 2.6230204280596601 | 2.6230204280596630 | 1.2e-15 | 66.965643420568298 / 66.9656434206 |
| nearni | 3.6207441565056731 | 3.6207441565054603 | 5.9e-14 | 36.217462587156312 / 36.2174625872 |
| d2 | 139.60201255240534 | 139.60201255240537 | 2.0e-16 | −200.42387974360915 / −200.4238797436 |
| slow | 151.32464223626712 | 151.32464223626680 | 2.1e-15 | −222.25259992313252 / −222.2525999231 |
| airline4 | 83.387427267258630 | 83.387427267258630 | 0 | −126.11539520515969 / −126.1153952052 |
| mixed4 | 88.681391164984419 | 88.681391164984390 | 3.2e-16 | −128.06298704136319 / −128.0629870414 |

GLS coefficients agree to 5e-15 (0.39425527898571122 vs 0.39425527898570940).
The observed agreement band between two mathematically equal machines —
≤ 6e-14 relative on ssq, ≤ 4e-9 absolute on the profiled log-likelihood
across the gap-analysis workload — is the yardstick any future "exactness-
preserving" claim must sit inside; this probe adopts ssq ≤ 1e-12 relative and
ll ≤ 1e-9 absolute as the qualification bands (§6).

## 3. kalos exact-path measurements at `ec17543`

Medians of 9 repetitions, one thread (`tables/cost-phases.txt`):

| case | n | m (r+k) | eval ms | init ms | objective (REG) ms |
| --- | --- | --- | --- | --- | --- |
| real-airline-noreg | 144 | 27 (14+13) | 9.139 | 0.417 | — |
| real-airline-x | 144 | 27 (14+13) | 8.552 | 0.405 | 8.919 |
| real-airline-gaps-x | 144 | 27 (14+13) | 8.135 | 0.399 | 8.754 |
| real-airline-redundant-x | 144 | 27 (14+13) | 8.526 | 0.395 | 9.421 |
| real-airline-zerocol-x | 144 | 27 (14+13) | 8.443 | 0.405 | 9.071 |
| syn-airline12 | 156 | 27 (14+13) | 9.300 | 0.392 | — |
| nearur / nearni / slow | 144/144/156 | 27 (14+13) | 8.595 / 8.501 / 9.266 | ≈ 0.40 | — |
| syn-airline4 | 96 | 11 (6+5) | 1.391 | 0.132 | — |
| syn-mixed4 | 96 | 10 (5+5) | 1.220 | 0.059 | — |
| d2-arma12 | 144 | 4 (2+2) | 0.367 | 0.011 | — |
| syn-stat-arma21 | 240 | 2 (2+0) | 0.222 | 0.007 | — |
| syn-stat-sarma11 | 240 | 14 (14+0) | 27.015 | 0.274 | — |

Key attribution numbers:

- **The step is the cost.** The truncated-length sweep gives a per-step slope
  of **52.6 µs/step** at m = 27 (8.675 ms at n=144 minus 2.369 ms at n=24,
  over 120 steps); the init phase (stationary representation + stationary
  initialization + assembly + `FiniteStateFactor` construction) is **0.40 ms
  ≈ 4.6%** of an airline evaluation post-SB03MD — the 28% init share quoted
  from M9 is obsolete at this commit. The per-step cost is the
  `FiniteStateFactor` machine: 287 QR compressions per airline evaluation
  (`compressions=287` in every airline golden), each an m×m `dgeqrf` on the
  27-column factor, twice per step.
- **Per-evaluation ratios vs the JVM reference now stand at 22.4×
  (airline-x: 8.92 ms vs 399 µs) and 38.6× (airline-noreg: 9.14 ms vs
  237 µs)** — the 83× datum predates the SB03MD landing; other shapes:
  airline4 10.2×, mixed4 11.7×, d2 2.9×, stat-arma21 1.5×, and the outlier
  below.
- **`FiniteStateFactor` carries the full m columns although only r ≈ m−k are
  ever finite.** The airline factor is 27-wide on a rank-14 covariance:
  the k = 13 history block is provably zero after the pivots (§4), yet the
  factorization never drops columns by contract ("never decides the diffuse
  rank or drops weak positive finite directions",
  `include/kalos/finite_state_factor.hpp`) — that contract sentence is about
  *weak* directions; a *certified-zero* block is a different statement, see
  design D4.
- **Adjacent finding (not item 29):** the k = 0 stationary path runs the
  dense `sandwich`/`joseph` fallback with heap churn at **112 µs/step at
  m = 14** — twice the per-step cost of the m = 27 QR path. `syn-stat-sarma11`
  (27.0 ms vs the JVM's 0.41 ms, ratio 66.7×) is the worst shape in the
  corpus. Recorded here because it dominates the stationary regime and
  mis-frames any steady-state gain measured there; a factorization routing
  fix for k = 0 is an independent candidate item.
- **Frozen-gain tail prototype** (`tail` phase): a scalar frozen-gain loop
  over the post-transient tail costs **2.5 µs/step at m = 27**, 0.61 µs/step
  at m = 14, 0.084 µs/step at m = 4 — i.e. 20–200× cheaper than the exact
  step; the binding constraint is exclusively the transient length.

## 4. What steady state can mean inside the exact path

After the last diffuse pivot (`last_diffuse = k+1`, k = d + seasonal_d
degree, pivot count certified exactly by `CountsHistoryCertificate` in every
golden: `exactpivot=13,13,5,5,2…`), the kalos recursion restricted to the
stationary block is the ordinary time-varying Kalman Riccati iteration of the
stationary ARMA representation, started from the exact stationary prior
`blockdiag(P_stat, 0)`. Two mathematically distinct shortcuts exist and they
must not be conflated:

**(E1) Gain freezing (the JVM's switch).** `F_t → F̄` geometrically at the
closed-loop contraction rate; for a minimum-phase model the closed-loop poles
sit just inside the unit circle whenever the invertibility radius is near it,
and the airline class `(1−0.45B)(1−0.55B¹²)` has invertibility radius
`1/0.55^(1/12) ≈ 1.05`. The frozen goldens make the decay concrete (relative
gap of `F_t` to `F̄` in `real-airline-noreg`): 6.3e-3 at t=37, 1.3e-5 at
t=100, 2.3e-13 at t=143 — measured `t*(1e-14) = 143` of 144 observations.
**There is no ulp-level steady-state tail inside an airline sample, in either
machine** (§1a table). Any switch that fires earlier (JVM's 1e-15 absolute
row test is such a case on other shapes) changes computed values by the
remaining transient and is an *approximation adoption*, which item 29
explicitly excludes. A correct E1 implementation needs a prospective
certificate (monitored contraction of `ΔF_t` bounding the residual tail by a
geometric sum under an explicit band) and must be **fail-closed**: airline-
class shapes never satisfy it in-sample, so its measured benefit on the
flagship workload is 1/144 ≈ 0.7% of loop steps (tail step counts at
τ = 1e-14: airline 1, airline4 1, mixed4 7, nearur/nearni 1, slow 3; it is
material only on fast shapes: arma21 223/240, d2 119/144, sarma11 132/240 —
and those shapes are either cheap or dominated by the dense-path anomaly
above). **E1 is measured as not the lever this item hoped for.**

**(E2) Certified block-rank reduction (design D4 below).** The k history
coordinates are *deterministic* given the first k observations (canonical
history design, observation variance 0): `P⁺Z = 0` after each exact
projection, so every history row/column inserted into the finite covariance by
`predict` is identically zero in exact arithmetic, by induction over the
shift structure (base: `P₁ = blockdiag(P_stat, 0)`; the first history row of
`TP⁺T'` is `Z'P⁺T' = 0`; shift rows inherit zeros). The goldens'
`exact_history_certification=true` with exactly-k pivots proves the premise on
every integrated case; in floating point the block sits at rounding residuals
(~1e-17 here). Replacing the m-wide factor and 27-wide state bookkeeping with
the r-wide stationary factor plus an observed-history ring buffer changes the
computed values only the way a re-association does — every dropped quantity is
a mathematically-zero quantity currently carried at rounding level — and no
rank decision is ever taken on a *weak* direction. This is the exactness-
preserving reading of "steady state": not "stop updating" but "stop
maintaining columns the certificate proves to be zero". The stationary regime
(k = 0) is unaffected (no history block; see the adjacent finding).

**Near-boundary pathology stays on the exact path — and is safe under E2.**
The corpus pins the pathologies: `nearur` (AR 0.995 against d = 1, ssq 2.62,
ρ→1 keeps the transient past the sample), `nearni` (MA root at −1.02, F̄ =
1.047, ldet 25.9), `slow` (sma 0.999, t\*=153/156). Under E1-style freezing
these shapes must never switch (certificate fails closed); under E2 the
certificate is structural (holds for any admissible parameters, roots at the
boundary included), and the frozen goldens show both machines agreeing to
≤ 6e-14 right on the boundary. The E2 change therefore does not degrade
near-boundary destinations.

**M9-G basin datum, resolved.** M9-G showed a synthetic near-unit-root shape
reaching a *better* likelihood (+1.19) under a different start. That is a
start-destination (optimization-basin) effect: the destination is determined
by the start point and the likelihood surface; the per-evaluation *path* an
exact machine uses to compute that surface (frozen gains or shortened
factorizations, within the §2 bands) contributes ≤ 1e-9 to surface values —
eight orders of magnitude below the basin separation. No steady-state
implementation can move a corpus destination, because none changes the
surface beyond the band; shapes whose *start* matters keep the HR-start
remedy (roadmap item 20 lane), not a filter change. Corpus verification duty
for any implementation: destinations bit-identical on all 15 frozen cases,
and the rank-deficient pairs keep `status=rank_deficient` with the frozen
coefficient NaN pattern (`real-airline-redundant-x`,
`real-airline-zerocol-x` goldens).

## 5. Why the JVM-style differenced representation (D3) is not exactness-preserving

The JVM object under gaps is *not* the kalos object. `jvm-airline-gaps-x`
(5 masked months in the same series/design) shows `RegArmaModel.of` differencing
across the mask and re-materializing the leading differenced rows as **five
extra profiled regressor columns** (nx 1 → 6 in the frozen capture), returning
ll = −111.9889631799 against kalos's exact-path −115.87191036788437 — a
structural objective change (3.9 nats), not a constant offset. On gap-free
data the two agree to ≤ 4e-9 (§2) because the differencing Jacobian is
parameter-free there. Adopting D3 for the exact path would (i) redefine the
gapped likelihood, (ii) require an external constant bookkeeping contract, and
(iii) translate JVM semantics the repo deliberately authored independently.
D3 remains legitimate only as a *separate declared parity lane*; it is out of
scope for item 29's "exactness-preserving" mandate and is recorded for the
maintainer decision register.

## 6. Designs, projection, recommendation, acceptance budget

Cost model per integrated evaluation, measured coefficients (m state, r
stationary block, k history, n steps, loop share ≈ 1 − init share):

    eval ≈ init(0.40 ms, m=27) + n · step(m),  step(27) ≈ 53 µs (QR-dominated)

| design | change | exactness class | contract impact | measured/projected gain (airline-x) |
| --- | --- | --- | --- | --- |
| **D1** in-evaluation gain freeze after certified transient (E1) | freeze gain/F̄ once a geometric-tail certificate holds | exact within certificate band; fails closed | new certificate + band bookkeeping in the filter loop | **1/144 steps (0.7%) — measured zero for airline-class**; material only on fast stationary shapes |
| **D2** per-(spec, parameters) gain cache across evaluations | reuse swept gains while θ fixed | exact (identical arithmetic) | cache identity vs reentrant fits | **0** — BFGS/LM evaluates a new θ every evaluation; the JVM does the identical per-evaluation sweep |
| **D3** differenced-stationary representation (JVM approach) | whiten Δy with the scalar Durbin-form sweep | **not** exactness-preserving under gaps (§5); parameter-free constant contract needed on full spans | redefines the gapped object; translation-policy question | 20×+ (would match JVM) — out of mandate |
| **D4** post-pivot certified block-rank reduction (E2): drop the k history columns from `FiniteStateFactor` and the state bookkeeping after `last_diffuse`, keep an observed-history ring | certified-zero block removal; ulp-level re-association only | exact within re-association band (expected ≤ n·eps ≈ 1e-14 relative on the stationary block) | none for `diffuse_innovations` users (generic h > 0 models keep the full path); canonical-SARIMA path only; no weak-direction rule change | QR work scales `(r/m)³ = (14/27)³ ≈ 0.14`: projected eval **2.4–3.0 ms vs 8.92 ms (3.0–3.7×)**; per-evaluation ratio vs JVM **6–7.5×** |

**Recommendation: D4** as the item's deliverable scope — it is the only
exactness-preserving option with a measured, flagship-relevant effect; the
item's steady-state framing is honestly recorded as a *negative result* for
E1 (D1) on seasonal models, which §1a/§4 evidence. D1 may ride along as the
certificate-guarded tail of D4's post-pivot loop (both share the r-wide loop),
but must be documented as zero-gain on airline-class shapes. D2/D3 rejected as
above. The dense k = 0 fallback anomaly (§3) should open its own roadmap line;
do not fold it into this item's budgets.

**Acceptance budget (target factors SET here for the implementation slice):**

| budget | today (measured, `ec17543`) | target after D4 |
| --- | --- | --- |
| per-evaluation cost factor vs pinned JVM, fixed-order airline-x | **22.4×** (8.92 ms / 399 µs) | **≤ 10×** (roadmap reference magnitude; expected ≈ 6–7.5×) |
| same, airline-noreg | 38.6× | ≤ 12× |
| seasonal-ML wall clock, fixed-order workloads | 85.5 evals × 8.9 ms (M9 count) | ≥ 3× improvement, **evaluation count bit-identical** (no corpus decision flip) |
| decisions (status, rank, pivots, divergence, selections) on the 15 frozen cases | — | **bit-identical, zero flips** |
| values on the frozen cases | — | ssq ≤ 1e-12 relative, profiled ll ≤ 1e-9 absolute (bands anchored on §2 cross-machine agreement) |
| near-boundary shapes (nearur, nearni, slow) | exact path | exact path kept; bands as above; **no new switch may fire on them** |
| init share, d2/quarterly shapes | 4.6% (airline) | unchanged shape budgets |

Fallback per the roadmap: if the implementation slice cannot hit the ≤ 10×
factor with decisions bit-exact, the exact path stands and item 29 closes as
documented not-adopted, with this dossier as the reason.

## 7. Frozen corpus inventory (capture domains explicit)

`validation/evidence/native/diffuse-steady-state-probe/` — 56 files, digest
manifest `SHA256SUMS.csv`; inputs (`cases.csv`, `series/`, `x/`), exact-path
goldens (`goldens/`, %.17g sums, numerics counters, FTRACE, UKIND, FBAR,
TSTAR), Double-JVM captures (`jvm/`, `jvm-determinism.txt`), tables
(`tables/`), and the probe drivers (`drivers/`, build+rerun recipe in
`README.md`). Capture domains: single-thread x86-64 host + pinned jars +
Temurin 21.0.11, `ec17543` class build, `variance = 1`, mean 0; synthetic
inputs generated from a fixed LCG (seeds in `build_corpus.sh`) so the corpus
is host-RNG-independent; real inputs byte-copied from the pinned M9
gap-analysis probes. Regimes: stationary (`syn-stat-*`), stationary-after-
differencing (`syn-airline12/4`, `syn-mixed4`, real airline family),
near-unit-root / near-non-invertible / slow-seasonal (`nearur`, `nearni`,
`slow`), double differencing (`d2-arma12`), rank-deficient designs
(`redundant`, `zerocol`), diffuse delay by leading mask (`leadgap`), internal
gaps (`real-airline-gaps-x`). The JVM-side gap-domain divergence datum (§5) is
frozen deliberately as `jvm/jvm-airline-gaps-x.txt`.

## 8. Maintainer decision register

1. **Adopt D4 as the item-29 implementation scope** (exactness-preserving
   certified block-rank reduction; acceptance budget of §6), or close item 29
   as not-adopted with this dossier as the documented reason.
2. Confirm **E1/D1 is recorded as measured-zero** on seasonal workloads and
   does not gate D4.
3. Decide whether the **dense k = 0 stationary fallback anomaly** (§3,
   112 µs/step at m = 14, worst corpus ratio 66.7×) opens its own roadmap
   item.
4. D3 (differenced-stationary parity lane) stays out of item 29; adopt/reject
   as a separate declared lane only.

No production source is touched by this slice: `git show --stat` on the slice
commit lists only this document, the evidence directory, and the evidence
registration in `tools/source-files.json`.
