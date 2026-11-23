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

- Added the fitted monthly TRAMO-SEATS component pipeline with checked
  restoration of deterministic effects on observed, forecast and backcast domains.

- Added raw monthly TR0 estimation and the corresponding RSA0 product
  component pipeline, verified against the public reference kernels.

- Added an independent RSA0 original-series forecast standard-error projection
  using the preprocessing model’s public reporting scales.

## 0.1.0 — initial repository split

Preserved the delivered Kalos numerical scope through 22b.2.
See IMPLEMENTATION_STATUS.md for repository-specific work and qualification.
