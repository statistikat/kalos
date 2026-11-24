# Combined SI seasonality

The all-years `diagnostics.seas-si-combined` getter reports `Present`, `ProbablyNone` or `None`. The native result also returns the stable-seasonality F statistic, moving-seasonality F statistic and Kruskal–Wallis statistic, each with its upper-tail probability. The verdict follows the pinned combined test and its stable and moving test thresholds of 0.001 and 0.05.

`combined_si_seasonality` accepts a complete finite monthly or quarterly SI series on a one-based calendar. Fewer than three years of observations returns an empty optional. Stable seasonality and the rank statistic use the entire series. Moving seasonality uses complete calendar years, with absolute deviations from zero in additive mode and from one in multiplicative mode.

`seats_si_seasonality` uses final observed WK seasonal and irregular components on their common domain. It adds them for level fits and multiplies them for log fits. With one component available, the public operation uses that component directly. With neither component available the result is empty.

`x13_si_seasonality` reads the public D8 table over the observation interval, excluding backcasts and forecasts. Log-additive D8 is exponentiated on export. The combined test treats log-additive and multiplicative modes as multiplicative, and pseudo-additive and additive modes as additive.

The two-JVM fixtures cover short and off-cycle series, tied observations, all verdicts, and public X-13 and TRAMO product getters. [Numerical and product qualification](../../tests/test_combined_seasonality.cpp)

## Residual seasonality of the adjusted series

`monthly_adjusted_seasonality` computes the public all-years monthly Qs
and seasonal F tests on the model-scale adjusted observations. The fitted
preprocessing model supplies the ordinary difference count `d + bd` and
its mean flag. X-13 uses the observed portion of public D11, logged in
multiplicative and log-additive modes. TRAMO-SEATS uses the stochastic
adjusted series from the initial Wiener–Kolmogorov extraction.

Qs uses positive seasonal autocorrelations at lags 12 and 24. The F test
regresses the stationary response on a constant, one lag and eleven
seasonal contrasts. Within the observed panel, fewer than 14 stationary
observations give an absent F test. At exactly 14 the saturated model
returns undefined statistics. The results expose each test independently.
