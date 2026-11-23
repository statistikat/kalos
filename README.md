# Kalos

Experimental C++17 time-series engine extracted from Persephone. The core has no
R, Rcpp, Python or Java dependency.

Qualified surface (each unit is pinned by a dated contract under
[docs/contracts/](docs/contracts/); `IMPLEMENTATION_STATUS.md` keeps the dated
history):

* Fixed-order SARIMA estimation with the qualified inference/prediction modes.
* Complete X-11 seasonal adjustment through the full selection/processing chain
  and the decoder-shaped composition `kalos::x13_fit` (fixed spec).
* Automatic-spec X-13 `kalos::x13_automatic_fit` (21-H): the pinned automatic
  decision chain with arbitrary integer X-11 backcast/forecast horizons - the
  21-H.4 semantics (month counts; a negative value means |h| annual
  frequencies), normalized in int64 at the entry and bounded there by a
  2^24-month extended-series cap: beyond it the driver answers the pinned
  refusal statuses before any estimation work (pipeline contract, 2026-11-23
  entry-horizon-bound appendix). Calendar, easter, automatic-trading-day and
  user-variable detection are fail-closed (21-I).
* The TRAMO estimation pipeline `kalos::tramo_fit` (23-F): the live
  transformation/differencing/ARMA-selection/outlier/regression-test/kernel-
  round/final-estimation chain of the pinned jar. Witness tiers per the
  tramo-fit contract: decisions and seam facts are bit claims, landing
  quantities carry measured bands. The TRAMO-SEATS assembly (`TramoSeatsFit`)
  is NOT part of this surface - the SEATS side exists as separate qualified
  units (24-B UCARIMA decomposition, 24-C WK extraction) and the assembly
  stays open (24-D).
* Calendar-detection machines (`kalos::calendar_detection`, 21-I.3), calendar
  generation, the SEATS WK extraction machine and the UCARIMA decomposition
  seam.

## Build

Install an LP64 OpenBLAS with CBLAS, LAPACK and LAPACKE (baseline 0.3.34), then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOpenBLAS_ROOT=/path/to/openblas
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/install
```

`find_package(KalosOpenBLAS)` is the single provider validation point
(REVIEW-1.C): it checks the LP64 integer ABI and links the CBLAS and LAPACKE
entry points against the selected library; an explicit `OpenBLAS_ROOT` is
exclusive and the selection is revalidated whenever it changes inside an
existing build directory. Installed consumers get the identical check through
`kalosConfig` (`find_dependency(KalosOpenBLAS REQUIRED)`), and the
`test_cmake_provider_cases` ctest gates the matrix. `kalos_standalone_headers`
(also ctest-registered) compiles every public header as an include-only TU -
the libc++ portability guard (REVIEW-1.A). Consumers use
`find_package(kalos CONFIG REQUIRED)` and link `kalos::kalos`.
Use `-DKALOS_OPENMP=OFF` for a serial build; OpenMP is otherwise detected optionally.
`KALOS_NUM_THREADS` selects 1–64 owned worker threads (default 2).
`PERSEPHONE_NUM_THREADS` is a compatibility alias; an explicit Kalos setting wins.
No process-wide BLAS/OpenMP policy is changed. Configure the external provider's
thread count separately to avoid oversubscription.

Numerical sources use an internal nine-routine provider boundary. Standalone and
Python link OpenBLAS; rkalos supplies R's configured BLAS/LAPACK implementation.
The provider is selected at build time, never switched dynamically.

See [implementation status](IMPLEMENTATION_STATUS.md), [contracts](docs/contracts/),
and [original provenance](validation/reference/NATIVE_PROVENANCE.md).
This initial series does not promise a stable binary ABI.

On macOS, compiler and OpenBLAS OpenMP runtimes must be the same library.
Configuration runs a joint provider/OpenMP probe and selects serial fallback if
it fails. With Homebrew LLVM and OpenBLAS, use
`-DOpenMP_omp_LIBRARY=/opt/homebrew/opt/libomp/lib/libomp.dylib` (adjust the prefix
for Intel Macs). Never bypass duplicate-runtime errors with environment overrides.
