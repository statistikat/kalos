# SEATS model admissibility and decomposition mirror (24-A)

Unit-qualified transcription of the LIVE pinned jdplus 3.9.0 SEATS path
between the `SeatsKernel.validate` and `SeatsKernel.decomposeModel` seams:
`DefaultModelValidator` (the XL parameter-cutoff machine), the
`SeatsToolkit.of` selector chain (`TrendCycleSelector` + `SeasonalSelector`)
and `RootDecomposer` (the split linear system), and the decomposability
decision of `UcarimaModel.setVarianceMax(-1, noisy)` (the
`Spectrum$Minimizer` minima-sum test), plus the `SeatsKernel` flags and the
`ProcessingLog` frame they emit. It mirrors DECISIONS and the raw
(pre-variance-allocation) component structure bit for bit. It does NOT
implement what happens after a null decomposition
(`DefaultModelApproximator`, decision register D6 — the unit reports
`approximation_required` and stops; the live record pins what the kernel then
did), the spectral factorization of the split components
(`SymmetricFilter.factorize` → the one-sided MA and innovation-variance
facts, `getMa`/`getInnovationVariance`), or the variance allocation itself —
those are item 24-B scope; parameter-estimation uncertainty is 24-C.

Bytecode sources: javap decodes `DefaultModelValidator`, `SeatsToolkit`,
`SeatsModel`, `SymmetricFilter`, `BackFilter`, `Polynomial`, `RootsSolver`,
`RootDecomposer`, `ModelDecomposer` (selectors), `UcarimaModel`, `LULinear
SystemSolver2`, `FastMatrix` under `/data/home/kowa/seasonaladjustment/
tmp_opencode/jdprobe/{m5,m524}/` at slice time; the double-captured corpus is
the durable behavioural record.

```cpp
kalos::tramo_seats_model_request req;        // the frozen all-Fixed SarimaSpec
req.d = 1; req.bd = 1;                       // orders ARE the vector lengths
req.theta = {0.5}; req.btheta = {0.4};       // (probe "spec/phi/theta/..." lines)
req.xl = 0.95; req.mode = kalos::seats_approx_mode::legacy;
auto v = kalos::validate_tramo_seats_model(req);
// v.parameters_cut_off + validated orders/parameters (v.* facts),
// v.raw_components (sar/ur/sfd, pre-allocation), v.variance_max_ok (d.decompose),
// v.approximation_required / v.seats_exception / v.noisy_model_used /
// v.model_changed, v.events (the kernel validate+decompose frame)
```

## Capture and corpus

`validation/reference/TramoSeatsModelProbe.java` (public-getter observer: raw
doubles via `%.17g`, enum constant names, ordered ProcessingLog events with
kind/depth/message — pinning notes 6.1/6.2; it drives the LIVE pipeline
exactly as `SeatsKernel` does, including full `SeatsKernel.process` for the
kernel flag/event surface) driven by
`validation/reference/capture-tramo-seats-model.R` (rJava, OpenJDK 21, the
G2 intrinsic-disable recipe with a live guard), 35 cases captured twice from
independent JVM processes with the `--repro` byte-identity gate. Corpus:
`validation/evidence/native/tramo-seats-model/` (`cases.csv` decision matrix,
`inputs.csv` protocol `src` + `req`/`values` lines, `model-facts.csv` v./r./
d./k. facts, `model-events.csv` the `evt`+`kevt` streams, `metadata.R`,
`SHA256SUMS.csv`). `tools/gen_tramo_seats_model_golden.py` harvests the
assertable grammar into `tests/jvm_tramo_seats_model.h`; the shared driver
`tests/tramo_seats_model_replay.h`, `kalos::validate_tramo_seats_model`,
`tests/test_tramo_seats_model.cpp` and `tools/verify_tramo_seats_model.cpp`
complete the layer.

Numeric comparison is IEEE-754 BIT equality: golden doubles are `%.17g`
captures, re-parsed through `strtod` (exact round trip) and bit-compared — a
formatter-independent comparison. At landing every golden-asserted numeric
(validator parameters, the raw `sar`/`ur`/`sfd` component coefficients) is
bit-exact on all 30 non-refused cases; no measured band was needed. The
minima-sum `variance_floor` is exposed for the sweep report; the decision
uses only its sign, as `setVarianceMax` does.

## Pinned semantics (bytecode → unit)

* **Validator order** (`validate`): `simplifyModel()` then `changeMA()` then
  `changeAR()` (whose body is commented out in 3.9.0 — a verified no-op);
  returns true only when nothing fired. `parameters_cut_off` is `false` from
  the validator's return, and the kernel emits "arima parameters cut off"
  when the model changed.
* **`simplifyModel`**: ONE pass of `toBuilder().adjustOrders(true).build()`;
  `canSimplify` is non-empty AND `|last| < 1e-6`; `Builder.adjust` trims
  trailing `|v| < 1e-6` from all four parameter vectors at once (the
  `v_tpl_zero` row trims an airline to (0,1,0)(0,1,0) + cutOff).
* **`changeMA` with xl < 1** (`stabilizeMA`): degree-1 vector → clamp to
  `±xl` (strict `lt`/`gt` — a value AT ±xl is kept, the boundary-airline row
  lands at exactly −0.95 with cutOff); degree ≥ 2 → roots, every root with
  `1/|z| > xl` rescaled to modulus `1/xl`, `fromComplexRoots`, divide by
  c(0), keep the first q coefficients.
* **`changeMA` with xl ≥ 1** (`fixMaUnitRoots`, `lim = 1 − 1e-4`):
  `btheta[0]` outside `±lim^period` → ∓1/`+1`; regular `theta` degree-1
  outside `±lim` → **−1 BOTH ways** (the released quirk, transcribed as-is,
  never +1); degree ≥ 2 → roots with `|z| < lim` projected onto the unit
  circle, rebuilt via `fromComplexRoots` keeping the first q coefficients
  (the `v_fixma_q2proj` row projects {≈1, 4} to {1, 3.5617…}).
* **Polynomial identities** (three JVM bit-probes re-qualified this slice,
  see `tmp_opencode/kalos-m5-pinning.md` §8): (1)
  `Polynomial.fromComplexRoots` is the faithful scaled `∏ (z − root)` — the
  leading copy IS the B^(i+1) term and the inner loop starts at index i;
  (2) `SymmetricFilter.convolutionOf` receives the BackFilter coefficients
  REVERSED (`BackFilter.weights()` is `k → polynomial.get(-k)`, lower bound
  −degree), i.e. the symmetric convolution accumulates per-lag products in
  descending order (bit-visible on non-palindromic inputs); (3)
  `AbstractArimaModel.symmetricMa()` is `convolutionOf(ma, OWN variance)` and
  the SarimaModel's own variance is the SarimaSpec default **1.0** —
  `SeatsModelSpec.innovationVariance` NEVER reaches the decomposition (golden
  `model var 2.0` is the SeatsModel echo; `k.ucar` reports 1.0 and the
  variance-free `r sfd` facts pin it). Seasonal MA sign: `bq = 1` keeps the
  `btheta` sign (`p_factor(-btheta[0], period)`).
* **Selector chain** (exactly as `SeatsToolkit.of` wires it):
  `TrendCycleSelector(rmod)` with the default low-frequency threshold, then
  `SeasonalSelector(period, seasTolerance)` with
  `k = (bd > 0 || bp > 0) ? smod : smodAtPi`. The greedy `D1` division fires
  per repeated unit root (on (1−B)(1−B¹²) it produces the D2 part
  `m_sur = [1, −2, 1]` and `m_nur = [1]` twelve times).
* **`RootDecomposer.calc`**: `m_sfds = conv(sar·sur)`, `m_sfdn =
  conv(nar·nur)`; system dim `(n1+1)+(n2+1)` with `n1 = max(ub−1, 0)`,
  `n2 = (na ≥ nb ? na−ub : max(uc−1, 0))`; rhs from the symmetric MA with
  `a[0]/2` before the fill; `x1[0]` and `x2[0]` doubled after the solve;
  `checkSpecialCases` fires iff `m_sfds.length() == 1` (single component with
  `sfd = [0]`, variance-free).
* **LU** (`LULinearSystemSolver2`, eps 1e-13): flat column-major
  `n × (n+l)` array (stride n, `FastMatrix` layout), `ipiv` of length `n+1`
  pre-filled −1, solution block at offset `n·n`, readback
  `x[i] = a[ipiv[i] + n·n]`; pivot scan is `dcmpl`+`ifle` (a NaN candidate is
  NOT taken). A singular system throws — `r.decompose = "null"` (the
  `decomposition_threw` mirror).
* **`setVarianceMax` mirror**: per raw component the spectral minimum —
  `den.length() ≤ 6` → `minimizeByRoots` (SFR integrals, candidates {0, π/2,
  π} plus the real roots of `w = P'Q − PQ'` in (−1,1) via `acos`, deflating
  `minimizer_evaluate`); else `minimizeByGrid` (denominator-bracketed
  Newton). Zero-minimum components are skipped; `ok = noisy || !(sum < 0)`.
  `d.decompose` is this mirror's `variance_max_ok`.
* **Kernel state machine**: `approximation_required` ⟺ mode ≠ None ∧ um null;
  `seats_exception` ⟺ mode None ∧ um null ("non decomposable model" +
  "canonical decomposition failed"); the "noisy model used" warning fires
  iff mode Noisy ∧ sum < 0 (and only there `model_changed` may be 1 outside
  the D6 continuation); legacy_approx cases stop at `approximation_required`
  and the unit's event stream is an exact PROPER PREFIX of the captured
  `kevt` stream (the live continuation is D6 territory; their captured
  `k.modelChanged` is skipped).

## Mode observations and fail-closed matrix

Corpus decision matrix (recorded as data, never re-implemented): mode **None**
with a negative minima sum raises `SeatsException("Invalid decomposition")`
(3 cases); mode **Legacy** with a negative sum hands the model to
`DefaultModelApproximator` — 13 cases are `legacy_approx`, of which two
(`v_fixma_pos`/`v_fixma_neg`, the fixMa degree-1 quirk) crash probe-side in a
`MatrixException` INSIDE the approximator after the unit-scope prefix
completed, and `d_nondec_noisy` crashes probe-side with the Noisy-mode NPE
(their `kevt` streams are complete and asserted); mode **Noisy** mirrors the
warning branch. `legacy_approx = 1 ⟺ mode == Legacy ∧ d.decompose == null`.

`validate_tramo_seats_model` refuses (`std::invalid_argument`, register D11):
period or frequency outside {4, 12}; non-empty `bphi` (its root set needs the
degree > 2 `FastEigenValuesSolver.qr` cascade); stationary AR degree > 2; MA
degree > 2 (outside the measured corpus band); negative d/bd; any non-finite
parameter or knob; and any request whose live path enters `roots()` of degree
> 2 (stabilizeMA/fixMaUnitRoots/minimizer `w`). Five corpus rows are
REFUSAL-asserted (their JVM path enters the QR cascade): `d_d2`, `q_airline`,
`q_d2` (degree-4 minimizer numerator), `v_cut_bq2` (degree-10 minimizer
numerator on the bq=2 split), `v_simplify` (stationary `bphi` reconstruction).
Their captured facts remain in the csv corpus as 24-B evidence.

## Implementation-actual deltas at landing (24-A)

* **`SymmetricFilter.convolutionOf` and `symmetricMa` variance** were
  transcribed wrong in the M5-open sketch and are now pinned by JVM raw-bit
  probes (root causes in `tmp_opencode/kalos-m5-pinning.md` §8): the
  BackFilter reversal (2) and the SarimaSpec-default variance (3) are each
  bit-visible in the corpus (`d_ar1_slow` lag7/lag10 signatures; `d_wn`
  var=2 → `r c2 sfd [1.0]`).
* **`Polynomial.fromComplexRoots`** initial leading-coefficient handling was
  re-derived from bytecode plus a clean-roots probe (`{2, 3}` → monic-scaled
  `∏`): the copy `z[i+1] = z[i]` is load-bearing, the inner loop starts at
  index `i`.
* `d_d2` is MONTHLY, not quarterly: degree-4 minimizer numerators are
  reachable monthly (`q = 2` plus D2), so the QR-cascade refusal class spans
  both corpus shapes — recorded, refused, not widened.
* The `variance_max_ok` mirror consumes the RAW component structure; the
  captured `k.ucar` variances and `r c{i} ma|var` allocations stay
  record-only (24-B).

## Gates

`test_tramo_seats_model`: 35 corpus cases — 30 replayed with bit-exact facts
(validator v.\*, raw `r` sar/ur/sfd, `r.count`, `r.decompose`, the
`d.decompose` mirror, `k.cutOff`, `k.modelChanged` where not D6-skipped),
exact kevt frames (exact-prefix for the 13 `legacy_approx` rows), flags from
the decision matrix, plus the 5 refusal-asserted D11 rows: 860 checks, 0
failures, bit-exact in BOTH the OpenMP and serial lanes (66/66 each, fresh
trees). ASan/UBSan builds of unit and sweep: clean.
`tools/verify_tramo_seats_model.cpp` re-drives the raw csv corpus (the
evidence layer itself, not the generated header): 35/35 cases, 896 checks
(including the determinism double-run and the six envelope-refusal probes).
Existing suite unchanged: the nine sweeps byte-identical, the full automatic
sweep 71401 checks 0 bad (13/13), the tramo-builder sweep 22/22,
`verify-migration.py` 406.
