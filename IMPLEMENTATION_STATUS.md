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
| 22b.3a | Moving five-year dispersion and two-pass sigma weights; accurate and Java-free legacy rules | **Existing default decision remains pending.** A spike after zero dispersion regains full weight in JD+; the limiting formula gives zero. JD+ also gives full weight below 1e-5 dispersion. Qualify exact cutoffs, unit scaling, partial years, gaps and protected extensions before choosing the default. This does not block the repository split. |
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
