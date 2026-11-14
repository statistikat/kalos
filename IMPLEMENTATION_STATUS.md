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
