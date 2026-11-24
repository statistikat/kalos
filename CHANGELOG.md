# Changelog

## Unreleased

- Run the stock monthly TR3 reference-model retries, threshold reduction
  and last-chance fits before the seasonal and differencing controllers.

- Stabilize higher-degree SARIMA polynomials with the pinned QR root solver
  and support the stationary outlier-scan orders selected on later TR3 passes.

- Re-estimate the selected monthly TR3 model after the fast regression test
  removes the mean, preserving its automatic outliers.

- Support stationary monthly seasonal ARMA(1,1) outlier scans with regular
  AR(2) or ARMA(1,1) and an estimated mean.
- Monthly TRAMO can refit a selected calendar and outlier specification and restore its calendar effects to SEATS components and forecasts.

- Refit raw monthly TR3 and RSA3 after a late seasonality restart.

- Added raw monthly TR3 preprocessing and RSA3 seasonal adjustment for fits
  accepted by the first automatic comparison and controller sweep.

- Apply the monthly automatic TRAMO controllers in order, including selected
  candidate transfers and the initial seasonality restart.
- The late working-days controller can redetect AO, LS and TC variables when comparing monthly airline fits.

- Monthly TRAMO outlier scans now account for calendar regressors in selection and re-estimation.

- Build the complete monthly seasonality-controller diagnostic panel from
  linearized data or fitted residuals, including autoregressive spectral peaks.

- Compute monthly Tukey spectral-peak significance for TRAMO controllers.

- Added the monthly Qs and Friedman tests used on TRAMO fitted residuals
  and linearized data in the late seasonality controllers.

- Compared the first automatic TRAMO candidate with the independent
  reference fit and corrected seasonal AR indexing at model reversion.

- Qualified monthly TR1 residual standard error, Ljung–Box and Doornik–Hansen statistics.

- TRAMO computes its first automatic candidate from dated data through
  differencing, ARMA selection, renewed outlier detection and estimation.

- The monthly TRAMO loop builds its first reference fit from the dated
  observations, including its automatic outliers and mean-test refits.

- Automatic TRAMO computes the initial mean decision from the post-transform
  model at the kernel's intermediate precision.

- Added the raw monthly RSA1 seasonal-adjustment pipeline and the SEATS
  model-change indicator. Six public cases reproduce all 3,960 product values.

- Corrected the cosine used in the SEATS spectrum minimum search to reproduce
  the pinned x86-64 HotSpot kernel on monthly level-shift fits.

- Added monthly TR1 preprocessing from dated observations, with automatic
  log/level selection and outlier detection on the fixed-order airline model.

- The monthly TRAMO seasonal F test now refits the series with dated regressors and eleven monthly contrasts.

- Automatic TRAMO replay can build the final fitted model, residuals,
  information criteria and regression covariance with no final-record input.

- Added the public RSA0 residual Doornik–Hansen normality test.

- The monthly automatic TRAMO replay now computes its round diagnostics
  and regression-test decisions from the fitted likelihood. It reports the
  fixed-column regression covariance in the original variable order.

- Added Fast regression retesting of automatically identified TD, LP and Easter effects at their fitted positions.
- Added monthly TD2 candidate estimation, mean retesting and model selection for the late TRAMO controller with outlier detection disabled.
- Added monthly TRAMO model comparison diagnostics for skewness and stability of the residual mean and variance.
- Added the late TRfull residual working-day F test on the fitted monthly domain.
- Added the stock TRfull initial mean, leap-year and Easter decisions on level or log monthly data.
- Reduced CPU time for monthly fixed-order SARIMA fits by skipping zero
  transition entries when propagating the finite state factor.

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
