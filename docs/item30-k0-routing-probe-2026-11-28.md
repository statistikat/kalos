# Item 30 — k = 0 stationary-factorization routing: probe slice (2026-11-28)

Measurement-only probe dossier for roadmap item 30, in the
[`item29` probe-dossier style](item29-diffuse-filter-probe-2026-11-26.md).
This slice ships **no production code**: it re-measures the k = 0 stationary
regime at `1e2e3c8`, attributes the cost, answers the routing question by
falsification, freezes the measurement set under
`validation/evidence/native/k0-routing-probe/`, and sets the acceptance
budget the implementation slice must hit. Any implementation is gated on
maintainer acceptance of the design in §6; the roadmap item-30 line carries
the probe outcome as an annotation.

Everything kalos-side runs against a Release build of `main` at
`1e2e3c896ade4f67124f057c0aa5a0fedd4fc734` (GCC 13.3.0, `-O3 -DNDEBUG
-ffp-contract=off`, OpenBLAS 0.3.34 from the pinned provider, one thread),
driving the frozen item-29 probe corpus (`diffuse-steady-state-probe/`,
whose two `d = 0, seasonal_d = 0` cases are the k = 0 shapes). Timed phases
bracket `/proc/loadavg`; the capture lane ran at 5.29–5.63 and quiet lanes
at 3.16–4.40 on 8 cores (co-tenant load — all lanes are interleaved A/B,
and every table's bracket is committed); no round was discarded and no
bracket exceeded the 6.0 reload threshold. perf is blocked on the host
(`perf_event_paranoid = 4`), so attribution uses an in-process SIGPROF
self-sampler (1 ms `ITIMER_PROF`, PC from `ucontext`, `-no-pie` symbol
map) — the same attribution role as the item-29 sampling recipe — plus a
namespaced byte-copy of the filter with TSC section scopes whose fidelity
control (`copy0`) is byte-identical to the library on both k = 0 shapes
(`tables/correctness.txt`).

## 1. Current state at `1e2e3c8`

Live routing definitions (`src/diffuse_state_space.cpp`):
`sarima_innovations` assembles the canonical state
`[stationary ARMA state (r); history (k)]` and arms the square-root
`FiniteStateFactor` — the factorized route — **only under `if (k > 0)`**
(line 712); at k = 0 it passes `history = stable_history = finite_factor =
nullptr` (lines 715–716), and the filter's observation/update/predict
branches fall through to the dense covariance path: `pz = product(p, z)`,
`joseph(...)` and `sandwich(T, p)` plus the process-noise add (lines 368–
375, 464–476, 577–581). The design D4 post-pivot route is unreachable by
construction (`d4_eligible` requires `initial_rank > 0`, and the header
documents `post_pivot_route = 0` for k = 0). This probe verified the
routing claim before theorizing: the frozen goldens of the two k = 0 cases
re-run **byte-identical** at `1e2e3c8` against their `ec17543` bytes
(`tables/golden-rerun.txt`; the k > 0 cases show exactly the D4 in-band
movement adopted in M12, e.g. `real-airline-x` ssq 2.2e-14 relative).

The two corpus k = 0 shapes: `syn-stat-sarma11` (expanded
ARMA(13,13), r = m = 14, n = 240) and `syn-stat-arma21` (expanded
ARMA(2,1), m = 2, n = 240). Per-evaluation cost at HEAD (frozen item-29
cost driver, median of three rounds, capture lane bracket 5.29–5.63; JVM
column = warm `eval=1..5` medians from the frozen `jvm/` captures):

| case | kalos ms | JVM ms | ratio |
| --- | --- | --- | --- |
| **syn-stat-sarma11 (k = 0, m = 14)** | **27.94** | **0.405** | **68.9×** |
| syn-stat-arma21 (k = 0, m = 2) | 0.221 | 0.150 (calibrated) | 1.5× |
| real-airline-x | 3.354 | 0.399 | 8.4× |
| real-airline-noreg | 3.459 | 0.237 | 14.6× |
| nearur / nearni / slow | 3.41 / 3.32 / 3.48 | 0.226 / 0.223 / 0.266 | 15.1 / 14.9 / 13.1× |
| syn-airline12 / airline4 / mixed4 | 3.52 / 0.58 / 0.43 | 0.237 / 0.136 / 0.104 | 14.8 / 4.2 / 4.1× |
| d2-arma12 | 0.197 | 0.126 | 1.6× |

(full table: `tables/current-state-ratios.txt`).

**Yes — 66.7× is still the worst datum, essentially unmoved.** Across
lanes the sarma11 evaluation medians land at 25.7–29.6 ms (host-load
dependent; item-29's 27.0 ms ran at load 2.7–4.0), i.e. a 63–73× band
around the frozen 66.7×, and it remains the corpus-worst ratio by a factor
of 4.5 over the next-worst shape (nearur 15.1×). The per-step slope
reproduces item-29's 112 µs/step at 105–117 µs/step depending on lane
(`tables/nsweep-*.txt`). The m = 2 stationary shape was already 1.5× — the
anomaly is specifically the large-m stationary step, as recorded.

Production exposure goes well beyond the corpus: the X-13 estimation
requests run on the differenced response with the differencing carried in
`spec_d/spec_bd`, i.e. `out.d = 0; out.seasonal_d = 0`
(`src/x13_automatic.cpp:546-547`), and the ARMA-selection request likewise
(`src/arma_refinement.cpp:74-75`). Every candidate-model likelihood in the
X-13/TRAMO selection and refinement loops therefore evaluates through the
k = 0 dense fallback with the candidate's expanded stationary state size —
airline-class candidates evaluate at m = 14, i.e. at the sarma11 shape.

## 2. What dominates the k = 0 evaluation

Instrumented copy (TSC 0.357980 ns/tick, 9 reps,
`tables/instr-syn-stat-sarma11-copy0.txt`; scope pairs add ~4 clock pairs
per step, so sections are quoted against their own instrumented wall of
26.29 ms): predict `sandwich(T,P)` + process add **13.97 ms (53%)**, finite
`joseph` update **13.44 ms (51%)**, observation branch 0.34 ms (1.3%),
init (representation + stationary initialization + assembly) 0.35 ms
(1.3%). (The TSC-calibrated section sums overshoot that wall by ~4% —
clock-scale drift — so shares are read as magnitudes, not an exact
partition.) The `k > 0` observability counters confirm the LAPACK census:
`compressions = 0`, `square_root_finite_covariance = 0` on every k = 0
dump — **no LAPACK call executes inside the loop at all**; the only
LAPACK-family samples are the init-side Lyapunov solve (< 0.2%).

The SIGPROF sampler over 150 library evaluations (4118 samples,
`tables/sample-syn-stat-sarma11.symbols.txt`) independently confirms the
attribution and adds the decisive line item:

| share | symbol |
| --- | --- |
| 69.1% | `sandwich(DenseMatrix const&, DenseMatrix const&)` |
| 25.6% | `DenseMatrix::operator()(size_t, size_t)` (const + non-const) |
| 1.6% | `product(DenseMatrix const&, vector)` (the P·z observation branch) |
| 1.6% | `filter(...)` loop body |
| 0.7% | `joseph(...)` body |
| 0.2% | `checked(long double)` |
| ~0.1% | init-side LAPACK (`sb04px_`, `sb03mx_`) |

(The small `joseph` share is not a contradiction of the instrumented 51%
update scope: the Joseph update performs its two products *through*
`sandwich`, which the two methods share; the copy's scopes split the
`sandwich` time by call site 13.44/13.97.)

`DenseMatrix::operator()` is declared in the header but **defined in
`linear_algebra.cpp`**, so every element access in `diffuse_state_space.cpp`
is an out-of-line call with a `.at()` bounds check on top of the 80-bit
accumulate. The dense step itself is ~3m³ ≈ 8.2k long-double products per
step on matrices that are structurally almost zero for the canonical
stationary form — `Z = e_1` exactly, `T` = coefficient column + superdiagonal
shift (`src/state_space.cpp:441-446`), `h = 0`, and `k = 0` diffuse basis
empty. One correction to the item-29 §3 wording: the "heap churn" framing
is not supported — no allocator symbol reaches the sample floor; the churn
is a red herring, the cost is the accessor-mediated dense scalar products.

## 3. Does the factorized route degenerate at k = 0? Measured: no

The routing question is whether `k > 0` guards a mathematical degeneracy or
a conservatism. Falsification experiment (probe-local, namespaced filter
copy; no core change): arm the factor at k = 0 in both arming shapes —
`fac1` with the zero-dimension certificate/frame (the canonical k > 0 call
shape; `CountsHistoryCertificate(0,0,period)` and `SparseHistoryBasis(0,0,·)`
are well-defined at dimension 0 and the loop's certificate branches are
inert at rank 0) and `fac2` with the factor alone (the copy relaxes the
filter's `finite_factor → stable_history` guard for this route). Both run
to completion with no rank or certificate event on either shape:

- decisions byte-equal to the dense path (`STATUS`, `COUNTS`, `UKIND`,
  counts, statuses); the comparator's residual decision-line diff against
  the dense path is exactly the two observability lines (`NUMERICS`
  `exactcert`/`stable`/`compressions`/`sqr` and `RES factorinit`), which is
  the route difference itself, not a state change; `fac1 == fac2` differ
  only in the NUMERICS line (`exactcert`/`stable` flags), values
  byte-identical;
- the factor does exactly the canonical work: `compressions = 479` per
  evaluation (240 Joseph `dgeqrf` updates + 239 predict `dgeqrf`s) plus one
  `dsyev` factor construction at init;
- all named values sit inside the item-29 anchored bands: max relative
  deviation 6.1e-14 (`PRED`, sarma11; ssq byte-equal), max absolute
  deviation 2.3e-13 (ssq at |ssq| = 1588, arma21 — 1.4e-16 relative;
  profiled ll 1.1e-13 absolute).

(`tables/correctness.txt`.) The bypass is therefore **routing conservatism
at the single `k > 0` gate, not degeneracy** — the factorized route works,
and the design question is purely which representation the k = 0 regime
should pay.

## 4. The measured routes

One process per case, six timed routes, per-repetition order rotation, 9
reps × 3 rounds per lane; medians below (per-round `REP` lines and per-phase
load brackets are in the frozen tables; the quiet lanes ran at load
3.22–4.40, the capture lane 5.29–5.63). Slopes come from the n = 24…240
sweep; `init` is the cost
driver's init phase (representation + stationary initialization +
assembly, factor construction excluded/included as listed):

route (sarma11, m = 14)     | eval ms (all lanes) | slope µs/step | vs JVM 0.405 | value class |
| --- | --- | --- | --- | --- |
| `lib` — HEAD dense fallback | 25.7–28.1 | 107.6–117.2 | 63–69× | today |
| `copy0` — fidelity control | 25.7–27.0 | ≈ lib | — | byte-identical to `lib` |
| `fac1/fac2` — factorized | 3.50–3.87 | 12.6–13.9 (+0.30 init for `dsyev`) | 8.6–9.6× | re-association band |
| `struct3` — option-(ii) skeleton | **1.00–1.21** | **2.9–3.5** | **2.4–3.0×** | **byte-identical** |
| `stat` — `stationary_innovations` | 25.8–26.7 | ≈ lib | ≈ 64× | different conventions |

route (arma21, m = 2)       | eval ms (quiet lanes) | slope µs/step | vs JVM 0.150 |
| --- | --- | --- | --- |
| `lib` | 0.216–0.222 | 0.76–0.88 | 1.4× |
| `fac1` / `fac2` | 0.245–0.254 / 0.228–0.244 | 0.86–0.93 | **+11–18% / +2–13% regression** |
| `struct3` | **0.077–0.079** | 0.27–0.28 | **0.52×** |

Three measured facts decide the design:

1. **The factorization is not the cheap representation at k = 0.** Its two
   `dgeqrf` per step plus factor-side scalar assembly land at ~13 µs/step
   — 8× cheaper than dense, but 4× dearer than the O(m²) covariance-domain
   structural step below, and it *regresses* the m = 2 shape (LAPACK call
   overhead at trivial size, plus a 0.3 ms init `dsyev`). Option (i) —
   widening the routing to the factor — is measured-dominated on cost, on
   exactness (band vs byte), and on the small-m shape. Falsified as the
   item's deliverable.
2. **Structure beats QR at k = 0.** The option-(ii) skeleton keeps the
   covariance domain and the exact per-step rounding sequence, and skips
   only *exactly zero* structural terms under a shape witness
   (`basis.columns == 0`, `h == 0`, `Z` exactly `e_1`, `T` exactly the
   coefficient-column/superdiagonal companion). Skipping exact-±0 products
   cannot change a long-double accumulator that starts at +0
   (`+0 + (−0) = +0`), the same argument the shipped D4 case E1 uses — and
   the claim was tested, not assumed: the full named-output dumps
   (predictions, variances, innovations, standardized innovations, sums,
   final state, all four covariance matrices, counters, statuses, `%.17g`)
   are **byte-identical** to the dense path on both k = 0 shapes
   (`OPTION-II-BIT-IDENTICAL`, `tables/correctness.txt`).
3. **The accessor dominated the residual cost.** Structural skipping alone
   (lane `A` skeleton) took 26 ms → 3.43–3.57 ms; keeping the same terms
   and order but replacing `DenseMatrix::operator()` with persistent
   `values()` pointer views took it to 0.99–1.21 ms, re-verified
   byte-identical — i.e. two thirds of the surviving v1 cost was per-
   element accessor overhead, the sampler's 25.6% dense share persisting
   pro-rata on the reduced element count. Instrumented
   sections of the final skeleton: upd 0.185 ms + pred 0.160 ms
   (1.4 µs/step each) + init 0.304 ms + unscoped loop remainder ≈ 0.29 ms
   (O(m) gain division, log/variance accumulation, output vectors) out of
   a 1.04 ms wall (`instr-syn-stat-sarma11-struct3.txt`).

The existing `stationary_innovations` primitive was timed as the cheap
option-(iv) falsification: 25.8–26.7 ms — the same Joseph/sandwich machine
with the same accessors; dispatching k = 0 there buys nothing and would
re-point the named quantities. Falsified.

## 5. Where the remaining cost sits (floor interaction)

After option (ii), sarma11 spends ≈ 0.30 ms init (stationary representation
+ SB03MD initialization + assembly, ~30% of the evaluation) + ≈ 0.70 ms
loop (0.35 ms structural covariance steps + ~0.29 ms unscoped O(m) loop
work + output bookkeeping). The item-29/M12 contract floor of ≈ 5.5–6.1×
is **not the relevant floor here**: it was an airline-class reading
(init 0.59 ms + pre-pivot exact-certification steps 0.66 ms + the
two-`dgeqrf` QR floor 7.5 µs/step of the reduced route). The k = 0 shape
pays none of those components — no pivots, no certificate, no QR — which
is exactly why the structural route lands *below* that floor at 2.4–3.0×.
Pushing past it would require touching the arithmetic contract itself
(long-double accumulation policy, or vectorized double-width forms) — the
same future-item boundary M12 recorded at item 29's close; item 30
should not chase it.

## 6. Design options, measured

| option | change | measured bound (sarma11 / arma21) | ratio vs JVM | exactness class | effort | verdict |
| --- | --- | --- | --- | --- | --- | --- |
| (i) widen `k > 0` → arm `FiniteStateFactor` at k = 0 | one gate + guard | 3.50–3.87 ms / fac1 **+11–18% slower** | 8.6–9.6× | values move in the ratified band (measured ≤ 6.1e-14 rel, 1.1e-13 abs); route observability must grow at k = 0 | small | **dominated** by (ii) on cost, exactness and m = 2; falsified |
| (ii) structure-aware dense k = 0 steps (witness-gated `joseph`/`sandwich`/`product` replacements, persistent buffers, pointer-local accumulation) | ~100–150 lines inside the canonical filter + shape witness | **0.99–1.21 ms / 0.077–0.081 ms** | **2.4–3.0× / 0.52×** | **bit-identical by construction** (±0-skip argument; tested byte-for-byte on both frozen k = 0 cases) | small; probe skeleton exists and is committed | **recommended** |
| (iii) accept and document | none | 25.7–29.6 ms (today) | 63–73× | byte-identical | none | falsified by (ii)'s measured 24× at zero exactness cost |
| (iv) dispatch k = 0 to `stationary_innovations` | routing | 25.8–26.7 ms | ≈ 64× | different named-quantity conventions | small | no gain — same machine; falsified |

## 7. Recommendation and acceptance budget

**Adoption of option (ii)** as the item-30 deliverable: replace the dense
fallback of the canonical k = 0 path with witness-gated structural
covariance steps, in the D4 fail-closed idiom. The witness is shape-only
(`basis.columns == 0`, `observation_variance == 0`, `Z` exactly `e_1`,
`T` exactly the canonical companion pattern), evaluated once per call;
a shape failure keeps today's dense path bit-for-bit (the generic
`diffuse_innovations` entry is never armed, and the canonical k > 0 path
already runs the factor). The skeleton in
`validation/evidence/native/k0-routing-probe/drivers/m15-item30-filter_copy.cpp`
(routes 3) is the timing-frozen reference; it is probe code, not the
shipped design.

Acceptance budget for the implementation slice:

| budget | today (measured) | target |
| --- | --- | --- |
| decisions **and every named value** on the two frozen k = 0 cases | — | **byte-identical** (the design point; not merely the banded reading — dump protocol: `drivers/m15-item30-route_probe.cpp dump`) |
| k = 0 regime coverage | no gapped k = 0 case exists in any golden | implementation slice adds k = 0 goldens for gaps, `mean ≠ 0`, and near-boundary stationary roots before shipping |
| `syn-stat-sarma11` per-evaluation | 25.7–29.6 ms | **≤ 1.5 ms (≤ 3.7× JVM)**, expected 1.0–1.2 ms |
| `syn-stat-sarma11` per-step slope | 105–117 µs/step | **≤ 5 µs/step** (measured 2.9–3.5) |
| `syn-stat-arma21` per-evaluation | 0.216–0.222 ms | **≤ 0.10 ms and never slower than today** (expected ≈ 0.08 ms, i.e. ≈ 0.52× JVM) |
| LAPACK calls / heap allocations inside the k = 0 step loop | 0 LAPACK, ~6 small allocations/step | 0 LAPACK, 0 allocations on the witness path |
| all k > 0 and generic-diffuse shapes | — | **byte-unchanged** (witness fail-closed; route observability unchanged) |
| pinned budgets, suites | — | all held; full suite plus sanitizer and cancellation lanes green |

Fallback, mirroring item 29: if the implementation slice cannot hold the
byte-identity audit on a shape, that shape keeps the current path — the
witness already makes this the default behavior — and the item closes with
the audit table as the documented reason.

## 8. Frozen measurement set

`validation/evidence/native/k0-routing-probe/` — 68 files + digest
manifest `SHA256SUMS.csv` (manifest movement: +68 entries in
`tools/source-files.json` [evidence]): drivers (route probe, namespaced
filter copy with the k = 0 routes, build/capture scripts,
symbolizer/comparator), and tables: current-state ratios, three
interleaved A/B lanes with per-round `REP` lines and load brackets
(1-min load 3.16–5.63 across lanes, no discarded round, zero reload
retries), n-sweeps, instrumented section dumps, the SIGPROF sampler
output (raw PCs + symbolized histogram), the correctness gates, and the
golden-rerun record. Inputs are the item-29 corpus unchanged; no new
series were minted. Capture domain: single-thread x86-64 host, pinned
OpenBLAS provider, Release build at `1e2e3c8`, drivers at `-O2 -g -no-pie`.

## 9. Maintainer decision register

1. **Adopt option (ii)** as item 30's deliverable scope with the §7 budget
   (bit-identity is the acceptance object, and the budget is set at
   measured values), or close item 30 as documented not-adopted with the
   63–73× datum standing.
2. Record **options (i) and (iv) as measured-dominated** — they should not
   open separate items.
3. The accessor datum — `DenseMatrix::operator()` is an out-of-line
   bounds-checked call carrying 25.6% of the dense stationary evaluation —
   is cross-cutting; whether the pointer-local accumulation idiom should
   be widened beyond the k = 0 path is a maintainer call, deliberately
   **not opened** here.
4. Confirm the corpus gap that no gapped k = 0 golden exists and that the
   §7 second row assigns it to the implementation slice.

No production source is touched by this slice: `git show --stat` on the
slice commit lists only this document, the item-30 roadmap annotation, the
evidence directory, and its registration in `tools/source-files.json`.
