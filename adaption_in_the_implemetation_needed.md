# Kalos implementation changes needed

Reviewed 2026-11-23 on macOS ARM64, based on `ce7da0d`.

## P1 — Fix the standalone build on libc++

**Fixed.** The SEATS model header and its callers include `<exception>`, and the TRAMO battery includes `<string>`. The standalone tests include the declarations for their C library calls. A clean serial Release build succeeded with Clang 23.1.5 and libc++. An installed consumer compiled the SEATS header first, linked against `kalos::kalos`, and ran. [Installed consumer test](tests/check_installed_provider.py)

## P1 — Validate OpenBLAS selection on reconfigure and downstream install

**Fixed.** The build and installed package both use the same finder. It searches the selected root afresh, validates explicit overrides against that root and link-checks the actual OpenBLAS, CBLAS and LAPACKE entry points with 32-bit integer headers. The downstream regression reconfigures one build directory across two valid roots, a missing root and an incomplete library, then restores the valid configuration. [Provider finder](cmake/FindKalosOpenBLAS.cmake) · [CI regression](tests/check_installed_provider.py)

## P1 — Bound automatic X-13 horizons before estimation

**Fixed.** The automatic entry point converts signed annual counts in 64-bit arithmetic and bounds the combined observed, backcast and forecast length before estimation. Composition receives the checked month counts. The refusal tests cover minimum signed input, annual multiplication overflow and combined-length overflow. The existing automatic horizon oracle passed. [Implementation](src/x13_automatic.cpp) · [Boundary tests](tests/test_x13_automatic.cpp)

## P2 — Update the current-scope documentation at the same time as the code

**Fixed.** The README and public automatic-fit header now describe the fixed and automatic X-13 routes, horizon support, and TRAMO’s witness inputs. End-to-end TRAMO-SEATS assembly remains on the roadmap. [README](README.md) · [Automatic-fit header](include/kalos/x13_automatic.hpp) · [Horizon contract](docs/contracts/x13-automatic-pipeline-contract.md)

## Verification of the current fixes

Clean builds with Homebrew OpenBLAS 0.3.34 and with the checksum-pinned OpenBLAS 0.3.34 each passed **78 of 79 CTests**, including Ucarima, SEATS extraction, TRAMO differencing and both automatic X-13 oracles. The export manifest, historical migration audit, installed header and provider-selection regressions passed. The remaining failure was the exact BIC comparison in `test_arma_refinement`. A separate build of the pre-change revision `ce7da0d`, using the missing standard headers as compiler includes and checksum-pinned OpenBLAS 0.3.34, reproduced the same assertion.

## Original review checks at `0e064dd`

The export manifest and historical migration audit passed. The clean build and provider-cache issue were reproduced locally. The scratch build used force-included headers. The complete CTest suite and the horizon-limit cases were outside this review run.
