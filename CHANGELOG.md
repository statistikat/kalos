# Changelog

## Unreleased

- Added dated, structured initial and bias-corrected SEATS component series
  to the Wiener–Kolmogorov extractor for M5 pipeline assembly.
- Corrected the white-noise ARMA candidate's BIC on macOS ARM64 by using
  the same pinned likelihood as the reference panel.
- Froze five fitted-model reference handoffs from TRAMO to SEATS and added
  a two-JVM replay against the pinned JDemetra jars.
- Added the monthly, fitted TRAMO-to-SEATS model handoff for the five
  qualified AO/LS and no-effect reference cases.

## 0.1.0 — initial repository split

Preserved the delivered Kalos numerical scope through 22b.2.
See IMPLEMENTATION_STATUS.md for repository-specific work and qualification.
