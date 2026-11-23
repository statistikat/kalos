# Tramo final-estimation decision contract (item 23-F, F.1 layer)

Unit: `kalos/tramo_final_estimation.{hpp,cpp}` — the decision layer of the
pinned `jdplus.tramoseats.base.core.tramo.FinalEstimator` (rjd3 3.9.0,
`jdplus-tramoseats-base-core-3.9.0.jar`), the TRAMO side of the 21-H fork.
The bytecode decode and the probe adjudication live in
`tmp_opencode/kalos-m5-pinning.md` SECTION 26 (26.2, 26.6 amendments); the
x13 side of the fork keeps its own contracts (`final-estimator-contract.md`,
`final-information-contract.md`) and stays the qualified producer for the
x13 pipeline.

## Fork verdict (closes the roadmap fork-candidate row)

`tramo.FinalEstimator` is FORKED from `x13.regarima.FinalEstimator` — not
interchangeable with `kalos::final_estimator_test`:

* `test()` has no `checkRoots` stability gate (the 0.95 slot at bytecode
  offsets 36-39 is stored and never read); every order>0 block tests its
  last coefficient directly.
* the takeover ratio starts at 99999.0 (x13: 999).
* `Bq` fires without updating the ratio (offsets 452-462); `P`, `Bp` and
  `Q` update it (offsets 179-181, 265-267, 357-359).
* the fire flags are 0/1 takeovers (not counts) and the order decrements
  are one per surviving flag; `total` is the post-takeover sum.
* `estimate()` (the F.2 round loop) adds `checkUnitRoots` / `checkCommon
  Roots` promotion stages, the `nnsig` field (`getChangedParametersCount`)
  and the `outliers && pass <= 1` failure arm absent from the x13 loop.

The shared skeleton (last-coefficient t-test with `dcmpl(v,0) > 0`,
`dcmpg(t,tsig) < 0`, `dcmpg(a,sig2) < 0` gates and the 150-sample sig2
plateau) is structural only.

## Decision semantics (probe-adjudicated, corpus-pinned)

Root space. `getRegularAR()`/`getSeasonalAR()` build
`Polynomial.ofInternal([1 | block])` — TRIMMING highest-order zero
coefficients (corpus `mr06`) — and `mirror().roots()` (fast solver: the
bit-exact `detail::p_roots` mirror, degree <= 2 closed forms, degree >= 3
the 24-A.1 QR cascade) then sees roots at the coefficients themselves.
`searchur` counts roots with scaled `Complex.abs() > ur` STRICT (dcmpl:
NaN excluded on either side — including `ur = NaN`, `sr13`; infinities
count, `sr16`), `|getIm()| <= 0.05` INCLUSIVE (dcmpg: NaN excluded,
infinite imaginary part excluded) and `getRe() > 0` STRICT (dcmpl: `-0.0`
excluded, `sr15`). Net rule: promotion fires when an AR coefficient lands
near +1 — the unit-root side of the `1 + phi(B)` convention. The 23-C
`DifferencingModule.searchur` (>= semantics, closed forms only) is a
different method; no cross-use.

`checkUnitRoots`. Gates `(p > 0 && d < 2)` (inline `iconst_2`) and
`(bp > 0 && bd < 1)`; a nonzero count promotes `p -> p - 1, d -> d + 1`
(`bp -> bp - 1, bd -> bd + 1`); both blocks run in one call; a promotion
rewrites the specification (the caller-side setSpecification resets every
parameter to the order defaults — an F.2 driver effect, audited here via
the post-promotion parameter count). `unchanged == true` is the pinned
return (true = proceed to `checkCommonRoots`; false = re-estimate). The
`MAXD = 2` / `MAXBD = 1` ConstantValue fields are record-only.

`checkCommonRoots`. Fires iff `bp == 1 && bq == 1` and
`|bphi(1) - btheta(1)| < cancel` STRICT with dcmpg polarity: `cr09` pins
the exactly-equal boundary as no-cancel, NaN never cancels (`cr04`), and
bp == 2 / bq == 0 block the rule (`cr05`, `cr07`). Firing clears `bp`,
`bq` and rewrites the specification; the return is `!cancelled`. When
`p != 0 && q != 0` the bytecode computes and inverts both regular root
arrays without ever reading them; the arrays are unobservable but the
`roots()` calls are not (the QR cascade raises where the JVM NPEs), so the
calls execute faithfully and only the results are dropped.

`test`. `sig2 = 0.15` when the estimation-domain length `<= 150` else
`0.1`; `tsig` is the builder field — 1.0 in EVERY live build (the Builder
has no `significanceThreshold` setter; the probe injects other values by
final-field reflection only to parameterize the port). Block order P, Bp,
Q, Bq on the running index `p, p + bp, p + bp + q, p + bp + q + bq` over
the `[phi | bphi | theta | btheta]` layout; the asymptotic-covariance
diagonal indexes the same positions and covers ARMA coordinates only (the
mean is not a coordinate — corpus `tc_vdiag` with `mean = true` has length
`p + bp + q + bq`). Per-block gates: `v > 0` STRICT (NaN variance skips),
`t = a / sqrt(v)` with `dcmpg(t, tsig) < 0` and `dcmpg(a, sig2) < 0` (NaN
never fires); Bp/Q/Bq additionally require `dcmpg(ratio, t) > 0` STRICT —
the `tc04` golden pins the blocked-at-equal case (ratio 0, t 0) and `tc05`
the strictly-below takeover at a 1-ulp t difference. Firing clears the
dominated flags as above. `total == 0` leaves the specification untouched
(no clone, no event); otherwise one decrement per surviving flag, the
"arima spec changed" info event fires with the NEW orders rendered as
`(p,d,q)(Bp,Bd,Bq)` (`info(String, Object)` routes to the three-argument
interface as `(msg = null, obj = message, det = orders)` — the exact
shape pinned in the capture), then `setSpecification`.

## Request envelope and fail-closed behavior

All parameter vectors are RAW `SarimaModel` coordinates; the AR blocks use
the `1 + phi(B)` sign convention (the Kalos-convention blocks of the
estimation layer flip signs at the caller). Validation mirrors the JVM
read sites — the pinned `SarimaModel` cannot represent a block shorter
than its order and the private methods touch only the blocks their gates
reach, so the guards fail closed exactly there and nowhere else: negative
orders throw `std::invalid_argument` in every entry; `checkUnitRoots`
length-checks the `phi`/`bphi` blocks when their gate opens (`p > 0`
scanned under `d < 2`, `bp > 0` scanned under `bd < 1`);
`checkCommonRoots` length-checks `phi`/`theta` only in the `p != 0 &&
q != 0` roots branch and `bphi`/`btheta` (non-empty) only under the
`bp == 1 && bq == 1` rule, accepting short unused blocks; `test` throws
when `parameters` or `asy_covariance_diagonal` is shorter than
`p + bp + q + bq`, the last position it indexes. A QR cascade that
exhausts (JVM: null roots -> NPE) throws `std::runtime_error`; in the
live `estimate()` driver such an escape is caught by its RuntimeException
handler (remark "estimation faied" — the jar typo, verbatim) and belongs
to the F.2 layer.

## Round loop and Point tail (F.2)

`tramo_final_estimator_run` mirrors `estimate()` over the qualified
`estimate_sarima_fixed` seam: one `"full estimation"` push before the loop
(the log context stays open across rounds), AT MOST five round bodies
(`iconst_5 if_icmplt`), and per round `estimate(computer)` assigns the
estimation — including the point-only tail — before any gate reads it.
Short-circuits after the assignment (`dim == 0`, corpus `es02`/`es05-R1`/
`es08-R1`; `!ami`, corpus `es03`) pop and return true with the fresh
estimation in place; the `nnsig` field keeps its previous value there
(`es08` returns true with `nnsig == 2` left by an earlier round). A false
`checkCommonRoots` sets `nnsig = 2` AND rewrites (`bp == 1 && bq == 1` with
`|bphi(1) - btheta(1)| < cancel = 0.044`: `setBp(0)`, `setBq(0)`,
`setSpecification` -> estimation null, pars cleared, corpus `es05`/`es08`);
otherwise `nnsig = test()`. `nnsig == 0` returns true, `nnsig == 1`
continues, and `nnsig >= 2` with `outliers && pass <= 1` pops and returns
false WITHOUT a remark; the loop exhaustion is the same silent false. The
`"estimation faied"` remark (jar typo, verbatim) fires ONLY in the
RuntimeException catch.

The Point tail is `RegArmaProcessor.compute`'s exact-derivatives branch
(the FE builder's `computeExactFinalDerivatives`), produced by the 21-H
`final_information` unit: `LinearModel.calcResiduals` regresses the landing
allCoefficients off the differenced response (the mean column included),
the DESIGN-FREE deviance machine re-runs on those residuals under the
SarimaMapping2 stencil, and the score/information scale with
`0.5 * nobs / fx` (`fx = Likelihood.ssq() * factor`, the pinned `ssq_e`)
on the cdf nobs `|y_d| - X.getColumnsCount() - dim`. The concentrated
deviance can be indefinite at the landing (`es01` variance 1 = `-0.0989…`,
negative), which the pinned `SymmetricMatrix.inverse` Cholesky-then-
Doolittle fallback absorbs; a singular inverse mirrors the MatrixException
into the round catch. The Gauss-Newton curvature of the minimizer is NOT
this quantity and reaches no observable here.

## Corpus and gates

`validation/evidence/native/tramo-final-estimation/`: the split-package
probe (`Scratch23f.java`, reflection only for the four private ladder
methods and the final `tsig` field — every decision runs the pinned
bytecode), the 75-case request file (`cases-fe1.txt`: 18 searchur polarity
cases, 10 mirror-root cases over degrees 1-6 including the QR cascade, 19
checkUnitRoots cases, 9 checkCommonRoots cases, 9 live-landing cascade
cases with their exact asymptotic-covariance diagonals, 10 live
estimate()-driver cases), two byte-identical captures, a third run with
the `-Xlog:class+load` witness (`FinalEstimator` + `FinalEstimator$Builder`
from the pinned jar, ZERO `x13.regarima.FinalEstimator` loads), the probe
runner and metadata. `tests/jvm_tramo_final_estimation.h` is generated from
the capture by `tools/gen_tramo_final_estimation_golden.py` (every double
as raw IEEE-754 bits); `tests/test_tramo_final_estimation.cpp` replays all
decision rows bit-exactly plus hand-built fork edges (ratio start and
takeover direction, Bq no-update, NaN/strict-boundary polarities, the
cancel exactly-equal boundary, validation throws).

The 10 `fe_est` rows are ACTIVE with F.2: `tramo_final_estimator_run`
replays them bit-exactly on every slot (return, `nnsig`, mean, orders,
landing parameters, estimation state, cl dim/ssq/allCoefficients, the
asymptotic-covariance diagonal and the event log), including the
two-round cancel flows (`es05`, `es08`) and the point tail witnesses
(score/information) diffed bit-for-bit against the per-round capture.
