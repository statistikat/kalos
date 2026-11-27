# Kalos implementation changes needed

Reviewed 2026-11-23 on macOS ARM64, based on `ce7da0d`.
Refreshed 2026-11-27 from Linux qualification lanes; current main `d1db8ae`. All five
review items below remain fixed and certified through every subsequent re-pin; the
historical verification section is retained as recorded.

## Status refresh (2026-11-27)

Since the review: the numbered roadmap program (items 15–28 plus item 29) closed, and the
M9–M12 speed program landed three core slices, each qualified against the unchanged
JVM-pinned budgets: **M10-D** pinned-LM fixed-block eligibility (airline 2.4–3.4×),
**M11** vendored SLICOT SB03MD stationary init behind the m ≥ 8 gate (init ÷12 at m=14;
fit matrix −17.7%), and **M12** the certified post-pivot diffuse-filter path (2.2–2.7×
per evaluation, per-step slope 3.67×, route observable via `post_pivot_*` fields).
Matched reference state: RSA0 pipeline 2.3–3.1× ahead of JDemetra+ (bit-exact), flagship
seasonal-ML evaluation 8.5× behind JVM against a measured contract floor of ≈5.5–6.1×.
Current suite state: ctest 97/97 in Release-OpenMP, serial, and sanitizer lanes
(98/98 as of the same-day M13 failure-likelihood pin, item 3 below); manifest
219 core / 112 fixtures / 472 evidence; migration audit 406 files. All external-library
candidates carry dispositions in [EXTERNAL_LIBRARY_EVALUATION.md](EXTERNAL_LIBRARY_EVALUATION.md).

## Remaining items (2026-11-27)

1. **Item 30** (roadmap): k = 0 stationary factorization routing — the worst JVM ratio in
   the frozen corpus (66.7× at m=14); probe-first per its own text.
2. **Arithmetic-contract follow-ups** named in the M12 ledger (per-step QR cost,
   accumulation precision) — the only route below the ≈5.5–6.1× contract floor; not yet
   opened as roadmap items.
3. **Stale-likelihood leak**: `initial_unavailable` estimation results return the
   process's last-computed log-likelihood instead of a missing value (pristine
   reproducible; reproducer `drivers/llrepro.cpp` in the M9-H lane scratch). Small fix +
   pinning test, independent of performance work. — **Fixed 2026-11-27**: the two
   no-candidate returns in the estimation entry points now write the documented
   missing value (quiet NaN); the re-derived repro (the leaked number matched the
   prior fit's bit-for-bit) and the pinning test `test_estimation_failure_likelihood`
   (suite 97→98) are recorded as the M13 entry in
   [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md).
4. **Ledger notes awaiting decision** (M7): D8-panel pseudo-additive zero-composition
   publication (pre-existing, pinned, fail-soft-handled) and RSA2c estimated-route TD
   capture drift (no parity claim made).
5. **Platform qualification** (evaluation protocol step 6): hosted Linux/macOS/Windows
   matrices with a Fortran-capable toolchain for the vendored SLICOT closure; MSVC
   platforms run the qualified `KALOS_WITH_SLICOT=OFF` QR-mode build until the COFF mixed
   ABI is qualified.
6. **Information contract**: TRAMO covariance and joint regression uncertainty remain
   outside the supported surface.

## P1 — Fix the standalone build on libc++

**Fixed.** The SEATS model header and its callers include `<exception>`, and the TRAMO battery includes `<string>`. The standalone tests include the declarations for their C library calls. A clean serial Release build succeeded with Clang 23.1.5 and libc++. An installed consumer compiled the SEATS header first, linked against `kalos::kalos`, and ran. [Installed consumer test](tests/check_installed_provider.py)

## P1 — Validate OpenBLAS selection on reconfigure and downstream install

**Fixed.** The build and installed package both use the same finder. It searches the selected root afresh, validates explicit overrides against that root and link-checks the actual OpenBLAS, CBLAS and LAPACKE entry points with 32-bit integer headers. The downstream regression reconfigures one build directory across two valid roots, a missing root and an incomplete library, then restores the valid configuration. [Provider finder](cmake/FindKalosOpenBLAS.cmake) · [CI regression](tests/check_installed_provider.py)

## P1 — Bound automatic X-13 horizons before estimation

**Fixed.** The automatic entry point converts signed annual counts in 64-bit arithmetic and bounds the combined observed, backcast and forecast length before estimation. Composition receives the checked month counts. The refusal tests cover minimum signed input, annual multiplication overflow and combined-length overflow. The existing automatic horizon oracle passed. [Implementation](src/x13_automatic.cpp) · [Boundary tests](tests/test_x13_automatic.cpp)

## P2 — Update the current-scope documentation at the same time as the code

**Fixed.** The README and public automatic-fit header now describe the fixed and automatic X-13 routes, horizon support, and TRAMO’s witness inputs. End-to-end TRAMO-SEATS assembly remains on the roadmap. [README](README.md) · [Automatic-fit header](include/kalos/x13_automatic.hpp) · [Horizon contract](docs/contracts/x13-automatic-pipeline-contract.md)

## Verification of the current fixes

Before the subsequent ARMA panel fix, clean builds with Homebrew OpenBLAS 0.3.34 and with the checksum-pinned OpenBLAS 0.3.34 each passed **78 of 79 CTests**, including Ucarima, SEATS extraction, TRAMO differencing and both automatic X-13 oracles. The export manifest, historical migration audit, installed header and provider-selection regressions passed. The remaining failure was the exact BIC comparison in `test_arma_refinement`. A separate build of the pre-change revision `ce7da0d`, using the missing standard headers as compiler includes and checksum-pinned OpenBLAS 0.3.34, reproduced the same assertion.

The follow-up fixed the one-ULP white-noise BIC difference by evaluating the
candidate with the pinned JVM arithmetic. The original exact assertion now
passes with both providers. The full serial suite on the checksum-pinned provider passed **79/79 CTests**. [ARMA identification contract](docs/contracts/initial-arma-identification-contract.md)

## Original review checks at `0e064dd`

The export manifest and historical migration audit passed. The clean build and provider-cache issue were reproduced locally. The scratch build used force-included headers. The complete CTest suite and the horizon-limit cases were outside this review run.
