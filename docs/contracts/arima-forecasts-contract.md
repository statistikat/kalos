# Exact ARIMA forecasts/backcasts (21-H.4)

```cpp
namespace kalos {
struct ArimaForecastModel { int period, p, d, q, bp, bd, bq;
    std::vector<double> phi, bphi, theta, btheta; bool mean_correction; };
struct ArimaForecastsResult { std::string status, message;
    std::vector<double> backcasts, forecasts; double mean; };
ArimaForecastsResult arima_forecasts(const ArimaForecastModel&,
    const std::vector<double>& series,
    std::size_t backcast_horizon, std::size_t forecast_horizon);
}
```

This unit is the faithful port of the pinned JVM consumer stack behind
`X13Kernel.initialStep`: `ExactArimaForecasts` running a `CkmsFilter` with
the diffuse fast initializer (`CkmsDiffuseInitializer` wrapping the
`ArimaInitialization` `OrdinaryFilter$Initializer`) over the landing
SarimaModel point — unit innovation variance, unit-root polynomials kept in
the AR, coefficients on the JVM polynomial surface (1 + c1·B + ...; the
pipeline contract's export sign convention applies at the call site). The
series is the preadjusted linearized data on the estimation calendar (the
`r = v30 − v27` alignment of the pinning notes); the `mean_correction` flag
mirrors `isMeanCorrection` (a mean/TrendConstant variable in the landing
model): one regular difference is folded out of the filtered model (the AR
unit roots and the MA polynomial are multiplied by 1−B) and the bar-
polynomial mean of the final state is reported.

Pinned machine (decode of the 3.9.0 jars, pinning notes
`kalos-21h4-pinning.md` sections A–J):

* **Stationary initialization** (`d + bd == 0`): M = acf(dim) with the
  model autocovariances, F = acf[0], l = T·M; filtering starts at row 0.
* **Diffuse initialization** (`d + bd > 0`, the corpus cases): P =
  sigma(dif)·stVar(stpsi, stacf, var)·sigma(dif)ᵀ (the sigma matrix is the
  outer factor; P[0][0] equals stacf[0]), F = P[0][0], M = P.col(0),
  l = T·M, W = −F·(T·P·Tᵀ + V − P − l·lᵀ/F) with V = var·psi·psiᵀ (not
  diagonal), l = W.col(k)/sqrt(W[k][k]) for the largest positive diagonal
  entry (a rejected W — max|element| ≤ 1e-6 — zeroes l), and filtering
  starts at row start = dim − 1; the diffuse rows are never re-filtered.
* **Per step** (`process`): v = y[i] − a[0] (complete data; Z = e0),
  update a += (v/F)·M, then `next`: while not steady, with
  zx = l[0], if |zx| > neps (= eps·F, eps = 1e-15, F fixed at initialize):
  k = zx/F, F -= zx·k and the symmetric deflate L[t] -= M[t]·k,
  M[t] -= L[t]·k; when |l|₂ ≤ eps the steady position is recorded (the
  l-deflate stops and l keeps evolving by T only); l ← T·l and a ← T·a
  always.
* **Output**: `backcasts` ascending to the first observation, then
  `forecasts` after the last one (the JVM full/back_to_original split is
  done by the callers, not here); `mean` is the correction mean.

Acceptance tiers (measured, never bit-claimed along the trajectory):

* Unit (`tests/test_arima_forecasts.cpp`, values embedded from the JVM
  probes of the pinning notes): MA(1)-bit-exact state and forecasts, the
  airline-case forecasts within 3.3e-13 relative and the final-state
  trajectory within the pinned probe values; the diffuse INIT state (F, M,
  l, a) reproduces the JVM `CkmsDiffuseInitializer` post-initialize state
  bit-for-bit modulo 1–2 ulp.
* Integration (`tests/test_x13_automatic_horizon.cpp` and the horizon
  captures): the route input is the native banded landing point, so the
  a1a/out tail inherits the oracle test's landing-coefficient basin band
  (~1e-6..1e-7 relative); `a1` itself — the estimate-domain correction the
  landing point also feeds — stays BIT-exact. A model the steady-state
  initializer rejects answers `failed:arima_forecasts` (the JVM "Invalid
  fast filter" SsfException); zero/negative horizon requests return empty
  vectors (the JVM never constructs the filter for them).
