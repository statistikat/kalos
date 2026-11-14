# Kalos

Experimental C++17 time-series engine extracted from Persephone. The core has no
R, Rcpp, Python or Java dependency. It supports fixed-order SARIMA estimation,
qualified inference/prediction modes and X-11 primitives through initial additive
tables. Full seasonal-adjustment fitting and automatic model selection remain open.

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

Numerical sources use an internal nine-routine provider boundary. Standalone and
Python link OpenBLAS; rkalos supplies R's configured BLAS/LAPACK implementation.
The provider is selected at build time, never switched dynamically.

See [implementation status](IMPLEMENTATION_STATUS.md), [contracts](docs/contracts/),
and [original provenance](validation/reference/NATIVE_PROVENANCE.md).
This initial series does not promise a stable binary ABI.
