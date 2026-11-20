# Kalos implementation status and next-work plan

Updated: **2026-11-14**. Numerical scope through **22b.2** is preserved. The core
is now an independent C++17 library, without R, Rcpp, Python or Java dependencies.
Sibling bindings consume pinned generated snapshots. Persephone will require
rkalos; Python intentionally exposes only a minimal initial surface.

## Repository split delivery

1. **Completed — freeze/history.** Original commit `8450d6e`, 713-file baseline,
   preserved planning inputs, filtered histories, original commit/path maps.
2. **Completed — provider boundary.** Nine checked BLAS/LAPACK operations,
   standalone OpenBLAS 0.3.34 LP64 provider, separate R provider, CMake target
   `kalos::kalos`, installed headers, optional owned OpenMP and serial fallback.
   A joint provider/OpenMP runtime probe detects incompatible runtime libraries.
3. **Completed — shared preparation.** Integer domains/windows, regressor
   coverage, fixed/free coefficients, mean/drift origins and log-Jacobian policy
   now live in C++; R retains conversions and result formatting.
4. **Completed — pinned export.** Explicit allowlist, source/fixture checksums,
   exact core revision, generated version header and offline verification.
5. **Completed — language bindings and reference migration.** The R package
   and minimal Python binding call the same pinned Kalos core. Active reference
   commands use the new repositories and audit the original capture hashes.
6. **Completed — packaging and publication.** The standalone source archive
   and installed CMake consumer, R source package, and Python wheel passed
   local checks. The repositories are published under `statistikat` on GitHub.

## Preserved numerical defaults and capabilities

- Fixed-order SARIMA likelihood/regression/optimization, structural diagnostics,
  observed information, conditional/joint regression uncertainty.
- Accurate final-fit covariance by default; explicit Java-free legacy comparison
  covariance. Accurate residual/interpolation reporting by default, with the
  requested Java-free legacy reporting option.
- Conditional interpolation and forecast/backcast distributions, original-scale
  Gaussian/lognormal moments and optional profile-ML ARMA uncertainty.
- X-11 symmetric/Musgrave/seasonal endpoints, fixed seasonal processing,
  Henderson selection, stage policy, MSR tables/pilot/selection, additive
  normalization and B1/B2/B3/B4a/B4anorm/B4d initial tables.
- Both MSR iteration policies; **JDemetra behavior remains the default**.
- Full X-11 passes, automatic preprocessing, TRAMO/SEATS and seasonal-adjustment
  fitting are not delivered. No capability/default has been promoted by the split.

## Validation so far

All 37 standalone C++ programs pass in Release serial and real OpenMP builds
on macOS ARM64, using OpenBLAS 0.3.34. The R preparation slice passes 4,281
assertions. The relocated initial-X-11 verifier passes 10,090 checks with the
unchanged 2e-12 budget and explicit missing-policy audits. Further artifact
results and timings will be recorded after final qualification.

OpenBLAS source checksum was verified against the downloaded 0.3.34 tag archive;
provider notices/settings are in `dependencies/`. A macOS duplicate-runtime
conflict was resolved by selecting the same libomp as the provider, with a
configuration probe and documented serial fallback; no bypass setting is used.

## Detailed next numerical steps

| Item | Implementation | Acceptance / decision |
| --- | --- | --- |
| 22b.3a | Moving five-year dispersion and two-pass sigma weights; accurate and Java-free legacy rules | **Resolved 2026-11-17: `legacy` is the product default** (the rule proven bit-exact against the pinned `DefaultExtremeValuesCorrector`), applied at the binding/product layers; the core keeps the policy a required argument and delivered both rules unchanged. |
| 22b.3b | Same-phase replacement neighbors, endpoints and correction factors | Independent formulas, neighbor provenance, insufficient-neighbor cases, public getters, cancellation and serial/OpenMP equality. |
| 22b.4 | Complete fixed additive B/C/D passes, then connect qualified selector stages | Preserve intermediate tables and correction history; validate decomposition identities and original/extended calendars. |
| 22b.5 / 22c | Other decomposition modes, then a restricted fixed-model X-13 fit | Qualify multiplicative/log/pseudo-additive and deterministic-effect allocation before exposing a fit capability. |
| 21 / 23 / 24 / 25 | Automatic X-13 preprocessing, TRAMO, SEATS, diagnostics and benchmarking | Separate method-specific numerical and decision-boundary qualification; no default rsa3 support until its full pipeline exists. |
| 16c / 18d.2 / 26 | Distribution review, difficult models and wider platform evidence | Preserve original reference bytes/tolerances; independent invariants and installed artifacts. Product-level parity remains Persephone-owned. |
| Performance | Profile airline/mixed-factor fits and long seasonal/filter workloads | Separate arithmetic, evaluation counts, provider and boundary costs. Repeat matched JDemetra/native timings after changes; no tolerance relaxation. |

The [detailed roadmap](docs/NUMERICAL_ROADMAP.md) retains all acceptance rules.
The [split plan](docs/REPOSITORY_SPLIT.md), [contracts](docs/contracts/),
[external-library evaluation](EXTERNAL_LIBRARY_EVALUATION.md) and
[historical baseline](docs/history/persephone-implementation-baseline.md) retain
provenance and prior evidence. Each next numerical slice is a separate commit,
with this status updated after validation.

## Standalone and migration qualification

All **37/37** C++ programs also pass address/undefined-behavior sanitizer builds.
The migration audit confirms **406** unchanged historical files. Base and extended
corpus integrity verifiers pass offline. Linux/macOS standalone and Linux/macOS/
Windows R CI lanes are configured; standalone Windows qualification remains
explicit future work, and no remote lane is claimed to have run yet.

## Linux (x86_64) baseline qualification — 2026-11-15

Machine: Ubuntu 24.04 x86_64, g++ 13.3.0, CMake 3.28.3, branch `main` at
`a01473e`. Provider: pinned OpenBLAS **0.3.34** (source archive sha256
`cd7e129868320cc2d033afa920e31202dfe0b8066a5b66661900ccc0f197dfed` per
`dependencies/openblas.lock.json`) built with `tools/build-openblas.py` to
`/tmp/opencode/openblas` outside the repository (2,605 LAPACKE symbols;
installed library sha256 `84de4eab95a0f3e056b6efee2e7fd7c3731a9bec2f4552401f2b1fdf90420ae3`
for `libopenblas-r0.3.34.so`). Builds were out-of-tree.

Provider resolution required an environment workaround: this machine has the
system `libopenblas-pthread-dev` **0.3.26** (LP64, CBLAS, but **no** LAPACKE
exports) whose `openblas.pc` is preferred by `find_package(KalosOpenBLAS)`
HINTS over `-DOpenBLAS_ROOT`, silently mixing provider headers with the
system library and failing test links on `LAPACKE_*_work`. Configuring with
explicit cache entries `-DKalosOpenBLAS_INCLUDE_DIR=/tmp/opencode/openblas/include
-DKalosOpenBLAS_LIBRARY=/tmp/opencode/openblas/lib/libopenblas.so` resolves
both to the pinned provider. Find-module precedence is an open M0 follow-up;
no CMake source was changed for this qualification. A global
`LD_LIBRARY_PATH` containing `/usr/lib/x86_64-linux-gnu` also shadows the
provider's SONAME at runtime and must be unset when running the tests.

Configure probes against the pinned provider: `KALOS_LP64_PROVIDER`
**success**; OpenMP 4.5 found and `KALOS_OPENMP_RUNTIME_OK` **success**
(OpenMP build enabled, libgomp). Release OpenMP build and serial build
(`-DKALOS_OPENMP=OFF`): **zero warnings** under `-Wall -Wextra -Wpedantic`.

Test programs (37 total):

| Configuration | Passed | Failed |
| --- | --- | --- |
| Release OpenMP build, default harness (per-test property pins `KALOS_NUM_THREADS=2`, `OMP_NUM_THREADS=2`, `OPENBLAS_NUM_THREADS=1`) | 34 | 3 |
| Same, `ctest` re-run with `KALOS_NUM_THREADS=1` and `=4` in the shell | 34 / 34 | 3 / 3 (the per-test `ENVIRONMENT` property overrides the shell variable, verified; these runs are therefore not a true serial/4-thread change) |
| True serial build (`-DKALOS_OPENMP=OFF`, `ctest`) | 34 | 3 |
| OpenMP binaries run directly with `KALOS_NUM_THREADS=4`, `OMP_NUM_THREADS=4` | 34 | 3 |
| gcc ASan+UBSan build (`-DKALOS_SANITIZERS=ON`, Debug; `ctest`, leak detection disabled because the uninstrumented provider would false-positive on its retained thread arenas) | 34 | 3 |

The **same three** programs fail identically in every configuration above:
`test_diffuse_state_space` (uncaught `std::underflow_error` from the
`long double`→`double` checked conversion in `finite_state_factor.cpp`,
observed value ≈ `-1.5e-347`), `test_forecast_arma_information` (status
`"derivatives_unresolved"`; 18 of 19 vector-Jacobian evaluations rejected as
`"prediction_projection_range"`), and `test_regression_likelihood`
(`covariance_underflows` sees `"regression likelihood arithmetic underflows"`,
value ≈ `1e-400`, thrown before the platform fallback that produces the
expected `"regression coefficient covariance underflows"`). Root cause family:
on x86-64 `long double` is 80-bit extended (exponent range to ~3.4e-4932), so
guarded intermediates below the double subnormal range are nonzero and throw,
while on the macOS ARM64 baseline `long double` is double-width, those
intermediates are already zero and the guards are vacuous. These are platform
numerics findings for milestone M1 (items 22b.3a/3b territory), not BLAS,
thread-count or configuration faults — results are identical serial, 2- and
4-thread, Release, Debug and sanitizer builds. No sanitizer diagnostics
(AddressSanitizer or UndefinedBehaviorSanitizer) were produced by any test.

External consumer check passed: a separate project with
`find_package(kalos CONFIG REQUIRED)` and `kalos::kalos` (installed to
`/tmp/opencode/kalos-install`, same explicit-provider cache variables) built
and printed core version `0.1.0` with embedded revision `a01473e`.

Not run on Linux: Windows; Clang toolchains; R/Python binding lanes; remote CI
lanes. macOS evidence above remains as previously recorded. No capability or
default has been promoted by this qualification.

## Platform-independent range-guard semantics — 2026-11-15

Resolved the three Linux x86_64 findings above at provider-independent source
level. Decision (full rule and rationale in
[extended-precision-range-contract.md](docs/contracts/extended-precision-range-contract.md)):
binary64 is the numeric contract and `long double` is accumulation scratch only.
Guarded conversions round once to binary64 and throw only on non-finite
results; a nonzero extended-precision intermediate that rounds to zero **is
zero** (flush-to-zero at binary64 granularity), which is exactly the
structurally unreachable state of the removed `value != 0 && result == 0`
throw on the qualified macOS baseline. Every explicit underflow contract
remains enforced at binary64 granularity (squares, covariance diagonals,
variance profiles and innovation scalings now guard on the binary64 value of
their expression on every platform). Accumulation stays extended-precision
scratch; overflow and relative-margin guards are unchanged.

Changed files: `finite_state_factor.cpp`, `diffuse_state_space.cpp` (`checked`
and `square`), `sparse_history.cpp`, `regression_likelihood.cpp` (`checked` and
the whitened-residual square), `state_space.cpp` (standardized-innovation
square), `prediction_arma_information.cpp`, `forecast_arma_information.cpp`,
`interpolation_arma_information.cpp` (`finite_prediction`) and
`arma_residuals.cpp` (`legacy_checked`). The
`regression_likelihood.cpp` diagonal guard now fires with its own
`regression coefficient covariance underflows` message on every platform
instead of a generic conversion throw masking it.

Validation on this machine, pinned OpenBLAS 0.3.34, `env -u LD_LIBRARY_PATH`:
Release OpenMP build **37/37**, true serial build (`-DKALOS_OPENMP=OFF`)
**37/37**, zero warnings under `-Wall -Wextra -Wpedantic` in both, and the
cross-build outputs of every touched area's program are identical (only the
self-reported thread-team banner differs, as expected). The three previously
failing programs pass: `test_diffuse_state_space`,
`test_forecast_arma_information` (`available`, serial/OpenMP equality) and
`test_regression_likelihood` (message-matched covariance underflow). No
fixture, oracle, tolerance or test file changed; nothing was rebaselined.
No capability or default has been promoted.

## FindKalosOpenBLAS provider precedence — 2026-11-15

The open follow-up from the Linux baseline qualification is fixed in
`cmake/FindKalosOpenBLAS.cmake`: an explicit provider root (cache entry
`-DOpenBLAS_ROOT=<dir>` or the `OpenBLAS_ROOT` environment variable) is now
exclusive — header and library are searched only under that root, so pkg-config
or system hints can never contribute one of the two components. A root missing
either `lapacke.h` or `libopenblas` fails the configure loudly with an
explanatory warning instead of silently mixing providers. Without an explicit
root, the previous pkg-config/system discovery applies unchanged.

Verification on this machine (which has the system `openblas.pc` 0.3.26 without
LAPACKE exports): a fresh build directory configured with only
`-DOpenBLAS_ROOT=/tmp/opencode/openblas` resolved both cache entries to the
pinned provider, `KALOS_LP64_PROVIDER` and `KALOS_OPENMP_RUNTIME_OK` probes
succeeded, the full build produced zero warnings, `ldd` of the test binaries
shows `libopenblas.so.0 => /tmp/opencode/openblas/lib/libopenblas.so.0`
(pinned 0.3.34), and `ctest` passed **37/37**. The same resolution holds for
the environment-variable form. A bogus `OpenBLAS_ROOT` fails configure with
the documented message and no system fallback. The explicit
`-DKalosOpenBLAS_INCLUDE_DIR/-DKalosOpenBLAS_LIBRARY` cache entries remain
supported. No capability or default has been promoted.

## Moving five-year dispersion and two-pass sigma weights — 2026-11-15

Delivered item 22b.3a for both reserved rules without promoting a default:
`include/kalos/x11_sigma.hpp` / `src/x11_sigma.cpp` compute per-calendar-year
moving five-year dispersion windows and the two-pass sigma weights of the
default X-11 extreme-value stage (weights only; replacement values and
correction factors remain 22b.3b work). `X11SigmaPolicy::legacy` reproduces
the pinned JDemetra+ default corrector operation for operation — the `1e-5`
tiny-dispersion year exemption, the `1e-15` threshold slack, two passes with
exclusion of exactly-zero weights and re-detection on the original values,
absolute-year window indexing over the full series, and projection trimming
that affects only the dispersion windows. `X11SigmaPolicy::accurate` applies
the scale-limit rule (no cutoff, no slack, zero dispersion grants weight 1
only to exactly zero deviations). The policy is a required argument; the
spike-revival-versus-limit default decision stays open and **no capability or
default has been promoted**. (Superseded 2026-11-17: the decision is resolved
— `legacy` is the product default; see the dated entry below. The core
behavior and every fixture above are unchanged by that resolution.)

Evidence: `validation/reference/capture-x11-sigma.R` drives the pinned
`DefaultExtremeValuesCorrector` (rjd3 3.9.0 jars, rJava) and captures 125
deterministic fixture cases to `validation/evidence/native/x11-sigma` with
17-significant-digit round-trip exact CSVs of pass-1/final weights, pass-1/
final dispersion arrays and pass counts; the private exclusion helper re-run
confirms the second-pass rule bit-exactly. A throwaway harness compared all
125 cases with exact double equality (0 failures; legacy policy); the two
policies diverge on exactly the seven tiny-scale fixture cases (spike revival
versus scale-equivariant limit). `test_x11_sigma` embeds six pinned cases plus
closed-form geometry/exclusion/missing/multiplicative/threshold controls,
input validation (including the sub-one-period rejection, where the pinned
reference itself runs off the array), interruption, and serial/OpenMP exact
equality. Rules and semantics: docs/contracts/x11-sigma-contract.md.

Validation on this machine, pinned OpenBLAS 0.3.34, `env -u LD_LIBRARY_PATH`:
Release OpenMP build **38/38** (38 programs after adding `test_x11_sigma`),
true serial build (`-DKALOS_OPENMP=OFF`) **38/38**, zero warnings under
`-Wall -Wextra -Wpedantic` in both. No fixture, oracle or tolerance was
rebaselined; existing tests are untouched.

## Same-phase replacements, endpoints and correction factors — 2026-11-15

Delivered item 22b.3b on top of the 22b.3a weights:
`include/kalos/x11_replacement.hpp` / `src/x11_replacement.cpp` compute the
same-phase replacement values of the default X-11 extreme-value stage with
full neighbor provenance. The pinned discovery walk collects up to two
fully weighted (weight exactly one) same-phase rows per side and extends the
opposite side to four at the endpoints; fewer than four neighbors select the
insufficient-neighbor fallback (kind `period_average` with the pinned
ascending phase sum), never a partial blend. The blend keeps the pinned
reciprocal-multiply arithmetic `(w*x + s1 + s2 + s3 + s4) * (1/(4+w))`.
Masked rows carry weight one with an effective NaN value and poison blends
and phase averages exactly like the pinned reference. `outside_span` marks
rows dropped by the projection span; `backcasts + forecasts > n` is rejected
(the pinned masking then writes outside its output array), `== n` is defined.
Correction factors follow the pinned public getter — additive `x*(1-w)`,
multiplicative `x/(1+w*(x-1))` with the zero-denominator IEEE result, mode
defaults elsewhere — and the corrected series is the apply-corrections
identity, so weight-one rows cancel completely. B17/B20 table placement and
the CV correction variant are not delivered or claimed; **no capability or
default has been promoted**.

Evidence: `validation/reference/capture-x11-replacement.R` drives the pinned
`DefaultExtremeValuesCorrector` (rjd3 3.9.0 jars, rJava) through
`analyse` → `getObservationWeights` → `computeCorrections` with both
projection-masking booleans → `getCorrectionFactors` → `applyCorrections`
plus static `periodAverages`, and re-runs the private neighbor walk through
JNI for provenance; the capture re-derives every factor and kind itself and
refuses to write on disagreement. The 22-case fixture
(`validation/evidence/native/x11-replacement`, 17-significant-digit
round-trip exact) holds 1358 rows: 135 four-neighbor blends, 83 pinned
null-walk fallbacks (218 flagged rows overall), 54 `outside_span` rows in the
masking mode, plus NaN-poisoned, whole-phase, multiplicative and legacy-
revival controls. A throwaway harness compared every artifact of every case
— 22b.3a final weights, both replacement modes, kinds, all neighbor slots,
factors, corrected values and averages — with exact double equality, 22/22.
`test_x11_replacement` embeds six pinned cases plus closed-form walk,
fallback, poisoning, endpoint-extension, validation, interruption and
serial/OpenMP-exact-equality controls. Rules and semantics:
docs/contracts/x11-replacement-contract.md.

Validation on this machine, pinned OpenBLAS 0.3.34, `env -u LD_LIBRARY_PATH`:
Release OpenMP build **39/39** (39 programs after adding
`test_x11_replacement`), true serial build (`-DKALOS_OPENMP=OFF`) **39/39**,
zero warnings under `-Wall -Wextra -Wpedantic` in both. No fixture, oracle or
tolerance was rebaselined; existing tests are untouched.

## Core export manifest for the 22b.3 units and manifest guard — 2026-11-15

The item 22b.3a/22b.3b units were committed without their per-file export
manifest entries, so `tools/export-source.py` snapshots of `0d20d21`/`8e91ee4`
silently omitted `src/x11_sigma.cpp`, `src/x11_replacement.cpp`, their public
headers and both fixture directories; the downstream rkalos/pykalos pins were
held at `c6a1881` and the `8e91ee4` snapshot fails at configure. Completed
`tools/source-files.json` with exactly the 24 missing tracked files (4 core
sources/headers, 20 fixture files), matching the existing per-file,
alphabetically sorted convention.

New guard `tools/check-manifest.py` (run as the first CI step in
`.github/workflows/core.yml`): every `add_library` source, every tracked
file under `src/`, `include/`, `backends/` and `cmake/`, and every tracked
file under `validation/fixtures/` must appear in the manifest, in sorted
order with no duplicates or untracked entries. It passed on the completed
manifest and deliberately removed entries (a library source, a public header,
the sigma fixture set) were each refused with a named file list. Export
snapshots of this revision (`--kind python` and `--kind r`) self-verify and a
minimal consumer project built the full library, including both 22b.3 units,
from the exported snapshot alone. No fixture, oracle, tolerance or build
semantics changed. **Downstream rkalos/pykalos pins may now advance past
`c6a1881` to this revision.**

## Profiled variance as a binary64 quotient — bit-identical recomputation — 2026-11-15

The downstream rkalos re-qualification against the underflow-guard-fixed core
exposed the last scratch-width divergence of the 0cbd012 family: on x86-64 the
pinned forecast suite showed 15 of 64 cases where `forecast_information`'s
`variance` and the fit's stored `profiled_variance` differed by exactly 1 ULP
at identical optimizer coordinates (thread-count-independent and deterministic;
structurally absent on the macOS baseline where the scratch is binary64).
Mechanism: both likelihood kernels computed the named `profiled_variance` as
the once-rounded **extended** quotient of the unrounded scratch sum
(`checked(squares / n)`), while every consumer — `forecast_information`,
`interpolation_information`, `arma_information`, `arma_residuals` and the
internal invariant check in `forecast_arma_information` — divides the stored
binary64 `sum_squared_standardized` in binary64. Extended-precision-range
contract rule 3 forbids a named quantity from carrying a different rounding
than the binary64 expression consumers re-evaluate, so both producers now
compute the profiled variance as a binary64 division of the named binary64 sum
(the quotient is bounded by the finite sum, so the removed `checked()`
overflow guard cannot fire), and the "profiled variance underflows" guard
tests the named binary64 sum, closing the residual cross-platform branch in
the sub-binary64-sum corner. Contract rule 3 and the enforced-site inventory
updated accordingly; no fixture, oracle or tolerance was touched.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env -u LD_LIBRARY_PATH`: Release OpenMP build **39/39**, true serial build
**39/39**, zero warnings under `-Wall -Wextra -Wpedantic` in both. The values
move at most 1 ULP toward the macOS-qualified baseline by construction; the
rkalos downstream re-run is recorded in the rkalos repository.

## Sanitizer re-qualification at cd004b0 — 2026-11-16

Re-qualified the sanitizer configuration on the full 39-program suite at
`cd004b0` (the M0 ASan/UBSan row above predates `0cbd012` and the 22b.3
units and covered only 37 programs with three then-failing platform-numerics
cases now fixed): a fresh Debug build with gcc 13.3 `-fsanitize=address,undefined`
(`-DKALOS_SANITIZERS=ON`, `-DKALOS_OPENMP=OFF`, bare `-DOpenBLAS_ROOT`
resolving to the pinned 0.3.34 provider, `env -u LD_LIBRARY_PATH`) passed
**39/39** with **zero ASan/UBSan diagnostics** across all logs; leak
detection stays disabled (`ASAN_OPTIONS=detect_leaks=0`) for the M0 reason —
the uninstrumented provider would false-positive on its retained thread
arenas. The Release OpenMP and true serial builds were rebuilt and re-run at
this exact commit: **39/39** each, zero warnings. No source, fixture, oracle
or tolerance changed for this qualification.

## Complete fixed additive B pass (22b.4 S1) — 2026-11-16

Delivered `kalos::x11_b_pass` (item 22b.4 slice S1): the complete fixed
additive B pass with all 20 public pinned tables, the three extreme-value
correction rounds (detection series, analysis positions, weights, correction
history) and the fixed per-phase seasonal requests and Henderson length. The
pinned arithmetic is reproduced bit for bit: the trend filter weights are the
pinned `DiscreteKernel.henderson`/`LocalPolynomialFilters.of2_3` recomputation
(the classic closed form differs by up to 16 ULP), the interior is the
`IFiniteFilter` slow-path ascending dot, and the endpoints are the pinned
`AsymmetricFiltersFactory.mmsreFilter2` closed scalar pipeline (the qualified
`x11_musgrave_*` primitive documents the independent classic rule and is not
used here); the seasonal phases and normalization use the pinned stable gate,
classic decimal endpoint tables and restoration set file-locally.

Evidence: 18-case pinned JDemetra+ 3.9.0 fixture (20 tables, 24,352 rows
including the B9d round-2 detection intermediate; `SHA256SUMS.csv` verified)
is bit-exact in the Release OpenMP build, the true serial build and under
`KALOS_NUM_THREADS=4`. `test_x11_b_pass.cpp` embeds five quarterly pinned
cases verbatim and qualifies the additive composition identities, the
correction-history linkage and counts, argument validation, serial/OpenMP bit
equality and a full interruption boundary sweep. The fixture and its source
scripts are added to the export manifest.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env -u LD_LIBRARY_PATH`: Release OpenMP build **40/40**, true serial build
**40/40**, zero warnings under `-Wall -Wextra -Wpedantic` in both; the new
unit spot-run under gcc 13.3 `-fsanitize=address,undefined`
(`-DKALOS_SANITIZERS=ON`, `-DKALOS_OPENMP=OFF`, `detect_leaks=0`) passed with
zero diagnostics. No default, capability or tolerance was promoted. The C/D
passes and the mean-endpoint fallback below the trend length remain
undelivered and unclaimed.

## Complete fixed additive C pass (22b.4 S2) — 2026-11-16

Delivered `kalos::x11_c_pass` (item 22b.4 slice S2): the complete fixed
additive C pass with all 14 pinned `X11CStep` tables and the final
extreme-value analysis on C13 (sigma weights, replacements and correction
history; C17/C20 are its delivered tables). The composition mirrors the
pinned `X11Kernel` call sequence — C1 = series − B20, the trend-cycle
interior C2 and C4, the pinned seasonal/normalization stages at
`getPosition(c2drop)`/`getPosition(0)`, the fixed Henderson with mmsre
endpoints over C6, C11/C13 on the original reference series — and the API
enforces the pinned coupling by requiring the finished `X11BPass` of the
same series, settings and policy. The S1 pinned machinery moved unchanged
into the internal `detail/x11_pinned.hpp` shared by both passes (B
re-qualified 18/18 bit-exact after the move); `X11BPass` gained the
`first_period` echo used by the coupling check.

Evidence: 18-case pinned JDemetra+ 3.9.0 fixture (14 tables, 17,320 rows,
capture-side identities including an independent re-application of the
pinned trend-cycle filter to C1; `SHA256SUMS.csv` verified) is bit-exact
through the native B+C composition in the Release OpenMP build, the true
serial build and under `KALOS_NUM_THREADS=4`. `test_x11_c_pass.cpp` embeds
five quarterly pinned cases verbatim and qualifies the composition
identities, the correction-history linkage and counts, the B-pass coupling
and argument validation, default-versus-explicit bounds, serial/OpenMP bit
equality and a full interruption boundary sweep. Fixture and core entries
added to the export manifest.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env -u LD_LIBRARY_PATH`: Release OpenMP build **41/41**, true serial build
**41/41**, zero warnings under `-Wall -Wextra -Wpedantic` in both; the new
and the B unit spot-run under gcc 13.3 `-fsanitize=address,undefined`
(`-DKALOS_SANITIZERS=ON`, `-DKALOS_OPENMP=OFF`, `detect_leaks=0`) passed
with zero diagnostics. No default, capability or tolerance was promoted.
The D pass remains undelivered and unclaimed.

## Complete fixed additive D pass (22b.4 S3) — 2026-11-16

Delivered `kalos::x11_d_pass` (item 22b.4 slice S3): the complete fixed
additive D pass with all 16 pinned `X11DStep` tables and the D9 MSRE
diagnostic, composed from the S2 C pass exactly like the pinned `X11Kernel`
(D1 = series − C20, the initial-request seasonal stage at
`getPosition(d2drop)` with the `iStart = d2drop` extension, the mmsre
Henderson over D6 and D11bis, the final requests over D9bis at
`getPosition(0)`, D11/D8 on the original reference series) and enforcing the
finished-C-pass coupling in the API (status, first-period/trend/filter
echoes, policy, series equality against `X11CPass::reference_series`). The
sigma policy stays an explicit required argument and the D pass performs no
new extreme-value analysis: its correction round is the C-pass round. The
calendar gate of the 22b.4 acceptance text was qualified directly: the
pinned D step never extends the series internally — only the MSRE diagnostic
is calendar-sensitive, dropping the context horizon rows and indexing at
`getPosition(backcast)` — so the pass takes explicit `backcast`/`forecast`
echoes over the extended reference series and the fixture captures both
calendars on the same inputs (four hand-extended cases). The pinned fixed
`defaultMsrTable` additive branch is the plain subtract `x - mean` (the
+1-offset branch belongs to the other decomposition modes and quantizes
tiny residuals around 1.0 — the revive cases pin the difference); the 7-MA
is the zero-outer-tap-trimmed `makeSymmetricFilter(7)` with plain-average
filtered-mean padding. `X11CPass` gained the `first_period` and
`reference_series` echoes mirroring the pinned getters; the shared
`detail/x11_pinned.hpp` gained the MSRE machinery and B and C were
re-verified 18/18 bit-exact after the changes.

Evidence: 22-case pinned JDemetra+ 3.9.0 fixture (18 zero-horizon cases with
the consumed C13/C17/C20 linked against the installed x11-cpass fixture, 4
hand-extended calendar cases; 16 tables, 24,800 rows plus the per-phase MSRE
table; capture-side identities, trend-filter and endpoint recompositions and
the pinned `defaultMsrTable` reproduction; `SHA256SUMS.csv` verified) is
bit-exact through the native B+C+D composition in the Release OpenMP build,
the true serial build and under `KALOS_NUM_THREADS=4`.
`test_x11_d_pass.cpp` embeds five quarterly and one extended pinned cases
verbatim and qualifies the decomposition identities, the MSRE diagnostics,
the D9/C-round count linkage, the original/extended calendar behavior, the
C-pass coupling and argument validation, serial/OpenMP bit equality and a
full interruption boundary sweep. Fixture and core entries added to the
export manifest.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env -u LD_LIBRARY_PATH`: Release OpenMP build **42/42**, true serial build
**42/42**, zero warnings under `-Wall -Wextra -Wpedantic` in both; the new
and both predecessor units spot-run under gcc 13.3
`-fsanitize=address,undefined` (`-DKALOS_SANITIZERS=ON`,
`-DKALOS_OPENMP=OFF`, `detect_leaks=0`) passed with zero diagnostics. No
default, capability or tolerance was promoted. The selector-stage connection
(S4), the mean-endpoint fallback below the trend length, multiplicative
modes and ARIMA-regression inputs remain undelivered and unclaimed.

## Pass-level X-11 selection flow (22b.4 S4) — 2026-11-16

Delivered `kalos::x11_selection_pass` (item 22b.4 slice S4): the pass-level
additive selection flow connecting the qualified selector stages to the
S1–S3 fixed-pass arithmetic without deciding anything new. The staged
seasonal requests run on the qualified 22b.3d plan arrays (Msr/X11Default
→ provisional 3×3/3×5, bytecode-mirroring the pinned context getters); the
automatic Henderson stages reproduce the pinned selection points verbatim —
the **inline B7 branch** (ratio ≥ 1 → period + 1, *not* `selectFilter`
alone), `selectFilter` at C7/D7 and the D final step, and the always-
computed D-final `iCRatio` — each consuming `detail::pinned_ic_ratio`, the
row-exact `AutomaticHenderson.calcICR` replication (pilot period + 1 over
the interior span, extension drops on both windows, divisor `count − 1`,
pinned ×3/×6 multipliers). The global-MSR D-final decision runs the pinned
`doMSR` loop (horizon drop, trailing partial-year trim at
`getPosition(backcast)`, FMEND-7 pilot + qualified additive `MsrTable`
over leading windows, trailing-edge year removal per ambiguous band) with
the qualified 22b.3c band rule and both qualified iteration policies
(jdplus default; the five-year cap checked by truncating the same pinned
19-evaluation trace at the sixth), fills the D-local `seasFilter` copy
leaving the normalizer on the context's provisional array (bytecode-
verified), and echoes `getSeasFilter`/`getD9msriter` on the D pass; the
captured `isD9default` fact (always false) keeps `using_default` on
`MsrFilterSelection.isUsingDefault`. Pinned degenerate choices are **not**
reproduced: a NaN I/C ratio (pinned takes the longest filter) and a NaN
MSR table (pinned takes 3×9) yield explicit `unsupported:<stage>` statuses
— the constant-series fixture cases pin the contrast.

Evidence: `validation/reference/capture-x11-selection.R` drives the pinned
kernel (rjd3 3.9.0, JVM 21) through `X11Kernel.process` for 15 cases
(captured through the pinned kernel itself, with capture-side `stopifnot`
per-stage `calcICR`/`selectFilter`, standalone `doMSR` and FMEND-7 +
`MsrTable.of` attempt-walk cross-checks on the kernel context) into
`validation/evidence/native/x11-selection` (28,382 table rows, per-stage
selection records, per-attempt MSR traces, `SHA256SUMS.csv` verified).
`test_x11_selection_pass` embeds all 15 decision-record cases and six full
22-table selected-state cases verbatim, qualifies the inline-branch
contrast, the bit-exact attempt traces, the cap-policy truncation with the
B..D9_g_bis-identical/D10-diverging invariant, decomposition and MSRE
identities, serial/OpenMP/4-thread bit equality, argument validation and
the full interruption boundary sweep. The capture script additionally
guards its own CSV structure (header and row-count re-read) against
matrix-coerced single-line serialization.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`: Release OpenMP build
**43/43**, true serial build **43/43**, zero warnings in both; the shared
header touch was regression-gated by re-running the B/C/D sweepers against
the installed fixtures at 4 threads and serial (**18/18, 18/18, 22/22**
bit-exact); the selection unit and all three fixed-pass units spot-ran
under gcc 13.3 `-fsanitize=address,undefined` (`-DKALOS_SANITIZERS=ON`,
`-DKALOS_OPENMP=OFF`, `detect_leaks=0`) with zero diagnostics. Rules and
semantics: docs/contracts/x11-selection-pass-contract.md. No fixture,
oracle, tolerance, default or capability was promoted. Cross-validation,
multiplicative and non-seasonal modes, the period-specific corrector, the
mean-endpoint fallback below the trend length and ARIMA-regression inputs
remain undelivered and unclaimed.

## Multiplicative decomposition mode (22b.5 M3a) — 2026-11-16

Delivered the pinned **multiplicative decomposition mode** (item 22b.5
slice M3a) across the S1–S4 passes. `kalos::X11DecompositionMode`
(include/kalos/x11_decomposition.hpp) mirrors the pinned
`DecompositionMode`; every pass entry point gained a last optional mode
argument defaulting to `additive` (item 22b.4 surface byte-unchanged) and
echoes it on the pass structs. The mode drives exactly the decoded sites:
the `ctx.remove` ratio at every composition point, the `X11Kernel.check`
positivity gate on present rows (pinned message
`"multiplicative decomposition of non positive series"`; flagged-missing
rows stay legal at the fixed-pass level, the selection completeness rule is
separate), the `makePositivity` repair at its four decoded sites (B7/C7/
D7/D12 — the delivered trend tables are the repaired pinned tables, every
row strictly positive, fixture repair counts prove the branch live), the
mode-default-1 sigma center and `x/(1+w(x−1))` corrector, the
`defaultMsrTable` multiplicative branch `obs = x / mov` with the
`MsrTable.of` **relative-delta flag set exactly when the mode is
Multiplicative** (false under PseudoAdditive — one reason the pseudo path
is not the ratio arithmetic), the mode-aware `doMSR` irregular, and the
new pinned fact that `calcAbsMeanVariations` under the flag makes every
lag-1 variation **relative in both** the pilot and the irregular window of
`AutomaticHenderson.calcICR` (`detail::pinned_ic_ratio` mode-aware): this
decides the additive-NaN `const-msr` degenerate at exactly 3.515625 in
multiplicative mode, while `const-fixed` stays NaN and keeps the
`unsupported:trend-b7` refusal. The D9 mask is mode-free (verified).
`log_additive` and `pseudo_additive` are declared but fail every entry
point with `"<pass>: decomposition mode is not delivered"` — the log
transform's `exp` belongs to the `X11Kernel.fill` export stage (D10..D13)
and the pseudo-additive pipeline runs the dedicated
`X11{B,C,D}StepPseudoAdd`/`PseudoAddUtility` classes (decoded, two paths
differing in the MSR flag, not qualified); `x11_mode_is_multiplicative`
exposes the pinned predicate but passes gate on the full mode.

Evidence: all four capture scripts take `--mode=Additive|Multiplicative`
(default Additive; re-captured additive fixtures verified byte-identical,
all five selection CSVs included), append the mult-only `mode`/`b7_repair`/
`c7_repair`/`d7_repair`/`d12_repair` columns last so additive schemas stay
byte-exact, and cross-check the ratio arithmetic, the `makePositivity`
rebuild and the mode-aware MSR/ICR facts on the pinned context (rjd3 3.9.0,
JVM 21) into `validation/evidence/native/x11-{bpass,cpass,dpass}-mult`
(24/24/28 cases, 32,656/23,224/31,616 table rows — nine additive
categories at positive levels plus `deep`, repair-carrying `spikeend`/
`wideband`; D carries the same four extended-calendar cases) and
`x11-selection-mult` (complete 15-case kernel inventory, 28,382 rows).
Capture-side catch: the dpass `cases[[id]]$col <-` inside `capture()`
shadowed the global (plain `$<-` copies) — mult columns silently dropped
until switched to `<<-`; the additive schema masked it.

`test_x11_multiplicative.cpp` (ctest #44) embeds 32 pinned cases verbatim:
5 B + 5 C + 7 D (both repair categories, masked case, two extended
calendars) plus the full 15-case selection inventory with the mode-aware
staged I/C ratios, relative-form MSRE tables and self-consistency, eight
engaged MSR walks bit-exact, ratio decomposition identities recomputed with
the same IEEE operations, the `const-msr` decided / `const-fixed` refused
contrast, five-year-cap truncation, strict positivity of repaired tables,
serial/OpenMP equality, the interruption boundary sweep and the mode/
positivity gate messages. `tools/verify_x11_{bpass,cpass,dpass}` read the
optional `mode` column by header name (additive behavior unchanged,
mult correction-count checks use the mode default) and sweep the full
mult fixtures.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`: Release OpenMP build
**44/44**, true serial build **44/44**; mult sweeps **24/24, 24/24, 28/28**
bit-exact beside the unchanged additive sweeps **18/18, 18/18, 22/22**;
all six fixture directories re-swept at `KALOS_NUM_THREADS` 1 and 4
(12/12 bit-exact); the mult unit and the B/C/D/selection units plus
sanitizer-built mult and additive D sweeps (28/28, 22/22) are diagnostic-
free under gcc 13.3 `-fsanitize=address,undefined` (`detect_leaks=0`).
Rules and semantics: docs/contracts/x11-multiplicative-contract.md. No
fixture, oracle, tolerance, default or capability was promoted.
Cross-validation, the period-specific corrector, log-additive and
pseudo-additive modes, the mean-endpoint fallback and ARIMA-regression
inputs remain undelivered and unclaimed.

## Log-additive decomposition mode (22b.5 M3b-i) — 2026-11-16

Delivered the pinned **log-additive decomposition mode** (item 22b.5
slice M3b-i) across the S1–S4 passes. Pinned decode (KERNEL.txt/
DoubleSeq bytecode in /tmp/opencode/jdprobe): `X11Kernel.check()` gates
positivity on `mode == Multiplicative || mode == LogAdditive` with one
pinned constant message; `X11Kernel.process` applies `DoubleSeq.log()`
(`fn(Math::log)`, bytecode-verified) to the input before the plain
(non-pseudo) steps; `isMultiplicative()` is false under LogAdditive, so
the whole additive-form context (difference removes, centre-0 sigma, 0
mode default, absolute-form MSRE/MSR flag, no `makePositivity` sites)
runs on the log-domain sequence and the step getters are log-domain
tables. The export-stage `exp` of D10..D13 and the `LogAdditive &&
seasonal && bias == None` bias-skip live in the results builder only —
not delivered, not claimed. A probe over all 6,701 fixture input values
confirmed Java `Math.log` is bit-identical to C `log` on this machine
(the precondition for bit-level qualification). Implementation: the mode
gate accepts log_additive with the shared positivity message on the raw
present rows; `detail::log_domain` transforms present rows only; every
fixed-pass entry shadows its series parameter with the log-domain copy
after the gate (so the C/D predecessor-echo checks compare against the
transformed tables) and the selection pass's manual C-stage composition
consumes `log_domain(series)` while the internal fixed-pass calls keep
the raw series plus mode (single transform). All four capture scripts
take `--mode=LogAdditive`: they feed `log(values)` to the steps (raw ts
to the kernel), keep raw values in `inputs.csv`, keep `rem`/`dflt`/
repair columns at the additive forms (the mode is not multiplicative)
and re-derive every identity in the additive form on the log-domain
tables (rjd3 3.9.0, JVM 21). Fixtures: `x11-{bpass,cpass,dpass}-log`
(24/24/28 cases, 32,656/23,224/31,616 rows, masked + four extended
calendars, additive-form MSRE) and `x11-selection-log` (15-case kernel
inventory, 28,382 rows; the dpass capture cross-links C13/C17/C20
against the installed cpass-log fixture). In the log domain both
constant-series controls degenerate like the additive fixture (zero
deltas → NaN I/C → `unsupported:trend-b7`) — the documented contrast
with the multiplicative decided `const-msr`.

`test_x11_log_additive.cpp` (ctest #45) embeds 32 pinned cases verbatim:
5 B + 5 C + 7 D (masked, two extended calendars) + the 15-case selection
inventory with additive-form staged I/C ratios, absolute-form MSRE
tables, difference decomposition identities over the `Math.log` input,
the `B1 = log(input)` transform identity, the five-year-cap truncation
of the 19-attempt walk, mode echoes through B→C→D, both degenerate
refusals, serial/OpenMP equality and the interruption boundary sweep.
`test_x11_multiplicative.cpp` gate block updated: `pseudo_additive`
alone refuses with the not-delivered message; the pinned
`isMultiplicative()` truth table is asserted for all four modes (log
false, pseudo true). `tools/verify_x11_{bpass,cpass,dpass}` parse the
`LogAdditive` mode name.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`: Release OpenMP build
**45/45**, true serial build **45/45**; log sweeps **24/24, 24/24, 28/28**
bit-exact beside the unchanged additive **18/18, 18/18, 22/22** and mult
**24/24, 24/24, 28/28** sweeps (OMP and serial binaries); the three log
fixture directories re-swept at `KALOS_NUM_THREADS` 1, 2 and 8 (9/9
bit-exact); the log and mult units are diagnostic-free under gcc 13.3
`-fsanitize=address,undefined` with leak detection. Rules and semantics:
docs/contracts/x11-log-additive-contract.md. No fixture, oracle,
tolerance, default or capability was promoted. Cross-validation, the
period-specific corrector, the pseudo-additive mode, the log-additive
export stage (`exp` of D10..D13, bias-skip), the mean-endpoint fallback
and ARIMA-regression inputs remain undelivered and unclaimed.

## Pseudo-additive decomposition mode (22b.5 M3b-ii) — 2026-11-16

Delivered the pinned **pseudo-additive decomposition mode** (item 22b.5
slice M3b-ii) across the S1–S4 passes. Pinned decode (PADD_{B,C,D,UTIL}.
txt, MSRFSEL_PADD.txt bytecode in /tmp/opencode/jdprobe): `X11Kernel`
selects `pseudoadd.X11{B,C,D}StepPseudoAdd` and
`MsrFilterSelectionPseudoAdd` under mode `PseudoAdditive`;
`isMultiplicative()` is TRUE, so every non-overridden site keeps the ratio
context (ratio removes, centre-1 sigma, 1 mode default, relative-form
`calcICR`, the four `makePositivity` trend guards live) while the
subclasses replace exactly twelve composition sites with the bytecode-exact
`PseudoAddUtility` forms — `b4d = B3 − B4anorm + 1`, `b6/b6/c6/d6` with
ratio border rows and interior `x − f·(s − 1)`, `b9d = B8 − s9 + 1`,
`b11 = B1 − B7·(B10 − 1)`, `c1 = Y + B7·(B13/B20 − B13)`,
`c11 = Y − C7·(C10 − 1)`, `d1 = Y + C7·(C13/C20 − C13)`,
`d8 = (D1/D7)·C20`, `d11 = Y − D12·(D10 − 1)`,
`d11bis = D1 − D7·(D10 − 1)` (lambda capture orders verified against the
invokedynamic bootstrap records) — and the C/D steps carry the finished
predecessor tables through their constructors (B7/B13, C7/C13/C20) exactly
like the kernel wires them. `X11Kernel.check()` imposes NO positivity
requirement on this mode (the gate is Multiplicative/LogAdditive only) and
`X11Utility.defaultMsrTable` takes its own `case 1`: `obs = x − mov + 1`
with `MsrTable.of` relative flag FALSE; `MsrFilterSelectionPseudoAdd.
calcIrregular` is `x − seas + 1` while `doMSR` keeps the strict
`mode == Multiplicative` flag (false). Implementation: the mode gate
accepts all four modes (the not-delivered message is now unreachable),
`detail` gained the four pseudo forms plus the three-form mode-driven
`pinned_msr_table`, the twelve override branches select on the mode in the
B/C/D passes, the selection pass's manual C-stage sites and the global-MSR
walk (`msr_walk` mode parameter) take the pseudo forms. All four capture
scripts take `--mode=PseudoAdditive`: the fixed captures compose the
pseudo step classes like the kernel and re-derive every identity through
the bytecode forms (cross-checked against the pinned statics), the
selection capture runs the kernel itself plus a standalone
`MsrFilterSelectionPseudoAdd.doMSR` and a pinned-component attempt walk.
Fixtures: `x11-{bpass,cpass,dpass}-padd` (24/24/28 cases, 32,656/23,224/
31,616 rows, masked + four extended calendars, removeIrregular-form/
absolute-evolution MSRE diagnostic, repair-count columns) and
`x11-selection-padd` (15-case kernel inventory, 28,382 rows; dpass
cross-links the installed cpass-padd fixture). In the pseudo domain the
`const-msr` contrast is decided (I/C 3.5156250000000027, one decisive 3x9
evaluation) like the multiplicative run while `const-fixed` stays
NaN-degenerate, and the 15-attempt `q-longmsr` walk decides at its
fifteenth — the five-year-cap run (six evaluations, `five_year_cap`
reason, default 3x5) delivers **identical tables** with differing MSR
metadata only.

`test_x11_pseudo_additive.cpp` (ctest #45) embeds 32 pinned cases verbatim:
5 B + 5 C + 7 D (masked, two extended calendars) + the 15-case selection
inventory with the ratio/pseudo decomposition identities on the delivered
tables, the strictly-positive repaired trend tables, the `B1 = input`
no-transform identity, the removeIrregular-form MSR walks with the cap
contrast, both constant-series controls, serial/OpenMP equality and the
interruption boundary sweep. `test_x11_multiplicative.cpp` and
`test_x11_log_additive.cpp` gate blocks updated: the not-delivered-mode
assertions are gone (message unreachable), both prove the four-mode truth
table and the positivity gate holding for mult/log alone; the log test now
asserts pseudo availability on zero/negative inputs and the mult test
proves pseudo tables differ from the plain multiplicative ones.
`tools/verify_x11_{bpass,cpass,dpass}` parse the `PseudoAdditive` mode name
(cpass mode default from `x11_mode_is_multiplicative`).

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`: Release OpenMP build
**46/46**, true serial build **46/46**; padd sweeps **24/24, 24/24, 28/28**
bit-exact beside the unchanged additive **18/18, 18/18, 22/22**, mult
**24/24, 24/24, 28/28** and log **24/24, 24/24, 28/28** sweeps (OMP and
serial binaries, 24/24 after the header-documentation touch); the three
padd fixture directories re-swept at `KALOS_NUM_THREADS` 1, 2 and 8 (9/9
bit-exact); the pseudo, log and mult units are diagnostic-free under gcc
13.3 `-fsanitize=address,undefined` with leak detection. Rules and
semantics: docs/contracts/x11-pseudo-additive-contract.md. No fixture,
oracle, tolerance, default or capability was promoted. Cross-validation,
the period-specific corrector, the log-additive export stage (`exp` of
D10..D13, bias-skip), the mean-endpoint fallback and ARIMA-regression
inputs remain undelivered and unclaimed.

## X-13 deterministic-effect allocation staging (22c M3c) — 2026-11-16

Delivered the pinned **deterministic-effect allocation staging** of the
X-13 pipeline (item 22c slice M3c): `x13_preadjust` (the fixed-coefficient
`X13Kernel.initialStep` seam — class/bucket allocation into the stored
a-tables, the `out` series fed to X-11 and the `X11Spec` mode forcing) and
`x13_restore` (the `X13Kernel.finals` seam — d11final/d12final/d13final/d16/
d18/e1/e2/e3/e11 with the pinned op/invOp/mean truth table and the
`correctSeries` rule). The kernel gives no fixed-coefficient entry point, so
the qualification captures the full pinned run and proves a compositional
recompute seam: `X13AllocationProbe.java` recomposes all 22 stored tables per
case bit-exactly from the pinned `deterministicEffect`/`backTransform`/
TsData-fold/`correctSeries` pieces and the capture refuses on any
disagreement, removed variable or mode-forcing violation. Pinned semantics
now proven: class predicates (`isOutlier`/`isDaysRelated`/`isMovingHoliday`/
`isUser`) with the **EXACT** `regeffect` attribute match — an outlier outside
Trend/Seasonal/Irregular contributes to no table, only user variables carry
the a9 buckets, days/holiday ignore the attribute — the `DataBlock.addAY`
rule (0 skips the column unread, 1 adds undamped, else plain mul-add)
accumulated in model-description order, the left-associative a8/a9/v26 fold
orders with `a9u` excluded from `out` and the finals, `out`'s
subtract/divide keyed on `isLogTransformation` (not the mode), the exp
back-transform of every stored a-table while a1 stays the complete input
bit-exactly, the `updateSpec` forcing, and the log-additive
op-subtract/invOp-multiply asymmetry (`isMultiplicative()` includes
LogAdditive). The X13 `X11Results` tables consumed by the restore are the
raw step tables outside log-additive; the LA results-builder rescale
(`ratioBiasCorrection`, the `X11Spec` default bias: D10=exp(d10)/avg(exp
(d10)), D12=exp(d12)·v13, D11=out/D10, D13=(out/D10)/D12) is reconstructed
**for verification only** (M3b-i export scope, not delivered). Fixtures:
`validation/evidence/native/x13-allocation` — 7 cases (full additive mix of
11 variables incl. 6-column TD7, Easter and an Undefined outlier; empty
model; forced Additive; log→Multiplicative; LogAdditive; PseudoAdditive;
monthly), 154 in-pipeline recomposition checks, all CSVs at 17 significant
digits and byte-identical across reruns (an R `write.csv` 15-digit
round-trip bug in the first capture was caught by the bit-exact sweeps and
fixed with the corpus `%.17g` convention).

`test_x13_allocation.cpp` (ctest #47) embeds all seven cases verbatim and
proves per case the mode forcing, the native preadjust (bit-exact; the
exp'd log-model buckets at 1e-15 across the backTransform boundary), the
native selection pass over the preadjusted series against the captured
D10..D13/C17 (bit-exact; LA through the results-builder reconstruction at
1e-12) and the restore over both the captured and native X-11 tables —
252 table checks per thread leg at 2/1/8 pinned threads, 756 total per run;
the fail-closed shapes, the empty-model rule and the addAY damping rules
run through the public surface. `tools/verify_x13_allocation.cpp` sweeps the
CSV corpus: 280 checks, 0 failures, identical at `KALOS_NUM_THREADS` 1/2/8.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`: Release OpenMP build
**47/47**, true serial build **47/47**, ASan/UBSan Debug build **47/47**;
the unit and the sweep are diagnostic-free under gcc 13.3
`-fsanitize=address,undefined` with leak detection; transcendental-boundary
worst deviation 1.88e-16 (std::exp vs Math.exp, documented). Coefficient/
ARIMA estimation, the `ExactArimaForecasts` backcast/forecast route with the
a1a/a1b tables, missing-row interpolation, length-of-period transforms,
arimadiffreg/bty variables, benchmarking and diagnostics remain undelivered
and unclaimed (docs/contracts/x13-allocation-scope-22c.md); no fixture,
oracle, tolerance, default or capability was promoted.

## Restricted fixed-model X-13 fit (22c M3-finale) — 2026-11-16

Delivered the **restricted fixed-model X-13 fit** (item 22c M3-finale,
M3's exit gate): `x13_fit(const X13FitRequest&)` composes the qualified
pieces in the pinned `X13Kernel.process` order — the fixed-coefficient
`initialStep` seam, the `X11Kernel` selection run over `out` under the
`updateSpec`-forced mode (non-log → Additive; log → Multiplicative unless
LogAdditive/PseudoAdditive, echoed as `mode_effective`), the pinned
results-builder stage as fit composition where the mode is log-additive
(`finalResults` → `ratioBiasCorrection`, the `X11Spec` default **Ratio**
bias: `A=exp(d10)`, `v9=avg(A)`, `v13=v9·avg(exp(d13))`, `D10=A/v9`,
`D12=exp(d12)·v13`, `D11=out/D10`, `D13=(out/D10)/D12`; `makePositivity`
is provably the identity on that path, every factor being a strictly
positive `exp`), and the `finals` seam. Final components are the pinned
`X13Finals` triplet `D11final/D12final/D13final`, exposed as
`finalSA`/`finalTrend`/`finalIrregular` — **no separate final-trend or
final-irregular table exists** anywhere in the pinned jars. **Forecast
domain: the fixed path supports {0, 0} exactly.** `initialStep` engages its
extension branch iff `back > 0 || forecast > 0` (`updateSpec` takes
absolute values first, so negative requests mean `|h| · annualFreq`, not
"auto") and runs the ARIMA machinery — `linv − v27`, `ExactArimaForecasts`,
`a1b`/`a1a`, the extended `out` calendar and the split
`d11a/d12a/d16a/d18a`/`d11b/...` finals. There is no fixed-coefficient route
to a nonzero horizon, so the fit answers `unsupported:x13 fit requires zero
horizons` on every other request and the corpus keeps the boundary under
evidence: `f_h6` (forecast 6) and `f_b3h3` (3/3) run the same fixed specs
through the pinned engine and record only the deterministic route facts
(`a1a_len`/`a1b_len` equal the requested lengths, `out_len = 126`,
`d11a_len` non-null, `d11final_len` stays 120).

Fixtures: `validation/evidence/native/x13-fit` — 11 monthly cases executed
end to end through the **pinned fast entry point**
`jdplus.x13.base.r.X13.process(ts, spec, ctx)`, exactly the method
`rjd3x13::x13_fast` calls (`X13Kernel.of(spec, ctx).process(ts
.cleanExtremities(), null)`), on fully fixed specs: 9 fixed-route cases
covering all four modes after the forcing (Additive, Multiplicative both by
a log model and by a forced non-log request, LogAdditive, PseudoAdditive),
fixed seasonal requests Stable/S3X1/S3X9/S3X15, Henderson 5 and 13, sigma
cuttings 1.5/2.5 and 2/3, and models from empty through TD7-only and
user-only to the full 11-variable mix, plus the 2 route-evidence cases.
`X13FitProbe.java` asserts no free coefficient is ever read (the estimation
cursor is untouched), the zero-horizon domain (a1a/a1b null, unextended
`out`, no split finals) and recomposes all 22 stored tables per fixed case
bit-exactly from the pinned pieces — **198 checks, 22 per case, refusal on
any disagreement**; every CSV is byte-identical across reruns. The `f_empty`
case is additionally re-executed through the **R `x13_fast` result path**
(R ts marshalling, the pinned `process` call, the proto result transform)
and compared bit-exactly against the probe capture — 14 tables
(`x13fast.csv`), so the R driver and the probe see the same fixed run.

`test_x13_fit.cpp` (ctest #48) drives both corpora — the 7 allocation-seam
cases with a uniform fixed S3X9/Henderson-5/legacy/1.5-2.5 spec and the 9
fixed-route fit cases with their own specs, 16 cases in all — checking per
case the `available` status, the full provenance echo including requested
and forced mode, the 13 a-tables and `out`, the four results tables as
`finals` consumed them, `c17`, the nine finals and the final-components
triplet identity: **30 checks per case per thread leg at 2/1/8 pinned
threads, 1440 total**, plus the fail-closed boundary table (non-fixed
seasonal requests, automatic Henderson length, nonzero requested horizons
including negatives, missing sigma policy), the throw shapes and the
`updateSpec` forcing per executable. Comparison policy: bit-exact outside
the log models; inside them everything downstream of `backTransform`'s
`exp` — `out` included — is checked at the transcendental boundary (a-tables
1e-15, results tables and `c17` 1e-12, finals 1e-9; **worst observed
deviation 4.49e-16**), because Java's `Math.exp` and the native libm can
flip a final ulp, which the M3c corpus did not exercise but `f_log` does.
`tools/verify_x13_fit.cpp` sweeps both corpora with per-table diff
reporting plus the x13_fast cross-driver guard: **528 checks, 0 failures,
identical at `KALOS_NUM_THREADS` 1/2/8**.

Validation on this machine, pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`: Release OpenMP build
**48/48**, true serial build **48/48**, ASan/UBSan Debug build **48/48**;
the fit unit and both sweep binaries are diagnostic-free under gcc 13.3
`-fsanitize=address,undefined` with leak detection. Coefficient/ARIMA
estimation, the ARIMA backcast/forecast route values (the boundary itself
is pinned), missing-row interpolation, length-of-period transforms,
arimadiffreg/bty variables, quarterly fit cases, the general
`BiasCorrection` options, benchmarking and diagnostics remain undelivered
and unclaimed (docs/contracts/x13-fit-contract.md,
docs/contracts/x13-allocation-scope-22c.md); no fixture, oracle, tolerance,
default or capability was promoted.

## Contract-doc friction consolidation (M4-S0) — 2026-11-16

Resolved the twelve boundary-friction items rkalos accumulated verbatim
across M2–M3 in its `IMPLEMENTATION_STATUS.md` upstream-feedback lists,
documentation side only (no behavior, no struct, no message and no fixture
file changed — fixture corpora are hash-pinned): the B9d table mapping
(19 struct members + three rounds carry the 20 public tables; B9d is
`rounds[1].detection`) and the plain B17/B20 round-duplication statement
(B20 holds correction *factors*, replacements live only in the round
history) plus the shadowed quarterly radii gate (the trend gate precedes
`n >= 3*(frequency/2) + frequency`, so the n>=10 message is reachable only
with `trend_length <= 9`) into `x11-bpass-contract.md`; the
`x11-cpass/inputs.csv`-equals-`x11-bpass/inputs.csv` note and the S2→S3
echo-member growth into `x11-cpass-contract.md`; the unsupported-run
record semantics (records past the refused stage are default-constructed —
only records up to and including it carry information), the `msr.csv`
row-to-`X11MsrAttempt` mapping (per-attempt columns vs case-level
`final_filter`/`using_default` restatements, `no_history` markers,
standalone cross-check rows) and the mode-conditional fixture semantics
(`b20nz` counts deviations from the mode-neutral B20 — 1 multiplicative /
0 additive-and-log; `const-msr` flips from the refused degenerate control
to available in the multiplicative family) into
`x11-selection-pass-contract.md`; the per-corpus `out` comparison policy
(seam corpus bit-exact, fit corpus at the exp boundary under a log model —
extending seam expectations to the fit corpus fails 1 ulp on `f_log`) and
the proposed deterministic regression-side fingerprint for
`X13FitResult` (count, per-variable class/effect/coefficient/column
shapes, FNV-1a over IEEE bit patterns in pinned consumption order;
implementation deferred) into `x13-fit-contract.md`; and two canonical
conventions into `docs/contracts/README.md` — **struct growth is contract
surface** (schema change for every consumer) and **verbatim statuses and
messages are pinned**. The `check_decomposition` label+mode message and
the missing `X11SelectionPass` mode echo cannot be fixed unilaterally
(downstream tests pin the message and the result shape): both are
documented with their agreed targets and flagged
**coordinated-change-needed** in the selection contract. Tests unchanged:
48/48.

## Sigma-weight policy default resolved (22b.3a) — 2026-11-17

**The long-open 22b.3a default decision is resolved. `X11SigmaPolicy::legacy`
is the product default.** Rationale: legacy is the rule proven bit-exact
against the pinned `DefaultExtremeValuesCorrector` — the JDemetra behavior —
while the accurate rule is the scale-limit construction whose separation from
legacy appears in seven of the 125 pinned sigma cases (`revive-*`,
`tiny-*`, `zeroish-*`).

The default is applied at the **binding and product layers**: the rkalos
wrapper default is `legacy`, and a later Persephone spec default will carry
it. The **layering design keeps the C++ core explicit** — the policy stays a
required argument of `x11_sigma_weights`, the fixed passes, the selection
pass and the fit, so an omitted policy is still refused exactly as before.

What this decision does **not** change: no core signature, error message or
status changes; no fixture, oracle, tolerance or captured expectation
changes; no capability flips (`capabilities()` is untouched); the accurate
rule stays delivered and qualified; the seven separation cases keep pinning
the difference between the two rules. What it does change: the wording that
recorded the choice as open — `docs/contracts/x11-sigma-contract.md` (the
resolved section), `docs/contracts/README.md`, the sigma-policy wording in
`x11-bpass-contract.md`, `x11-selection-pass-contract.md` and
`x13-fit-contract.md`, the roadmap row above and a superseded-by marker on
the 2026-11-15 delivery entry. Tests unchanged: 48/48.

## Item-21 roadmap decomposition (M4-S1) — 2026-11-16

Delivered the planning decomposition for item 21 (automatic X-13
preprocessing and selection): `docs/item21-roadmap.md`. The pinned master
pipeline was enumerated from the rjd3 3.9.0 bytecode —
`RegArimaKernel.process`/`calc` (model build, `calcCv` critical-value
defaulting, the fully-specified short-circuit, the round structure,
`pass2`/`redoEstimation`/`checkUnitRoots`/`checkMA`, the 2.5/2.8 final
rules), `X13ModelBuilder`, the `X13SpecDecoder` module assembly, and the
decision modules (`LogLevelModule`, `OutliersDetectionModule` →
`ExactOutliersDetector`/`ExactSingleOutlierDetector` with the
`CriticalValueComputer` forms, `DifferencingModule`, `ArmaModule`,
`AutoModellingModule`, `RegressionVariablesTest`, `MeanController`,
`FinalEstimator`, `RegSarimaComputer` with its `DEF_EPS`/`DEF_INTERNAL_EPS`
and UR-cancellation passes) — each stage cited class::method with its
observed threshold constants, mapped to the qualified Kalos units it
consumes (likelihoods, `optimize_arma_objective`, `arma_identification`,
`arma_residuals`, `arma_information`, `preparation`) and the new
primitives it needs. The recommended slice sequence runs
21-A estimation control → 21-B transformation → 21-C outlier scoring →
21-D differencing → 21-E/21-F ARMA identification and refinement → 21-G
regression tests/mean → 21-H integration (the 22c seams and the pinned
{0,0} horizon boundary are its downstream contract; the `horizons.csv`
evidence rows become its first cross-checks), and the roadmap carries the
maintainer sign-off list (nine decision-boundary groups). No code, no
fixtures, no defaults; planning-only commit.

## Post-split evidence relocated to `validation/evidence` — 2026-11-17 (CI hotfix)

**Root cause.** Since the fixtures relocation of slice `0d20d21`, post-split
capture corpora (131 files under `validation/fixtures/native/`:
x11-sigma*, x11-replacement*, x11-{bpass,cpass,dpass,selection}* including
the -mult/-log/-padd variants, x13-allocation/ and x13-fit/) landed inside
`validation/fixtures/`, which `tools/verify-migration.py` scans as the
IMMUTABLE pre-split historical root. The scanner correctly raised
`Unrecorded historical artifact` on every one of them, so the GitHub Actions
"Historical evidence" step was red on every commit since `0d20d21`. The
scanner logic, the pre-split manifest and every recorded historical byte are
untouched — the files were simply in the wrong root.

**Fix.** All 131 unrecorded files were relocated byte-identically
(`git mv`, 131 `R100` rename records) to the new root
`validation/evidence/`, preserving inner structure. `validation/fixtures/`
now contains only files present in `validation/migration/pre-split-manifest.json`
(112 fixture files), and `verify-migration.py` verifies the full historical
set again (406 files across fixtures, source snapshot and reference reports)
without rebaselining.

**Relocation policy (forward-looking).** New evidence ALWAYS goes under
`validation/evidence/`; the historical roots scanned by
`tools/verify-migration.py` (`validation/fixtures/`,
`validation/migration/source-snapshot/`, `validation/reference/{information,timings}/`)
are immutable forever. `tools/source-files.json` gains a third sorted key
`"evidence"` (131 entries, schema unchanged); `tools/check-manifest.py` was
extended minimally to require, deduplicate, sort-check and stale-check the
`validation/evidence/` tree like the fixture tree. The export allowlist in
`tools/export-source.py` is deliberately unchanged: `inst/tinytest/fixtures`
keeps exporting exactly the recorded historical fixtures, and post-split
evidence is not part of the R/Python source snapshots.

**Path updates (references only, no semantics).** The five capture scripts
that read new-root case files (`capture-x11-{sigma,replacement,bpass,cpass,dpass}.R`),
the five corpus sweeps (`tools/verify_x11_{bpass,cpass,dpass}.cpp`,
`tools/verify_x13_{allocation,fit}.cpp` default roots), the corpus-path
comments in `tests/test_x11_*.cpp`/`tests/test_x13_*.cpp` and in
`src/x11_sigma.cpp`, `src/x11_replacement.cpp`,
`include/kalos/{x13_allocation,x13_fit}.hpp`, the eleven contract documents
under `docs/contracts/` that name the corpora, `validation/reference/README.md`
and the live path mentions in this file now point at
`validation/evidence/native/...`.

**Gates.** `python3 tools/verify-migration.py` green (406 files);
`python3 tools/check-manifest.py` green (101 core, 112 fixture, 131 evidence);
full ctest 48/48 in the Release OpenMP build; and the moved corpora were
swept from the new root — `tools/verify_x11_bpass` 18/18 cases and
`tools/verify_x13_fit` 528 checks / 0 failures with the default
`validation/evidence/...` roots. No fixture byte, oracle, tolerance,
expectation or scanner rule changed.

## Fixed-order estimation control (21-A.1) — 2026-11-17

**Scope.** Item 21 stage E, first of the two planned 21-A increments: the
bytecode-pinned estimation-**CONTROL** layer of `RegSarimaComputer`
(eps/feps, starting points, UR-cancelling passes, per-pass decision records)
composed onto the already-qualified fixed-order 20b.2 objective/optimizer
kernels. New unit `include/kalos/estimation_control.hpp` +
`src/estimation_control.cpp` (`estimate_sarima_fixed`); no Rcpp boundary. The
second increment (21-A.2, the Hannan-Rissanen initializer enabling the
HannanRissanen/Multiple starts) is NOT in this commit.

**Oracle.** The pinned computer is drivable through the rjd3 3.9.0 jars, so
the oracle is the pinned `RegSarimaComputer.process` itself: the public-API
observer `validation/reference/EstimationControlProbe.java` (builds a
fixed-order `RegArimaModel<SarimaModel>` + pinned builder-default control, no
estimation algorithm) driven by
`validation/reference/capture-estimation-control.R`. The 17-case corpus is
deterministic (re-run reproduces byte-identical cases.csv) and installed at
`validation/evidence/native/estimation-control/` (standing policy: new
evidence never enters `validation/fixtures/`).

**Sign convention (probe-verified).** rjd3 stores the raw AR coefficients of
`1 + phi(B)`; Kalos uses `1 - phi(B)`; MA is `1 + theta(B)` on both sides.
Fixtures store the KALOS convention (AR signs flipped). The Default start
(`SarimaModel.setDefault(-0.1, -0.2)` in pinned raw space) is Kalos
ar/sar = +0.1, ma/sma = -0.2; Zero is all zero. Mean/drift enter only as
explicit regression columns per the regression-likelihood contract; the
pinned `meanCorrection(true)` column is not reproduced.

**Pinned transcription.** The UR-cancellation gates and deflations reproduce
the `tryUrpCancelling`/`tryUrmCancelling` bytecode branch by branch: the
regular pass is gated on nonempty regular AR and MA blocks, `d > 0` and both
raw polynomials within `UR_LIMIT = 0.1` at `B = 1`; the seasonal pass uses the
same regular-block nonemptiness gates, `seasonal_d > 0` and the `B = -1`
toggle accumulator (first entry negated), deflating only the regular blocks.
A candidate is kept only on a strictly greater adjusted concentrated
likelihood; equality or any candidate-side failure keeps the incumbent (the
pinned passes swallow all exceptions). `mt` is a stored, never-read pinned
field (bytecode-verified). `computeExactFinalDerivatives(v)` sets `fast = !v`.

**Deliberate rejections (fail closed, not silent fallbacks).** The
HannanRissanen-dependent starts (HannanRissanen, Multiple) and `ml = false`
throw `invalid_argument` until 21-A.2 / a residual-variance kernel qualify
them. `estimate_sarima_fixed` keeps every control field as required request
data — no default-constructed control.

**Tolerances.** Independent solvers (pinned Levenberg-Marquardt vs Kalos BFGS)
agree at fit tolerance: 2e-3 coefficients / 5e-4 likelihood, bounded by the
pinned side's own Zero-vs-Default run-to-run spread (worst 4e-4), not by the
Kalos error (observed worst agreement 3.99e-4 / 3.12e-6). The three
likelihood-only guard cases compare at 1e-9 with an added bit-equality check
against the direct physical objective. Solver status strings are not asserted
on fixtures — on flat near-unit-root directions the pinned function-precision
stop and the qualified gradient criterion can disagree while outcomes agree;
the near-threshold UR fixtures pin record consistency
(`accepted == attempted && strictly-better`), not a boundary-dependent gate
value.

**Sanitizer fix.** ASan/UBSan caught the aggregate-initialized `estimate`
pass record leaving `accepted` indeterminate; it is now explicitly the
available-initial-candidate bit.

**Gates.** `check-manifest.py` green (103 core, 112 fixture, 135 evidence);
`verify-migration.py` green (406 historical files, no rebaseline); full ctest
49/49 in Release OpenMP and in the serial build; ASan/UBSan
`test_estimation_control` clean; `tools/verify_estimation_control` over the
on-disk corpus 17 cases / 0 failures (worst 3.99e-4 coeff, 3.12e-6 likelihood,
one gate firing, zero acceptances). Contract in
[estimation-control-contract.md](docs/contracts/estimation-control-contract.md).

**Maintainer-decision items held at pinned values (roadmap stage-E ledger,
nothing promoted to configuration).** (1) `eps`/`feps` promotion: both stay
caller-passed pinned values (builder defaults 1e-7/1e-4; PROCESSOR's 1e-9 not
adopted anywhere). (2) Starting-point policy per call site: no default — the
caller passes the policy; HannanRissanen/Multiple remain rejected pending
21-A.2, so the pinned builder default (HannanRissanen) is not reproduced as
a Kalos default either. (3) The `legacy`-style product-default question does
not arise at this seam: the core keeps all control fields as required
arguments (consistent with the 22b.3a resolution pattern).

## Hannan-Rissanen initializer and the HR-family starts (21-A.2) — 2026-11-17

**Scope.** Item 21 stage E, second and last planned 21-A increment: the
bytecode-pinned transcription of `HannanRissanenInitializer` and its
`HannanRissanen` three-stage pipeline (new unit
`include/kalos/hannan_rissanen.hpp` + `src/hannan_rissanen.cpp`) and the
`HannanRissanen`/`Multiple` starting policies of `estimate_sarima_fixed`
(the pinned two-pass `feps` internal-precision chains, the pinned alternate
start and the strict-likelihood `dcmpg` selection). Supporting qualified
primitives: `polynomial_roots` (companion matrix + DGEEV) in the
linear-algebra unit and the `dgeev` provider adapter. NONE of the nine
maintainer-decision items was promoted: `feps` becomes numerically active
(read by the Multiple chains) but remains required request data with no
default; the initializer's `stabilize`/`useDefaultIfFailed`/`failifunstable`
and `biascorrection`/`finalcorrection` settings stay pinned call-site values,
not configuration.

**Oracle.** The 10-case HannanRissanen/Multiple corpus
(`validation/evidence/native/estimation-control-hr/`) reuses the same
public-API observer through
`validation/reference/capture-estimation-control-hr.R`: complete responses,
fully free designs, builder-default 1e-7/1e-4 plus the pinned `PROCESSOR`
case at 1e-9, order-shape coverage (regular-only, MA-only, seasonal-only,
full seasonal, extra regressors) and an over-differenced near-unit-root case
whose pinned outcome exercises the stabilization branch
(`initialize.detail = "fitted,stabilized"`). The capture was run twice in
independent JVM processes and is byte-identical. The rms < 1e-9 variance-guard
branch is deliberately NOT a corpus case: the guard fires only where the ML
variance is zero and the likelihood unbounded, and the pinned landing point
there proved process-dependent (measured: same input in two pinned processes
landed at `[-0, -0.9]`/ll 1869.854 and `[-9.7e-10, -3.6e-9]`/ll 1841.836); the
branch is qualified at the unit level (`Source::ZeroVariance`, zero blocks)
instead.

**Qualification catches.** The bit-exact reuse assertion (new in this
increment, also over the 10-record Multiple chain) exposed an out-of-bounds
design read in the first HR transcription: the pinned regression warmup is
the largest design LAG `max(p + P*bp, q + P*bq)`, not the parameter count —
with seasonal blocks the initial version indexed `warmup - lag` below zero.
Fixed before install; the initializer is now bit-deterministic across calls,
processes and thread counts. The HR block-order seam (`[ar, sar, ma, sma]`
returned by the initializer, `[ar, ma, sar, sma]` request blocks) is an
explicit permutation at the control layer.

**Deliverables.** `tests/test_hannan_rissanen.cpp` (new ctest, deterministic
LCG fixtures: AR(1)/MA(1) recovery, explosive-root stabilization,
zero-variance guard, defaulted fill, design-residual recovery, interrupt
propagation, input contract); `tests/test_estimation_control.cpp` regenerated
from both corpora (27 embedded cases, chronological record vocabulary, chain
shapes, selection bit, HR scope rejections); generator archived at
`validation/reference/generate-estimation-control-test.py`;
`tools/verify_estimation_control.cpp` sweeps both corpora (27/27, worst 6.2e-4
coefficient / 7.25e-6 likelihood — tolerances unchanged at 2e-3/5e-4);
`test_linear_algebra` extended for `polynomial_roots`. Gates: ctest 50/50 OMP
and serial; ASan/UBSan clean for `test_estimation_control` (906 s),
`test_hannan_rissanen` and `test_linear_algebra`; manifest 105/112/139;
`verify-migration.py` unchanged at 406.

**Maintainer-decision ledger (stage E, 21-A.2).** (1) `feps`: activated as
data for the Multiple chains; still no core default, promotion undecided.
(2) Starting-point policy: the pinned builder default (HannanRissanen) is
now a fully supported request value but remains caller-passed; no default is
introduced. (3) The initializer's boolean settings and the guard tolerance
1e-9 stay pinned transcription constants; adding them to a future options
struct is undecided. Next: 21-B (transformation decision) per the roadmap.

## Transformation decision, log vs levels (21-B) — 2026-11-17

**Deliverable.** Stage-T decision unit `include/kalos/transformation_decision.hpp`
+ `src/transformation_decision.cpp`: `decide_transformation` (and the exposed
`log_level_choose_log` comparison) transcribes the pinned sa-core
`LogLevelModule.process` over the 21-A control layer — the `dcmpg` non-positive
short-circuit ahead of all validation (both branches unrun, AICCs at the
cleared 0.0), the finite-response rejection (a positive NaN/infinity takes the
pinned missing-data staging, unqualified here), the level estimate, the
`copyOf + setLogTransformation(true)` twin, the `LogJacobian` window
(`-sum log y` over `[d + D*period, L)`, left-to-right), the `LikelihoodStatistics`
adjustment/counts/AICC forms (`-2 * (adjusted - n*k/((n-k)-1))`, NaN-sanitized
adjustment, `+1` innovation-variance count) and the `dcmpl` rule (log iff
`aiccLevel > aiccLog + correction`; equality and NaN select levels; missing-log
→ levels, missing-level → logs, both missing → `"failed"` keeping levels).
A branch exception is the pinned null estimation: recorded as unavailable,
never escaping as a decision failure. No fit seam is wired; the unit decides.

**Call site.** Decoder-pinned (verified in the `X13SpecDecoder` bytecode): the
module builds exactly `DemetraUtility.processor(true, 1e-7)` — ML, `Multiple`
start, builder-default `feps = 1e-4` — with `preadjust = None`,
`outliersCorrection = false` and the `TransformSpec DEF_AICDIFF = -2.0`. The
unit accepts the correction only as required request data; starting point,
fixed blocks, masked/fixed-design requests and too-short responses are
explicit rejections (the pinned `estimate()` paths they would reach are
unqualified). The preadjust and outlier-correction pre-fit staging stays with
stage O.

**Oracle.** The 11-case corpus `validation/evidence/native/transformation-decision/`
runs the REAL module through the public-API observer
`validation/reference/TransformationDecisionProbe.java` (`LogLevelModule`,
`ModelDescription`, `RegSarimaModelling`; design columns via the product's own
`UserVariable` path, since a `Constant.C` Variable is silently dropped by the
pinned `check()` — the captured `regarima()` matrix is stored so the replay
re-creates identical columns). Captured by
`validation/reference/capture-transformation-decision.R`, byte-identical in
two JVM processes (standing double-capture policy). Coverage: decisive log,
near-tie log (gap +0.15), decisive level, past-boundary level (+2.51),
seasonal multiplicative, user columns at `d = 0`/`d = 1`, both short-circuits.

**Qualification catches.** Two fixtures exposed a replay-fidelity class the
21-A corpora never reached: on oscillatory/seasonal-growth data the discrete
`+0.9/-0.9` alternate-start sign of the Multiple policy flips under
JVM-vs-Kalos landing micro-drift near the sign change, so the qualified
replay legitimately lands in the other local basin (observed drift up to
1.65 log-likelihood / 2.0 coefficient on `reflected_level`, `seasgrowth_log`);
`growth_log` (log branch) and `cal_column` (log branch) stop at different
points of the same flat near-unit-root ridge (4.3e-3 / 0.11). The corpus is
tiered accordingly — decision bit, availability, counts, Jacobian data
function and AICC identity pinned for every case; landing exact except the
flat-ridge bands (1e-2/5e-1 ll) and basin-unpinned on the two flip fixtures
with decision margins asserted > 1.0 (observed ≥ 32). Same qualification
class as the 21-A variance guard; flagged for a future 21-A replay-fidelity
pass in the contract and roadmap ledger.

**Deliverables.** `tests/test_transformation_decision.cpp` (new ctest #51,
generated by `validation/reference/generate-transformation-decision-test.py`:
11 corpus cases, comparison boundary/NaN semantics, scan-before-validation
ordering, all six scope rejections, bit-level reuse determinism);
`tools/verify_transformation_decision.cpp` sweeps the corpus (11/11, worst
clean-band drift 3.9e-4 coeff / 1.5e-6 ll). Gates: ctest 51/51 in the Release
OpenMP and serial builds; ASan/UBSan clean for `test_transformation_decision`;
manifest 107 core / 112 fixture / 143 evidence; `verify-migration.py`
unchanged at 406. Contract:
[transformation-decision-contract.md](docs/contracts/transformation-decision-contract.md).

**Maintainer-decision ledger (stage T, roadmap decision 3).** Nothing
promoted: the −2.0 comparison value is captured call-site data and stays a
required argument; the preadjust-on-decision question stays open and this
slice pins only the `preadjust = None` call site. Next per the roadmap:
21-C (outlier scoring, stage O) or the 21-A replay-fidelity pass on the two
basin fixtures.

## Outlier scoring and forward selection (21-C) — 2026-11-17

**Deliverable.** Stage-O scoring/selection units
`include/kalos/outlier_scoring.hpp` + `src/outlier_scoring.cpp` +
`src/outlier_scan_engine.cpp`/`src/outlier_detail.hpp`: the pinned critical-
value chain (`intProbability`/`getProbabilityInverse` Cody transcription,
`calcCv` three-anchor interpolation with the `calcVAL` operand-stack fix —
`d11/d7 + (d7 − d9)`, LULinearSystemSolver2 at 1e-13, floor 2.8), the four
x13 factories (fills, zones, `transitory_change_rate(freq, monthly)`),
`regression_t_statistics` (the `tstats(0, false)` vector path over the
`"available"` status string, `llround(ssq/var)` dimension recipe), the
`ExactSingleOutlierDetector` scan (`scan_outlier_candidates`: Ansley with
the pinned `cnt − 1` `lcholesky` update bound, MaLjungBox `calcg(int)` with
the pinned column-*overwrite* recursion — not a decrement, verified bit for
bit against `m_G` —, `ljungBox(true)` ML residuals (`block[q..n+q)`),
Mad(50), pivoted-QR initialization, `Polynomial` trailing-zero stripping of
the expanded stationary polynomials, permanent exclusions, `searchMax`), and
the `ExactOutliersDetector` driver (`detect_outliers`: full-chain re-estimation
on open/add, warm rollback/verifymodel via the new `optimize_sarima_from`
core entry, `changed` never reset, pinned stop reasons, exclusion feedback).
The pinned `estimateModel` gate is *null estimation*, not convergence: a
stalled-but-finite fit is adopted and its tstats drive the decisions, and the
callback contract mirrors exactly that. One documented deviation: the
`getMaxTStat` sentinel escape (Kalos reports sentinel + 0 where the pinned
code indexes `T(−1,−1)` and throws).

**Qualification.** New corpus `validation/evidence/native/outlier-scoring/`
(7 files) through the new observer `OutlierScoringProbe.java` +
`capture-outlier-scoring.R`, double captured across two JVM processes
(byte-identical case files; the observer carries no algorithm). The sweep
`tools/verify_outlier_scoring.cpp` passes 98/98: grids 77 exact + 7 pinned
throws; 6 scan cases with exact winners and the sentinel deviation asserted;
8 driver cases with exact selections/stop reasons/rounds and final fits
inside the flat-valley basin band (t `1e−2 + 1e−4·|t|`, worst 0.37; params
`2e−3`, worst 0.61) — verified basin-not-surface noise: warm re-entry from
the JVM's own final parameters converges back to the Kalos fixed point at an
identical log-likelihood.

**Deliverables.** `tests/test_outlier_scoring.cpp` (ctest suite 51→52:
bit-level cdf/quantile/calccv reuse, factory fills/zones, t-stats guard and
NaN/zero propagation, scan request validation and scripted whitening status,
driver mechanics over scripted callbacks, warm-start record identity).
Gates: ctest 52/52 in the Release OpenMP and serial builds; ASan/UBSan clean
for `test_outlier_scoring`; manifest 111 core / 112 fixture / 150 evidence;
`verify-migration.py` unchanged at 406. Contract:
[outlier-scoring-contract.md](docs/contracts/outlier-scoring-contract.md).

**Maintainer-decision ledger (stage O).** Nothing promoted: cv stays required
request data (with `default_critical_value` exposed as the pinned formula,
floor 2.8 a transcription constant), the monthly tcrate 0.7, `eps = 1e-7`,
maxRound/maxOutliers 50/30 are captured call-site facts for 21-H, and the
`ZERO`/THRSH constants stay pinned. Next per the roadmap: 21-D (differencing
decision, stage F) or the 21-A replay-fidelity pass on the basin fixtures.

## Differencing decision, d/D selection and the mean test (21-D) — 2026-11-18

**Deliverable.** Stage-F decision unit `include/kalos/differencing_decision.hpp`
+ `src/differencing_decision.cpp`: the pinned `DifferencingModule` machine —
`calc/step0/initstep` (raw Hannan-Rissanen seam, 1.02 unstable gate, whole-
block stabilization, `setDefault` fill, trial fit on the filtered uncentered
response), `searchur` (mirrored quadratic roots, `ComplexType.abs` scaled
norm), `maincondition`/`cond1` (with its A-then-B fallthrough)/`allcond`/
`finalcond`, and `calcMean` (algebraic `stMean`, `nstMean` as the pinned
`meanCorrection` = constant regressor over the differenced surface with the
spec orders carried in new `spec_d`/`spec_bd` request fields). Every threshold
(0.9/0.88/0.91/0.89/0.869/0.136/0.15/0.16/0.17/0.19/0.25/1.02/1.12/1.005/
0.002, the 2.55/2.0/1.96 and 1.96/1.98/2.1/2.3/2.5 tables, the −0.1/−0.2 fill)
is a transcription constant; the module scalars are required request data.
New `EstimationSolver::PinnedLevenbergMarquardt` seam in estimation control
(21-D.2): the transcribed `RegArmaSsqFunction` deviances machine —
Yule-Walker `AutoCovarianceFunction` + internal `KalmanFilter` (`sfilter`/
`mfilter` + 1-column HWP/QRLeastSquares) + `DeterminantalTerm` +
`ConcentratedLikelihoodWithMissing` deviances `·sqrt(exp(ldet/n))` — under the
transcribed QR-LM (`detail/ssq_levmar`), qualified **bit-exact** against the
pinned JVM at both seam classes (0/131-entry residual mismatches, `ssq/ldet/
factor/ll` bit-identical), with `detail/fdlibm` (FdLibm Exp/Log bytecode
kernels, 230k-input fuzz bit-exact) and an MXCSR FTZ/DAZ guard reproducing the
JVM default FP environment. BFGS stays the default solver; only this unit
selects the pinned machine.

**Hannan-Rissanen refactor (support cluster).** The 21-D A/B isolation against
JVM probe vectors exposed four transcription bugs in the 21-A.2 pipeline, all
fixed in `src/hannan_rissanen.cpp` and now pinned by four new double-captured
JVM goldens (~1e-13): the `LevinsonAlgorithm` middle-slot temporary
(P·(1+k), not P·(1+2k)), `npi()` = `max((int)(log n)², max(p+P·bp, 2(q+P·bq)))`
(two `Math.max`, not a product), `updatemodel`'s btheta section start
`nP + q + block·(1+q)`, and the `finalcorrection` `BackFilter.apply` causal
layout. The pipeline's OLS now runs the bit-exact `fastLeastSquares`
transcription (DGELSY agreed only at full rank and steered the Multiple chains
by ulps), plus the OlsComputer epsilon-norm guard (defaulted start on near-zero
responses) and the `pinned_ols_1col`/`pinned_backfilter_apply`/FdLibm-`log`
transcriptions; the raw pipeline moved to `detail::hannan_rissanen_pipeline`
(no guard/stabilize/fill — the pinned `calc()` never soft-fails) and
`stabilize_block` to `detail/sarima_stabilization`, both shared with 21-D.
This changes existing 21-A.2 HR numerics toward the pinned JVM; all
pre-existing HR goldens stay in their bands and `verify_estimation_control` is
**27/27 unchanged** (worst coefficient 6.2e-4, worst likelihood 7.25e-06),
21-B 11/11, 21-C 98/98 — the corpora do not move.

**Qualification.** New corpus `validation/evidence/native/differencing-
decision/` (39 cases) through `DifferencingDecisionProbe.java` + `capture-
differencing-decision.R`: the observer builds the module exactly as
`X13SpecDecoder.readAutoModel` (cancel 0.1, `1/1.0309278350515465`,
`1/1.1363636363636365`, precision 1e-7, maxd/maxbd 2/1, ml never set) and
reads the end state (`iter, mlused, useml, rmax, rsmax, c_`, final orders and
parameters) reflectively. **Double capture** verified: the second independent
JVM process (OpenJDK 21.0.11, `_dexp/_dlog` intrinsics disabled with the
in-process guard — the contract fixes the bytecode FdLibm semantics as ground
truth) reproduced the csv files byte-identically. Three short-series cases are
pinned-crash rows (`NegativeArraySize`/`AIOOBE` in the pinned short-window HR)
and require a Kalos throw. Sweep `tools/verify_differencing_decision.cpp`:
39/39, decisions/flags/counters/`c_` exact, all 39 t-statistics and all
final-round parameters measured **bit-identical**; the only landing drift is
the step0 root-scan pair of `m_i2_144` (3.2e-5/1.31e-4), banded at 2e-4 as the
documented trial-fit basin tier.

**Deliverables.** `tests/test_differencing_decision.cpp` adds ctest suite 53:
Schur stability NaN flavours, mirrored-root/quadratic/`ComplexType.abs` bit
patterns, `searchur`, threshold tables, `finalcond` clamps, `cond1`
fallthrough, clamped-module mean-test replays solved for exact target
t-values, the JVM-invalid spike case, validation rejections, corpus replays,
determinism. Gates: ctest 53/53 in the Release OpenMP and serial builds;
ASan/UBSan clean for `test_differencing_decision`; sweeps 39/39 + 27/27 +
11/11 + 98/98; manifest 122 core / 112 fixture / 154 evidence;
`verify-migration.py` unchanged at 406. Contract:
[differencing-decision-contract.md](docs/contracts/differencing-decision-
contract.md).

**Maintainer-decision ledger (stage F).** Nothing promoted: decision 5 stays
open, the period domain {4, 12} is a qualification scope (the pinned JVM
accepts any period; the estimation seams are qualified there), and the
decoder reciprocals of `DEF_UB1/DEF_UB2` are captured call-site facts.
`SeasonalityDetector` and the fit-seam wiring stay with 21-H. Next per the
roadmap: 21-E (initial ARMA identification, stage M part 1).

## Initial ARMA identification, BIC candidates (21-E) — 2026-11-18

**Deliverable.** Stage-M decision unit `include/kalos/initial_arma_identification.hpp`
+ `src/initial_arma_identification.cpp`: the pinned `ArmaModule` initial-
identification machine — the `RegArimaUtility.olsResiduals` cursor (pinned
differencing + back-filter, fresh ones column, column-ascending residual
projection through the bit-exact `detail::pinned_ols_1col` and the DGELSY
rcond-1e-9 `qr_least_squares` for multi-column designs, a documented deviation
agreed at 1e-9), the Hannan-Rissanen start policy (`stabilize`,
`useDefaultIfFailed`, the single pinned-LM chain of 21-D.2), the three-stage
grid enumeration, the design-free candidates (21-C estimator with
`design.columns = 0` on the cursor, `BIC2 = (-2 · logLikelihood + k · log
n)/n`, `k = parameters + 1`, using the FdLibm log
of 21-D.2), the `mergeInto` panel of `models_count` slots with the pinned
`Double.compare` ordering, and the eleven `regARIMARefine` selection rules plus
the mixed-design grids. `acceptWhiteNoise` is transcription data the pinned
module stores and never reads. Pinned aborts (short differenced cursor,
zero-norm OLS response, unfilled estimations slot, rejected start/landing)
surface as `std::runtime_error`, request validation as `std::invalid_argument`,
and an interruption always propagates — the JVM exception-equivalence table is
in the contract.

**Qualification.** New corpus `validation/evidence/native/initial-arma-
identification/` (72 case ids `<series>/u{0,1,2}.o{d}{bd}.{m|n}`, the user-
kind explicit after an id collision on the two-user-kind series silently
overwrote cursors in early captures) through `InitialArmaIdentificationProbe.
java` + `capture-initial-arma-identification.R`: the observer builds the
module through the pinned package-private `Builder` exactly as
`X13SpecDecoder.readAutoModel` (panel 5, max orders 2/2/1/1, balanced false,
mixed true, `acceptWhiteNoise` false, eps 1.0E-7; variants vary the fields to
exercise the refinement rule groups and the panel-eviction path) and reads
`olsResiduals` cursors, per-candidate `RegArmaBic.sort` results with the
private fitted `SarimaModel` reflectively, the `processor(true, eps)`
likelihood statistics and the `select` outcome plus `getPreferedModels` panel.
72 cursors, 1,908 candidates, 288 selects, **double capture** byte-identical
(OpenJDK 21.0.11, `_dexp/_dlog` intrinsics pinned off). Sweep
`tools/verify_initial_arma_identification.cpp`: 4,368 checks, **0 failures**
(1,858 exact, 29 cells in the documented basin/iteration-drift/panel-reorder
classes, 1 documented skip); all 288 select decisions and panel memberships
exact, cursors bit-exact on single-column designs and within 2.13e-14 on the
multi-column ones, landings in the tight band (1e-5 BIC2 / 5e-4·n loglik /
2e-3 coefficients) except the basin-class cell (worst 0.097 BIC2, `wn36/
u0.o00.n/m2,2,0,0`) and the 8 drift cells; `ar120/u2.o11.n` (differencing
annihilates the design, cursor dev 0.588) is the counted skip. The C++
Hannan-Rissanen starts agree with the JVM probe's `hr` output to ≤1 ulp, and
the shared smoke battery is 37/48 bit-exact (worst 5.18e-07 in BIC2).

**Deliverables.** `tests/test_initial_arma_identification.cpp` adds the 54th
ctest suite: the smoke replay (JVM cursor bit-exact, 15-evaluation grid
enumeration, 14-order oracle table, panel and winner (1,1,0,1)), request
validation, the pinned abort equivalences, interrupt propagation, the
stored-never-read `acceptWhiteNoise` bit-identity both ways, determinism.
Gates: ctest 54/54 in the Release OpenMP and serial builds; ASan/UBSan clean
for `test_initial_arma_identification`; sweeps 4368-checks/0 + 27/27 + 11/11
+ 98/98 + 39/39; manifest 124 core / 112 fixture / 162 evidence;
`verify-migration.py` unchanged at 406. Contract:
[initial-arma-identification-contract.md](docs/contracts/initial-arma-
identification-contract.md).

**Maintainer-decision ledger (stage M).** Nothing promoted: decision 6 stays
open — the BIC refinement thresholds (0.0025…0.25) and the balanced/mixed
switches are transcription constants, and the decoder call-site state (panel
5, max orders 2/2/1/1, eps 1.0E-7) stays required request data rather than a
config default. The period domain {4, 12} is a qualification scope (the pinned
grid builder takes any period). The `SarimaComponent` hill-climb, AICc
comparison and the round structure stay with 21-F/21-H. Next per the roadmap:
21-F (refinement and model comparison, stage M part 2).

## ARMA refinement and comparison, AutoModellingModule (21-F) — 2026-11-18

**Deliverable.** Stage-M part-2 decision unit `include/kalos/arma_refinement.
hpp` + `src/arma_refinement.cpp`: the pinned `AutoModellingModule.process`
machine — `RegArimaUtility.linearizedData` (user regressors removed with the
concentrated-likelihood coefficients in the pinned ascending-column order, the
mean coefficient skipped so the mean stays in the series), the
`DifferencingModule.process` re-entry on the linearized response (21-D unit,
verbatim), the module's own re-differencing with the pinned `UnitRoots.D`
**one-tap** polynomials — `(1 - B^d)(1 - B^period)^bd`, genuinely different
from the 21-E cursor filter at `d = 2` (transcribed as the local
`automod_unitroot_polynomial`) — the `ArmaModule.select` machine on that
sequence, and the `SarimaOrders.equals`/mean-flag comparison producing
`specification_changed`/`mean_changed`/`changed`. The whole guarded region sits
inside the pinned `RuntimeException` catch whose handler is the
`SarimaOrders.airline(period)` fallback (mean off, records stopped at the
failed stage, `fallback_stage` naming it); the interrupt contract arms on the
first callback firing so post-fire exceptions rethrow instead of converting
into the fallback. 21-E was refactored to expose the clean seam
`select_arma_orders` (semantics and records unchanged); the unit consumes the
current estimation's `cl.coefficients()` as request data — the ARMA landing
provably never enters the machine. The roadmap's `SarimaComponent` hill-climb
/`AICcComparator` vocabulary is dead or stage-R code in the pinned 3.9.0
bytecode (zero `SarimaComponent` users; `AICcComparator` only in the
calendar/TD modules) — resolved in the pinning ledger and the contract.

**Qualification.** New corpus `validation/evidence/native/arma-refinement/`
through `ArmaRefinementProbe.java` + `capture-arma-refinement.R` (public
pinned API plus the reflection-built `ArmaModule`, no decision rule in the
observer): 23 cases — white noise/AR/random-walk/seasonal-walk/doubly-
integrated series, n in [16, 144], period {4, 12}, mean on/off, 0–2 user
regressors, current specs driving `d → 2` — 21 normal runs and 2 genuine
airline-fallback runs (`m_sw20`, `m_sw16`). Double capture from independent
JVM processes: byte-identical, `SHA256SUMS.csv` pins the bytes.
`tools/verify_arma_refinement.cpp` replays **423 checks with 0 failures**:
linearized data, re-differencing filter (incl. the `d = 2` one-tap cases),
differencing quadruple, all 21 `tmean`s, every decision/flag and all 105 panel
slot orders bit-exact; panel cells 86 exact + 16 tight-band + 3 basin/drift-
class (two drift `(2,2,1,0)` slots in the `m_i2_d2a`/`m_i2_d2b` pair, one
converged basin cell `m_i2_s7/4`; worst dBIC2 7.9e-06, dcoef 1.5e-2 — no
decision affected). The 21-E tight/basin/drift ceilings are reused unchanged.

**Defect finds (both fixed here).** (1) `ArmaRefinement::fallback` was read
before assignment on the normal path (uninitialized `bool`): the first corpus
sweep run landed every case in the fallback through the garbage flag; fixed
with default member initializers in the result struct. (2) The 21-D
`DifferencingIterationRecord` left `trial_fit`/`hr_stabilized` uninitialized
whenever the healthy path skipped the writing branches: UBSan caught the
invalid-bool load during this slice's sanitizer gate, and the 21-D sweep's
trial-fit band selection had been riding on that indeterminate value. Fixed
by default-initializing the record (reads only, zero decision effect — 55/55
both lanes, 21-D sweep `39/0` with `tmean`/params drifts still 0), making the
sweep classify a run into the step0 root-scan tier from any round's trial fit
(the contract's own tier text), and correcting the 21-D contract wording: six
cases take the 2e-4 root-scan tier (`m_i2_144` worst at 3.2e-5/1.31e-4, the
other five ≤1.5e-6), disclosed in both contracts.

**Deliverables.** `tests/test_arma_refinement.cpp` adds the 55th ctest suite:
the validation battery, the `m_wn96_a`/`m_walk96_trend`/`m_i2_d2a`/`m_sw20`
replays (panel BICs, `tmean`s, linearized data and filtered heads bit-exact),
the interrupt contract on both paths, the stored-never-read `acceptWhiteNoise`
bit-identity and determinism. Gates: ctest 55/55 in the Release OpenMP and
serial builds; ASan/UBSan clean for `test_arma_refinement` and
`test_differencing_decision`; sweeps 423/0 + 4368/0 + 27/27 + 11/11 + 98/98 +
39/39; manifest 126 core / 112 fixture / 170 evidence; `verify-migration.py`
unchanged at 406. Contracts: [arma-refinement-contract.md](docs/contracts/
arma-refinement-contract.md) plus the 21-D tier-correction note.

**Maintainer-decision ledger (stage M part 2).** Nothing promoted: decision 6
stays open — both decoder-built module states (differencing clamps/boundaries
and the ArmaModule panel/grid/policy scalars) stay transcription constants
arriving as required request data; the qualified period domain stays {4, 12};
the re-differencing `d` clamp rides the request `max_d` (decoder 2). The
round-structure wiring (and with it the first fit-seam consumer of
`refine_arma_model`) stays with 21-H. Next per the roadmap: 21-G (regression
tests + mean rules, stage R) or the 21-H round structure, per priority.

## Regression variable tests and mean rules, stage R (21-G) — 2026-11-19

**Deliverable.** Stage-R decision unit `include/kalos/
regression_variable_tests.hpp` + `src/regression_variable_tests.cpp`: a
literal transcription of the three live stage-R machines —
`RegressionVariablesTest.process` (the trading-days pass with the
`DerivedRegressionTest(t, ml = true)` second life, the width-1 LoP pass, the
immediate-removal moving-holiday pass, the all-fail-or-none batch gate with
the adjusted-only preadjustment reset and the meanTest removal),
`MeanController.test` (the checkMu machine, strict `|tstat(0, 0, false)| >
cval`) and the kernel's final mean rule (`automodelling && !mean && checkMu
&& rtval > CVALFINAL 2.5`) — on top of the bit-exact concentrated-likelihood
statistics machine (`tstats(0, false)` with no zero guards, the scalar
`tstat(0, 0, false)` with its two pinned NaN guards, the derived joint
statistic on the pinned left-fold summations, the `TRegressionTest` accept
counting).

**Scope resolution (the slice NOTE).** Both NOTE candidates are not stage-R
code in the pinned 3.9.0 bytecode: user-variable elimination never enters
`RegressionVariablesTest.process` (filters: isTradingDays/
isLengthOfPeriod/isMovingHoliday, never isUser; user detection is the
separate `UserVariablesDetectionModule`, dead on the default decoder path,
deferred to 21-I), and the outlier-fix integration is stage O/round
structure (`OutliersDetectionModule.process` re-adds the variables; the
kernel's `isOutlier(v, true)` sweep is 21-H round logic). The live stage R —
the two decoder-built `RegressionVariablesTest` runs, the `checkMu` call
sites and the final mean rule — is what this unit qualifies.

**Pinning.** `FRegressionTest` is a dead decoder path (the builder defaults
`ftd = 0` and the decoder never calls `tdJointTest`) — rejected fail-closed.
Call-site cutoffs: initial test `tmu = ttd = tmh = 1.96`, derived on; the
final test re-uses the builder with `meanTest(1.0)`; `MeanController`
constants 1.96 (round-0 `checkMu`), 1.6 (per-round `redoEstimation`), 2.5
(`CVALFINAL`, strict) and the 1.0E-5 mean-added guard; `checkMu =
usingAutoModel || MeanSpec.isTest()`. The estimations the machines consume
(current / mean-added through `processor(true, 1.0E-5)`; the
`ModelController` rtval) are required request data at the 21-H boundary —
no estimation runs inside the unit.

**Qualification.** New corpus `validation/evidence/native/regression-tests/`
through `RegressionTestsProbe.java` + `capture-regression-tests.R` (public
pinned API; variables built exactly as `X13ModelBuilder` builds them; no
decision rule in the observer, the OBS accept rows are direct pinned-form
calls for observation only): 18 cases — period {4, 12}, TD4/TD2, LoP,
one/two Easter windows, user columns, mean on/off, log/preadjusted states —
double capture byte-identical across two independent OpenJDK 21.0.11
processes, `SHA256SUMS.csv` pins the bytes.
`tools/verify_regression_variable_tests.cpp` replays **494 checks with 0
failures and no tolerance tiers**: the `tstats(0, false)` vector, the mean
`tstat(0, 0, false)` on all 36 MEANSTAT rows (both estimation states) and
the derived value/statistic on the 8 OBS derived rows bit-identical; every
pass accept, every `process` outcome (changed/mean/preadjustment/remaining
variables, four builder shapes per case) and every MeanController decision
(cutoffs 1.96 and 1.6) exact; the strict-`>` final rule on the 18 captured
rtvals. Coverage includes the all-fail batch (m_allfail, m_derived), the
b-gate with an immediate MH removal (m_tdpass), the adjusted preadjustment
reset firing and not firing (m_adj), user-variable inertness, mean
add/keep/remove, and the nx = 0 zero-regression description (m_meanadd,
m_log_meanoff). All captured derived tests reject; the derived-rescue branch
is qualified by the unit test.

**Deliverables.** `tests/test_regression_variable_tests.cpp` adds the 56th
ctest suite: the bit-exact `m_allfail` statistics/decision and `m_meanadd`
mean-added replays, the strict-cutoff boundary, both NaN guards, the
isolated b-gate/c-gate/derived-rescue/preadjustment-reset branches, the
fail-closed validation battery and determinism. Gates: ctest 56/56 in the
Release OpenMP and serial builds; ASan/UBSan clean for
`test_regression_variable_tests` and the corpus sweep; sweeps 494/0 + 423/0
+ 4368/0 + 27/27 + 11/11 + 98/98 + 39/39; manifest 128 core / 112 fixture /
179 evidence; `verify-migration.py` unchanged at 406. Contract:
[regression-tests-contract.md](docs/contracts/regression-tests-contract.md).

**Maintainer-decision ledger (stage R).** Nothing promoted: decision 6 stays
open — the builder cutoffs (1.96/1.0/1.6/2.5), the dead-path rejection and
the qualified period domain {4, 12} stay transcription constants; every
module scalar, statistics block and variable layout is required request
data. The round-structure wiring — when each machine fires, the
re-estimations, the mean-added estimation and the `ModelController` rtval
computation — stays with 21-H; user-variable detection with 21-I. Next per
the roadmap: 21-H (integration, stage Z).

## 21-H integration slice 1: decision units for p-values, diagnostics and the test cascade — 2026-11-19

**Scope.** 21-H's stage-Z composition needs three JVM decision machines
that no earlier item qualified: the chi-square p-value path behind
`ModelController`/`LjungBox`/regres's diagnostics, the ARMA diagnostic
statistics themselves (regres's acf, Ljung-Box test length and test, the
residual t-statistic and the accept rule), and the estimation-time
significance-test cascade with its root-promotion rules. This slice
delivers all three as qualified units — pure decision units with
deterministic inputs; the round driver that calls them is the next slice.

**Units.** `special_functions` (`include/kalos/detail/special_functions.hpp`)
transcribes `jdplus.toolkit.base.core.dstats.SpecialFunctions` —
`logGamma` (Stirling with the six-term correction polynomial; Lanczos
below 150), `IGSeries`, `IGContFract` (10000 iterations, 1e-10 exit, the
negative-terminator exception) and `incGamma` (series below x = 150,
continued fraction above) — plus `ChiSquareTest`'s statistics;
`arma_diagnostics` (`include/kalos/arma_diagnostics.hpp`) transcribes
regres's `AutoCovariance.autoCorrelation`, `LjungBox.calcLbLength`/`test`,
`TestsUtility.calcResidualStatistics` and the `ModelController.accept`
rule (NaN rtval accepts; NaN p never reaches the gate because the JVM
throws on NaN statistics first); `final_estimator`
(`include/kalos/final_estimator.hpp`) transcribes regres's test cascade —
`checkRoots` promotion, the AR/MA loop with the `sig2` 150-boundary and
`ar[1] == -1` rules and the takeover semantics — with `check_roots`
pinned verbatim from `FilterUtility` into `kalos::detail::check_roots`
(`src/ssq_levmar.cpp`'s local copy promoted without byte change).
The shared mlj evaluation engine (Hannan-Rissanen ACVF, both Ljung-Box
recursions, the ARMA filter utilities, rational coefficients, and the
sarima polynomial expansion extracted verbatim from
`SarimaItem.computeAuto` as `mlj::expand_sarima_polynomials`) moved
byte-preserved from `src/outlier_scoring.cpp` into `src/mlj_engine.hpp`;
`outlier_scoring.cpp` now includes it. All fdlibm bit-rep operations use
multiplication by 0x100000 (shift-form was signed-shift UB; bit-identical,
UBSan-clean).

**Evidence.** Three JVM-pinned goldens, captured twice (the captures are
byte-identical across processes, built with the `_dexp`/`_dlog` intrinsics
disabled): `validation/reference/special_functions_golden.txt` (2261 rows;
the 46 continued-fraction throw rows pinned as throws; all 2261 bit-exact),
`arma_diagnostics_golden.txt` (1637 rows + 9 exact-recurrence Ljung-Box
series across the MA1/ARMA(1,1)/ARMA(2,1)/SARIMA(1,1,1)(0,1,1)[12] models;
the 58 `df <= 0` throw cases pinned; all exact), and
`check_roots_golden.txt` (584 polynomials; all bit-exact). New ctest
suites `test_special_functions`, `test_arma_diagnostics` and
`test_final_estimator` replay them with 0 failures; the final-estimator
suite adds the 16-case cascade truth table.

**Pinning findings.** `AutoCovariances.autoCovarianceNoMissing` divides
covariances AND variance by n (not n − lag) — `AutoCorrelation`'s extra
n/(n − lag) factor applies only to its PACF/se output; a corrected
`acfDivisor` test against the JVM confirms this bit-exactly (the 21-C
engine's `dot/ssq` helpers were already on this basis). `Polynomial`'s
root enumeration (`getCheckRoots`, hence `getComplexRoots`) is
`Random`-seeded nondeterministic upstream — already documented for
`Polynomial.checkRoots` at degree >= 3 in the 21-G contract; the decision
machines route only through the deterministic `FilterUtility.checkRoots`,
which the goldens pin directly, and the degree >= 3 path is guarded
fail-closed in `kalos::detail::check_roots` (verified against bytecode
that negative limits always accept there). `logGamma(0.5)` and
`logGamma(8)` match `lgamma` exactly; the general form is pinned via the
golden rows.

**Deliverables and gates.** 3 new headers, 3 new sources, 1 moved engine
header, 1 shared header, 3 new test suites (ctest now 59), 3 golden
capture generators + 3 golden files, `tools/source-files.json` [core] at
136. Gates: ctest 59/59 in the Release OpenMP and serial builds; ASan/
UBSan clean on all three new suites; sweeps 494/0 + 423/0 + 4368/0 + 27/27
+ 11/11 + 98/98 + 39/39 unchanged after the engine move and the
`check_roots` promotion (the 21-C sweep re-proven at 98/98); manifest and
`verify-migration.py` unchanged at 406. Contracts:
[special-functions-contract.md](docs/contracts/special-functions-contract.md),
[arma-diagnostics-contract.md](docs/contracts/arma-diagnostics-contract.md),
[final-estimator-contract.md](docs/contracts/final-estimator-contract.md).

**Maintainer-decision ledger (stage Z, slice 1).** Nothing promoted: the
iteration counts and epsilons (10000/1e-10/8e-11, 50/1e-10, 1.25/0.1, the
calcLbLength table) are transcription constants; the Ljung-Box `nhp`, the
cascade `tsig` and the significance levels arrive as request data exactly
as the JVM call sites pass them. The round-structure wiring — when these
units fire inside `ModelController`/`OutliersDetectionModule` rounds — is
the subject of the next 21-H slice.

## 21-H integration slice 2: pipeline contract and design boundary — 2026-11-19

**Scope.** The stage-Z integration contract for the automatic-spec
pipeline: the seam design (an `x13_automatic_fit` request/result pair
composed on the qualified fit downstream — preadjust, the selection pass
on the decoder X-11 surface, the results-builder stage, the finals — with
the restricted fixed fit untouched and the Bfgs solver default and the
explicit sigma policy preserved), the current-jar `RegArimaKernel.calc`
composition order with the per-step map onto the qualified units (21-B
transformation, 21-G mean/regression tests, 21-C outlier rounds with the
forward-add and `isOutlier(v, true)` fixing vocabulary, 21-F refinement
wrapping the 21-D/21-E differencing/identification, the slice-1
`ModelController` accept machine and `final_estimator_test`, the pinned
pass2/`lastSolution`/`checkUnitRoots`/`checkMA`/`redoEstimation` round
machinery), the estimation adapter call-site table, the chained
decision-record provenance vocabulary, the scope envelope (monthly,
horizons {0, 0}, the four modes, the 21-I deferrals) and its fail-closed
matrix, and the slice-3 tier policy per the 21-B/21-C/21-F basin
precedent. Contract:
[x13-automatic-pipeline-contract.md](docs/contracts/x13-automatic-pipeline-contract.md).

**Pinning resolutions this slice.** `SeasonalityDetector` is dead in the
current jar: the interface has zero callers and zero implementors across
the `cur/` dumps of the live pipeline (grep of every dumped class), the
same resolution as the 21-F `SarimaComponent` finding — the roadmap's
stage-F "backed by SeasonalityDetector" dependency is void and the
driver builds no seam for it. The landing CL-statistics producer for the
round driver is the 21-C-sweep recipe (`sarima_regression_likelihood` +
`regression_t_statistics` at the retained candidate; basin tiers, not
bit-exact — the bit-exact `detail::ssq_deviances` CLWM covers only the
design-free/single-column machines); recorded as gap G2 with the
maintainer's alternative (a general bit-exact CLWM unit) kept on the
table. The LS `regeffect` pairing (AO capture-pinned Irregular; LS
expected LevelShift) stays unverified until the first LS-selected
capture (G3).

**Boundary.** The driver code did NOT land at this boundary. The final
cascade of the pinned `FinalEstimator.estimate` loop consumes the final
point's asymptotic-covariance diagonal, and its producer (the pinned
`NumericalDerivatives` stencil → information → `SymmetricMatrix.inverse`
path behind `final_estimator_test`) is not implemented and not qualified
(gap G1; the pinning notes recorded it as the remaining gap and the file
`src/final_information.cpp` was never created). Landing the driver around
an unqualified covariance producer would invent numerics at the product
seam, so the slice plan is extended per the contract: **21-H.0** the
final-information unit (double-JVM-captured golden qualification),
**21-H.1** the driver (`src/x13_automatic.cpp`, the modelling state, the
adapter, the decision chain, the fail-closed matrix, `test_x13_automatic`),
**21-H.2** the end-to-end oracle. A maintainer decision request on G1
(build the unit) vs a deferred final cascade (an explicit fail-closed
status while the automatic kind ships restricted) is outstanding.

**Gates.** Docs-only commit (contract, roadmap stage-Z ledger, this
entry): no source, CMake or manifest file changed, so the manifest stays
at 136 core and `verify-migration.py` at 406 by inspection (re-run green);
ctest in both lanes re-run from the final tree unchanged at 59/59; the
seven sweeps and the sanitizer surface are unchanged by construction (no
code path exists to alter) — re-proven at the next code commit.


## 21-H.0 Final-information producer, gap G1 closed — 2026-11-19

**Unit.** `kalos::final_information` (`include/kalos/final_information.hpp`,
`src/final_information.cpp`) transcribes the final-point tail —
base deviance → `SarimaMapping2` epsilon domain (`STEP =
Math.sqrt(2.220446E-16)`, `final_information_step()`) →
`NumericalDerivatives(point, false)` (`checkepsilon` halving/flip over the
whole-vector `checkBoundaries`, eager forward nodes, `calcgrad` with the
`fp == fx` guard, `calch` diagonals over `sqrt(|eps|)` and four-corner
cross terms, all Java comparison flavours literal) → the
`RegArmaProcessor.compute` scalings → `SymmetricMatrix.inverse`
(`lapack_Cholesky.lcholesky` tol 0 → `LowerTriangularMatrix.inverse` →
`LtL`, Crout–Doolittle fallback at `Constants.getEpsilon`). The request
carries the raw-convention point, the literal `nobs` (the JVM passes ≤ 0),
the limits and the deviance callback; `nullopt` models "the pinned
evaluation threw" (catch → fx inside the stencil, base rejection before
it). Statuses: available / inverse_failed / final_point_rejected. Design
columns stay with 21-H.1. Contract:
`docs/contracts/final-information-contract.md`.

**Qualification (the 21-C sweep recipe as the standing standard, per the
maintainer ruling of this date — double JVM capture, byte-identical
processes, bit-exact tiers, no relaxed tolerances).** Corpus
`validation/evidence/native/final-information/` (27 cases / 362 deduped
nodes; 13 available, 9 inverse-failed, 5 rejected — the probe
(`validation/reference/FinalInformationProbe.java` +
`capture-final-information.R`) replays the whole tail at the estimator's
own Point and aborts on any bit mismatch, so the 8 est rows certify
tail == estimator bit for bit). ctest #60 `test_final_information` replays
all 27 cases bit-exactly through a raw-bit node-table oracle (misses and
uncovered nodes are stencil drift; THROW nodes exercise the catch) plus
structural cases; the generator is
`tools/gen_final_information_golden.py` → `tests/jvm_final_information.h`
(base node injected: `fx_hex`, or the THROW for rej rows). Sweep
`tools/verify_final_information.cpp`: 266 bit-exact checks, 0 failures,
with the kernel gated on its qualified domain (est converged points 8/8
bit, rejected points 5/5) and node-level kernel agreement tallied (283
agree / 93 outside-domain).

**Pinning findings.** `inverse` deep-clones for the Cholesky branch and
its catch decomposes the ORIGINAL argument (`CroutDoolittle.decompose`
mutates only its `toArray` copy); `lcholesky` tol 0 throws iff
`!(s > -tol)` — NaN passes, the `s < tol` branch is dead; the Crout pivot
guard (`|p| < 1.1102230246251565E-16`, `dcmpg`) also lets NaN through, and
`solveUx`/`solveLx` singularity guards exclude NaN rhs via `dcmpl`. The
stencil's `checkepsilon` checks the WHOLE vector through
`SarimaMapping2.checkBoundaries` — that is why points sitting exactly on
a limit zero their eps and why an all-zero-eps row collapses to a single
base node (its forward `newval(i, 0)` dedups onto the base key). Recorded
for follow-up (outside the 21-D.2 kernel qualification domain, explicitly
not gated by the sweep): the pinned deviance kernel drifts 1–2 ulp from
the corpus stencil values at some ±√eps nodes of the (0,1,1)(0,1,1)12
rows and structurally for the single bq = 2 row (i_dim6); the producer
surface is bit-pinned against the capture independently of the deviance
source.

**Rulings recorded (maintainer, 2026-11-19).** (1) 21-H.0 lands before
21-H.1 — the driver never composes an unqualified covariance producer
(closes the decision request in the slice-2 entry). (2) The 21-C sweep
recipe is the standing qualification standard for every future double-JVM
unit. (3) Scratch root: the orchestrator moved all scratch from
`/tmp/opencode` (left to die) to
`/data/home/kowa/seasonaladjustment/tmp_opencode` — provider, builds,
dumps and pinning notes live there now; successors must never write to
`/tmp`. Build trees at this boundary: `kalos-build2` (Release, OpenMP),
`kalos-build2-serial`, `kalos-build2-san`, all configured with
`-DOpenBLAS_ROOT=<scratch>/openblas`; runs need
`LD_LIBRARY_PATH=<scratch>/openblas/lib`.

**Gates (final tree).** Fresh clean configure; ctest 60/60 in the OpenMP
lane and 60/60 serial (59 pre-existing + #60); ASan/UBSan clean on the new
unit (0 findings); the seven existing sweeps unchanged — differencing 39,
transformation 11, estimation-control 27 (+ hr 10), initial-ARMA 4368,
arma-refinement 423, outlier-scoring 98, regression-tests 494;
`check-manifest.py` green at 138 core / 112 fixture / 184 evidence;
`verify-migration.py` unchanged at 406; the 406-root historical corpus
untouched.

## 21-H.1 Automatic-spec X-13 pipeline driver (stage Z) — 2026-11-19

**Delivered.** `src/x13_automatic.cpp` + `include/kalos/x13_automatic.hpp`:
the pinned `RegArimaKernel.calc` transcription composed on the qualified
stage-Z units (21-B transformation decision, 21-G mean controller /
regression-variable tests / final mean rule, 21-C outlier rounds on the
isOutlier(v, true) vocabulary, 21-F refinement, slice-1 model_controller
accept, the seven-filter pass2 gate, checkUnitRoots/checkMA/redoEstimation/
lastSolution, and the FinalEstimator cascade on `final_estimator_test` +
the 21-H.0 final_information producer). The result reuses the restricted
fit's `X13FitResult` verbatim as its downstream composition block
(preadjust -> `x11_selection_pass` over the decoder X-11 surface ->
results-builder -> finals); the fixed path is untouched by construction
and `x13_fit.{hpp,cpp}` carry no change. `X13FitSpecKind` lives on the
automatic header so the fixed header stays byte-identical. `tests/
test_x13_automatic.cpp` lands with the driver (140 checks).

**Gaps closed at landing.** G3 resolved by direct JVM probe (jshell,
`SaVariable.defaultComponentTypeOf`): AO/TC/SwitchOutlier -> Irregular,
LevelShift -> Trend, SO -> Seasonal — the earlier LS/SO expectations were
wrong (pinning notes SECTION 15.1). G1 fed through the 21-H.0 producer;
G2 stands as the 21-C landing recipe (never bit-claimed).

**Engine fix found by the suite (ASan).** The JVM `MaLjungBox.calcg` reads
`psi[m_n − j]` for `j < q` from an `m_n`-length window — the JVM throws an
ArrayIndexOutOfBoundsException (failing the run) when `q ≥ m_n + 2`; the
native read was undefined behaviour. `src/mlj_engine.hpp` now traps both
this and the window-below-AR-degree case as `std::runtime_error` (the
driver maps them to `failed:<stage>`, matching the JVM null); no
corpus-qualified case sits in range — all eight sweeps bit-identical.
C++ gotcha recorded: `std::out_of_range` is a `logic_error` (a sibling of
`runtime_error`, not derived) — it would NOT be caught by the driver's
`catch (const std::runtime_error&)` that models the JVM catch.

**Driver fixes applied during the session** (audit of the recovered
draft, adjudicated against `cur/RegArimaKernel_full.txt`): pass-2 continue
rule and curva guard restored to their bytecode NaN flavours (`plbox <=
pcr` under dcmpg; dcmpl skip at 2.8) with `java_max` floors; cascade
`dim == 0` now checks the ARMA parameter count (`SarimaMapping2` getDim is
ARMA-only) instead of the regression design; the refinement `Changed`
branch clears the retained candidate before the round re-estimates; the
frequency refusal precedes the monthly-shape throws; dead code and an
inverted dcmpg/dcmpl comment removed. Pinning-note 12.4 pseudocode
corrections in SECTION 15.3.

**Test design (no oracle before H.2).** The suite gates the fail-closed
status matrix with the gate precedence pinned, the shape-throw matrix, the
stage-chain record vocabulary (every entry's tag in vocabulary, exactly the
payload the contract pins for the tag, recorded scalars equal to the
payload fields bit for bit), the final-description shape and downstream
composition (tables, triplet identity, pinned mode forcing), the
final-record echo against the public fields, the JVM variable vocabulary
(name format, G3 regeffect, factory column), and run-to-run bit-exact
determinism (cheap-shape twin). Cost policy: the full outlier search on
the probe corpus (~230 s) runs exactly once, everything rides on it; the
outlier-free twin (~20 s each) carries determinism. Pinned observations on
the probe corpus (LCG 12345, n = 96, AO +15 @ index 48, decoder 12.1
defaults): quick shape available / log / mean / (1,0,1)(0,1,1) / chain 12;
full case available / (3,0,0)(0,1,1) / one AO "AO (1994-01-01)" at 48 /
chain 24; `estimation_status` "converged". CMake raises this test's ctest
timeout to 3600 s (suite wall ~250 s at the test thread env).

**Gates (final tree).** ctest 61/61 in the OpenMP lane (new #60, 260 s)
and 61/61 serial (248 s); ASan/UBSan lane 61/61 with the new suite clean
in the foreground (677 s, zero findings); the seven sweeps unchanged —
differencing 39, transformation 11, estimation-control 27 (+ hr 10),
initial-ARMA 4368, arma-refinement 423, outlier-scoring 98,
regression-tests 494 — and final_information 266; `check-manifest.py`
green at 140 core / 112 fixture / 184 evidence; `verify-migration.py`
unchanged at 406; warning-free across lanes (cxx_std_17).

**21-H.2 cold-resume pointer.** The end-to-end JVM oracle per the
contract's slice plan item 3: automatic-modelling captures under
`validation/evidence/native/` (double-JVM byte identity, `jdprobe`/
`jarcache` infra), decisions diffed bit-exact against the driver's chain
record vocabulary (contract chain section + the landing deltas section —
which also supersedes the contract's G3 text), slice-3 boundary removals
fed by the `AutoUnitRootsRecord` margins. The restricted fit stays green
unchanged throughout; the suite data reproduces the probe corpus bit for
bit, so its pinned observations transfer to corpus cases directly. The
two untracked files (`src/kalos_write_probe.cpp`, `cpp-out.txt`) remain
user-pending, never `git add`.

## End-to-end JVM oracle: slice 21-H.2 lands the machine, the first qualification run, four driver fixes — 2026-11-20

The contract's slice-plan item 3 landed: `X13AutomaticProbe.java` +
`capture-x13-automatic.R` observe the real decoder-equivalent JVM
automatic pipeline (`X13Kernel.of(spec, ctx).process(...)`) with a
ProcessingLog observer, 13 cases captured under
`validation/evidence/native/x13-automatic/` with the double-JVM
byte-identity gate, golden header generated
(`tools/gen_x13_automatic_golden.py` -> `tests/jvm_x13_automatic.h`),
oracle test (`tests/test_x13_automatic_oracle.cpp`, ctest #62 with the
qualified subset quick+seas as registered args, timeout 1800, evidence
dir as argv) and the full-corpus sweep tool
(`tools/verify_x13_automatic.cpp`, argv case filters,
`KALOS_DUMP_ODM=1` diagnostic). The planned "plain" case was removed
pre-capture under the slice-3 boundary policy (TC@69-vs-AO@63 knife
edge, G2 seam - recorded in the contract's qualification section).

**Driver fixes (oracle-found).** (1) `process_outliers` passes the
RETAINED estimation's coefficients to the ODM, never the stale
spec-change-surviving carrier. (2) `Ansley::apply` unsigned underflow
at `p_ == 0` (pure-MA models) - the scan engine segfaulted the oracle
run; Java int-loop semantics restored (the 21-C corpus stays green -
both suites exercise the engine). (3) `checkMu` mean-on design
mismatch: the retained estimation now carries whether it was fitted
with the mean column (`estimation_has_mean_`), and a mean-on test
without a mean-included estimation runs the mean-added trial (the JVM
estimate-if-needed-before-the-mean-test machine) instead of reading
statistics from a mean-free carrier. (4) the ODM estimates first when
the model holds no estimation (the pinned `mod.estimate` before
`estimation.getArima()`), replacing the zero-block fallback. Fixes
3+4 gate latent states and left the corpus numbers unchanged; 1+2
changed results.

**Sweep verdict (contract qualification section).** Ten cases carry
bit-exact decision chains (quick, seas, ls, so, ao_nodet, d2, rw,
mode x3); quick+seas bit-identical across every family; 3730/68815
total check deviations are all in the measured display classes
(selection-coefficient sign, panel-BIC/AICC display scales, 2e-7 a8
drift) except the two over-accepting cases (ao, noout) whose round-0
ODM acceptance divergence is root-caused to the scan's first-entry
state with the follow-up's first move pinned (pinning notes 16.4-16.5:
the probe must record the JVM's best REJECTED |t|, then the 21-C entry
state gets re-pinned against the measured delta). Stage Z keeps this
single open item.

**Gates (this tree).** ctest 62/62 OpenMP lane and 62/62 serial lane
(new #62 at 19 s); ASan/UBSan oracle subset (quick+seas, 698 checks)
clean, exit 0; all nine sweeps unchanged; `check-manifest.py` green at
140 core / 112 fixture / 190 evidence (six x13-automatic files);
`verify-migration.py` unchanged at 406; `test_x13_fit` and
`test_outlier_scoring` green with the shared-engine fix; warning-free
(cxx_std_17). The two untracked files (`src/kalos_write_probe.cpp`,
`cpp-out.txt`) remain user-pending, never `git add`.

## CI red root-caused and fixed: the pinned FP-contraction semantics — 2026-11-20

CI has never been green (all 14 runs since the workflow's creation failed);
the diagnosis agent reproduced the exact 17-test CI set locally under
`-march=x86-64-v3` (the ubuntu-latest image migration of Oct-Nov 2026 moved
the label onto x86-64-v3/amd64v3 toolchain defaults, AVX2+FMA; macos-latest
arm64 has FMA architecturally). One root cause: the JVM golden corpus and
the vendored fdlibm transcription encode UNFUSED IEEE arithmetic - every
`a*b+c` rounds twice, and the split-precision Horner kernels depend on the
discarded low word staying discarded - while GCC/Clang default to
`-ffp-contract=fast` and fuse wherever the target exposes FMA (1-8 ULP
drift, occasional boundary flips: the IGContFract guard rows, the
differencing `threw` threshold, anchored_dot tables, the fdlibm 480/2261
special-function rows reproducing the CI counts bit-identically). Not a
regression, not the provider (the pinned OpenBLAS 0.3.34 is LAPACKE-complete
and provably outside every bit-pin path: OPENBLAS_CORETYPE sweeps all pass),
not a locator issue (goldens are embedded headers).

Fix: `target_compile_options(kalos PUBLIC -ffp-contract=off)` (GNU/Clang/
AppleClang; `/fp:precise` for MSVC) - PUBLIC because the inline kernels in
the shipped headers (`x11_pinned.hpp`, `parallel.hpp`, ...) compile into
consumer translation units. Evidence (this machine, gcc 13.3, pinned
provider): flag-off is bit-neutral (both lanes fully green, all nine sweeps
unchanged); the proof tree (`-march=x86-64-v3` + fix) passes the
previously-failing bit-pin subset (special_functions, parallel, x11_sigma,
final_information, arma_refinement), where without the fix exactly the CI's
17 tests fail. The optional workflow extras (ubuntu-24.04 pin, provider
sanity step) await maintainer decision and are NOT applied.

Evidence (this session): fix trees configured fresh - OpenMP lane ctest
62/62, serial lane ctest 62/62 (both zero warnings, -ffp-contract=off in
every kalos and test compile line); proof tree
`CMAKE_CXX_FLAGS=-march=x86-64-v3` + fix: the previously-failing bit-pin
subset (test_special_functions, test_parallel, test_x11_sigma,
test_final_information, test_arma_refinement) 5/5 green, where the same
ISA without the fix fails 480/2261 special-function rows and aborts on
`m.bic == bic[k]` - the CI texts bit-identically. Bit-neutrality
spot-checks vs the pre-fix library: verify_final_information 266/0,
verify_outlier_scoring 98/0, x11-bpass sweep 18/18 (counts identical).
`check-manifest.py` 140/112/190; `verify-migration.py` 406.
