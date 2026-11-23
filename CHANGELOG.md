# Changelog

## Unreleased

- Automatic TRAMO replay can build the final fitted model, residuals,
  information criteria and regression covariance with no final-record input.

- Added the public RSA0 residual Doornik–Hansen normality test.

- The monthly automatic TRAMO replay now computes its round diagnostics
  and regression-test decisions from the fitted likelihood. It reports the
  fixed-column regression covariance in the original variable order.

- Added the stock TRfull initial mean, leap-year and Easter decisions on level or log monthly data.
- Added the monthly TRAMO airline F-test for working-day and trading-day effects.

- Added the public monthly RSA0 residual Ljung–Box diagnostic with its
  zero-mean convention and paired JVM evidence.

- Added SEATS seasonal AR-to-MA re-estimation for the fitted mixed AO/LS model.
- Added the monthly SEATS Legacy airline approximation, with JVM-verified
  seasonal MA thresholds and component extraction after a successful retry.

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

- Initialized empty regression designs in the warm SARIMA optimizer and
  qualified the two explicit maximum-likelihood TRAMO scans.

- Added monthly observation input to the TRAMO final estimator and checked
  its decisions and fitted coefficients at 16 real kernel calls.

- Added the dated RSA0 preprocessing residuals and residual standard error.

- Reproduced automatic TRAMO final estimation with dated outlier regressors,
  including all 45 final-estimator calls on the fitting panel.

- Computed monthly TR0 forecast uncertainty directly from its finite MA state.

- Ran automatic TRAMO differencing with fitted outlier effects and preserved
  the airline model on the short-series fallback.

- Qualified stationary monthly AR(1)/AR(2) TRAMO outlier scans with a mean.

## 0.1.0 — initial repository split

Preserved the delivered Kalos numerical scope through 22b.2.
See IMPLEMENTATION_STATUS.md for repository-specific work and qualification.
