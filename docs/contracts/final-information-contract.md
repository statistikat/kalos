# Final-information producer (21-H slice 0)

Unit-qualified transcription of the information half of the pinned
final-estimation tail: `RegArmaSsqFunction.ssqEvaluate` (deviance at the
final point) → `NumericalDerivatives(point, false, false)` (step domain,
`checkepsilon`, forward/central stencil, `calcgrad`/`calch`) → the
`RegArmaProcessor.compute` scalings (`0.5·nobs/fx` on the curvature,
`(-0.5·nobs)/fx` on the gradient) → `SymmetricMatrix.inverse` (Cholesky
with Crout–Doolittle fallback). Bytecode source: pinned javap dumps of the
jdplus 3.9.0 jars (`NumericalDerivatives`, `SymmetricMatrix`,
`LowerTriangularMatrix`, `UpperTriangularMatrix`, `lapack_Cholesky`,
`CroutDoolittle`, `Constants`, `RegArmaProcessor`, `IFunctionPoint`,
`SarimaMapping2` under `/data/home/kowa/seasonaladjustment/tmp_opencode/jdprobe/cur/`
at slice time); this document and the goldens are the durable record. The
design-matrix columns (the `CompoundMapping` jacobian block that consumes
`e_mean/e_reg/i_mean/i_reg`) belong to slice 1; this unit produces exactly
the ARMA-parameter information quantities.

```cpp
final_information_request req;
req.orders = {p, bp, q, bq};       // SarimaOrders (21-H slice 1 shape)
req.point  = rawConventionPoint;    // [phi | bphi | theta | btheta]
req.nobs   = nobs;                  // literal; the JVM passes <= 0 too
req.deviance = [](const std::vector<double>& x) -> std::optional<double> {
    return ssq_e_or_nullopt;        // nullopt = the pinned evaluation threw
};
auto r = kalos::final_information(req);
if (r.status == final_information_status::available) /* use r.asymptotic_covariance */;
```

Public surface: `include/kalos/final_information.hpp`
(`final_information_request`, `final_information_result`,
`final_information_status`, `final_information`, `final_information_step`).

## The machine

- **Base evaluation.** The deviance is evaluated at the point first
  (`Evaluation.getSsqE`): `nullopt` there means the JVM threw before any
  stencil work — status `final_point_rejected`, no stencil output.
  `fx` is that value; it doubles as the fallback the stencil uses whenever
  a node evaluation returns `nullopt` (the pinned `newval` catch returns
  `fx`, which is why a throwing node silently enters the differences).
- **Step domain.** Per coordinate the epsilon is
  `x[i] < 0 ? STEP·max(1, −x[i]) : (−STEP)·max(1, x[i])` with the Java
  comparison flavours (NaN takes the negative branch; `Math.max(1, NaN) =
  NaN`) and `STEP = Math.sqrt(2.220446E-16)`
  (`final_information_step()`, bit `0x3e4ffffffa0bc6d1`).
- **`checkepsilon`** (`SarimaMapping2`, `checkAll = true`): a nonzero
  epsilon whose node `x[i] + e` fails `checkBoundaries` is halved up to
  three times; if still bad the sign is flipped and halved up to three
  times; if still bad `eps[i] = 0` (the coordinate is frozen: the forward
  node coincides with `fx`, `calcgrad`'s `fp == fx` guard zeroes the
  gradient entry and `calch` skips the row). `checkBoundaries` is
  `FilterUtility.checkRoots` per block (AR blocks against `ar_limit`, MA
  blocks against `ma_limit` only when `check_all`), and the whole-vector
  node check explains the JVM's eps-zeroing at points sitting exactly on
  a limit. `±0.0` epsilons skip the check entirely; NaN enters it.
- **Stencil** (`calchess`, eager forward pass then lazy second order):
  `fp[i] = newval(i, eps[i])`; diagonal over `h = sqrt(|eps[i]|)` as
  `(f(x+h·e_i) − 2·fx) + f(x−h·e_i)` stored only when the result and
  `eps[i]` are nonzero, divided by `h·h`; cross terms over the four
  corners `(±a/2, ±b/2)` as `(f(++) + f(−−)) − f(+−) − f(−+)` stored
  (symmetrically) only when the sum, `a` and `b` are all nonzero, divided
  by `a·b`. Comparison flavours are the literal `dcmpl`/`dcmpg` from the
  bytecode, so NaN propagates exactly where the JVM lets it.
- **Scalings and inverse.** `information = hessian·(0.5·nobs/fx)`,
  `score = gradient·((−0.5·nobs)/fx)` (signed-zero outcomes such as
  `−0.0` scores are reproduced bit-exactly). The inverse runs on the JVM
  column-major storage (`lcholesky(tol = 0)` → `LowerTriangularMatrix
  .inverse` → `LtL`), falling back to `CroutDoolittle.decompose` with
  `Constants.getEpsilon = 1.1102230246251565E-16` pivot tolerance plus
  `LUDecomposition.solve` on identity when the Cholesky throws
  `MatrixException` (NaN entries pass both singularity guards exactly as
  in the JVM; the catch decomposes the ORIGINAL matrix — Cholesky mutates
  only the deepClone and decompose only its own `toArray` copy, so the
  caller's information is never touched). `inverse_failed` is the
  caught-`MatrixException` status; it emits no covariance.

## Qualification

- Capture: `validation/reference/FinalInformationProbe.java` +
  `validation/reference/capture-final-information.R`, double-JVM (two
  independent intrinsic-disabled JVM processes, byte-identical
  `capture.csv`/`nodes.csv`). The probe observes the real
  `NumericalDerivatives` behind the estimator's final round (reflection
  into the stencil's `eps` field) and *replays the whole tail at the
  estimator's own Point*, aborting if any captured bit differs from a
  fresh evaluation, so a corpus row is proof the captured tail equals the
  estimator's own numbers. Corpus:
  `validation/evidence/native/final-information/` (27 cases / 362 distinct
  nodes: 13 available, 9 inverse-failed, 5 rejected — 2 ArimaException, 3
  EcoException; modes cover interior points, boundary eps-zeroing, NaN
  coordinates, oversized theta, `freq 4`, dimension 6, `nobs <= 0`, short
  series and `eps2x`).
- `tools/gen_final_information_golden.py` → `tests/jvm_final_information.h`
  (raw-bit arrays; `yd` decimals round-trip at `%.17g`, everything else is
  big-endian hex bits).
- `tests/test_final_information.cpp` (ctest #60): bit-exact replay of all
  27 cases through a raw-bit node-table oracle — a lookup miss means the
  C++ stencil touched a node the JVM never evaluated, an uncovered node
  means it skipped one it did, and THROW nodes exercise the `newval` catch
  — plus structural checks (the step constant, the rejected base point,
  the empty-parameter degeneracy, determinism/symmetry of an analytic
  smoke case). All 27 cases replay bit-exact, OMP and serial lanes,
  ASan/UBSan clean.
- Sweep: `tools/verify_final_information.cpp` re-verifies the corpus
  end-to-end from the csv files (no golden header involved). Tier 1 (the
  producer, bit-exact, all 27 rows) replays each case through the unit with
  a raw-bit node-table callback rebuilt from `capture.csv`/`nodes.csv`
  (base node injected) and bit-compares status, fx, eps, gradient, score,
  hessian, information, covariance and diagonal, with zero table misses and
  full node coverage: **266 checks, 0 failures**. Tier 2 gates the
  deviance kernel where it is qualified: est rows demand bit-exact
  `detail::pinned_deviances_evaluate` reproduction of the estimator's own
  final ssq at the converged point (8/8), rej rows demand the matching
  rejection (5/5). Tier 3 tallies kernel-vs-node agreement as a
  diagnostic only (283 agree / 93 drift): the corpus stencil values are
  produced by the pinned `RegArmaFunction` path and the kernel's bit
  qualification covers estimator-converged paths, so 1–2 ulp drifts at
  `±sqrt(eps)` stencil nodes of the (0,1,1)(0,1,1)12 rows and a structural
  drift for the single bq = 2 row (i_dim6) are outside the 21-D.2 kernel
  domain — recorded here for follow-up, not gated; the producer surface is
  bit-pinned by tier 1 regardless of the deviance source (the replay table
  replaces it).

## Envelope

`point.size() == p + bp + q + bq` and non-negative orders (throws
`std::invalid_argument` otherwise); `nobs` is used literally — the JVM
does not guard it and the corpus pins `nobs <= 0` cases. `epsilon` and
the limits default to the pinned builder values (`STEP`, `0.99999`,
`1.0`, `check_all = true`). A `deviance` returning `nullopt` is the only
failure channel the unit models; it maps exactly onto the JVM's
"evaluation threw" cases (catch → `fx` inside the stencil, base rejection
before the stencil). The unit never calls the JVM's `getPars`/
`getAjustedPars` conventions — the point is handed in already raw.
