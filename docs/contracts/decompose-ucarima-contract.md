# SEATS decomposition seam mirror (24-B)

Unit-qualified transcription of the LIVE pinned decomposition seam that
`DefaultModelDecomposer.decompose` + `UcarimaModel.setVarianceMax(-1, noisy)`
+ `SeatsModel.compactUcarimaModel(false, false)` form between admissibility
validation and SEATS filter construction. `kalos::decompose_ucarima` replays
that seam on a `tramo_seats_model_request`: it runs the 24-A validator,
applies the D6 fail-closed envelope, then mirrors (bit-exactly) the raw
selector-chain split ("r"), `setVarianceMax(-1, false)` ("d"), the compact
reduction ("cmp") and the deterministic SeatsException status row, emitting
the fact grammar of `validation/reference/DecomposeUcarimaProbe.java`
line for line.

```cpp
kalos::tramo_seats_model_request req;   // same request the 24-A unit takes
auto d = kalos::decompose_ucarima(req);
// d.decision()  - the 24-A decision (pass-through)
// d.facts()     - "d.decompose", d block, "cmp" block, "r.decompose",
//                 "r.count", r cN blocks, optional "status error:...Seats
//                 Exception:Invalid decomposition"; per-getter JVM
//                 failures appear as "cN maerr <class>" / "cN varr" /
//                 "cN sfdr" lines carrying the probe's dangling prefix.
```

## Capture and corpus

`validation/reference/DecomposeUcarimaProbe.java` (public entry points only:
the validator, the toolkit-wired decomposer and the public `UcarimaModel`/
`ArimaModel` getters; raw getters, `%.17g` Locale.ROOT doubles, space-
separated rows — pinning notes 6.1/6.2) driven by
`validation/reference/capture-decompose-ucarima.R` (rJava, OpenJDK 21), 49
cases captured twice from independent JVM processes with byte-identical csv
output. Corpus: `validation/evidence/native/decompose-ucarima/` (`cases.csv`,
`inputs.csv`, `model-facts.csv`, `SHA256SUMS.csv`). 32 cases replay (21 FULL,
2 Eco-status, 9 SeatsException-status fact sets — 1620 fact lines total);
17 requests (16 Legacy `approximation_required`, 1 Noisy) are pinned
refusals. `tools/gen_decompose_ucarima_golden.py` harvests the corpus into
`tests/jvm_decompose_ucarima.h` (the golden unit `test_decompose_ucarima`)
and `tools/verify_decompose_ucarima.cpp` re-drives the same corpus from the
raw csv files.

## D6 envelope (fail-closed refusals)

`decompose_ucarima` throws `std::invalid_argument` with the pinned reason
before the seam for exactly two classes, both classified by the validator:

* `request.mode == seats_approx_mode::noisy` →
  `"decompose_ucarima: Noisy approximation mode is outside the D6 envelope"`;
* `decision().approximation_required` (Legacy requests whose live
  `setVarianceMax` returned null, i.e. the JVM continues into the crashing
  `DefaultModelApproximator`) →
  `"decompose_ucarima: approximation_required decision is outside the D6 envelope"`.

The approximator is NOT ported (register D6, APPROVED policy: the validator
already classifies the class, the unit refuses rather than reproducing a
crash). Validator refusals propagate unchanged. Inside the envelope the
seam itself never throws: every JVM abort class (MatrixException, NPE,
AIOOBE, ArithmeticException) is captured as data.

## Numeric spine (bytecode → unit)

All transcriptions in `src/decompose_ucarima.cpp` name their pinned javap
dump (`tmp_opencode/jdprobe/m524/…`); operation order and dcmpg/dcmpl
flavors (`gt_g/lt_l/...`) are preserved and the build pins
`-ffp-contract=off`.

* **SymmetricFilter.factorize** (drives `ArimaModel.getMa()`/`getIV()`):
  degree-0 shortcut, then the factorizer chain EV2 → EV1 → null. A live
  JVM chain probe over ALL 150 distinct corpus symmetric filters settles the
  mirror scope: 124 resolve in EV2 (`EigenValuesDecomposer2`), 5 in EV1
  (`EigenValuesDecomposer`: the FULL-degree companion, coefficients into
  `column(degree-1)` rows 0..degree-1 via `setAY(-1/a[n], …)`, `(0,n)=1`,
  `|z| > 1` root filter, leja + `fromComplexRoots`), 1 in the fast
  decomposer and 20 in none. The single fast-tier filter belongs to a
  validator-refused case, so EV2 → EV1 → null reproduces every corpus ma/
  var/sfd line; the fast/robust tiers are out of envelope.
* **EV2 spine**: shared `removeUnitRoots` (UnitRootsSolver factorize,
  `UnitRoots.sqrt` with its stale-capacity/AIOOBE quirks, exact LS divide
  with the discarded `divide()` return and the unconditional `getQuotient`
  NPE, symmetrization `(a + a*)/2`, trim), the z = x + 1/x companion
  (`column(m-1) = -wts/wts[m]`, `subDiagonal(-1).add(1)`, `(0,1) += 1`,
  superdiagonal `add(1)`), `GeneralEigenSystem` (array copy, `balance`,
  `isHessenberg` guard, `hessenbergQR` maxIter 120), the real-pair /
  unit-circle classification (dedup `|re - ur[j]| < 1e-6`, the JVM array
  bounds as faithful AIOOBE traps), `lejaOrder` + `fromComplexRoots` +
  the `w0 / ssq(bf)` scaling.
* **Spectral minimization** (`setVarianceMax` loop): dispatch
  `denom.length() <= 6` where `SymmetricFilter.length() = 2*degree+1` —
  in stored half-coefficient counts the by-roots tier applies only when
  the count is `<= 3`; longer denominators take the bracketed-Newton grid.
  `min == 0` skips the component (`dcmpl ifeq`: NaN proceeds), `m < 0`
  returns null.
* **compact**: `compact(2, 2)` iff components > 3, Builder with verify
  off, exactly as `SeatsModel.compactUcarimaModel(false, false)`.
* **Rendering**: `fmt_g` reproduces Java `String.format(Locale.ROOT,
  "%.17g")` — the SHORTEST round-tripping decimal (std::to_chars) PADDED to
  17 significant digits (Java never strips: `1` renders
  `1.0000000000000000`), scientific iff exp < -4 or exp >= 17. Plain C
  `%.17g` is NOT equivalent.

## Implementation-actual deltas at landing (24-B)

Transcription bugs found and fixed while reaching bit-exactness (each
verified against the bytecode and the corpus):

* `hessenbergQR` left ("row") update: `s2 = a[pos3] + v2*a[pos3+1] (+
  w2*a[pos3+2])` — slots hold v2/w2, not hv/hw; `jm = min(k, i+3)`; the
  `m == 0` path carries cu/cv/cw across the look-ahead and skips the
  reload at @925.
* `ComplexMath.sqrt` re<=0 branch returns `cart((0.5*im)/t, t)` (real part
  FIRST argument position `(0.5*im)/t`).
* `UcarimaModel`/`ArimaModel` chain tiers: the EV1 decomposer is LIVE
  (5 corpus components), the original EV2-only scope note was wrong; its
  coefficients sit in `column(degree-1)`, not the last column.
* Minimizer dispatch compared the stored half-length against 6 instead of
  the JVM span `2*degree+1` (quarterly seasonal components take the grid
  tier, not by-roots).
* `detail::jvm_decompose_abort` does not derive from `std::exception`; the
  seam driver needed explicit catches mirroring the JVM try around the
  chain + `setVarianceMax` (MatrixException → `d.decompose null`).
* `balance` shrink loop: `dcmpl ifle` — NaN EXITS the shrink loop.
* The probe's StringBuilder dangling prefix makes a failing getter part of
  the NEXT line: `key ma key maerr <class>` as ONE emitted line.

Recorded facts (not bugs): the classification AIOOBE bounds, the always-
recomputed `larfg` norm, ASCENDING `applyQt`, `sign_like` != copysign, the
`simplifyAr` tail always running, and a `d.decompose null` outcome with
`r.decompose ok` (quarterly D=1/B=1 families) are all legal corpus
behaviour, reproduced as data.

## Downstream caveat (D1)

This lands the UCARIMA seam only: the SEATS WK-period Wiener filters still
consume the WK-only ucarima, so the cmp/d split here is not yet what the
seasonal component is built from; 24-C wires the seam into the pipeline and
re-pins WK-vs-sfd.

## Gates

`test_decompose_ucarima`: 49 corpus cases (32 byte-identical fact replays +
determinism double-run, 17 pinned refusals), bit-exact in BOTH the OpenMP
and serial lanes. `tools/verify_decompose_ucarima.cpp` re-drives the raw csv
corpus: 32 cases / 1620 lines exact, 17 exact-reason refusals, 32 grammar
audits, 0 failures. ASan/UBSan builds of unit and sweep: clean. Existing
suite unchanged; `tools/check-manifest.py` and `tools/verify-migration.py`
green.
