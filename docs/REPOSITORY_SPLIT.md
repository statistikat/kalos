# Repository split delivery record

Updated 2026-11-14. Target sibling repositories: `~/git/kalos`, `~/git/rkalos`,
`~/git/pykalos`; the existing application stays at `~/git/persephone3x`.

The maintainer confirmed **required rkalos**: `persephone3 → rkalos → kalos`,
with `pykalos → kalos`. The Python scope is a minimal working binding and package
structure; full model APIs, NumPy adapters and distributable binary wheels are
explicitly deferred. These decisions supersede the older optional-adapter and
Python-parity clauses in the recorded plan below.

Filtered histories retain 116 relevant core commits and 102 R commits, with
original-to-filtered maps. All original fixtures and historical reports retain
their bytes. Active reference commands now use rkalos and Kalos paths, with an
explicit frozen output-request inventory and separate original capture snapshots.
The original source repository and branch history remain intact.

The implementation/status MD files are owned by their repositories. Kalos holds
the numerical roadmap, contracts, reference tooling and external-library evaluation;
rkalos holds R API/build/test work; pykalos holds only the minimal Python scope;
Persephone holds application and required-dependency integration work.

Local artifact qualification and publication progress are recorded in the root
implementation status. No registry release, default-backend switch or merge into
Persephone's default branch is part of this migration.

---

# Split into kalos, rkalos, and pykalos, using OpenBLAS

Recorded: **2026-11-13**. Status: **agreed direction saved for later implementation**.
This document describes future repository and package work; it does not record
an executed split or completed release qualification.

Related documents: [implementation status](IMPLEMENTATION_STATUS.md),
[external library evaluation](EXTERNAL_LIBRARY_EVALUATION.md), and
[numerical provenance](tools/reference/NATIVE_PROVENANCE.md).

## Decisions and current scope

- Create public repositories **statistikat/kalos**, **statistikat/rkalos**, and
  **statistikat/pykalos**, using `main` for the new default branches.
- Make `kalos` the authoritative C++ engine. Keep `rkalos` a lean R interface
  and deliver working Python bindings in the initial split.
- Preserve relevant filtered Git history in `kalos` and `rkalos`, with mappings
  to the original commits. Python starts new binding history with core provenance.
- After validation, replace this branch's embedded native implementation with
  a required `rkalos` dependency. Keep Persephone's R6 objects, hierarchies, plots,
  dashboards, and existing calendar functions in `persephone3`.
- Use **OpenBLAS 0.3.34** as the standalone/Python default. This was the current
  stable release verified during planning; recheck release status and advisories
  when executing the plan, and record any deliberate pin update. Keep R's
  configured BLAS/LAPACK provider for `rkalos`.
- Defer the shared calendar port, AT/EU calendar presets, additional time-series
  adapters, and registry publication. The split itself does not implement X-11,
  TRAMO, SEATS, or automatic model selection.

The current engine has advanced beyond fixed-parameter likelihoods: delivery
through 20c.1a includes fixed/free regression, stable/invertible ARMA maps,
bounded fixed-order coefficient optimization, structural transfer diagnostics,
and physical-coefficient observed information. Preserve both profile-ML and
conditional-regression/residual-df inference methods and their availability gates.
The latter has pinned X-13 qualification; TRAMO covariance, joint regression
uncertainty, and wider platform qualification remain open. Freeze the actual
reviewed source state before splitting rather than relying on the historical
capability summary in this planning document.

## Repository layout

```text
kalos/
├── include/kalos/                  # Public C++ value types and interfaces
├── src/                            # Kernels, preparation, estimation, diagnostics
├── backends/openblas.cpp           # CBLAS/LAPACKE implementation
├── cmake/                          # Dependency discovery and exported targets
├── dependencies/                  # OpenBLAS pin, hashes, build settings/notices
├── tests/                          # Existing standalone C++ tests
├── benchmarks/                     # Kernel and complete likelihood/fit workloads
├── validation/
│   ├── reference/                  # Pinned JD+ probes and comparison tooling
│   ├── fixtures/                   # Original artifacts and conformance cases
│   └── migration/                  # Source and original-to-filtered commit maps
├── docs/                           # Contracts, provenance, roadmap
├── tools/                          # Core source-export tooling
├── .github/workflows/
└── CMakeLists.txt

rkalos/
├── R/                              # Public API, ts conversion, result methods
│   └── RcppExports.R               # Generated bindings
├── src/
│   ├── kalos/                      # Generated, pinned core-source snapshot
│   ├── rcpp_*.cpp                  # Adapted existing binding implementations
│   ├── lapack_r.cpp                # R-configured BLAS/LAPACK implementation
│   ├── RcppExports.cpp
│   ├── Makevars.in
│   └── Makevars.win.in
├── inst/tinytest/                  # Migrated native R tests
├── tests/tinytest.R
├── man/
├── vignettes/                      # Getting started and migration guide
├── tools/sync-core.py
├── core.lock.json
├── configure
├── configure.win
├── cleanup
├── cleanup.win
├── .github/workflows/
├── DESCRIPTION
└── NAMESPACE

pykalos/
├── src/pykalos/                    # Public API and Python conversions
├── bindings/                      # pybind11 bindings
├── vendor/kalos/                   # Generated, pinned core-source snapshot
├── tests/
├── docs/
├── tools/sync-core.py
├── core.lock.json
├── .github/workflows/
├── CMakeLists.txt
└── pyproject.toml
```

Each repository receives its own README, changelog, license notices, and
development instructions. The core owns the numerical roadmap and contracts;
binding documentation explains its supported public surface and embedded version.

## Core and numerical-provider boundary

Move the numerical sources and standalone tests into `kalos`, rename the C++
namespace to `kalos`, and export a CMake target `kalos::kalos`. Maintain C++17,
owned storage, column-major matrices, explicit missing masks, interruption
callbacks, and current numerical/resource limits. Separate public headers from
implementation details. Do not promise a stable binary ABI in the initial series.

Replace direct `R_ext/Lapack.h` usage with a small internal BLAS/LAPACK interface.
Cover the complete routine inventory at the frozen split commit, including the
Cholesky, QR, SVD, eigenvalue, Householder, and matrix-product calls. Link exactly
one provider implementation into each core consumer:

- Standalone C++ and Python use the OpenBLAS CBLAS/LAPACKE provider.
- `rkalos` supplies an adapter using R's declared Fortran interfaces and
  `$(LAPACK_LIBS) $(BLAS_LIBS) $(FLIBS)`.

Retain algorithms, validation, workspace handling and failure semantics during
this refactor. Verify that core headers and numerical implementation files build
without R/Rcpp headers or an R runtime. External-library experiments from the
evaluation document are separate changes and are not prerequisites for extraction.

Move shared model preparation from the native R helpers into C++: integer period
alignment, inclusive estimation windows, fresh-prior semantics, regressor domains,
fixed/free coefficient handling, mean/drift construction, and likelihood
adjustments. Preserve parameter-map/objective/optimizer, transfer-diagnostic and
observed-information contracts. R/Python layers retain conversion, naming,
serialization and display.

## OpenBLAS build and distribution policy

The selected baseline is [OpenBLAS 0.3.34](https://github.com/OpenMathLib/OpenBLAS/releases/tag/v0.3.34).
It supplies BLAS, CBLAS, LAPACK and LAPACKE; enable all four in bundled builds.
Use **32-bit BLAS/LAPACK integers** on 64-bit hosts, matching the current checked
interfaces, and reject a mismatched integer ABI at configuration time.

Pin the source version and archive checksum with build settings and notices.
Build LAPACK from its Fortran sources rather than selecting the older C-translated
fallback. Enable runtime CPU dispatch with a conservative baseline per target
architecture; produce separate x86-64 and ARM64 artifacts. Release builds must
not inherit a build host's unsupported instruction set through `-march=native`.
See the upstream [build options](https://github.com/OpenMathLib/OpenBLAS/blob/v0.3.34/Makefile.rule)
and [installation guidance](https://github.com/OpenMathLib/OpenBLAS/blob/develop/docs/install.md).

Standalone and Python source builds discover a preinstalled compatible OpenBLAS
with the required interfaces. Configuration errors identify missing headers,
symbols, or an incompatible integer ABI. Wheel CI builds the pinned provider and
bundles required runtime libraries and license notices. Give the bundled library
private names/symbols so importing NumPy or another numerical extension does not
redirect Kalos calls to a different provider. Verify loading in both import orders.

Initially build the private wheel provider single-threaded with concurrent-call
locking (`USE_THREAD=0`, `USE_LOCKING=1`). Retain Kalos's optional OpenMP regions.
This is an initial threading policy to validate, not a claim of optimal speed.
Externally supplied OpenBLAS and R providers retain their configured thread policy;
Kalos must not change process-global BLAS settings behind other packages' backs.

Keep optional OpenMP detection and a genuine serial fallback. Interpreter calls,
interrupt checks and the currently sequential LAPACK/objective operations stay
on the calling thread. Rename core controls to the Kalos vocabulary and retain
`PERSEPHONE_NUM_THREADS` as a documented compatibility alias during migration;
an explicit `KALOS_NUM_THREADS` setting takes precedence.

## R and Python interfaces

Retain **Rcpp and tinytest**. Build the R package with R's standard toolchain and
the existing C++17/OpenMP compile-link-runtime probe. Enumerate nested core objects
explicitly in `Makevars`; copying sources into a subdirectory alone is not a build
rule. Keep R-specific headers and generated registration code outside the core
snapshot. R linking follows [Writing R Extensions](https://cran.r-project.org/doc/manuals/r-release/R-exts.html#Using-Makevars).

Build Python with **pybind11 and scikit-build-core**. Copy/convert inputs into
owned C++ buffers initially; zero-copy work is a separate optimization. Release
the GIL during computation and reacquire it for calling-thread signal checks.
No Python/R API runs inside OpenMP workers. See the
[build guidance](https://scikit-build-core.readthedocs.io/en/latest/guide/getting_started.html)
and [GIL guidance](https://pybind11.readthedocs.io/en/stable/advanced/misc.html).

The initial documented surface in both packages includes:

- `capabilities()` and `core_version()`, including embedded core revision and
  numerical-provider/build metadata.
- `prepare_inputs()`, `fixed_likelihood()`, and `regression_likelihood()` with
  the meanings and result fields of the existing native helpers.
- `arma_objective_setup()`, `arma_objective()`, `arma_optimize()`, and
  `arma_identification()` for the delivered bounded fixed-order estimation path.
  Preserve the current preparation/map conventions and caller-supplied starts.
- `arma_information()` for the delivered observed-information methods, preserving
  the explicit `profile_ml` and `conditional_residual_df` conventions and all
  unavailable-inference statuses.

Keep low-level bindings available internally for tests. Additional inference
methods require their own qualification; an optimizer success flag does not
make covariance or forecasts available.

R initially accepts monthly/quarterly `ts` objects and named regressor series.
Python provides `RegularSeries(values, start=(year, period), frequency=...)` using
NumPy arrays, and named regressor series. R result series retain `ts` metadata;
Python results use arrays with explicit domain metadata. Map response missingness
and fixed/free coefficient states to the existing core contracts. Preserve
unavailable-result statuses rather than manufacturing successful values.

The first packages are experimental engine interfaces. Capabilities must
distinguish fixed-order estimation from seasonal-adjustment fitting. Constructors
such as `seas()`/`x11()` and automatic selection are deferred to their actual
implementation milestones. Keep small print/summary methods in `rkalos`; leave
Persephone's extensive visualization and workflow dependencies in Persephone.

## Source synchronization and validation ownership

Use generated, committed source snapshots rather than a floating core checkout.
Each binding's `core.lock.json` records the repository, exact commit, source
manifest/checksums, and embedded version. `tools/sync-core.py` exports an explicit
source allowlist from that commit, preserves notices, and provides a verification
mode that detects missing, changed, or unexpected vendored files. Development
may use a local core checkout only through an explicit override recorded in build
metadata. Release artifacts must match their lock.

The R source package and Python source distribution contain all their core sources;
installation does not fetch the core from GitHub. Python source builds still need
the declared OpenBLAS development dependency. Binding releases evolve independently
and always report their embedded core revision.

Move native mathematical contracts, C++ tests, numerical reference tooling and
its relevant immutable fixtures to the core repository. Move native R binding
tests to `rkalos`; add Python boundary and cross-language conformance tests in
`pykalos`. Keep Persephone workflow/integration tests in Persephone.

Preserve historical reference bytes, generation manifests, hashes and tolerance
tables. Relocation must not be presented as regeneration. Adapt tooling that
currently assumes `DESCRIPTION`, `R/persephone.R`, or the `persephone3` namespace
through new entry points and an explicit frozen output-request inventory.
Legacy workflow fixtures still needed by Persephone remain available there as
unchanged historical artifacts. Add language-neutral conformance cases separately,
with their own provenance; do not replace the original RDS corpus.

## Migration sequence

1. **Freeze and inventory the source.** Record a reviewed split commit, relevant
   source paths, test results and fixture hashes. Account for all modified and
   untracked native work. Preserve unfinished work on a dedicated checkpoint
   branch for later migration, rather than dropping it or labelling it delivered.
   Keep an immutable pre-split baseline for comparison and rollback.
2. **Extract history in fresh clones.** Use `git filter-repo` with explicit path
   lists for the core and R binding/test histories. Include the R helper history
   needed to trace preparation ports. Preserve authorship, messages, original-to-
   filtered commit maps and old/new path mappings. Keep the source repository's
   history intact. Start the Python binding history with recorded core provenance.
3. **Make separately reviewable changes.** Separate extraction/renaming, the
   BLAS/LAPACK provider boundary, shared-preparation ports, and Python bindings.
   Keep the original implementation usable until equivalence checks pass.
4. **Validate artifacts and interfaces.** Build the standalone library and both
   language packages from their actual archives. Run native, cross-language,
   reference-integrity, platform and packaging checks, plus baseline timings.
5. **Connect Persephone.** Replace its embedded numerical implementation with an
   required `rkalos` dependency, remove its Rcpp build dependency, and delegate
   native capability reporting. Preserve public R6 behavior, specification/result
   envelopes, persistent backend selection and the `rjd3` default. Missing optional
   packages and unsupported native seasonal-adjustment requests remain explicit.
6. **Publish the validated repositories.** Publish the three public repositories
   and prepare the Persephone integration change for review. Carry forward the
   existing provenance/release gates. CRAN/PyPI submission and merging into the
   original default branch are subsequent release/integration steps.

## Acceptance and performance checks

- Run inherited C++ tests with strict warnings, ASan/UBSan where supported,
  genuine serial builds, actual OpenMP workers, and cancellation/reuse coverage.
  Standalone and Python builds must succeed without R or Java installed.
- Run migrated tinytest suites and R CMD check on Linux, macOS and Windows,
  including Linux R-devel and minimum-version checks. Retain the no-Java lane.
- Build/install Python source distributions and wheels on the three target
  operating systems; verify runtime library loading, NumPy coexistence, malformed
  arrays, missingness, exception translation and cancellation.
- Compare R and Python on shared inputs for monthly/quarterly domains, windows,
  transformations, fixed/free coefficients, mean/drift, structural redundancy,
  failed/unresolved likelihoods, optimizer result semantics, and the delivered
  observed-information methods, numerical gates and unavailable covariance.
- Preserve original reference hashes and tolerances. Retain independent Gaussian
  identities and pinned JD+ comparisons. Keep R/Java reference tooling in a
  separate validation job from the standalone core's build requirements.
- Use the existing fitted-case matrix and matched API timing as the baseline.
  Measure kernels and complete likelihood/fit calls, including setup and repeated
  independent series. Record CPU, compiler, provider, integer ABI, threading,
  evaluation counts, failures and timing distributions. Validate small-matrix
  threading overhead; do not infer end-to-end speed from large GEMM benchmarks.
- Test Persephone with `rkalos` installed and absent, preserving its existing
  backend, batch, hierarchy, plotting and dashboard behavior.
- Verify filtered-history mappings, source locks, source-package inclusion and
  documentation links. Mark the split complete only when the published source
  and tested artifacts can be traced to the validated commits.

Defaults remain **C++17**, **R >= 4.1**, and **Python >= 3.11**. Qualify the minimum
and current stable interpreter versions. Shared calendars, AT/EU presets,
additional time-series adapters, automatic model selection, X-11/TRAMO/SEATS,
and registry publication retain their separate roadmap milestones.
