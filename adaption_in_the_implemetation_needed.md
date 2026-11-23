# Kalos implementation changes needed

Reviewed 2026-11-23 at `0e064dd`. Kalos owns the C++ numerics, provider boundary, numerical contracts, and canonical fixtures. `rkalos` and `pykalos` consume generated snapshots. Make core changes here and refresh the binding pins after the core checks pass.

## P1 — Fix the standalone build on libc++

A clean serial build with Homebrew Clang 23.1.5 and OpenBLAS 0.3.34 failed on macOS ARM64. The installed SEATS model header uses `std::exception_ptr` and `std::rethrow_exception` without including `<exception>`. `decompose_ucarima.cpp` and `seats_extract_wk.cpp` also use `std::current_exception`. After supplying `<exception>`, the compiler found a second missing include for `std::string` in the TRAMO differencing battery. A scratch build and install succeeded with both headers force-included. [SEATS model header](include/kalos/detail/seats_ucarima_model.hpp#L180) · [Ucarima decomposition](src/decompose_ucarima.cpp#L83) · [WK extraction](src/seats_extract_wk.cpp#L708) · [TRAMO differencing](src/tramo_differencing_battery.cpp#L11)

Add direct standard-library includes in the files that use them. Compile the affected installed headers in isolation and build a clean macOS CMake consumer with `find_package(kalos CONFIG REQUIRED)`. Run the Ucarima, SEATS extraction, and TRAMO differencing tests with the qualified provider.

## P1 — Validate OpenBLAS selection on reconfigure and downstream install

After a successful configure with Homebrew OpenBLAS, changing `OpenBLAS_ROOT` in the same build directory to a nonexistent directory still succeeded. CMake retained both original `KalosOpenBLAS_*` cache paths, and the cached ABI probe stayed green. The current probe includes the LAPACKE header but only links a call to `openblas_get_config()`. An installed consumer runs the finder without this probe. A library with missing LAPACKE exports can therefore be accepted until a dependent target links or loads. [Provider finder](cmake/FindKalosOpenBLAS.cmake#L12) · [Configure probe](CMakeLists.txt#L7) · [Installed package configuration](cmake/kalosConfig.cmake.in#L1) · [Provider calls](backends/openblas.cpp#L18)

Invalidate auto-discovered paths and probe results when the provider selection changes. Validate any explicit path overrides against the documented precedence, including after a root change. Put the LP64 and CBLAS/LAPACKE link check on the shared finder path so builds and installed consumers get the same check. Cover a valid root, a changed root, a missing root, and a provider with headers but missing required entry points. Once released, migrate the Python package off its separate finder where the shared one meets the same contract.

## P1 — Bound automatic X-13 horizons before estimation

By code inspection, the automatic driver computes `-horizon * 12` in `int` during downstream composition. A negative annual count can overflow on negation or multiplication. The expanded length also feeds vector allocations and downstream index arithmetic. The current entry checks validate the series shape and feature envelope before starting estimation, while horizon normalization happens after it. [Composition](src/x13_automatic.cpp#L1906) · [Request checks](src/x13_automatic.cpp#L2080) · [Horizon contract](docs/contracts/x13-automatic-pipeline-contract.md#L227)

Normalize horizons in a wider signed type at the entry point. Check both normalized counts and their sum with the observed length against downstream index and allocation limits before doing model work. Pass the checked counts into composition and document the refusal. Preserve the month-count and annual-count equivalents already covered by the contract. Add boundary tests that fail before allocation or estimation, plus an unchanged replay of the existing horizon fixtures.

## P2 — Update the current-scope documentation at the same time as the code

The README still describes X-11 through initial additive tables and automatic selection as open. The automatic-fit header describes zero horizons in two places even though the request fields and current contract describe the 21-H.4 extension. Put the current qualified entry points and limitations in the README and header, with links to the detailed contracts. Keep the earlier implementation-status entries as dated history. Identify the TRAMO witness requirements and the remaining TRAMO-SEATS assembly step when describing the newer core surface. [README](README.md#L1) · [Automatic request header](include/kalos/x13_automatic.hpp#L19) · [TRAMO fit contract](docs/contracts/tramo-fit-contract.md)

## Review checks

The export manifest and historical migration audit passed. The clean build and provider-cache issue were reproduced locally. The scratch build used force-included headers. The complete CTest suite and the horizon-limit cases were outside this review run.
