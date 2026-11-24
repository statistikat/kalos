# Kalos

Experimental C++17 time-series engine extracted from Persephone. The core has no
R, Rcpp, Python or Java dependency. It supports fixed-order SARIMA estimation,
qualified inference and prediction, and the X-11 selection passes and finals.
The [restricted fixed X-13 route](docs/contracts/x13-fit-contract.md) uses fixed coefficients and zero horizons.
[Automatic monthly X-13](docs/contracts/x13-automatic-pipeline-contract.md)
supports backcasts and forecasts and reports unsupported detection options explicitly.
[Monthly TRAMO](docs/contracts/tramo-fit-contract.md) requires kernel facts,
seam witnesses and a final record for automatic identification. Its decomposable fitted models can feed a
[monthly WK component pipeline](docs/contracts/tramo-seats-bridge-contract.md)
that restores deterministic effects to the final series. The monthly RSA0
template also runs directly from the original series via `run_tramo_seats_rsa0`.

## Build

Install an LP64 OpenBLAS with CBLAS, LAPACK and LAPACKE (baseline 0.3.34), then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOpenBLAS_ROOT=/path/to/openblas
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/install
```

Consumers use `find_package(kalos CONFIG REQUIRED)` and link `kalos::kalos`.
Use `-DKALOS_OPENMP=OFF` for a serial build; OpenMP is otherwise detected optionally.
`KALOS_NUM_THREADS` selects 1–64 owned worker threads (default 2).
`PERSEPHONE_NUM_THREADS` is a compatibility alias; an explicit Kalos setting wins.
No process-wide BLAS/OpenMP policy is changed. Configure the external provider's
thread count separately to avoid oversubscription.

Numerical sources use an internal nine-routine provider boundary. The standalone core and
the Python and R bindings link the OpenBLAS C API.
The provider is selected at build time, never switched dynamically.

See [implementation status](IMPLEMENTATION_STATUS.md), [contracts](docs/contracts/),
and [original provenance](validation/reference/NATIVE_PROVENANCE.md).
This initial series does not promise a stable binary ABI.

On macOS, compiler and OpenBLAS OpenMP runtimes must be the same library.
Configuration runs a joint provider/OpenMP probe and selects serial fallback if
it fails. With Homebrew LLVM and OpenBLAS, use
`-DOpenMP_omp_LIBRARY=/opt/homebrew/opt/libomp/lib/libomp.dylib` (adjust the prefix
for Intel Macs). Never bypass duplicate-runtime errors with environment overrides.
