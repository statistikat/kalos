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
- Fixed duplicate symbols when linking both UCARIMA decomposition and
  Wiener–Kolmogorov extraction in one downstream binary.

- Reject SEATS horizons whose padded monthly domain exceeds the signed
  index range before allocating forecasts.

## 0.1.0 — initial repository split

Preserved the delivered Kalos numerical scope through 22b.2.
See IMPLEMENTATION_STATUS.md for repository-specific work and qualification.
