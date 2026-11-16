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
default has been promoted**.

Evidence: `validation/reference/capture-x11-sigma.R` drives the pinned
`DefaultExtremeValuesCorrector` (rjd3 3.9.0 jars, rJava) and captures 125
deterministic fixture cases to `validation/fixtures/native/x11-sigma` with
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
(`validation/fixtures/native/x11-replacement`, 17-significant-digit
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
`validation/fixtures/native/x11-selection` (28,382 table rows, per-stage
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
JVM 21) into `validation/fixtures/native/x11-{bpass,cpass,dpass}-mult`
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
`validation/fixtures/native/x13-allocation` — 7 cases (full additive mix of
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

Fixtures: `validation/fixtures/native/x13-fit` — 11 monthly cases executed
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
