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

## Automatic oracle fully qualified: slice 21-H.3 closes stage Z — 2026-11-20

Stage Z's last open item is closed: the automatic-pipeline JVM oracle
now carries **all thirteen cases decision-bit exact** and ctest
registers the full set (case filter removed, timeout 3600, full
in-lane run 390 s). The two H.2 residuals — ao/noout's round-0 ODM
over-accept and multi's (bp,bq) stage-grid — had the single root cause
that H.3's scan-replay probe facts exposed: the driver's `Description`
started at zero stationary orders instead of the `X13ModelBuilder`
airline (0,1,1)(0,1,1) the ODM entry spec carries, so the round-0
whitening ran on the wrong model. Three further transcription fixes
landed with the slice: the export sign convention (the JVM surface is
`SarimaSpec.parameters()`, all-plus AR/SAR signs — the export negates
the φ-blocks, the estimation keeps them), the landing AICC's adjusted
log-likelihood (the transformation Jacobian subtracts `sum(log y)`
over the estimation domain, the last `n − d − 12·bd` values, pinned by
the ao/rw stats facts), and the golden generator's factory-index
translation of the `Info` outlier picks (the SO-only case exposed it).

**Capture (this slice's owned double capture).** Fresh javac of the
probe (wrapper: the H.3 `odmscan`/`odmw`/`odmscanpost`/`refinestate`
measurement families); two independent JVM processes, four data files
byte-identical, `oracle-facts.csv` sha256 `798c81ac…` both; events and
all golden-relevant facts byte-identical to the H.2 capture (the
instrumentation is purely additive — the golden header diff is exactly
the four so-pick factory codes). The `KALOS_DUMP_ODM` src dumps used
during the diagnosis were all stripped (`outlier_scan_engine.cpp` back
at its pre-slice hot loop); the functional `ScanRecord` carrier stays.

**Tiers re-qualified on the full corpus** (contract tier table): panel
BIC2 1e-2 (measured 8.1e-3, membership bit), ARMA landing coefficients
1e-2 (measured 9.8e-3), tables 1e-6 (measured 5.0e-7), c17 1e-4 and
refine-x 1e-6 as sweep-only measured families, the scan-replay family
recorded informational (probe re-runs diverge legitimately at cv/
argmax near-ties — root causes in the contract; the real selections
are the bit-exact odm-decisions). T08's driver-internal trajectory
pins re-pinned to the airline-start path (contract 21-H.3 resolution).

**Gates (this tree, all fresh builds).** Oracle all-thirteen: 4637
checks 0 failures; full sweep 71401 checks 0 bad, 13/13 cases ok
(+13 informational); OpenMP lane 62/62 (433 s), serial lane 62/62
(569 s), ASan/UBSan oracle subset (quick+seas, 698 checks) exit 0 zero
findings; nine sweeps unchanged (differencing 39, transformation 11,
estimation-control 27 incl. hr 10, initial-ARMA 4368,
arma-refinement 423, outlier-scoring 98, regression-tests 494,
final-information 266); `check-manifest.py` 140/112/190;
`verify-migration.py` 406; evidence `SHA256SUMS.csv` verifies (5/5).
The two untracked files (`src/kalos_write_probe.cpp`, `cpp-out.txt`)
remain user-pending, never `git add`.

## Nonzero-horizon route: slice 21-H.4 lands `kalos::arima_forecasts` and wires the horizons — 2026-11-20

The `initialStep` extension branch is delivered: the automatic driver now
accepts arbitrary X-11 backcast/forecast horizons (month counts, negative
meaning |h| annual frequencies, normalized exactly as the pinned process
reads them) and drives the extension downstream — estimation and the
decision chain stay horizon-invariant (the horizon captures' orders/arma
facts are byte-identical to the zero-horizon oracle captures). Reusing
`forecast_information` was measured against the JVM stack and **rejected**:
a different machine (conditional endpoint prediction, not the CKMS filter
trajectory). Delivered instead as the faithful port of the pinned
`ExactArimaForecasts` consumer stack, unit `kalos::arima_forecasts`
(new contract arima-forecasts-contract.md; pinning notes
kalos-21h4-pinning.md sections A–J): stationary/diffuse CKMS
initializations, the 1e-15 steady rule, the mean-correction fold and the
JVM polynomial surface. The diffuse INIT state reproduces the JVM
post-initialize state to 1–2 ulp (probe11/12); the airline unit probe
lands forecasts within 3.3e-13, an MA(1) bit-identical.

The allocation seam takes the horizons (`X13PreadjustInput`
`backcast/forecast/arima`, `X13RestoreInput` `backcast/forecast`):
`a1a`/`a1b` on the forecast/backcast windows (pinned backcasts + v27 /
forecasts + v27, backTransform'ed), the extended `out` calendar with the
zero-ended outlier-column fills over n + nb + nf rows (reg-column probe:
LS −1 backward incl. back window, SO backward periodic, TC rate^k forward
with the 1e-15 write cutoff), the shifted first period into the X-11 pass,
the split finals `d11a/.../d18b` as fitToDomain slices over the a1a/a1b
domains. The horizon gate left the automatic driver's fail-closed matrix;
the fixed fit keeps `unsupported:x13 fit requires zero horizons` (extended
design not representable at that seam — recorded in the fit contract). A
driver-side fix landed with the route: the estimation block layout is
[ar, ma, sar, sma] and the AR blocks sign-flip onto the JVM surface (the
first end-to-end AR-bearing confirmation, a1 bit-exact in ao/multi).

**Capture.** Four horizon cases over the zero-horizon oracle series
(quick_h6, ao_b3, multi_b3h6, so_h6; horizons {0,6},{3,0},{3,6},{0,6}),
`validation/reference/X13AutomaticHorizonProbe.java` +
`capture-x13-automatic-horizon.R`, two independent JVM processes
byte-identical (repro gate 3/3), facts identical to the session probes.
`validation/evidence/native/x13-horizon/` (5 files, SHA256SUMS verified).

**Tiers (measured, `test_x13_automatic_horizon` 6459 checks 0 failures +
`test_arima_forecasts`).** Decisions and `a1` (all four cases) and
`d18`/`d18a`/`d18b` BIT; `a1a`/`a1b`/`out` ≤1.1e-6, split finals ~1e-7,
d13-class 2e-4, c17 7e-4, e2-at-outliers 1.7e-2. The floor is NOT the
route: the zero-horizon control proves the oracle's landing-coefficient
basin drift (measured 1.2e-4, banded family since H.2) is what the route
inherits and amplifies once into the extended tables; the route's own
trajectory matches the JVM filter state ulp-for-ulp at initialize and
≤3.3e-13 at the pinned forecasts.

**Gates (this tree, all fresh builds).** OpenMP lane 64/64 (412 s),
serial lane 64/64 (443 s) — both with the two new tests; ASan/UBSan
unit + oracle subset (quick+seas, 698 checks) + horizon quick_h6
(1616 checks) clean; full automatic sweep 71401 checks 0 bad, 13/13
(decisions untouched by the slice); nine sweeps unchanged (differencing
39, transformation 11, estimation-control 27, initial-ARMA 4368,
arma-refinement 423, outlier-scoring 98, regression-tests 494,
final-information 266); evidence SHA256SUMS verifies. `check-manifest.py`
and `verify-migration.py` 406 after staging. The two untracked files
(`src/kalos_write_probe.cpp`, `cpp-out.txt`) remain user-pending, never
`git add`.

## TRAMO/SEATS open slice: roadmap for items 23/24 with live probe evidence — 2026-11-21

Planning + pinning slice (docs/probes only, no `src/` changes): the item-23/24
decomposition lands as `docs/tramo-seats-roadmap.md`, grounded in LIVE probes of
the pinned rjd3 3.9.0 TRAMO/SEATS stack (the `jdplus-tramoseats-base-{api,core,
r,protobuf}-3.9.0.jar` family enters the probe closure for the first time; core
sha256 f919fda9f88dcae01bc122f049889ba1ac37747cc409a1f9fdf793ac4bb09db9, full
hashes in the pinning notes). Session record: `tmp_opencode/kalos-m5-pinning.md`
(new SIBLING file, sections 1–6 — M5 numbering restarts there, the 21-H notes
stay frozen). Live harness `tmp_opencode/m5probe/` (plain `java -cp`, no rJava,
G2 intrinsic recipe; seven artifacts — reach, specs, tramo tr3/trfull, seats,
pipeline rsa3/rsafull — double-captured BYTE-IDENTICAL across two independent
OpenJDK 21 processes, hashes in the notes §1); `javap -p -c` dumps, 63 files /
42,926 lines under `tmp_opencode/jdprobe/m5/` (`dump.sh` regenerable); an
`-Xlog:class+load` liveness matrix (120 classes on rsa3 / 125 on rsafull, the
facts re-emitted byte-identical to the double-capture).

**Findings that bind the roadmap.** (i) The live `TramoKernel.process` stage
order — seasonality test → log-level test → round-0 {outliers → estimation} →
round-1 {differencing → ARMA → outliers → estimation} — differs from the X-13
`RegArimaKernel` order: the native X-13 automatic driver is NOT a reusable
TRAMO skeleton. (ii) The TR template expansion is probe-materialized
(`TramoSpec` statics TR0–TR5/TRfull with a live knob matrix;
`TramoSeatsSpec.RSA0=DEFAULT`, `RSA1..RSA5/RSAfull` wrap TR1..TR5/TRfull +
`DecompositionSpec.DEFAULT`; the R glue renames rsa→tr before `fromString`) and
the TRAMO `AutoModelSpec` scalars (cancel 0.05, pcr 0.95, ub1 0.97, ub2 0.91)
provably differ from the X-13 decoder set (0.1/1.0309…/1.1363…) — the binding
layers need a SEPARATE TRAMO template table. (iii) `TramoSelection` and
`TramoSelection$AuxiliaryModule` DO NOT EXIST in the pinned jars
(ClassNotFoundException) — any plan naming them is void; the live selection
machinery is the `tramo.internal` trio (`ArmaModule` +
`ArmaModelSelector.FastBIC`, `DifferencingModule`, `OutliersDetectionModule`).
The selector's `HannanRissanen` call site is in the bytecode but never executed
on the clean probe series (HR not class-loaded — the 23-D corpus must force the
search). `TramoModelBuilder` is the live builder (class-load confirmed;
`DefaultModelBuilder` never loads). (iv) The SEATS default path resolves the
Wiener–Kolmogorov estimator — Kalman/Matrix estimators NOT loaded (register
D1); the XL=0.95 parameter cutoff is live at the boundary (fixed-airline probe
lands ±0.95 with `parametersCutOff=true` — a ready 24-A fixture); the
`TramoSeatsKernel.of(RegSarimaModel)` bridge is fully decoded (interpolated
series + log + mean-correction + variance + fitted orders re-pinned as Fixed
parameters), so 24-A consumes SEATS inputs without running TRAMO. (v) The
rsa3→rsafull class-load diff on the probe data is exactly the FTEST
trading-days stage (rsa3's `DefaultRegressionTest` is replaced by
`AutomaticFRegressionTest` + `TradingDaysController`) —
which makes the 21-I dependency precise: 21-I BLOCKS only the TRfull/23-E
calendar branch and the rsafull leg of 24-E, NOT 23-A…23-D/23-F or 24-A…24-D.

**Roadmap shape.** Reuse/fork table against the existing units (estimation
spine reuse-as-is — TRAMO's `ModelEstimator` builds the same pinned-LM
`RegSarimaComputer`; `final_estimator`/`final_information` fork-candidates
pending 23-F decode; `initial_arma_identification`/`arma_refinement`/
`regression_variable_tests` NOT applicable — different machines;
`residual_information`/`arma_information` (tramo-information contract) already
the TRAMO covariance convention); slices 23-A
(`tramo_model_builder`) … 23-G (end-to-end TRAMO oracle) and 24-A
(validator/cutoff) … 24-E (end-to-end, rsa3 AND rsafull per item acceptance),
order 23-A → 24-A → 24-B → 23-B → 23-C → 23-D → 24-C → 23-E → 23-F → 24-D →
23-G → 24-E; a cold-resume-ready spec for 23-A (oracle entries
`TramoSpec.fromString`/statics and `new TramoModelBuilder(spec, ctx).build(ts,
log)`, ~18–22-case corpus, double-capture fixture plan, full gate list); and a
maintainer decision register D1–D11 (WK-only envelope, rsa3-first coverage,
outlier iteration semantics, faithful FastBIC port, TD deferral to a joint 21-I
surface, monthly-only first envelope, etc.).

**Gates.** Docs-only slice: no build, tests, or manifest change (ctest stays
64; `check-manifest.py`/`verify-migration.py` untouched, no source staged).
The slice's own evidence gate — probe double-capture 7/7 byte-identical —
passed; tree changes are this entry plus `docs/tramo-seats-roadmap.md`.

## TRAMO model-builder decision mirror: slice 23-A lands `kalos::tramo_model_builder` — 2026-11-21

The first item-23 slice is delivered: the LIVE pinned
`TramoModelBuilder.build(TsData, ProcessingLog)` (the package-private
builder `TramoKernel` drives — `DefaultModelBuilder`/`TramoSelection` are
dead on the live path) is mirrored decision-for-decision by
`kalos::build_tramo_model` + `kalos::tramo_template`
(`src/tramo_model_builder.cpp`), qualified against a double-captured
22-case JVM corpus. The oracle path is the public entry point the roadmap
pinned (`TramoSpec.fromString`/`TR*` statics plus
`new TramoModelBuilder(spec, new ModellingContext()).build(ts, log)`,
reached reflectively for the package-private class — no instrumentation),
run under the G2 intrinsic-disable recipe on OpenJDK 21 with a live
`Math.exp != StrictMath.exp` guard; inputs are StrictMath series carried
as raw IEEE-754 bits, every probe value is a raw getter value, `%.17g`
(`validation/reference/TramoModelBuilderProbe.java` +
`capture-tramo-model-builder.R`, both byte-matched to the `metadata.R`
pins at capture time). `--repro` re-capture: all four data files
byte-identical across independent JVM processes; the corpus is locked in
`validation/evidence/native/tramo-model-builder/` (`SHA256SUMS.csv`
verifies 5/5).

**Decision approvals (maintainer sign-off, slice time).** The register
positions are approved as recommended: D2 (rsa3-first coverage order), D5
(pinned-LM at the TRAMO call sites, 21-D.2 precedent), D9 (TRAMO-side
uncertainty reuses the `tramo_residual` modes), D10 (calendar/Easter/user
detection deferred to the joint 21-I surface — no second TD
implementation), and D11 applies directly here: monthly frequency only at
the unit boundary, the annual/quarterly corpus rows are JVM-qualified
RECORD cases the unit refuses. D1/D3/D4/D6/D7/D8 stand as recorded for
their owning slices; D3 and D6 remain open.

**What the unit pins.** Template table `tr0..tr5,trfull` + uppercase
(the `fromString` spellings; `Trfull`, `trFull` and every `rsa*` refused);
start rule (autoModel → undefined airline + the post-`initializeVariables`
`setMean(true)` overwrite, else `setArimaSpec(withPeriod(12))`, declared
periods outside {0,12} refused); the `initializeMean` fixed-coefficient
quirk (a `const` TrendConstant with `regeffect=Undefined`, mean OFF); the
DERIVED `RegressionSpec.isUsed` gate (the `mean_off` row: TR1 +
`MeanSpec.none()` registers nothing); log iff function==Log with
`setPreadjustment(adjust)` always; the `AverageInterpolator` plan
(interior `(left + right) / 2.0` on outer neighbours, leading run
forward-filled — refuting the M5-open "leading NaNs stay NaN" decode note
— trailing run left-filled, ascending missing list, all-missing refused);
outlier factories with the 22c `ComponentType` regeffect vocabulary (AO/TC
→ Irregular, LS → Trend, SO → Seasonal; `withCore` keeps request
name/attributes, factory regeffect only when absent; unknown codes skipped
in silence); users → the pinned `"<name>: not found (removed from the
model)"` warning; the six-pass `sortVariables` order. The key decode
finding for 23-B/23-E: **all seven templates register NO calendar
variables** (TR2/TR4/TR5 tested; TRfull is automatic-FTEST + tested
IncludeEaster), so every template build carries zero variables.

**Fail-closed matrix** (`std::invalid_argument`, unit-asserted):
non-monthly frequency, empty/all-NaN/infinite series, bad months, declared
SARIMA periods ≠ 0/12, resolved user variables (21-I), stock/holidays/
user-trading-day calendars, TD types other than TD2, registered TD blocks
without the LeapYear LoP rule, julian/include Easter, outlier positions
outside the month domain. Registered TD with `LoP=None` (which the JVM
would build with a null `lp` core) is refused with the quirk documented.

**Gates (this tree, all fresh builds).** `test_tramo_model_builder`
(22 corpus cases — request echo + facts + events byte-identical — plus
the template-name, rejection and interpolation-plan matrices): OpenMP lane
65/65 (468 s), serial lane 65/65 (428 s). ASan/UBSan build of unit +
sweep: clean. `tools/verify_tramo_model_builder.cpp` re-drives the raw
csv corpus: 22/22 PASS (the two non-monthly rows refused as recorded).
Two JVM numeric identities the replay reproduces: Java `%.17g` renders
the SHORTEST round-tripping decimal padded to 17 digits (0.7 →
`0.70000000000000000`, not C's `%#.17g` expansion), and
`Parameter.isFree()` is `type != Fixed`. Existing suite unchanged: nine
sweeps byte-identical (differencing 39, transformation 11,
estimation-control 27, initial-ARMA 4368, arma-refinement 423,
outlier-scoring 98, regression-tests 494, final-information 266, x11-bpass
18/18), full automatic sweep 71401 checks 0 bad 13/13;
`check-manifest.py` 144/112/201; `verify-migration.py` 406. Docs:
`docs/contracts/tramo-model-builder-contract.md` (with the landing-delta
section), its README index entry, and the roadmap 23-A ledger line.

## SEATS admissibility and decomposition mirror: slice 24-A lands `kalos::validate_tramo_seats_model` — 2026-11-21

The first item-24 slice is delivered: the LIVE pinned path between the
`SeatsKernel.validate` and `SeatsKernel.decomposeModel` seams —
`DefaultModelValidator` (simplifyModel trim with cutOff, the xl<1
`stabilizeMA` XL projection versus the xl≥1 `fixMaUnitRoots` unit-circle
projection with its degree-1 **−1-both-ways** quirk), the `SeatsToolkit.of`
selector chain (`TrendCycleSelector`/`SeasonalSelector` wired from the
DecompositionSpec knobs, the greedy D1 division on repeated unit roots), the
`RootDecomposer` split linear system (`LULinearSystemSolver2` eps 1e-13,
bit-exact symmetric filters, `checkSpecialCases`) and the
`UcarimaModel.setVarianceMax(-1, noisy)` minima-sum decomposability decision
(`Spectrum$Minimizer` by-roots/by-grid) — is mirrored decision-for-decision
by `kalos::validate_tramo_seats_model` (`src/tramo_seats_model_validator.cpp`),
qualified against a double-captured 35-case JVM corpus
(`validation/reference/TramoSeatsModelProbe.java` +
`capture-tramo-seats-model.R`, G2 recipe, OpenJDK 21; the landing `--repro`
re-capture from an independent JVM process reported **4 data files
byte-identical**; `SHA256SUMS.csv` verifies 5/5 in
`validation/evidence/native/tramo-seats-model/`).

Every golden-asserted numeric — validator parameters, the raw
(pre-variance-allocation) `sar`/`ur`/`sfd` component coefficients — is
**IEEE-754 bit-exact** on all 30 replayed cases (the `%.17g` capture
re-parses exactly through `strtod`; comparison is on raw bits); no measured
band was needed at landing. The kernel flags and the `ProcessingLog` frame
are exact, **exact-prefix** for the 13 `legacy_approx` rows: what happens
after a null decomposition (`DefaultModelApproximator`) stays register D6 —
OPEN, un-implemented; the corpus records the live continuation, incl. the two
approximator `MatrixException` crashes (`v_fixma_pos/neg`, the degree-1 quirk
downstream) and the Noisy-mode NPE (`d_nondec_noisy`), as data only.

Three transcription-bug root-causes were re-qualified with JVM raw-bit
probes (full writeup in `tmp_opencode/kalos-m5-pinning.md` §8):
`SymmetricFilter.convolutionOf` receives the **BackFilter coefficients
reversed** (`weights()` is `k → get(−k)`; bit-visible on non-palindromic
inputs, it explained the `d_ar1_slow` lag7/lag10 1-ulp family);
`AbstractArimaModel.symmetricMa()` scales by the SarimaModel's **own**
SarimaSpec-default variance 1.0 — `SeatsModelSpec.innovationVariance` never
reaches the decomposition (`d_wn` var=2 → `r c2 sfd [1.0]`);
`Polynomial.fromComplexRoots` is the faithful scaled ∏ (the leading copy is
load-bearing, the inner loop starts at index i) — it pins the
`v_fixma_q2proj` root-projection reconstruction bit for bit.

Fail-closed matrix (`std::invalid_argument`, unit-asserted): period/
frequency ∉ {4, 12}, non-empty `bphi`, AR degree > 2, MA degree > 2,
negative d/bd, non-finite inputs, and any live path entering `roots()` of
degree > 2 — five corpus rows are refusal-asserted record cases
(`d_d2`/`q_airline`/`q_d2` degree-4 and `v_cut_bq2` degree-10 minimizer
numerators, `v_simplify` stationary-`bphi`; `d_d2` shows the QR-cascade class
is reachable MONTHLY). The `FastEigenValuesSolver.qr` cascade is not ported.

**Gates (this tree, all fresh builds).** `test_tramo_seats_model` (35 cases,
860 checks: 30 bit-exact replays incl. kevt frames and the decision matrix,
5 refusal assertions): OpenMP lane 66/66 (442 s), serial lane 66/66
(1152 s). ASan/UBSan builds of unit and sweep: clean.
`tools/verify_tramo_seats_model.cpp` re-drives the raw csv corpus (gating
the evidence layer itself): 35/35 PASS, 896 checks incl. the determinism
double-run and six envelope-refusal probes. Existing suite unchanged: nine
sweeps byte-identical (differencing 39, transformation 11,
estimation-control 27, initial-ARMA 4368, arma-refinement 423,
outlier-scoring 98, regression-tests 494, final-information 266, x11-bpass
18/18; x11 cpass 18/18 and dpass 22/22 also re-run), full automatic sweep
71401 checks 0 bad (13/13), tramo-builder sweep 22/22;
`check-manifest.py` 146/112/207 (+2 core, +6 evidence);
`verify-migration.py` 406. Docs:
`docs/contracts/tramo-seats-model-contract.md` (pinned-semantics table,
mode observations, landing deltas), its README index entry, and the roadmap
24-A ledger section. D6 stays open; spectral factorization and the variance
allocation are 24-B scope, estimation uncertainty 24-C.

## QR cascade port: slice 24-A.1 retires the five degree > 2 refusals — 2026-11-21

The 24-A fail-closed envelope is retired at its root: `roots()` of degree ≥ 3
now runs the faithful `FastEigenValuesSolver.qr` cascade — Householder
Hessenberg reduction through the JVM's pure-Java GEMM loops, the Wilkinson /
exceptional shift algebra, bulge + chase, `checkDeflation` at
`Constants.getEpsilon()` = 1.1102230246251565E-16, root extraction through
`eigenValues()` with Givens-rotated 2×2 blocks, the `dcmpl`/`dcmpg`
comparison flavors and the `!(|s| > eps)` simplification test verbatim.
Convergence profiles (start/stop/zero markers, per-block iteration counts,
chase counts) were bit-matched against reflected JVM solver-state dumps for
EVERY corpus polynomial entering the cascade (degrees 4/10/12). All five
refusal-asserted rows (`d_d2`, `q_airline`, `q_d2`, `v_cut_bq2`, `v_simplify`)
now replay as **positive** assertions of their captured facts; the unit
boundary refuses only period/frequency ∉ {4, 12}, negative d/bd and
non-finite inputs.

Two cascade ends end in JVM exceptions that `DefaultModelDecomposer`'s
`catch (Exception)` turns into null results — it_max (30n) exhaustion, whose
null `roots` array makes the consumer NPE, and the 2×2-tail `chase()` that
indexes `B[-1]`. Both are mirrored as internal aborts caught at the decompose
seam and reproduce captured decisions (exhaustion fires measurably on
`q_airline`/`q_d2`; JVM and unit agree on the full solver profile
stop 2 / itc 120 / chase 120). They are decisions, not refusals. The
`it_count % 25` bulge branch seeds its shift from `new Random()`: verified
across three fresh JVM processes that its root bits are NOT bit-reproducible
while every downstream captured decision is, so the unit substitutes a
deterministic shift — fires on `v_simplify`'s degree-12 stationary AR in both
engines, all 35 cases replay bit-exactly, and the csv corpus needed no
re-capture (landing `--repro` re-run: 4 data files byte-identical again,
SHA256SUMS 5/5).

Two further JVM quirks/bugs came out of the decode and are transcribed
verbatim: `SarimaModel.seasonalAR()/seasonalMA()` bound the STRIDE loop index
by the parameter COUNT (`for (i = s, j = 0; i < arr.length; i += s, j++)`), so
bp/bq ≥ 2 silently drops the seasonal component to 1 — `v_cut_bq2`'s
whole-MA-to-noise decomposition is exactly that path; and the 24-A sketch had
`SymmetricFrequencyResponse` transposed: `UpperTriangularMatrix.Ux` computes
`x' = Uᵀ x` (accumulate DOWN into powers ≤ k), which only bites where the
transform is applied — `v_simplify`'s `minimizeByRoots` component minimum,
bit-pinned by a raw spnum/spden → `Spectrum$Minimizer` oracle probe.

**Gates (this tree, all fresh builds).** `test_tramo_seats_model` (35 cases,
**983 checks, 0 failures**, all positive incl. the flipped five): OpenMP lane
66/66 (420 s), serial lane 66/66 (397 s). ASan/UBSan builds of unit and sweep:
clean. `tools/verify_tramo_seats_model.cpp`: 35/35 PASS, **1026 checks** incl.
the determinism double-run and eight envelope probes (five fail-closed + the
three 24-A.1 accepts: stationary bphi, degree-3 phi, degree-3 btheta); the
tool itself joins the tracked tree in this commit. Existing suite unchanged:
nine sweeps byte-identical, full automatic sweep 71401 checks 0 bad (13/13),
tramo-builder sweep 22/22; `check-manifest.py` 146/112/207 (unchanged —
`tools/` is outside the manifest scope); `verify-migration.py` 406. Docs:
contract pinned-semantics section (+ QR-cascade table, seasonal loop-bound
quirk, refusal list, 24-A.1 deltas), roadmap 24-A ledger extended. D6 stays
open; spectral factorization and the variance allocation remain 24-B scope.

## SEATS decomposition seam: slice 24-B lands `kalos::decompose_ucarima` — 2026-11-21

The seam between 24-A admissibility and SEATS filter construction —
`DefaultModelDecomposer.decompose` (selector chain), `UcarimaModel.
setVarianceMax(-1, false)` and `SeatsModel.compactUcarimaModel(false, false)`
— is mirrored decision-for-decision and BIT-FOR-BIT by
`kalos::decompose_ucarima` (`src/decompose_ucarima.cpp`) against the
double-captured 49-case corpus (`DecomposeUcarimaProbe.java` +
`capture-decompose-ucarima.R`; csv files byte-identical across JVM
processes, SHA256SUMS verify). The 24-A engine moved verbatim to
`include/kalos/detail/ucarima_engine.hpp` so the seam composes on the
identical validated code; the validator TU is now the thin 24-A entry.

The numerical core is the one-sided spectral factorization chain behind
`ArimaModel.getMa()/getIV()`: `SymmetricFilter.factorize` = degree-0
shortcut → `EigenValuesDecomposer2` → `EigenValuesDecomposer` → null. A
LIVE JVM chain probe over all 150 distinct corpus symmetric filters (124
EV2 / 5 EV1 / 1 fast / 20 none) fixed the tier scope: EV2 carries the
`UnitRootsSolver` + `IVector.sqrt` + exact-LS-divide spine, the z = x + 1/x
companion, `balance`/`hessenbergQR` (maxIter 120) and the real-pair /
unit-circle classification with faithful AIOOBE bounds traps; EV1 (live for
`v_simplify`/`d_d2_decomp` r c1) keeps the FULL-degree companion with the
coefficients in `column(degree-1)` and the `|z| > 1` root filter; the fast
tier's only corpus filter belongs to a validator-refused case, so
EV2 → EV1 → null reproduces every `ma/var/sfd` line (incl. the JVM's own
null-factorization `maerr`/`varr` failures). `setVarianceMax` applies the
`Spectrum$Minimizer` with the pinned dispatch `denom.length() = 2*degree+1
<= 6` and the `min == 0` / `m < 0` (dcmpl/dcmpg) branches; `fmt_g`
reproduces Java `%.17g` (shortest round-trip digits PADDED to 17 — plain C
differs). Fact scope: the d/cmp/r blocks + the SeatsException status row;
JVM abort classes are captured as data (dangling-prefix `maerr`/`varr`/
`sfdr` lines, `d.decompose null`); D6 CLOSED APPROVED: the 16
approximation_required and 1 Noisy requests refuse fail-closed with pinned
reasons, the crashing `DefaultModelApproximator` is NOT ported. The WK-only
downstream caveat (D1) stands: 24-C wires the seam into the pipeline.

Nine transcription bugs were root-caused to the bytecode on the way to
bit-exactness (full writeup `tmp_opencode/kalos-m5-pinning.md` §10): the
`hessenbergQR` left-update s2 slots (v2/w2, not hv/hw), `jm = min(k, i+3)`,
the `m == 0` carry-over slots, `ComplexMath.sqrt`'s re<=0 cart branch
(re = `0.5*im/t`), the Java-`%.17g` padding, the EV1 chain tier and its
`column(degree-1)` placement, the `2*degree+1` minimizer dispatch (the
engine compared the stored half-count to 6 — quarterly seasonal components
take the GRID tier, the by-roots quartic exhaustion on `q_airline`/
`q_decomp2` was self-inflicted), `jvm_decompose_abort` not deriving from
`std::exception` (escaped the driver's catch → terminate; the JVM try
around chain + setVarianceMax makes it `d.decompose null`), and the
`balance` shrink loop's `dcmpl ifle` NaN exit.

**Gates (this tree, all fresh builds).** `test_decompose_ucarima` (49
cases, **1669 checks, 0 failures**: 32 byte-identical replays +
determinism double-run, 17 exact-reason refusals): OpenMP lane 67/67
(426 s), serial lane 67/67 (463 s). ASan/UBSan builds of unit, sweep and
the 49-case smoke: clean. `tools/verify_decompose_ucarima.cpp` replays the
csv corpus DIRECTLY: 49 cases, 1620 fact lines exact + 17 exact-reason
refusals + 32 grammar audits, **81 checks 0 failures**. Existing suite
invariant: validator sweep 35/35 (1026 checks), tramo-builder 22/22, nine
sweeps byte-identical (differencing 39, transformation 11,
estimation-control 27, initial-ARMA 4368, arma-refinement 423,
outlier-scoring 98, regression-tests 494, final-information 266; x11 bpass
18/18, cpass 18/18, dpass 22/22), x13 allocation 280 and fit 528, full
automatic sweep 71401 checks 0 bad (13/13); `check-manifest.py`
149/112/213 (the 24-B probe commit had left the six corpus files out of
[evidence] — fixed here); `verify-migration.py` 406. Docs: new
`docs/contracts/decompose-ucarima-contract.md`, contracts README index,
roadmap 24-B ledger + D6 row CLOSED APPROVED, stale 24-A index note (the
five QR-cascade refusal rows) corrected.

## CI-FDLIBM: pinned-JVM transcendental routing lands — the machine-independence fix for run #17's two red tests — 2026-11-21

**Root cause (confirmed, `tmp_opencode/kalos-ci2-fix/VERDICT.md`).** The
automatic pipeline ran host-libm transcendentals where the JVM runs pinned
bytecode semantics; glibc-IFUNC/macOS libm variation moved real bits on the
estimation trajectory (±1ulp tanh/atanh noise alone reproduces the CI
signature exactly: 8/36 landing at 0.0622, same four cases; exp/log noise
flips the legacy odm p/q cascade). Compiler and threading ruled out.

**Dispatch verdicts (javap dumps of OpenJDK 21.0.11+10-1-24.04.2 in
`tmp_opencode/ci-fdlibm/dumps/`, "dump not trust").** Math.tanh dispatches
to the JDK 9+ expm1-based rewrite (NOT e_tanh.c); expm1/log1p/tanh have no
HotSpot intrinsic (Math == StrictMath bits). Math.cos/sin dispatch to the
LIVE C2 dcos/dsin stubs on this CPU — IBM-APML lineage, which glibc and the
macOS libm both derive from (stub-vs-bytecode: 337/14753 cos, 399/9507 sin
of the corpus-reached args differ; stub-vs-glibc: 19/16) — so host-libm
cos/sin ARE the corpora lineage and stay. Math.pow is the _dpow intrinsic
onto the host libm (Math.pow != StrictMath.pow at 6.4% of integral pairs);
the recorded domain is one pair, bit-identical everywhere. No atanh exists
in the JDK (reflection-verified): e_atanh.c transcribed as the anchor with
an explicit no-JVM-bit-claim (audited <=1ulp + specials + tanh round-trip).
An earlier header claim of "no intrinsic hits" was falsified and rewritten.

**Kernels (`src/fdlibm.cpp` +470 lines, header provenance rewritten).**
fdlibm_expm1/tanh/log1p/atanh/cos/sin transcribed operation-for-operation,
plus the complete mid-range Cody-Waite `$RemPio2` incl. the staged i>16 and
i>49 fixups (recorded sin args reach 348 -> n<=222); Payne-Hanek is
fail-closed unreachable by contract (`hi_word > 0x413921FB` throws).
Golden-proved BIT-EXACT against double-captured StrictMath goldens over
dense grids + special values + every corpus-reached argument recorded
through the LD_PRELOAD argument shim (record mode): expm1 4.40M, log1p
8.50M, tanh 4.52M, cos 10.02M, sin 10.01M, re-proved exp 6.00M, log 6.00M —
0 mismatches everywhere. glibc differs on the same sets (exp 9.6%, log1p
5.9%, cos/sin ~3.3%) — the routing moves real bits. ASan+UBSan clean over
3.3M kernel evaluations. Evidence + reproducers + SHA256SUMS:
`tmp_opencode/ci-fdlibm/goldens/`.

**Routing (30 call sites, 10 files):** tanh/atanh reflection-coordinate
maps (arma_optimizer, arma_parameters, estimation_control), decided-log /
selection-gate exp / likelihood log (x13_automatic), back-transform
(transforms), AICc value log (transformation_decision), scale/damping logs
(arma_information), scoring kernels + extreme constants (outlier_scoring).
LEDGER (raw host libm, recorded reason at each site): cos/sin
(ucarima_engine, decompose_ucarima — stub lineage == corpora); pow
(outlier_scoring — _dpow -> host libm); the three results-builder exp/log
copies + allocation staging log (x13_allocation, x13_fit, x13_automatic —
their fixture corpora were captured WITHOUT the DisableIntrinsic recipe, so
their bit claims froze STUB bits: routing fdlibm_exp there broke 50 q_log
boundary checks at the pinned 1e-15 class; capture-recipe caveat now
recorded in the pipeline contract); expm1/log1p (no double production
sites); long-double axis (item 25+ ledger).

**Landing solver (maintainer-approved ruling in the pipeline contract):**
adapter row 1 (and, with its delta retired, row 4 via the same
`base_request`) moved from Bfgs to `PinnedLevenbergMarquardt` — with the
transcendentals pinned, Bfgs deterministically lands the four knife-edge
cases in the wrong basin (the run-#17 0.0622 signature) while the
transcribed JVM LM machine lands 15/36 bit-exact, worst-rel 5.75e-4,
landing-stats 1.7e-9, tables 2708/3783 exact. The "never implicit" half of
maintainer-ruling #2 stands (solver explicit at every builder); the
"product default" half is superseded on measured evidence. Band
re-measurement, one family: diff-search tmean 1e-3 -> 5e-3 (measured 3.1e-3
at multi/diff0 on the pinned trajectory; every decision family stays
bit-exact; no band widened to absorb machine variation — the variation the
old pins encoded WAS the machine variation).

**Gates (this tree, fresh builds).** Oracle `test_x13_automatic_oracle`
**4637 checks, 0 failures** (both CI-red families green: landing 0 bad,
tmean 0 bad); legacy `test_x13_automatic` **140/140**. Full sweep
`verify_x13_automatic` 71401 checks **0 bad** (13/13; odm-scan 13 stays the
info-family), allocation 280 (boundary 1.9e-16), fit 528 (4.5e-16); ALL
seventeen verify tools 0 failures (arma-refinement 423, differencing 39,
estimation-control 27, initial-ARMA 4368, outlier-scoring 98, final-info
266, transformation 11, decomposition 81, regression-class tools). OpenMP
ctest **67/67** (324 s), serial lane **67/67** (455 s). ASan/UBSan: kernel
harness + allocation/fit/decompose unit tests + oracle + full automatic
clean. Machine-independence (LD_PRELOAD ±1ulp noise shim on the FINAL
binary; strict byte-identity is not the criterion — host-libm noise MAY
move band-tier values at the ledger sites): all four regimes
(shim-thatan, shim-explog, shim-all, `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2_FMA`)
leave **0 failures, identical bad-counts, identical worst-rel and every
decision family bit-identical**; the only movement in any regime is the
exact-match count of the band-tier tables family (2708 -> 2661..2707 exact
of 3783) at exactly the stub-lineage ledger sites — the designed behaviour
(the run-#17 selection flips are gone: the legacy suite under the all-shim
is **140/140**). `check-manifest.py` 149/112/213; `verify-migration.py` 406.

## TRAMO pre-stage decisions: slice 23-B lands `decide_tramo_transform` + round-0 `scan_tramo_outliers` — 2026-11-22

**Scope (D3 stop gate respected).** The 23-B first half lands as two
decision units over the pinned `OutliersDetectionModule`/`LogLevelModule`
corpora: Unit A `kalos::decide_tramo_transform` (calcCv grid, the
TramoSeasonalityDetector machine on the pinned LjungBox/Friedman/chi2
surface, the airline-pair log/level decision; 26-case bit-exact replay,
committed separately) and Unit B `kalos::scan_tramo_outliers`, the
**round-0** observation of the spec-driven outlier scan: the `make()`
comatip gate, the interpolation/log/design pre-stage, the
OlsComputer→HannanRissanen→stabilize→warm-optimize estimateModel chain, the
FastKalmanFilter initmodel, Mad2(50), the per-factory T/coefficient tables
and `searchMax` with the `!(|maxT| < cv)` gate. The multi-round driver and
the `curva`/`reduceVa`/pcr policy stay OPEN under D3; `sc_so`/`sc_spanout`
(round-0 picks the live driver later empties) and `sc_spannone` (collapsed
scan) are registered as the D3 brief evidence.

**Bit-level gate discipline.** Java's `%.17g` renders some doubles one
digit away from the C rendering of the SAME bits — three suspected 1-ulp
drifts during pinning (OLS mean coordinate, sc_gs2 maxT, sc_multi OLS b)
were pure rendering artifacts. Every JVM microscope dump (`%.17g#bits`) and
every replay comparison now runs on IEEE-754 bit patterns. On that gate
all 22 numerically observable corpus cases are bit-exact end-to-end, plus
the three comatip Failed cases and the Unchanged shapes.

**Transcription findings (contracts `tramo-transform-decision` /
`tramo-outlier-scan`).** (1) `RationalBackFilter.times` cross-cancels the
outlier filter denominator against the pi numerator BEFORE multiplying —
`SimplifyingTool.simplifyExact` = `LeastSquaresDivision` (Householder2
least squares, rcond 0, exact iff `ssqErr/m < sqrt(eps)`), so the LS/SO
columns run on the least-squares quotient (float residue included) over
the plain MA; skipping the cancel costs 2 ulp (SO) to 17 ulp (`sc_excl`)
and is pinned via the new `pinned_ls_divide`. (2) `FastKalmanFilter.tx` is
an IN-PLACE left-shift with the raw-AR tail term. (3) The round-0 OLS
machine is `OlsComputer.compute` (pivoting Householder, rcond 1e-9) and
the `hr.stabilized ⇒ optimizeModel` trigger is required (the raw HR
landing is routinely off the unit circle, e.g. bth −1.01186).

**Fail-closed set.** Pre-registered SO, missings without interpolation and
a general roots-pass cancellation throw `invalid_argument`; warm
re-estimation over a non-constant design (HR stabilization + prespec
design) throws — the multi-column GLS-profiled LM objective is its own
future slice — and corpus case `sc_pexao47` pins that refusal in the
replay (golden `seam_throw`).

**Gates.** Fresh-tree ctest 69/69 in BOTH lanes (new `test_tramo_outlier_scan`,
#39); ASan/UBSan clean — the sanitizer lane also caught a real
heap-overread in the `HouseholderWithPivoting` free-column init norms
(the JVM norms rows [nfixed, m) from `nfixed * (m + 1)` stepped by `m`,
not from the diagonal), fixed at the pinned machine with all pivot
decisions corpus-invariant; nine existing sweeps re-run at 0 failures;
`check-manifest.py` and `verify-migration.py` 406 pass.

## TRAMO differencing ladder: slice 23-C lands `decide_tramo_differencing` — 2026-11-22

**Scope (round-1 ladder; battery deferred with disclosure).** Unit 23-C
`kalos::decide_tramo_differencing` transcribes the live
`tramo.internal.DifferencingModule.process` over an estimated
`RegSarimaModelling`: the `comespd`/`TramoUtility.autlar` size gate with the
airline fallback (`default model selected`, the `isAirline` Unprocessed
guard), the step0/nextstep/initstep/maincondition/cond1/finalcond ladder
(the per-step raw HannanRissanen start with the −0.1/−0.2 default fills,
the 1.02 tolerance / 1.12 out-of-range / `din = 1.005 - ub2` / `c -= 0.002`
rule set, the iter-0 `cond1`, the `mlused`-aware `finalcond`, the iter ≥ 5
exit), the warm maximum-likelihood re-estimation on the 23-B LM seam, the
`searchur` unit-root scans behind rmax/rsmax, the `FastKalmanFilter`
`computeTMean` and the `isMeanCorrection` length table. The battery stage
(`RegularUnder(+2)`/`SeasonalUnder(+2)`, `SeasonalOverDifferencingTest`,
`SpectralPeaks`) is registered as 23-C.2, gated on the noncentral-beta F-CDF
prerequisite; its landing facts are pinned inert in the golden (12 BAT
stage lines + 3 FULL cases). The `ml = true` multi-round wiring is 23-F.

**Corpus and gate discipline.** 25 double-captured (byte-identical) cases
through the live module (`validation/evidence/native/tramo-differencing/`,
11 evidence files, load witnesses for the module and battery classes); the
replay covers 10 DIFF + 12 BAT cases, 120 module fact lines compared as
IEEE-754 bit patterns (`token_equal` reparse; Java `%.17g` pads where C
expands, so string equality would falsely fail on `0.97`-type literals).

**Transcription findings (contract `tramo-differencing-decision`).**
(1) `RegArimaUtility.differencingFilter` hands the BackFilter the REVERSED
polynomial while `mlj::apply_backfilter` reads ascending — the JVM
correlation is the mlj correlation times `(-1)^(d + bd)`; without the parity
pin five cases drift (tmean sign flips, ML landings). (2) The 1-ulp rmax
drift is rooted: `Polynomial.roots()` runs `Roots_Factory.FASTSOLVER` =
`FastEigenValuesSolver`, not a DGEEV companion; within the ladder's degree
domain (p ≤ 2, bp ≤ 1) its `factorize` dispatches only closed forms, ported
exactly (monic, the `2a`/`b*b − 4ac` quadratic, the `dcmpg` NaN branch) with
`ComplexType.abs` as the SCALED modulus, not `std::hypot`; the qr/chase
branch is unported and unreachable orders throw fail-closed. (3)
`computeTMean` runs `FastKalmanFilter.fastFilter`, a DIFFERENT machine from
the 23-B `process(x)`: warmup of `tmax = max(arLen, maLen) − 1` steps, the
sticky all-`|c4| ≤ eps` switch, the h update through the switch, NaN-h
abort, raw Durbin-Magenau tail — new `detail::pinned_fast_filter_residuals`
(the process() machine untouched), bit-identical d_air innovations 84/84.
(4) `tmean = s / sqrt((ss*n − s*s)/n)` over plain forward DataBlock folds;
the module never demeaning-mutates `y` (d_qtr_noseas panel pins it). (5) The
description log channel is a unit input: the fdlibm `log` per observation
(same application as `transform_series`), pinned by the growth_log pair.

**Fail-closed set.** Degenerate requests throw `invalid_argument` (empty /
non-finite series, period < 2, non-positive values under the log channel).
A warm-ML landing reaching the private optimize tail's `tryUrpCancelling` /
`tryUrmCancelling` gates throws `std::runtime_error` (the corpus never
reaches it); `searchur` beyond the ported closed forms throws likewise.
Pinned ladder failures (ESPDIF non-convergence, missing identification)
take the JVM catch into the airline fallback and never throw.

**Gates.** Fresh-tree ctest 70/70 in BOTH lanes (new `test_tramo_differencing`,
#38); ASan/UBSan clean (unit + the units exercising the shared deviances
machine); all existing sweeps re-run at 0 failures (outlier-scoring 98,
differencing 39, transformation 11, estimation-control 27, initial-ARMA
4368, arma-refinement 423, regression 494, final-info 266, x11 b/c/d
18/18/22, builder 22, validator 35/1026, decompose 81, allocation 280, fit
528, x13-automatic 71401); `check-manifest.py` 155/112/236;
`verify-migration.py` 406.

## The F kernel and the differencing battery: slice 23-C.2 lands `decide_tramo_differencing_battery` — 2026-11-22

**Scope (kernel first, then activation).** The noncentral-beta F-CDF
prerequisite landed as `detail::incBeta`/`incBetaCf`/`f_upper_p` in the
21-H `special_functions` unit (src/special_functions.cpp): the pinned
`SpecialFunctions` incomplete-beta with the symmetry gate, the Lentz
continued fraction (1e-300 tiny floor, the 1000-iteration
StatisticException throw surfacing as NaN, fdlibm exp/log). The golden is
`validation/reference/FcdfGolden.java` double-captured BYTE-IDENTICAL from
independent JVM processes (sha256 `ce9471e7…`): 18489 rows = a dense
grid + specials (cf 3342 rows with 1770 continue-throws, beta 11732/51,
fprob 3325/1081) plus the 90 corpus F triples of the battery capture; the
C replay in `test_special_functions` (`tests/jvm_fcdf.h`) is bit-exact on
ALL rows first run. `f_upper_p(v, k1, k2) =
incBeta(k2/2, k1/2, k2/(k2 + k1*v))` is exactly the SeasonalFTest p-value
the battery consumes.

**Battery activation.** `kalos::decide_tramo_differencing_battery`
(src/tramo_differencing_battery.cpp) turns the 23-C inert BAT stage lines
into positive assertions: the `SeasonalityController` first pass
(the `hasSeasonality` score → `ctx.originalSeasonalityTest`, the m011 +
`setBq(1)` = (0,1,1,0,0,1) swap, both seasonal-flip branches) and the
provably identity-short-circuited second pass (15/15 `seas2` Unchanged
pins: the reference model IS the live object), the four battery steps (ru1
mean/quasi-unit-root gates + direct Changed, su1 fixSeasonalRoots +
residual-ladder gate, ru2 lbp gate + the zero-mean-autocovariance sign
count, su2 `bd!=1 && bq!=1 && ost!=0` gate), the `ModelComparator` BIC port
over the builder defaults (exact 71/85 branch map, `checkAcceptable` + the
five tests), the `SpectralPeaks.hasSeasonalPeaks` length dispatch, the
ladder scores and the per-state `SeasonalOverDifferencingTest` codes — the
battery itself never calls the over-side, so the 90 captured codes are
observer force-calls the native port reproduces anyway. Estimation, the
periodogram and `ModelStatistics` stay pinned at the boundary; candidate
construction, the consumption stream (exactly 1..N-1), the Changed panel
transfers (bit-exact) and the Unchanged panel freezes are asserted per step.

**Corpus and witnesses.**
`validation/evidence/native/tramo-differencing-battery/` (22 files,
`SHA256SUMS.csv` verified): 15 cases (9 io=n replays rebuilt from the 23-C
seeded recipes + 6 forcing cases f_ru1/f_mean/f_su2/f_ru2q/f_peaks/
f_qpeaks), 1175 fact lines, 90 F triples (all non-null), 26 candidate
snapshots, 75 decision lines; double capture byte-identical (`--repro`, 18
data files); SEVEN `-Xlog:class+load` witness runs each load
`SeasonalOverDifferencingTest` and `SpectralPeaks` exactly once
(`loadwitness.csv`, f_peaks/f_qpeaks also force the monthly/quarterly peak
branch). Replay: 90 states, 90 F p-values bit-exact through the kernel, 90
ladder scores, 180 peak gates, 90 over codes, 75 decision lines with the
post-step spec/mean/seasonal/ost snapshots, 11 candidates, 9 panel
transfers, 66 frozen panels, 0 failures in both lanes.

**Transcription findings (contract battery section, pinning §12).**
(1) ru2's lbp gate is `dcmpl(lbp, 0.005) iflt` — a NaN p-value does NOT
proceed (the pre-decode note said the opposite). (2) `needProcessing`
returns `pos12 >= freq || pos12 >= 9 || total >= 17` — the sign of the
17-comparison is INVERTED relative to the note; catching it flipped
f_peaks ru2 from a phantom candidate to the pinned Unchanged, and NaN lags
COUNT as positive through `dcmpl ifle`. (3) An Unchanged battery step CAN
consume its candidate: the JVM estimates before comparing (rw0 su1/su2 pin
the shape). (4) `compare`'s 71/85 branch loads map to
`preferSecondModel(s2, s1) ? -1 : +1` / `preferSecondModel(s1, s2) ? +1 :
-1`; `checkAcceptable`'s bic guard is `dcmpl` (a NaN difference PASSES) and
`acceptableStab` is computed but unreferenced. (5) `SarimaOrders.m011` =
(0,1,1,0,0,0) → `setBq(1)`; the `isSeasonal()` guard in
computeReferenceModel is vacuous for period > 1. (6) The su1 fix-seasonal
gate is `bphi(1) < −0.91` (`dcmpl`), the SODT gate `btheta(1) < −0.7`, its
codes 0/1/2 as decoded; the 5-arg ladder matches the captured 4-arg ctlst
stage facts on all 90 states. (7) The F p-values pin the kernel beyond the
grid: corpus-reached arguments reproduce bit-exactly.

**Fail-closed set (negative probes in the unit).** Calendar-bearing su2
scan, root scans above the ported closed forms, an ru2 autocovariance
window below 12 residuals, missing/diverging candidate snapshots, a null
SeasonalFTest, the fixSeasonalRoots/buildNewModel failure paths — all
`std::runtime_error`; the io = y (ODM) cases and `ml = true` rounds stay
23-F.

**Gates.** Fresh-tree ctest 71/71 in BOTH lanes (new
`test_tramo_differencing_battery`, #39; `test_special_functions` now
carries the 18489-row fcdf golden); all sixteen existing sweeps re-run
corpus-invariant at 0 failures (outlier-scoring 98, differencing 39,
transformation 11, estimation-control 27, initial-ARMA 4368, arma-refinement
423, regression 494, final-info 266, x11 b/c/d 18/18/22, builder 22,
validator 35/1026, decompose 81, allocation 280, fit 528, automatic 71401);
shim-invariance of the two tramo differencing units: the ±1ulp libm noise
shim (modes thatan/explog/all) leaves both bit-identical to the unsuffixed
run; ASan/UBSan (fresh Debug tree): `test_tramo_differencing`,
`test_tramo_differencing_battery` and `test_special_functions` exit 0 with
empty sanitizer output and the identical battery summary;
`check-manifest.py` 157/112/256; `verify-migration.py` 406. History note:
the battery content rode in `6fb8b72` (the staged index collided with an
orchestrator workflow-guard commit); slice 23-C.2 is the pair `6fb8b72` +
this addendum (dated entry + contracts-README index update).

## TRAMO arma selection: slice 23-D lands `kalos::select_tramo_arma` — 2026-11-22

**Scope.** The live `internal.ArmaModule.process` arma selection with the
`ArmaModelSelector` FastBIC machine, monthly domain (register D11). The
unit owns the `comespa`/`maxInic` gate with the default-model and
already-airline branches, the `calcmaxspec` envelope, the three-stage
selector grid (stage-1 seasonal AR-block on the (3,0) base, stage-2 ARMA
on the preferred, stage-3 seasonal), the per-candidate HR +
`isStable(true)` + FastBIC sort, `getPreferred`, the `mergeInto` watermark
merge into the five-row panel and the eleven-gate `select()` tie-break; the
selection stage touches NO maximum-likelihood engine. The selector input is
the `olsResiduals` hand-over (request data; length `n − (d + period·bd)`),
`nobs` rides separately for the gate. Reuses `hannan_rissanen_pipeline`,
`pinned_fast_filter_residuals`, `check_roots` and the fdlibm logs
bit-for-bit; the new code is only the FastBIC panel machine
(`log(ssq/n) + k·log(n)/n`, ssq the finite-innovation forward fold).

**Corpus and witness.** `validation/evidence/native/tramo-arma-selection/`
(9 files, `SHA256SUMS.csv` verified): 16 cases (Changed/Unchanged/
Unprocessed/Failed, pcnt 3/2/0, the wn flag, seasonal=false + the WN-return
gate, noise, flat entries, log channel, quarterly refusal, constant-series
upstream failure), double capture byte-identical (`--repro`, 7 data files);
the probe lives in the `tramo.internal` split package to read the
package-private `comespa`/`maxInic`/`calcmaxspec` as the module calls them;
`-Xlog:class+load` witnesses (`load-witness.csv`, loadlogs a_base/a_srw)
load HannanRissanen 3×, `core.arima.estimation.FastKalmanFilter` 1× and
`core.arima.AutoCovarianceFunction` 3× with ZERO `RegSarimaComputer`/
`GlsSarimaComputer`/`internal.HannanRissanenInitializer`/
`LevenbergMarquardtMinimizer` loads — HR + fast conditional BIC, no ML,
decoded and witnessed.

**Replay.** `test_tramo_arma` (ctest #38): 15 live cases (14 module-line +
the D11 quarterly refusal) bit-exact — `direct_sel`, five `dpanel` rows,
`arm_result`, `after` and the full ProcessingLog event sequence with the
CInfo detail (`armaInfo best=… m0=… b0=…`); the CInfo detail's glued
`b2=<bic>` tokens are split at `=` and compared by re-parsed bit pattern
(Java `%.17g` shortest-vs-C exact renderings differ textually). The
constant-series case is record-only (its `olsResiduals` NPE upstream leaves
no hand-over); the `pcnt`/`maxspec` gate lines and `resvals` vectors ride
inert. Sweep `tools/verify_tramo_arma_selection.cpp` re-drives the raw
corpus: 14 replayed, 1 D11 refusal, 1 record-only, 0 failures; the shared
renderer `tests/tramo_arma_replay.h` is used by both.

**Transcription findings (contract, pinning §13).** (1) `checkespa`'s
second gate is the BARE autlar sign on `n − p − period·bp` — the pre-decode
note carried a phantom subtraction; the `n = 56` flat capture discriminates
(it flipped a_default from Changed to Unprocessed). (2) `calcmaxspec` and
`checkmaxspec` keep DIVERGENT seasonal order tables: `calcmaxspec` sets
bp = bq = 1 for m ∈ {1,2,3} UNCONDITIONALLY on bd, `checkmaxspec` only at
bd == 0 (the earlier shared-table note was wrong). (3) The ProcessingLog
convenience overloads `info(String, Object)`/`remark(String, Object)`
default-route as `kind(null, message, detail)` — the human text arrives in
the OPERATION argument and the Observer prints the first non-null string
(the captured "selected model:" line pins the routing). (4) The probe
needs `timeseries.Describe`-level construction in-package; the witness
class names are the `core.arima.estimation` FastKalmanFilter (NOT the
subsample-arima homonym). (5) The module's info line is "selected model: "
WITH colon (the bytecode string constant; a note dropped it).

**Fail-closed set.** Non-monthly period, empty/non-finite residuals:
`std::invalid_argument` (D11; never the X-13 21-E/F fallback). HR seam
exceptions, fast-filter aborts (the pinned ArimaException/AIOOBE mirror)
and the null/empty-panel selection follow the live RuntimeException catch
into Failed with the specification left as-is; negative probes in the unit
pin the refusal.

**Gates.** Fresh-tree ctest **72/72 in BOTH lanes** (new `test_tramo_arma`,
#38); all seventeen existing sweeps re-run corpus-invariant at 0 failures
(arma-refinement 423, decompose 81, differencing 39, estimation-control
27, final-info 266, initial-ARMA 4368, outlier-scoring 98, regression 494,
builder 22, validator 35/1026, transformation 11, x11 b/c/d 18/18/22,
allocation 280, fit 528, automatic 71401); ASan/UBSan `test_tramo_arma`
exit 0 (15 cases verified); shim-invariance of the new unit
(modes thatan/explog/all + `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2_FMA`,
unit and sweep bit-identical — the FastBIC logs are fdlibm-pinned);
`check-manifest.py` **159/112/265**; `verify-migration.py` 406; evidence
SHA256SUMS verified (8 files).

## SEATS WK extraction groundwork: slice 24-C.0 checkpoints the probe, corpus, evidence and shared seam — 2026-11-22

Checkpoint commit for slice 24-C (`kalos::seats_extract_wk`, the Wiener–
Kolmogorov extraction machine). No new unit lands yet; this commit lands the
JVM ground truth and the shared structured-model seam the extraction unit
will consume, so the byte-faithful transcription can proceed incrementally.

**Evidence (double-captured, byte-identical).** `SeatsWkExtractionProbe
.java` drives the LIVE public API (`SeatsKernel.process`) and emits the
request grammar plus `status`/`dom`/`init`/`final` fact rows (%.17g raw
getters); `capture-seats-wk-extraction.R` runs the 36-case corpus through
TWO independent JVM processes — `--repro` reports the 4 data files
byte-identical. Installed at `validation/evidence/native/seats-wk-extraction/`
(cases 36, inputs 324, facts 4828, events 310 + metadata + SHA256SUMS).
Corpus: 7 refusal/status-context rows (x_airline D6 approximation, x_stab_-
none SeatsException, x_crash_fixma MatrixException, x_quarterly D11, x_var0,
x_kalman + x_kalman_wn D1 KalmanSmoother context rows), the 24-B decomposable
mirror set, and 11 knob rows (forecast/backcast counts, meanCorrection, log,
bias None, generators C/E, n=60 with horizon −4). JVM quirks pinned for the
port:
`TsPeriod.annualPosition()` is 0-based; empty Backcast rows carry the
actual start; x_ar3/x_stat_ar3 abort through the EcoException "Negative
variance" surface; init/final row counts pin the D7 forecast/backcast
envelope (defaults 21/19, horizon-open 29/29).

**Shared seam (behavior-neutral by construction).** The structured
UCARIMA/AModel mirror (38 free functions, lines 58–1804 of
`src/decompose_ucarima.cpp`) moved verbatim into
`include/kalos/detail/seats_ucarima_model.hpp` under
`kalos::detail::seats_model` (each function prefixed `inline`); the cpp
keeps an anonymous namespace with `using namespace detail::seats_model;` so
`decompose_ucarima` is unchanged — `ctest -R decompose` passes with the
golden byte-identical.

**Request grammar (backward compatible).** `tramo_seats_model_request`
gains the 24-C fields with live-builder defaults: `forecast_count = -2`
(extrapolationCount sign convention), `backcast_count = 0`,
`components_method = burman`, `bias = legacy`, `series` + `start_year`/
`start_month` (1-based month; the probe's TsPeriod position is 0-based).
24-A/24-B ignore them; `seats_components_method::kalman_smoother` is the
D1 fail-closed refusal input (Kalman/Matrix estimator rows in the goldens
are context only). D7 stays in the first envelope (Burman + −2 default).

**Friction (environment, not code).** The harness exports
`LD_LIBRARY_PATH=/usr/lib/libreoffice/program:/usr/lib/x86_64-linux-gnu`,
which SHADOWS the RUNPATH pin to the in-tree LP64 OpenBLAS 3.3.4 and yields
spurious `undefined symbol: LAPACKE_dsyev_work` runtime failures in any
test whose dlopen order touches system libopenblas (run #18 class). Gates
must run with `env -u LD_LIBRARY_PATH`; recorded in the pinning doc.

**Gates.** ctest 72/72 both lanes; all eighteen existing sweeps
corpus-invariant at the recorded counts (arma-refinement 423, decompose 81,
differencing 39, estimation-control 27, final-info 266, initial-ARMA 4368,
outlier-scoring 98, regression 494, builder 22, validator 35/1026,
transformation 11, x11 b/c/d 18/18/22, allocation 280, fit 528, automatic
71401, tramo-arma 14/1/1); ASan/UBSan `test_decompose_ucarima` exit 0
(49 cases, 1669 checks) — the seam move is sanitizer-clean too;
`check-manifest.py` updated for the two reference files + evidence tree;
`verify-migration.py` 406.

## WK extraction linearfilters mirror: slice 24-C.1 cluster 1 lands `seats_linear_filters.hpp` — 2026-11-22

First increment of the 24-C.1 transcription (pinning doc SECTION 14 queue
item 1): the linearfilters mirror the WK extraction machine runs on, as
`include/kalos/detail/seats_linear_filters.hpp` (`kalos::detail::
seats_lfilters`), a bit-exact C++ transcription of the pinned 3.9.0 bytecode
of RationalFunction + Polynomial$SimplifyingTool, the linearfilters
hierarchy (Back/Fore/Symmetric core/Finite/RationalBack/RationalFore/
RationalFilter incl. the lazy numerator/denominator paths and the
Decomposition inner class), FilterUtility.frequencyResponse (Clenshaw
three-term recurrence, host-libm trig per the CI-FDLIBM ledger),
ComplexComputer/Complex.div/DoubleSeq.equals and the
LULinearSystemSolver2(1.0E-13) robust solver with the bytecode's own
column-major indexing. Reuses the 24-B seam primitives (Poly algebra,
QR/least-squares division, closed-form roots, rfr) untouched.

**SimplifyingTool re-adjudicated fresh from the jar** (pinning SECTION 15):
the real-root branch ends `149: goto 184`, i.e. the shared f2 divide DOES
run after the f1 divide (an earlier decode note claiming `goto 221` was
wrong); roots are taken of ARG2 (the smaller polynomial), the left/right
swap is unconditional, and when common grows past degree 0 the tool stores
`simplifiedLeft = big` UNCANCELLED with `simplifiedRight = small.divide
(common)` (least-squares quotient). The WK mirror carries its own faithful
copy of the tool; the 24-B seam's two latent divergences (roots of `big`,
stale f2 on NaN-imaginary roots) stay untouched and unfixed this slice —
the 24-B corpus never reaches them and the seam is behavior-frozen.

**Ground truth + gates.** New `tests/test_seats_linear_filters.cpp` replays
126 bit-for-bit checks against hex captured from the pinned jars via
DecProbe/RFProbe2 (probes under /tmp/opencode/ssf): decomposition system +
LU back-substitution, RationalFunction dd==0 path, 32-quantized cache with
extension, drop, plus/minus/times/divide through the tool's exact, false
and roots paths (incl. the real-root f1+stale-f2 double divide, the
left==big quirk and the degenerate LS quotient), cross-cancelling
Rational{Back,Fore}Filter.times with the leading-coefficient normalization,
lazy vs eager RationalFilter numerator/denominator (FiniteFilter vs
SymmetricFilter.convolutionOf), the filter statics, and the AIOOBE-parity
refusals. One JVM fact corrected mid-cluster from the probe: FiniteFilter.
isSymmetric compares `-(len-1)` against `lb` (a centered window like
[1,2,1]@-1 is NOT symmetric). `ctest -R "test_seats_linear_filters|
test_decompose_ucarima"` passes (ctest inventory 73); manifest now
161/112/271. Arithmetics and the SymmetricFilterAlgorithms decomposer stay
deferred to the cluster that consumes them (pinning SECTION 14).

## WK extraction acf/solver mirror: slice 24-C.1 cluster 2 lands `seats_acf_solver.hpp` — 2026-11-22

Second increment of the 24-C.1 transcription (pinning SECTION 14 queue item
2): `include/kalos/detail/seats_acf_solver.hpp` (`kalos::detail::seats_acf`)
mirrors, bit-exact, the pinned 3.9.0 bytecode of AutoCovarianceFunction
(the 36-quantized lazy acf cache, the Computer/SymmetricComputer dispatch,
the ivar scale with dcmpl-ifeq identity skip — NaN ivar multiplies, the
AR-recurrence extension whose zeros come out as **-0.0** past the first
block, get(i<0) AIOOBE parity after prepare ran), the
AutoCovarianceComputers default computer (lag-moment rhs through the
no-simplify RationalFunction.of + coefficients(p+1), symmetric Toeplitz
built with FastMatrix.add(i,|i-k|,c) with the `c != 0` dcmpl skip, solved
by the cluster-1 lu2_solve — MatrixException translates to
ArimaException("acgf of non stationary model")), the symmetric computer
(including its decompose branch), LinearProcess (the lazily built
Acf(numerator.weightsToArray, back.den x fore.den, var), one- and
two-sided) and AbstractArimaModel.getAutoCovarianceFunction on the seam's
AModel (isStationary guard with the same ArimaException).

The cluster-1 deferral **SymmetricFilterAlgorithms.decomposer was proven
necessary and landed**: WienerKolmogorovEstimators calls
SymmetricFilter.decompose directly (javap call-site inventory of the pinned
WKToolkit dump), and the AutoCovarianceComputers symmetric computer reaches
it through the 2-arg AutoCovarianceFunction ctor. Mirrored as `sf_decompose`
(length-1 scaling path with the c[0]/2 + |fb(0)| division and dcmpl skip,
general Toeplitz path with the reverse-before-ofInternal ordering, bare
MatrixException escaping). Fresh jar re-verifications while decoding:
RationalFunction.of(num, den) takes the NO-simplify ctor (iconst_0) — the
SECTION 14 memo claiming of() simplifies was wrong (the cluster-1 mirror
was already correct); LS_Factory ROBUST_FACTORY == LULinearSystemSolver2
(1.0E-13) is the solver both the acf Toeplitz systems and the cluster-1
decomposition use, so no second LU port was forked. The staged
RationalFunction.txt dump was a broken 1-line artifact and was regenerated
from the pinned jar.

**Ground truth + gates.** New probe /tmp/opencode/ssf/AcfProbe.java
(double-captured acfA.txt == acfB.txt, sha256 aac44359…08c4b, 39 lines):
sma-path white-noise-ish models incl. ivar 2/0/NaN and the -0.0 extension
zeros, computer path with the 36/37/80 quantization crossings and
interleaved get() cache evolution, p>q MA numerators, the two singular
denominators ("acgf of non stationary model"), LinearProcess with
one-sided and two-sided denominators, eight decompose cases (scaling path
incl. |−1| skip and the divide-by-zero Infinity row, Toeplitz path with a
hand-verified 2x2 system, the bare MatrixException singular row), the
2-arg-ctor symmetric computer through both branches, and the get(i<0)
AIOOBE. The replay lives in tests/test_seats_linear_filters.cpp sections
E–I: **165 checks, 0 failures** (cluster-1 126 + cluster-2 39) plain and
under ASan/UBSan; test_decompose_ucarima stays 49 cases / 1669 checks;
full-tree build clean; manifest 162/112/271. No shared seam or cluster-1
file touched. Remaining 24-C.1 queue (pinning SECTION 16): Arithmetics +
the WK estimator builder (29-row seats matrix), the bit-faithful
ExactArimaForecasts SSF cluster, Burman/DefaultBiasCorrector wiring, then
the `test_seats_wk_extraction` corpus replay + contract + full gates.

## SEATS Wiener-Kolmogorov extraction: slice 24-C.1 lands `kalos::seats_extract_wk` — 2026-11-22

The live `SeatsKernel` extraction machine — `SeatsModel` construction on the
frozen fixed-parameter spec, `WienerKolmogorovEstimator.decompose` (the
Burman/WK initial surface) and `DefaultBiasCorrector.correctBias` (the final
surface) — is mirrored operation-for-operation and BIT-FOR-BIT by
`kalos::seats_extract_wk` (`src/seats_extract_wk.cpp`) against the
double-captured 36-case corpus (`SeatsWkExtractionProbe.java`, the real
kernel end to end; csv files byte-identical across JVM processes). The unit
composes on the public 24-A/24-B surfaces: the request re-validates, the
24-B decomposition engine re-runs, then the mirror drives
`UcarimaModel.compact(2, 2)` (unconditional, short-model AIOOBE mirrored
fail-closed), `BurmanEstimates` on the transformed series, the 30-add
initial assembly and `fillLevels`/`correctLogs` + the four
fill{Fore,Back}casts passes, emitting the probe grammar (status, the three
`SeatsResults` domains, the init/final rows in the fixed enum order,
`%.17g`).

Key pinned decodings this slice added: `BurmanEstimates.initModel` reads
`ucm.getModel()` — the UCARIMA's OWN model (var 1.0 on every corpus path) —
for the mapoly/iv scaling and the LU+ExactFc series model, while
`innovationStdev` is `sqrt(SeatsModel.getInnovationVariance())`, the spec
echo; the two variances differ exactly at x_wn/x_seasonal and the bm
goldens cannot separate them. `TsData.add(a, {b,c})` folds LEFT from the
first non-null varargs index (null receiver aliases a trailing sole arg, a
null argument at j >= k dereferences null); `DoublesMath.add` skips trailing
nulls only before the fold starts. `DefaultBiasCorrector` runs its two
2-arg passes into the SAME builder after the value adds (Series-Fc fallback
= add(Se, {T, I}), saFc = subtract(S/Fc, Se/Fc); fcS null-or-empty ABORTS
the whole pass — x_fc0 has no final fc rows at all); `correctLogs` scales
exp()ed components by 1/saMean (avg over n-n%period), 1/irMean (full avg)
and saMean*irMean, corrects every stdev via `LogNormal.stdev2` and
overwrites the exp-fed Irregular/Value key with divide(SA, T). The
`CkmsFilter.process` mirror now carries the per-call
`PredictionErrorDecomposition(false)` SINK (save-before-update, the
ResidualsCumulator/DeterminantalTerm dcmpg/dcmpl throw chain; xf/bm goldens
unchanged, no corpus xf case ever trips it). Status tiers: 28 full runs,
3 JVM-status rows (x_stab_none SeatsException pre-estimation; x_ar3 and
x_stat_ar3 EcoException "Negative variance" mid-Burman, no dom/init/final),
6 pinned refusals (period 4 D11, two KalmanSmoother D1, innovation
variance 0 = the ConcentratedLikelihood path, classical airline + fixMa
crash D6 — the approximation retry loop stays unported, open question
recorded in the contract).

**Gates.** fresh-tree ctest 74/74 in BOTH lanes (new `test_seats_extract_wk`,
1208 line checks, 36/36 cases byte-exact); decompose 49/1669, validator
35/983 and the seats-linear-filters unit 8321/0 unchanged (PED sink is
value-neutral there); `tools/verify_seats_wk_extraction.cpp` sweeps the raw
csv corpus (36 cases, 6 refusals, 126 checks, 0 failures) and is
deterministic per case; all earlier tools sweeps corpus-invariant;
ASan/UBSan lane green (four SEATS units, empty sanitizer output);
shim invariance: `test_seats_extract_wk` bit-identical under the
thatan/explog modes and the no-AVX2_FMA tunable; under `all` only the
7 cases with ill-conditioned 24-B spectral-factorization filters
perturb (the EV-cascade branch flips are the established 24-B-core
behaviour — `test_decompose_ucarima` itself shows them under `all`),
while the clusters 2-6 machinery (`test_seats_linear_filters`, 8321
checks) is bit-identical under `all`; `check-manifest.py` 167/112/271; `verify-migration.py`
406. Envelope registers honored: D1 WK-only, D11 monthly-only, D6
decomposable-only (CLOSED APPROVED at 24-B), var-0 CL path and the D7
bias/auto-count defaults (Legacy bias, forecastCount -2) IN scope.

## Calendar detection groundwork: slice 21-I.0 plans `docs/calendar-roadmap.md` — 2026-11-22

The deferred 21-I surface (calendar / Easter / user-variable detection) gets
its live-probe groundwork and slice plan, closing the biggest unknown in the
23/24 blocking table. Live JVM probes of the pinned 3.9.0 stack (harness
`tmp_opencode/i21probe/`, 11 captures each double-produced in two independent
OpenJDK 21 JVMs, byte-identical; statics in `tmp_opencode/jdprobe/i21/`,
66 javap dumps) establish two headline verdicts. First, **X-13 rsa4/rsa5c
calendar detection is a pure AICcComparator battery**: `CalendarEffects
DetectionModule` (candidates [no td, td2]/[no td, td7]) and
`EasterDetectionModule` (candidates {none, easter [1], [8], [15]}) run inside
regAIC, re-firing per estimation round (3× each on a TD-effect series), while
`jdplus.toolkit.base.core.dstats.F` NEVER class-loads on those paths — the
F/Wald pvalue machines belong exclusively to the auto-method branches and to
the TRAMO fork (`TradingDaysController` + `AutomaticFRegressionTest`, loaded
exactly on TRfull runs, with raw `Info{nhp,ss0,ss1,ss6,ssmc1,ssmc6,pftd1,
pftd6,dof*}` oracles that keep TD at pftd=1.0 and drop it at
pftd1=0.8798/pftd6=0.3065 against the 0.99 gate — the kernel is the landed
23-C.2 `f_upper_p`). Second, the **generation layer is fully pinned byte for
byte**: 4128 lines of raw matrices (tdCount/meanDays, contrasts+raw for all
nine DayClusterings, LeapYear +0.75 / LengthOfPeriod days−mean, Easter
computus 1985–2015 with `easter`=`easter2`, EasterVariable durations {1,5,8,
15} × {Simple default, None}, JulianEasterVariable mixture, the NEGATIVE
Ramp staircase, UserVariables 0-fill out of domain) plus name conventions
(`td`, `lp`, `td2…td7`, `easter [8]`); the R-lane template bytes were
re-verified this session (2×Rscript, identical, equal to the jar's
`X13Spec.fromString` bytes for all six rsa templates; note `RSA2c`/`RSA5c`
have no public statics). No user-variable/holiday DETECTION module exists in
any 3.9.0 jar (user columns are pass-through), so the "user detection" half
of 21-I is a spec no-op and holidays stay refused. The roadmap lands: the two
machines and their liveness matrix, a reuse/fork table (reuse landed AICc,
`regression_likelihood` and `f_upper_p`; one new Wald `tstat` kernel only if
auto-methods enter; fork-preserving, honoring D10), a 4-slice ledger
(21-I.1 generation → 21-I.2 X-13 battery → 21-I.3 TRAMO controller → 21-I.4
integration/refusal ladder, with JVM oracle entry points and pinned-corpus
sizes), a decision register I-1…I-9 for maintainer sign-off, and the
downstream map (23-E calendar branch and the TRfull-FTEST part of 23-G
unblocked by I.1+I.3; the rsa4/rsa5c gap by I.1+I.2). Surprises recorded:
`DayClustering.toString()` throws (jar bug), TR5 + strong-TD NPEs inside the
pinned `ModelComparator` (AMI stage, recorded as I-6, not calendar), and the
rJava lane now needs an explicit `JAVA_HOME` (Java 17 default JVM otherwise).

**Gates.** Docs-only slice: no build, tests, or manifest change (ctest stays
74; `check-manifest.py`/`verify-migration.py` untouched, no source staged).
The slice's own evidence gate — 11 probe captures double-captured
byte-identical, R-lane template re-verification identical and byte-equal to
the JVM lane — passed; tree changes are this entry plus
`docs/calendar-roadmap.md`.

## Calendar design generation: `kalos::calendar_generation` lands 21-I.1 — 2026-11-22

The design-algebra layer 23-A registered variables for (deferred there per
D10) lands bit-exact: `include/kalos/calendar_generation.hpp` mirrors the
LIVE pinned generation machines `Regression.matrix` drives —
`GenericTradingDaysFactory` (tdCount, meanDays, all nine day-clustering
contrasts), `LPFactory` (None/LeapYear/LengthOfPeriod), the `Easter`
computus with `EasterVariable` (all four corrections × faithful endPosition)
and `JulianEasterFactory` (JDN conversion), `RampFactory`,
`TsVariablesFactory` placement and the pinned name/description conventions.
Per decision I-5 generation covers julian Easter and every duration/
frequency shape; the {none, 1, 8, 15} DETECTION batteries stay 21-I.2/.3 —
this unit contains no detection decision. Decision I-9 (user 0-fill, ramp
sign) pinned via the aligned-dump pairs.

**Oracle.** The primary `gen.out` (4128 lines, sha `cb8d7ac0…01857d`) plus a
NEW envelope-shape capture `gen2.out` (5904 lines, sha `075d58d6…e30f454`:
start-offset/quarterly/annual/leapless domains, correction × endPos ×
boundary-duration variants, eight user alignments, ramp clamps, the full
`probJulianEaster`/`probEaster` tables) — both double-captured byte-identical
in two OpenJDK 21 JVMs and now committed under
`validation/evidence/native/calendar-generation/` (metadata.R pins jars,
probe `I21Probe.java` sha `c66db448…ab08b2`, runner sha `aa1293d6…e60974`;
the probe and runner are committed verbatim under `validation/reference/`).
EVERY machine was first replayed bit-exact in a python reference against
both corpora (3960 + 5672 matrix rows, ALL MATCH) before C++ was written;
pinning-note corrections are recorded in the pinning file §8 and the
contract. Key verdicts: `GenericTradingDays.raw` and `.contrasts` are
BIT-IDENTICAL (the contrast flag is discarded on the variable path); the
"reversed" ramp is the ALL-ZERO column (corrects the I.0 note); LeapYear's
first-leap-year search is bounds-free arithmetic (a leapless range
completes, −0.25 cells only) and the +0.75 strikes every 4 PERIODS (century
non-leap years not skipped); LengthOfPeriod subtracts the CONSTANT 365.25/f;
the Gregorian factory SETS its month-pair shares while the Julian factory
ADDS (quarterly cells accumulate March+April), with x19 measured to the
EXCLUSIVE March end and the window `[e−dur, e−1]`; PreComputed indexes
25-entry mean tables. The probe-side `tdc` month labels are a display bug
(rows are weekday vectors — verified); jar bugs are not ported
(`DayClustering.toString` AIOOBE).

**Envelope.** Fail-closed `std::invalid_argument` per the contract table:
frequency outside {12,4,1} or unaligned month, durations outside [1, 28]
(PreComputed [1, 25]), annual TD/Easter domains (where the jar silently
leaves the column empty we REFUSE rather than emit unprobed zeros), user
data not aligned to the domain frequency, `mean_days_yearly`/
`ramp_description` beyond their captured ranges. Holiday families keep their
23-A refusals.

**Gates.** Fresh-tree ctest **75/75 in BOTH lanes** (new
`test_calendar_generation`, 137 golden cases, 18383 checks, every matrix
value compared as its captured IEEE-754 bit pattern); raw sweep
`tools/verify_calendar_generation.cpp` re-drives BOTH captures through the
public API (123 cases, 9632 row lines, 18305 checks, 0 failures, also clean
under ASan/UBSan) and is shim-invariant by construction (no JVM transcen-
dentals — only `strtod` round-trips); all nineteen earlier sweeps
corpus-invariant (0 failures each); the new unit is bit-identical under the
thatan/explog/all ±1ulp libm-noise shims and under
`GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2_FMA`; ASan/UBSan lane green for the
unit (empty sanitizer output); `check-manifest.py` 169/112/275;
`verify-migration.py` unchanged at 406.

## CI machine-independence completion: CI-FIX-2 vendors the hostlineage lineage and runs the shim-invariance battery — 2026-11-22

**Finding.** The qualification machine's libm (Ubuntu glibc 2.39-0ubuntu8.8)
is NOT the upstream glibc-2.39 generic dbl-64 C lineage its version number
claims: sin/cos resolve through x86-64 multiarch IFUNCs and exp/log/pow/
asin/acos/atan2 deviate from the pinned C sources on rare points. A
three-way harness (vendored vs original-C compiled out-of-tree vs host,
17M+ points per kernel) shows the vendored kernels are bit-identical to the
ORIGINAL glibc code (0 mismatches, all 9 kernels) while the host is the odd
one out. Dense-grid equality with an arbitrary distro libm is therefore
unattainable by construction and was never the gate: the gates are
corpus-reached-argument replay, machine-independent known-answer digests,
and golden invariance.

**Leg A — vendored hostlineage kernels.**
`include/kalos/detail/hostlineage.hpp` + `src/hostlineage{,_tables}.cpp`:
operation-for-operation transcriptions of glibc 2.39 dbl-64 sin/cos
(+ branred big-argument reduction), exp, log, pow, hypot, atan2, acos
(asin proof-only), multiarch FMA/TOINT branches compiled out (the generic-C
path), eight constant tables generated and SHA-pinned against the exact
upstream files (byte-identical when re-parsed as C). Licensing: an LGPL-2.1+
transcription inside an MIT tree, flagged with the pre-existing JVM-fdlibm
transcription for the licensing review — separate lineages, coexisting.
Routed every corpus-reachable double-precision host-libm bit-claim site:
31 calls across 14 files (hypot×6 files, cos/sin in ucarima_engine +
seats_ucarima_model + seats_linear_filters fu_freq, pow×4, atan2, acos,
exp×7 results builders, log×2) plus three TEST-side expected-value
recomputations. Census replay of all 57 test corpora + 13 recorded sweep
corpora + 16 golden-capture corpora (~88M distinct-argument replays):
hypot/log/pow/atan2/acos/asin 0 divergences; sin/cos/exp show 888 host
divergences (sin 651, cos 115, exp 122) — all PROVEN INERT: full ctest and
all twenty sweeps stay bit-identical with the vendored bits in the path.
`tools/verify_hostlineage.cpp` gate reworked: 59 pinned edge vectors +
SHA-256 over the FULL dense-grid output bit stream of each kernel
(machine/ISA/compiler-invariant under -ffp-contract=off; verified -O0/-O3),
--census-only corpus fingerprinting, --host probe informational,
--print-kat regeneration.

**Leg B — CI provider pinning.** `.github/workflows/core.yml`: pin comment
recording the locked tarball SHA (cd7e1298…, enforced by
tools/build-openblas.py) and the qualification provider .so SHA (84de4eab…);
the built-OpenBLAS cache key embeds the tarball SHA so a lock bump can
never resurrect a stale provider; a never-failing Provider fingerprint step
reports the LIVE provider .so hash and the runner libm identity on every
leg; extended diagnostics behind the existing `|| true` guards keep the
first-failing log visible. macOS legs stay informational-until-item-25 —
honest posture, matrix unchanged.

**Leg C — shim-invariance acceptance battery.** `tools/diagnostics/
{libm_shim,lapack_shim}.cpp` (DIAGNOSTIC ONLY; deterministic ±1 ulp at the
libm or provider boundary, plus the record mode that fed the census) and
`tools/ci-shim-invariance.sh` driving 22 bit-claim suites through
plain / ledger-all@100 / lapack1@100. Verdict: **22/22 ledger cells
byte-identical** — every bit claim now reads vendored code, not host bits.
The last two reactions found were test-side recomputations through raw
host pow/log (since routed). The informational lapack1@100 cell (1 ulp on
EVERY provider output — far beyond any real provider delta) reacts on
exactly the four documented families: oracle diff-tmean worst 3.1e-3 →
6.25e-3 crossing the 5e-3 band on 1/13 cases; EC band statistics drift
orders inside band; one arma_identification boundary decision; one diffuse
bit-equality. No band widened; the qualified provider reproduces the
goldens exactly.

**Gates.** Fresh ctest **75/75 in BOTH lanes** post-flip (ON 921s, OFF
1041s, RUNPATH-pinned provider); all twenty verify sweeps against the
flipped archive reproduce their recorded counts with 0 failures (WK 126/0,
calendar 18305/0, tramo arma-selection 14+1+1, automatic 71401/0,
decompose 81/0, bpass 18/18, EC 27/0 …), and seven of them record ZERO
ledger-libm calls (provably inert by construction); ASan/UBSan clean on
all eleven touched-kernel units; `check-manifest.py` 176/112/275;
`verify-migration.py` unchanged at 406.

**Gaps (honest).** Run #22's per-leg junit artifacts were unreachable from
this environment (401/403, no token) — the run-#22 table in the CI-FIX-2
verdict stays the record. macOS legs remain informational until the
item-25 long-double decision (x87-vs-quadmath is a semantic choice, not a
transcription). The lapack1@100 extreme cell marks the diff-tmean family
as the one place where a hypothetical all-1-ulp provider would need a band
re-measurement as a product decision.

## TRAMO regression tests, no-calendar branch: 23-E PARTIAL lands `kalos::run_tramo_regression_tests` + the ModelStatistics/refStats-gate/mean-rule layer — 2026-11-22

**Delivered.** The calendar-free branch of the tramo regression-test layer of
the pinned jdplus 3.9.0 stack, transcribed bit-exact (decisions AND values):
`kalos/tramo_regression_tests.hpp` carries `tramo_ami_check_mu`,
`tramo_round0_accepts`, `tramo_reduce_va`, `tramo_last_solution_orders`,
`tramo_estimate_model_retry`, `tramo_model_statistics` (the `ModelStatistics.
of` rows the refStats gate consumes — Ljung-Box + fixed-df-2 seasonal pair on
the 23-C.2 `auto_correlation`/`calc_lb_length` kernels with
`detail::chi2_upper_p` (no new distribution kernel), `se = sqrt(ssq/(dof−k))`,
fdlibm `bic`, the `df = m−k <= 0` un-built all-NaN JVM-throw surface,
obs/effobs/outliers-count semantics), `tramo_test_tstats` (both
`ConcentratedLikelihood.ser/tstats` arms — the TRAMO accept sites read the
ADDMU arm, denominator `dim − nx − k`, so the 21-G plain-arm
`RegressionTestStatistics::tstats` is NOT reused), `tramo_t_accepts`
(dcmpl buckets), `tramo_ref_stats_gate` (the full `TramoKernel.pass2`:
rules A–D + shapeA/shapeB guards, nr>nb skip, pcr +.025/+.015 ladder,
accept `dcmpg(lbp, pcr) <= 0`, pass-1 reduceVa + UNCONDITIONAL remark,
last-chance rewrite with `round := 1` and outlier-variable removal) and
`run_tramo_regression_tests` (the Fast/Default machines' mean-rule branch;
hand-overs are required request data, 21-G precedent). The likelihood
skewness/stableMean/stableVariance rows are decision-inert — recorded in the
corpus, not transcribed.

**Pinned corrections (bytecode re-verified, corpus adjudicating; SECTION 25
carries the dated notes).** (1) `dcmpl` pushes −1 for NaN: a NaN |t| counts
in NEITHER T bucket and a NaN lbp is EXCLUDED from rules B/D — the
session-start notes "NaN → HIGH" / "NaN enters B/D" had the dcmpl/dcmpg
polarity swapped; there is NO divergence from the landed 21-G
`t_test_accepts` after the correction. (2) pass2 sets
`needAutoModelling = !changed` (bytecode 534-544; corpus `pg_null_ref`/
`pg_remark` witnesses). (3) the pass-1 `log.remark` fires unconditionally
after `reduceVa()` (489-501 pops the return). The `am_*` checkMu witnesses
replay through the live `TramoSpec` decomposition (jshell: auto-model true
exactly for TR3/TR4/TR5/TRfull, `getMean().isTest()` false for every
constant).

**Boundary.** Calendar-kernel requests are the 21-I.3 seam (roadmap I-4
ruling): Fast ami td/lp/easter and Default calendar builder fields refuse
fail closed with 21-I.3 in the reason; auto moving-holiday refuses (no
filter exists in the pinned machine, no witness); quarterly rows refuse on
D11. The corpus pins BOTH sides: seam cases ran fully on the JVM (their
decisions are in the corpus) and their `--repro`-stable load logs show the
controller classes loading ONLY there. `TradingDaysController` and
`Automatic{,Wald}RegressionTest` are not ported.

**Corpus + replay.** `validation/reference/TramoRegressionTestsProbe.java`
(public entry points; pass2 reflectively driven) + the per-case capture
script (JVM-21 guard, dexp/dlog intrinsic-disable recipe, ground-truth
guards, load witnesses). Double capture, all nine data files byte-identical
across JVM processes, SHA256SUMS verified; 55 cases: 10 ModelStatistics
(incl. the df≤0 throw row and a record-only quarterly row), 12 Fast,
10 Default, 16 pass2 gate (rule A/B/C/D + short variants, shapeA/B±, NaN
ref, real ref, remark, last-solution, null-ref, keep), 7 AmiOptions.
`tools/gen_tramo_regression_tests_golden.py` → bit-pattern tables;
`tests/tramo_regression_replay.h` (shared ctest/sweep) rebuilds every
request from the hand-over echoes and compares bit-for-bit, asserts the
fail-closed refusals and qualifies the non-witnessed control members
synthetically (retry ladder shapes, round-0 NaN, reduceVa NaN/floor,
T-bucket boundary at |t| == t and |t| + 1ulp).

**Gates.** Fresh ctest **76/76 in BOTH lanes** (ON 899s, OFF 906s,
provider-pinned env; new #76 `test_tramo_regression_tests`, TIMEOUT 3600,
663 checks); ASan/UBSan lane green on the touched units; all twenty-two
verify sweeps rc=0 at their recorded counts (WK 126/0, calendar 18305/0,
tramo arma-selection 14+1+1, automatic 71401/0, decompose 81/0, b/c/d
18/18/22, EC 27/0, allocation 280, fit 528, seats-model 1026 …) with the new
`verify_tramo_regression_tests` 663 replay + 464 corpus cross-checks
(both coupling directions corpus↔golden); the shim-invariance battery is
**23/23 ledger cells plain=0 INVARIANT** with `test_tramo_regression_tests`
as the 23rd cell (the replay consumes only routed kernels + exact IEEE
sqrt); `check-manifest.py` 178/112/286; `verify-migration.py` unchanged at
406.

**Gaps (honest).** `tramo_estimate_model_retry` is transcribed control flow
without a live witness (the loop needs a full `estimateModel`; its
reduceVa/mean-test leaves are corpus-qualified, the ladder shape
synthetically) — qualified as PARTIAL in the contract. The Default
test-model estimation is hand-over data (captured at its corpus precision
1e-5), not unit work. The calendar branch, the automatic test families and
the `TradingDaysController` remain 21-I.3; the record-only
ModelStatistics surfaces stay recorded, not transcribed. Contract
`tramo-regression-tests`; roadmap row LANDED (PARTIAL).

## TRAMO final-estimation decision layer: 23-F F.1 lands `kalos::tramo_final_estimation` (forked from 21-H) — 2026-11-22

**Delivered.** The decision layer of the pinned `jdplus.tramoseats.
base.core.tramo.FinalEstimator`, transcribed bit-exact and adjudicated
FORKED against the landed 21-H `final_estimator` (roadmap fork-candidate
row closed): `kalos/tramo_final_estimation.hpp` carries
`tramo_mirror_roots` (`Polynomial.ofInternal` trimming + mirror + the
bit-exact `detail::p_roots` fast-solver mirror, degrees 1–2 closed forms /
≥3 the 24-A.1 QR cascade), `tramo_searchur_roots`/`tramo_searchur`
(scaled `Complex.abs`, `|r| > ur` strict, `|im| ≤ 0.05` inclusive,
`re > 0` strict — every NaN/infinity polarity corpus-pinned; promotion
fires when an AR COEFFICIENT lands near +1 under the `1 + phi(B)` raw
coefficient convention), `tramo_check_unit_roots` (the `p > 0 && d < 2` /
`bp > 0 && bd < 1` promotion pair, inline MAXD/MAXBD literals),
`tramo_check_common_roots` (strict `|bphi(1) − btheta(1)| < cancel`,
cancel 0.044 builder default — live-fixed, no setter; the p/q dead-work
`roots()` calls execute faithfully for their exception escape),
`tramo_final_estimator_test` (no root gates — the 0.95 slot is stored and
never read; ratio start 99999.0; P/Bp/Q update the ratio, Bq fires WITHOUT
updating it; 0/1 takeover flags; estimation-domain sig2 plateau; `tsig =
1.0` live-fixed — the Builder exposes no `significanceThreshold` setter)
and `tramo_final_orders_to_string` (the "arima spec changed" info detail;
the two-arg `info(String, Object)` routes to `(msg = null, obj = message,
det = orders)`). The validation guards mirror the JVM read sites (the
pinned `SarimaModel` cannot represent a short block).

**Corpus.** `validation/evidence/native/tramo-final-estimation/` —
75-case split-package probe (`Scratch23f.java`; reflection for ACCESS
only: four private methods + the final tsig field; all decisions run the
pinned bytecode), double capture byte-identical + third
`-Xlog:class+load` witness run byte-identical (tramo `FinalEstimator` +
`$Builder` loaded from the pinned jar, ZERO `x13.regarima.FinalEstimator`
loads), `cases-fe1.txt` 18 searchur / 10 mirror-root / 19 unit-root /
9 common-root / 9 live-landing cascade cases (with their exact
asymptotic-covariance diagonals — the diagonal covers ARMA coordinates
only, mean excluded) + 10 live `estimate()`-driver rows.
`tests/jvm_tramo_final_estimation.h` (generator
`tools/gen_tramo_final_estimation_golden.py`, raw IEEE-754 bits
throughout); `test_tramo_final_estimation` replays all 65 decision rows
bit-exactly + hand-built fork edges; the 10 `fe_est` rows are inert until
the F.2 round loop (count-pinned).

**Pinned corrections (SECTION 26.6 carries the dated notes).** (1) The
root space is the MIRRORED AR COEFFICIENTS (`getRegularAR() =
Polynomial.ofInternal([1 | phi])`, TRIMMING — golden `mr06` caught the
port's initial no-trim error): promotion fires at coefficient ≈ +1. (2)
`estimate()` field semantics: `nnsig` keeps an earlier round's value when
a later `mapping.getDim() == 0` round short-circuits TRUE (es05/es08);
`dim` counts mapped ARIMA parameters only. (3) `checkCommonRoots` cancels
on RAW lag-1 coefficients with strict `<` (exactly-equal pins no-cancel);
NaN never cancels. (4) The takeover fires when `t` is strictly BELOW the
incumbent ratio — a cheaper block wins (ratio 0 blocks t 0; `tc05` pins a
1-ulp takeover).

**Boundary.** The five-round `estimate()` loop, the multi-round
`FastOutliersDetector` machine, the `TramoKernel` iterate/pass/pass3
wiring and the `tramo_fit` assembly are 23-F F.2/F.3 (the D3
divergent=1 flip table lands there); the information stencil feeding the
cascade's covariance diagonal is decided when the F.2/F.3 estimation
hand-overs wire through the qualified seams. Round-0 scan behavior and
the 23-B contract stay untouched (historical fixtures immutable).

**Gates.** Fresh ctest **77/77 in BOTH lanes** (ON 1288 s, true serial
1215 s; new #44 `test_tramo_final_estimation` Passed both; baseline 76
pre-F). An early claim was DISCARDED under a recorded gate-hygiene
incident (pinning SECTION 26.9): two overlapping ctest runs on one build
tree (one with pre-fix binaries) and a shim battery sharing the tree —
all killed and re-run exclusively on the final binaries; the numbers
above are the exclusive fresh runs. ASan/UBSan lane green on the touched
unit (dedicated `KALOS_SANITIZERS=ON` build, replay + fork edges clean);
all twenty-two existing verify sweeps rc=0 at their recorded counts
(WK 126/0, calendar 18305/0, automatic 71401/0, decompose 81/0, EC 27/0,
allocation 280, fit 528, seats-model 1026, regression-tests 663+464 …)
— zero drift; the shim-invariance battery is **24/24 ledger cells
plain=0 INVARIANT** with `test_tramo_final_estimation` as the 24th cell
(scaled-modulus `abs`/sqrt + exact IEEE arithmetic only, no new ledger
arguments; lapack1 informational divergences match the prior profile);
`check-manifest.py` 180 core / 112 fixture / 295 evidence;
`verify-migration.py` unchanged at 406.

## 23-F F.2 catch-up (landed 2026-11-23 across e7e0784..b70f9ff; entry written at F.3) — 2026-11-23

F.2 landed in two machines and never got its status entry; both are green
and ACTIVE. (1) **Multi-round outlier machine** (`kalos::tramo_outlier_rounds`,
commits e7e0784 corpus / f1f5c8b unit): the `FastOutliersDetector.process`
port over the dual-machine Scratch23g corpus (`tramo-outlier-rounds`, 37
cases, identity + agreement + witness gates), 37/37 full-trace bit-exact in
the extended `test_tramo_outlier_scan`. Decoded constants: builder→ctor ARG
CROSS (ODM `maxRound(50)/maxOutliers(30)` runs with fields round-cap 30 /
count-cap 50, SECTION 27.2), the add-gate at scan count 7 with `mvx` re-arm,
and the optimizeModel firing rule (`install` iff identity||mvx||round==0;
the non-install branch sets stable:=true and SKIPS optimize — 27.6). The
**27.2→27.6 pivot correction**: the FULL `ConcentratedLikelihoodComputer`
(used by `process`) decomposes with nfixed = 0, FULLY PIVOTED (measured JVM
pivot [0 1 2 3 6 4 5]), no rank==ncols requirement — 27.2's "xfixed=true"
belonged to DEFAULT_COMPUTER (the estimateModel/landing seam); the fix is
`pinned_deviances_evaluate_multi` + `decompose(xx, 0)` (mr_ml2 root cause).
(2) **Round-loop driver** (`tramo_final_estimator_run`, 1574a2c goldens /
b70f9ff driver): one `push("full estimation")`, AT MOST five round bodies
(i<5), estimation assigned BEFORE the gates, the `dim==0` and `!ami`
short-circuits (nnsig untouched), `checkCommonRoots` false ⇒ nnsig=2 WITH
the setBp(0)/setBq(0) rewrite at cancel 0.044 STRICT, the silent
`outliers && pass <= 1` and exhaustion falses, the `estimation faied` jar
typo only in the RuntimeException catch. The **tail adjudication RETIRES
the fast=true claim of c6fedf2**: the FE computer is built
`computeExactFinalDerivatives`, so the Point tail is the
RegArmaProcessor.compute EXACT branch (calcResiduals at the landing, the
design-free deviance machine = ssq·factor on the residuals, the
SarimaMapping2 FD stencil — indefinite info allowed; golden es01 info11 =
−10.1078 is impossible for the Gauss–Newton curvature), and the score/info
scale is 0.5·nobs/fx on cdf nobs. **10/10 `fe_est` rows ACTIVE** at raw bit
equality (return/nnsig/orders/pars/state/cl/vd/events; SECTION 30.3/30.4
witness table). Gates at b70f9ff: ctest 77/77 BOTH lanes exclusively
(903.79 s / 893.11 s), sanitizer green on the four touched units, 22 sweeps
at recorded counts, shim battery 24/24 ledger cells INVARIANT, manifest
183/112/309 post-commit, migration 406.

## 23-F F.3 evidence (landed 2026-11-23, 3886a8c; entry written same day) — 2026-11-23

Probe corpus `validation/evidence/native/tramo-kernel-rounds/` (Scratch23r):
the REAL `TramoKernel.process(TramoContext)` and a bytecode-faithful MANUAL
driver of the same iterate/pass/pass3 logic run over the same spec in ONE
JVM, 35 cases; the per-case identity gate asserts jar vs manual equality of
the full ProcessingLog stream, the final model bits (spec/orders/mean/
outliers/pars) and the aggregate — 35/35 all-1, double capture byte-
identical, witness mode clean (zero unexpected loads). The corpus pins the
gate semantics the unit needed: `pass2(!changed, m)` operand negation
(q_air pass 2), the `round " + pass` step text, the round-2 no-outlier
guard (q_air iterate #3), the pcr ladder 0.95→0.975→0.99→1.005, the
lastSolution pass-3 re-estimate, and the last-chance warning; the quarterly
`kr_q_air` row is the D11 refusal witness. Dated adjudication (SECTION 30.8
guard): the Scratch23r manual driver transcribes refStats rules C/D with a
deviation that is NON-DISCRIMINATING on the corpus (all 34 monthly gate
rows revert 0); fresh javap re-confirmed the landed 23-E transcription
byte-correct — the unit uses the landed gate unchanged, corpus bytes
untouched. Evidence gates: SHA256SUMS.csv replay (evidence never
re-captured); manifest 183/112/320 at that commit.

## TRAMO kernel round driver: 23-F F.3 lands `kalos::tramo_run_kernel_rounds` — decision D3 executes — 2026-11-23

Unit (commits 0cfc768 unit + golden + test / 54a3011 sweep / 9dc49e3
manifest / this docs entry): the `TramoKernel.ami()` ten-iterate round-
driver seam — `initProcessing` (calcCv trigger on `dcmpl(curva,0)==0` with
the ACTUAL non-missing count: `kr_missing` pins 94 not 96), the iterate
guards (`needDifferencing = needAuto && !g && (d<2 || bd>=1)`, `g =
round==2 && outliers.isEmpty()`; outliers site has NO round-2 guard), the
change accumulator (differencing ASSIGNS, arma OVERWRITES with
`!(spec.equals(orders0) && mean==mean0)`, scan ORs), the pass-1 difference
drop (+ outlier removal + estimation invalidation), the `estimateModel`
ladder with the ONE persistent retry counter and the pass-1
`reduceVa(curva)` arm over the landed 23-E `tramo_estimate_model_retry`,
the round-0 re-arm/snapshot/accept via the landed `tramo_round0_accepts`
(silent converge — no `iterate done`), the `pass2(!changed, m)` gate over
the landed `tramo_ref_stats_gate` UNCHANGED (fresh javap re-confirmed the
23-E rules A–D; the probe driver's C/D text is non-discriminating, dated
note in the contract), the pass3 reset (`pass=1, pass3=true, needAuto=true`),
the raw-tsig mean-test `pass = 4` kill and the result-IGNORING controller
sweep. Heavy stages and module log blocks ride as ordered seam facts; every
JVM echo (taken, scan cv, retry idx/tsig, round0 accept/lbp, gate revert/
skip-nrnb/lbp/se, pass2 ret/pcr, iterate end) is a cross-check and every
desync throws (fail-closed). 34 monthly cases replay bit-exactly (event
stream, per-iterate snapshots, init bits, iterations, convergence) plus the
quarterly `kr_q_air` D11 refusal; the Ljung-Box p-values of all 65 stats
rows recompute bit-exactly from the captured e-arrays through the landed
primitives (ModelStatistics.of = acf(e, 0.0) NO mean correction,
defaultAutoCorrelationsCount = the landed `calc_lb_length`, df = k − nhp —
javap ModelStatistics.of re-confirmed).

**D3 flip CLOSED (option A):** `sc_so`/`sc_spanout`/`sc_spannone`
(divergent=1 in 23-B) are POSITIVE multi-round assertions — `kr_scso`,
`kr_spanout`, `kr_spannone` replay as 2-iterate ladders — and the pass3
divergences `kr_gs2`/`kr_shortfail` replay 3-iterate; asserted in
`test_tramo_kernel_rounds` and witnessed on every gate lane. Implemented-
unwitnessed monthly (throw-qualified, contract-listed): the `armaq1`
pass-3 rewrite, the estimation-failure arm + its reduceVa, the gate
`first` form, revert/restore, last-chance rewrite, regfail/estmodelfail
ends.

**Boundary.** `tramo_fit`/`TramoSeatsFit` assembly and the end-to-end
`TramoKernel.process` oracle stay 23-G/24-D; the calendar-test controllers
inside the sweep are 21-I seams (their results feed nothing in the kernel);
quarterly TRAMO stays refused (D11).

**Gates.** Fresh ctest **78/78 in BOTH lanes** (lane-OMP tree 23f3-laneB
2854 s; true-serial tree 23f3-laneC 2893 s; new `test_tramo_kernel_rounds`
Passed both; baseline 77 pre-F.3). ASan/UBSan dedicated build
(23f3-laneD): the full replay green, zero sanitizer reports. All
twenty-two existing verify sweeps rc=0 AT RECORDED COUNTS — zero drift —
plus the new `verify_tramo_kernel_rounds` PASS (35 cases, 1641 checks:
raw-corpus cross-qualification of identity flags/init bits/jar event
stream/rsnap fields/seam census/gate echo bits, then full replay + LB
recompute). Shim-invariance battery post-lane on the lane-B tree:
**25/25 ledger cells plain=0 ledger100=PASS INVARIANT** with
`test_tramo_kernel_rounds` as the 25th cell (lapack1 informational
divergences unchanged: estimation_control/x13_automatic_oracle rc=0,
arma_identification/diffuse_state_space rc=134 — the declared profile);
`check-manifest.py` 185/112/320 post-commit; `verify-migration.py` 406;
evidence `SHA256SUMS.csv` replayed 10/10 (corpus not re-captured).

## TRAMO fit assembly: 23-F F.4 lands `kalos::tramo_fit` — D9 executes — 2026-11-23

`kalos::tramo_fit` (item 23-F F.4, roadmap 23-F LANDED) assembles the
monthly automatic-TRAMO pipeline with ZERO own stage logic: the fail-closed
refusal matrix (D11 frequency, D10 calendar family, I-10 auto-methods, the
never-witnessed `+oc` pre-correction, D6 decomposition, missing values — 10
corpus refusal rows + the d6/i10 synthetic rows), the LIVE 23-A builder and
23-B transform mirrors (any desync with the captured pre-stage throws), the
decoded `isFullySpecified` `ami()` branch (transform decided && nothing
automatic — the seam stream cross-checks the verdict both ways), the F.3
round driver verbatim, per-seam LIVE 23-C/23-D re-runs over the captured
module ENTRY state with the EVOLVING seasonality context (`ctx.seasonal` at
module creation, tracked through the `seasonality_controller` seams), the
differenced-response recompute on the reversed `BackFilter` taps, and the
D9 regression-block covariance `V = unscaledCovariance * (ssq/(dim-nx-k))`
— **74/74 captured upper-triangle entries bit-exact**, the ARMA tail kept
on the separate `final_information` surface (D9 + the SEATS-variance
separation: no substitution across the two). The corpus is the
triple-identical Scratch23f capture under
`validation/evidence/native/tramo-fit/` (35 rows: 34 golden cases + the
pinned `fit_const` JVM-crash witness; `SHA256SUMS.csv` replayed 10/10; the
class-load witness shows zero `jdplus.spec.x13` loads and
`tramo.FinalEstimator` only from `tramoseats.tramo`). The golden
`tests/jvm_tramo_fit.h` regenerates byte-identically from BOTH captures;
measured live/fact seam tiers 27/54 are pinned per case — the census can
never drift silently.

**Debug correctives (dated, javap-decoded, corpus-adjudicated; six root
causes):** the differenced response applies the reversed filter directly
(`weightsToArray` emits reversed taps; the parity form landed `-0.0` where
the JVM sums `+0.0` at the fit_fct_air index-99 cancelled pair); the
stage-1 orders gate admits only the decoded `testSeasonality` m011
downgrade shape; the `ami()` branch reads the decoded `isFullySpecified`
(fit_nomean's auto transform runs the iterate loop); the stage-4 modules
read the EVOLVING context seasonal (gs2 seam 18, shortfail seams 6/17/19);
the 23-C unit's Changed-law compares against the ENTRY orders, not the
initial-round start, and its airline branches were re-transcribed to
`SarimaOrders.airline` (ARIMA(0,1,1)(0,1,1), `bp = 0`) — the battery never
entered with non-zero orders, so both stayed latent until the fit corpus;
the 23-D unit now mirrors the `ArmaModule.process` catch (no selector-
internal try exists — `ArmaModelSelector` carries no exception table) so an
HR pipeline RuntimeException answers `Failed` and never throws. Appendix in
the 23-C contract; dated notes in the 23-D contract; full table in the
tramo-fit contract.

**Boundary.** No interpolation/calendar/auto-method paths (refused); no
SEATS decomposition (the `TramoSeatsFit` assembly stays 24-D); the
end-to-end `TramoKernel.process` observer oracle stays 23-G; quarterly
TRAMO stays refused (D11).

**Gates.** Fresh ctest **79/79 in BOTH lanes** (tree 23f4-b1 1188 s; fresh
tree 23f4-b2 1218 s; new `test_tramo_fit` #79 passed both; baseline 78
pre-F.4). ASan/UBSan dedicated build (23f4-laneD): the fit replay + the
23-C/23-D suites green, zero sanitizer reports. All twenty-three existing
verify sweeps rc=0 AT RECORDED COUNTS — zero drift — plus the new
`verify_tramo_fit` PASS (34 cases, 584 checks: identity flags, fitseam/
post-state/scontroller/rsnap/fitresid censuses, the fitdec verdict and the
fituc = nx(nx+1)/2 = D9-entry identity against the raw corpus, then full
replay). Shim-invariance battery post-lane on the b1 tree: **26/26 ledger cells plain=0
ledger100=PASS INVARIANT** (the informational `:92` unbound-variable echo
is the known cosmetic line — the declared profile) with `test_tramo_fit`
as the 26th cell;
`check-manifest.py` 187/112/331 post-commit (the F.4 landing had missed
the manifest registration — closed here); `verify-migration.py` 406;
evidence `SHA256SUMS.csv` replayed (corpus not re-captured).

### M5 / 24-D — structured WK extraction gate (2026-11-23)

The first 24-D pipeline substep exposes the two decompositions that the
qualified 24-C kernel computes. Its frozen gate covers every initial and
bias-corrected component in the 36-case WK JVM corpus, including calendar,
row presence, binary64 values, and existing error/refusal behavior. The
TRAMO-to-SEATS bridge and deterministic-effects merge follow independently.
[Extraction contract](docs/contracts/seats-wk-extraction-contract.md)

**Implemented and qualified.** The fixed-model extractor exposes ordered,
dated binary64 arrays for both phases, including the conditional standard
deviations. Release and ASan/UBSan runs on macOS ARM64 reproduced the 36-case
fixture with 1,208 existing checks, 1,055 structured component rows, and
55,812 bit-exact observations. An additional one-month backcast across year
zero checked the calendar on the structured and text results. The source
manifest (187/112/331) and 406-file historical migration audit passed.

The next 24-D gate is the fitted-model bridge. The pinned
`TramoSeatsKernel.of(RegSarimaModel)` filters deterministic effects by their
regression-effect attributes, repins the fitted coefficients, and supplies
`ls_ssq / (ls_eff - ls_npar)` to SEATS. 23-F currently supplies witnessed
model fits. Qualifying this handoff and the subsequent deterministic-effect
merge requires reference input/output pairs from the combined kernel. The
extraction result alone does not enable an end-to-end TRAMO/SEATS fit.

### ARMA panel portability gate (2026-11-23)

The macOS ARM64 failure in `test_arma_refinement` came from the
`m_wn96_a` white-noise candidate. With no ARMA coefficients, its generic
physical likelihood used host long-double arithmetic and produced BIC2
2.6345110282485509. The captured JVM result is 2.6345110282485513.
The panel now evaluates that candidate with the existing design-free
pinned concentrated likelihood. All four other preferred candidates already
matched their exact reference values.

The original assertion passed against checksum-pinned and Homebrew OpenBLAS
0.3.34. The frozen 21-E identification sweep passed 4,368 checks with one
documented skip, and the 21-F refinement sweep passed 423 checks. All 14
evidence-file hashes matched. The identification, refinement and WK extraction
tests also passed in the OpenMP and ASan/UBSan builds. The full serial Release suite passed **79/79 CTests** on the pinned provider, including the X-13 automatic fit and horizon oracles.

### M5 / 24-D — fitted-model bridge reference gate (2026-11-23)

The gate replays five monthly fits (TR0, TR1 and TR3) from the frozen 23-F
observations in independent pinned JVMs. It records the fitted-model inputs
and the observed output of the real TRAMO-to-SEATS bridge, and checks bitwise
agreement on the dated series, differencing orders, fixed SARIMA coefficients,
mean/log flags and transmitted variance. The component merge has its own gate.

**Reference gate passed.** The actual bridge from JDemetra 3.9.0 was captured
and independently replayed on Temurin 21.0.11+10, macOS ARM64, with six
checksum-pinned jars. The five fitted-model handoffs contain 576 dated
observations. TR0 and TR3 on AirPassengers changed zero values, TR1 with an
AO changed one, TR3 with an LS changed 60, and TR3 with an AO and two LS
changed 70. Each fitted coefficient block, the mean and log flags, the
transmitted variance, and all corrected series values passed the direct
bridge checks. Capture SHA-256:
`498366de6882a369198922e85b2c81fd9cccf4499a80f246147b3bcba14225f1`.

The native bridge can now replay these frozen model inputs. A nonzero additive
effect and additional regressor classes need their own witnesses before
expanding that scope. Final-component restoration and the end-to-end TRAMO
and TRAMO/SEATS oracles remain open. [Bridge contract and replay](docs/contracts/tramo-seats-bridge-contract.md)

### 24-D prerequisite — fitted outlier coordinates

Bridge replay exposed an error in the 23-F fixture generator. It used the
first date of the differenced residuals when translating a fitted outlier's
date into an observation index. The January 1994 AO in `fit_tr1_ao` therefore
appeared at index 35 in the 1990-start series, while its JVM contribution
is at index 48. The correction uses the input estimation calendar for all
45 affected fixture records. The header regenerated identically from the
two original captures. Its 34-case replay passed, including date-to-index
checks for the returned variables, 74 D9 entries and the 27/54 seam counts.

### M5 / 24-D — native fitted-model bridge

`make_tramo_seats_model` consumes an available 23-F result, validates its
calendar/design envelope, and returns the fixed-model request used by 24-C.
The installed interface preserves fitted orders, flags and four coefficient
blocks. With identical fitted input values, five bridge replays match the
JVM variance and every one of the 576 adjusted observations bit-for-bit.
The test calls the real 23-F assembly for every case and independently checks
its decisions and calendars before comparing the bridge arithmetic at the
frozen fitted inputs. Unsupported regressor classes, missing observations,
subdomains and length-of-period corrections refuse explicitly. OpenMP and
ASan/UBSan each passed the three affected tests. The manifest (189/112/334)
and historical migration audit (406 files) passed.

### M5 / 24-D — deterministic-effects merge contract

The pinned JVM produced six extraction/merge pairs on the five frozen bridge
models. The gate covers 65 merged rows (4,005 binary64 values) over the
observed, forecast and backcast domains, plus the exclusion of extraction
standard deviations. The mixed AO/LS cases require SEATS model approximation;
their captured WK input permits isolated merge testing within the approved
D6 policy. Two fresh JVMs matched the capture, SHA-256
`ca671ddd71d9689ef0a2d5ab786bd7f5f098d580579b1b33186a88fb60d48054`.
[Merge contract](docs/contracts/tramo-seats-bridge-contract.md)

### Cross-binding integration — combined SEATS linkage

The R shared-library integration exposed 18 externally linked function
definitions in the shared UCARIMA detail header. Both the decomposition and
WK translation units defined those symbols. The header definitions are now
inline. The installed-consumer regression links and runs both public
operations in one executable, alongside the provider check. That regression
and both decomposition/WK corpus tests passed on macOS ARM64.

**24-D reference profile.** A second Temurin 21.0.11 JVM (x86-64) reproduced
the exact fitted bridge capture and independently double-produced the paired
merge capture with SHA-256
`292da22739c949e4a73ba2398e456c842bf52af85521b66f47a8679dca5a756f`.
The fixed-input JVM comparison identifies architecture-specific WK arithmetic
in TR3 AirPassengers and LS. The x86 profile is the existing Kalos math
target. Both exact captures remain available for testing component restoration.

### M4 refusal reference portability

The complete nine historical RSA0/RSA1/RSA2c reference demand records
reproduced on Temurin 21.0.11 x86-64. On the native ARM64 JVM, eight
records differed only in fitted coefficients. A second x86-64 run with
six math intrinsics disabled reproduced all nine ARM64 records byte for
byte. A replay checks the complete record hashes as well as three named
coefficient fields using the pinned jars and source series. These tests
qualify the M4 refusal evidence. Native template execution belongs to M5b.
[Capture and replay](validation/evidence/native/x13-refusal-jvm-profiles/README.md)

### 24-D prerequisite — checked SEATS horizons

Negative SEATS horizon counts represent years. The extractor converts them
in 64-bit arithmetic and validates the complete Burman domain, including
filter padding, before building forecast arrays. Both extreme signed
integer requests refuse deterministically. The normal WK corpus and the
overflow cases passed, including the ASan/UBSan build.

### M5 / 24-D — native deterministic-effects merge and fitted WK pipeline

The pipeline accepts an available fitted TRAMO result, builds the SEATS
request, runs WK extraction and restores fitted AO/LS effects. Across six
horizon/model cases, the standalone merger reproduces all 65 product rows
and 4,005 binary64 values on both JVM profiles. Four decomposable fitted
inputs reproduce all corresponding bias-corrected WK and product rows
against the pinned x86-64 JVM. The mixed fit requires SEATS approximation
and refuses under D6 on both tested horizons. The regression also covers
duplicate, missing and misdated WK rows and a zero-horizon fit. The focused
tests passed in serial, OpenMP and ASan/UBSan builds.

These comparisons use identical fitted inputs. An ordinary series-to-fit
route without 23-F witness injection still needs the 23-G estimator and
round-driver oracle. The 24-E end-to-end gate requires rsa3 and rsafull;
21-I remains the dependency for TRfull's calendar tests.
[Public API and contract](docs/contracts/tramo-seats-bridge-contract.md)

### M5 / 23-G and 24-E — public TR0/RSA0 reference panel

Six original monthly series drove the public TR0 and RSA0 JVM kernels
directly. The reference captures the two fitted MA coefficients, mean
coefficient, log likelihood, residual SSQ, effective and estimated parameter
counts and every final component. All six public fits were decomposable.
Two fresh x86-64 Temurin 21.0.11 processes produced the same 60 dated
final rows. Replay uses the existing pinned 3.9.0 classpath and the new
`--stage rsa0` option. Capture SHA-256
`a9eb8b11f7b062db2578de650bbe5a8ef323a28138cc7d47c97e3b4d280c4669`.
[Public kernel capture](validation/evidence/native/tramo-seats-bridge/rsa0-x86_64.txt)

### M5 / 23-G and 24-E — ordinary monthly TR0 fit and RSA0 components

`estimate_tramo_tr0` runs the 23-A monthly builder, Hannan–Rissanen
initialization, pinned Levenberg–Marquardt fit and the fitted likelihood.
`run_tramo_seats_rsa0` adds the 24-D extraction and product merge. The two
entry points take the original dated series. The raw-input oracle matches
both MA coefficients, mean coefficient, log likelihood, residual SSQ, both
degree counts, 60 final component rows and all 3,960 binary64 values on six
series against the public TR0/RSA0 JVM kernels. The focused serial and
OpenMP tests passed. Ordinary automatic RSA3 still needs its 23-G round
seams, and RSAfull needs the 21-I calendar tests.
[Usage and qualification](docs/contracts/tramo-seats-bridge-contract.md)

### 23-G prerequisite — exact ARMA selection inputs

The 23-F fixture generator had decoded the decimal length after `fitresid n`
as a leading binary64 observation. Both original JVM captures regenerate the
same corrected header. The raw sweep now compares the length and binary64
bits of all 30 ARMA residual records in the 34-case golden (2,991 values);
the separate constant-series crash witness has no golden. All 614 sweep
checks pass. The selected models, measured 27 live / 54 recorded seams,
10 refusals and 74 D9 covariance entries agree with the original decisions.

### M5 / 23-G — native ARMA input residuals for the mean-only design

The automatic TRAMO ARMA selector now derives its input from the original
series, the kernel's entry differencing orders and its mean flag when the
design contains no other regressors. The fitting path applies the selected
log transformation and exact BackFilter orientation, and uses the pinned
OLS solve for the optional mean. Supplied residuals are checked bit for bit.
Re-running 16 cases without the former 1,443 input residual values reproduced
the same fitted decisions and seam census. Corrupting a supplied witness is
detected. The 34-case fit regression and 614-check raw sweep passed.

### M5/M6 — RSA0 preprocessing forecast-error reference

The public RSA0 `y_ef(24)` accessor reproduced the fitted preprocessing
model's `forecasts(24).getForecastsStdev()` on all six raw series. The
paired reference includes both that series and the WK Series/StdevForecast
output. These are separate quantities. The unqualified public `y_ef` key
defaults to 12 months. The explicit 24-month key covers the default SEATS
horizon. Two fresh x86-64 Temurin JVMs matched every byte.
[Paired forecast errors](validation/evidence/native/tramo-seats-bridge/rsa0-forecast-errors-x86_64.txt)

### M5/M6 — RSA0 public original-series forecast standard errors

`tramo_tr0_forecast_errors(fit, months)` exposes the preprocessing model's
`y_ef` on demand. The projection uses the existing forecast covariance with
separate JVM scales for the process and the estimated mean. All 144 public
values from six monthly series met the established forecast tolerance,
with maximum absolute error 2.42e−13. The 144 paired WK conditional errors
remain bit-exact. The regression checks zero and one-month requests as well
as the default 24-month horizon.
[Forecast error contract](docs/contracts/tramo-seats-bridge-contract.md)

### M5 / 23-G — ARMA handoff with AO and LS design columns

ARMA module entries can now supply ordered, typed AO/LS positions. The fit
rebuilds each raw factory column, differences it on the same calendar as the
response, solves the full pivoted OLS regression and forms the residuals
in column order. Replaying all admitted cases without any ARMA residual
arrays reproduces every selection: 18 cases, 1,704 residual values. That
includes the mixed AO/LS and pass-three controller cases. Both reference
captures regenerate the same golden. The remaining automatic fit dependencies
include outlier scan/estimation stages and the final estimator.

### M6 — standalone Windows qualification job

The hosted matrix now includes a native Windows Server 2022 / UCRT64 job.
It builds the locked LP64 OpenBLAS from source, requires a working
provider/OpenMP pairing, runs the full standalone CTest suite, and exercises
the installed consumer with provider reconfiguration and refusal checks.
The job publishes the Windows JUnit report. Windows qualification awaits
the hosted result.
