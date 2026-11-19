# Final estimator decision cascade (21-H slice 1)

Unit-qualified transcription of the decision half of
`jdplus.x13.base.core.x13.regarima.FinalEstimator`: the ARMA reduction
cascade `test(RegSarimaModelling)` and the stability predicate
`jdplus.toolkit.base.core.math.linearfilters.FilterUtility.checkRoots`
(promoted to `kalos::detail::check_roots`, shared with the 21-B
ssq/Levenberg domain). Bytecode source: pinned javap dumps of the jdplus
3.9.0 jars (`v390_FinalEstimator`, `v390_FilterUtility` under
`/tmp/opencode/jdprobe/item21h/` at slice time); this document and the
goldens are the durable record. The estimation half of `FinalEstimator`
(the five-round `estimate` loop around `RegSarimaComputer` and the LM
minimizer) is composed by the slice-2 round driver on top of the qualified
21-B/21-D estimation machines; the loop logic itself is pinned here.

```cpp
auto r = kalos::final_estimator_test(orders, pars, asyDiag, sampleLen, 1.0);
if (r.status == 1) /* re-estimate with r.orders */;
```

Public surface: `include/kalos/final_estimator.hpp` (`SarimaOrders`,
`FinalEstimatorRound`, `final_estimator_test`),
`include/kalos/detail/filter_utility.hpp` (`check_roots`).

## The machines

- `check_roots(coeffs, length, limit)` — effective degree = position after
  the last non-zero coefficient; 0 → true; 1 → `|c0| < limit`; 2 → with
  `disc = a*a - 4*b` (dcmpl: NaN takes the complex branch), `disc > 0`
  checks both real roots `(-a ± sqrt(disc)) / (2b)` via `1/|r| < limit`
  (NaN → false), else `sqrt(b) < limit`; ≥ 3 → the deterministic
  Durand-Kerner port `polynomial_roots` (21-B) with the pinned
  `roots == null || limit < 0 ⇒ true` guard before the `1/|r| < limit`
  loop. The JVM ≥ 3 solver (`FastEigenValuesSolver`) is seeded by
  `java.util.Random` — its verdicts are the documented non-reproducible
  surface; the qualified corpus never reaches degree ≥ 3 (observed max
  block degree 2 across the automatic corpus captures).
- `final_estimator_test` — `sig2 = sampleLen <= 150 ? 0.15 : 0.1`,
  roots limit `1 / 0.95`, `ratioLimit = tsig` (the ODM kernel leaves the
  builder default 1.0), `ratio = 999`. Flags `fP, fBp, fQ, fBq` =
  `order > 0 && checkRoots(block, 1/0.95)` over the
  `[phi | bphi | theta | btheta]` concatenation (the extraction cursor
  advances unconditionally). All false ⇒ 0. Then each firing block tests
  only its **last** coefficient: `idx = -1` plus block-order accumulation,
  `a = |par[idx]|`, `v = asyDiag[idx]`, `v > 0` (dcmpl: NaN variance
  proceeds and is rejected by the t-comparison), `t = a / sqrt(v)`; fire
  needs `t < tsig` and `a < sig2`, plus `ratio > t` for every block after
  P. Takeovers: Bp sets `ratio = t` and clears P; Q clears P and Bp; Bq
  clears P, Bp and Q but does not update the ratio. `total = cP+cBp+cQ+cBq`
  is returned literally; with the pinned resets it can only be 0 or 1, and
  the order change applies to the first positive count in P → Bp → Q → Bq
  order (`setX(x - cX)`, i.e. one term). The pinned `estimate` loop: status
  0 → true, status 1 → re-estimate (at most five rounds, then false),
  status > 1 or estimation `RuntimeException` → false; the round driver
  transcribes this verbatim.

## Qualification

- Capture: `validation/reference/CheckRootsGolden.java` (package-placed
  beside `FilterUtility`), intrinsic-disabled JVM; golden data
  `validation/reference/check_roots_golden.txt` (584 rows: degrees 1–2 over
  `limits {1/0.95, 1.0, 0.99999, 2.0, 0.5, -1.0}`, disc>0/≤0, trailing-zero
  degree reduction, and the negative-limit ≥ 3 guard rows),
  `tools/gen_check_roots_golden.py` → `tests/jvm_check_roots.h`.
- `tests/test_final_estimator.cpp`: bit-exact replay of all 584 checkRoots
  rows plus a hand-derived cascade truth table (16 cases: every takeover
  ordering and its equality edge, the sig2 plateaus at 150/151, `t == tsig`,
  variance guards, unstable-block skips incl. both degree-2 branches, the
  Bq no-ratio-update rule and the envelope throws). Zero failures, OMP and
  serial lanes, ASan/UBSan clean.
- End-to-end: the slice-3 automatic corpus pins the cascade's observable
  effect — the post-cascade SARIMA orders per case, captured twice from
  independent JVM processes and byte-identical.

## Envelope

Parameters vector exactly `p + bp + q + bq`; covariance diagonal at least
as long (the pinned mapping puts ARMA coefficients first, regression
behind); orders non-negative. Degree ≥ 3 root blocks run the deterministic
port, whose ≥ 3 agreement with the JVM is not (and cannot be) bit-pinned —
the corpus stays within degrees 0–2.
