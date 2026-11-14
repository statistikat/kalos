# kalos implementation status

Updated: 2026-11-14. Repository extraction in progress.

## Completed

- Preserved relevant Git history from Persephone commit `8450d6ec8c205f52715fa36d8c6c6d0826e78c99`.
- Recorded original-to-filtered commit mappings and extraction path rules.
- Adopted the `kalos` C++ namespace and include paths; numerical algorithms are unchanged.
- Preserved all original reference fixtures byte for byte.

## Next steps

1. Separate R and OpenBLAS providers and qualify the standalone core.
2. Move shared preparation into C++ and connect the lean R package.
3. Pin generated core snapshots, add minimal Python bindings, validate artifacts.
4. Connect Persephone through a **required** `rkalos` dependency and publish the repositories.

The inherited numerical implementation extends through fixed additive initial X-11 tables
(22b.2). Full native seasonal-adjustment fitting remains unavailable. The historical
baseline and numerical contracts belong to Kalos; R boundary tests belong to rkalos.

## Provider boundary completed

- Added R-free interfaces for DGELSY, DGEQRF, DGESVD, DPOCON, DPOTRF, DPOTRS,
  DSYEV, DLARFG and DGEMM, preserving existing workspace and failure semantics.
- Standalone CMake target `kalos::kalos` links OpenBLAS CBLAS/LAPACKE with a
  checked 32-bit integer ABI. R's provider remains in the binding repository.
- Added optional OpenMP, install/export rules, sanitizer configuration and the
  Kalos thread setting with tested legacy-alias precedence.
- Validation: Clang 23.1.5, OpenBLAS 0.3.34, macOS ARM64 Release serial build;
  **36/36 standalone test programs passed**. No R includes or runtime linked.
  OpenMP, sanitizer and installed-consumer checks follow the preparation port.

## Shared preparation completed

Added the language-independent preparation contract and independent C++ tests.
All **37/37** standalone test programs pass with the OpenBLAS serial build.
R conversions now delegate these operations; package qualification is next.

## Pinned source export

Added an explicit committed source/fixture allowlist and a Git-archive exporter.
Bindings record the exact core commit, version and SHA-256 of every snapshot file.
Verification is offline and rejects changed, missing, extra or symlinked files.
The generated revision header is declared separately; original source/fixture
bytes remain unchanged. Refresh refuses to discard local snapshot edits.
