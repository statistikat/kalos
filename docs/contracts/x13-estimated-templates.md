# Fixed-structure estimated-coefficient X-13 templates

Roadmap item 28 covers the stock monthly RSA0, RSA1 and RSA2c presets.
The opening reference uses the three dated input series and pinned JDemetra+
3.9.0 jars recorded in `validation/evidence/native/x13-refusal-jvm-profiles`.
The complete public output is in `validation/evidence/native/x13-estimated-templates/templates-x86_64.txt`.

## Decision ledger

All nine models have SARIMA orders (0,1,1)(0,1,1) and estimate both moving
average coefficients without a mean. Their requested seasonal filter is MSR,
their requested Henderson length is zero and their requested forecast horizon
is minus one. The resulting forecasts span twelve months. The table records
the selected scale, estimated regressors, final Henderson length and seasonal
filter. The D7 and final Henderson lengths agree on this panel.

| Template | Input | Transformation | Estimated regressors | Henderson length | Seasonal filter |
| --- | --- | --- | --- | ---: | --- |
| RSA0 | air | Level | None | 13 | 3×3 |
| RSA0 | ao | Level | None | 13 | 3×5 |
| RSA0 | seas | Level | None | 13 | 3×9 |
| RSA1 | air | Log | None | 9 | 3×3 |
| RSA1 | ao | Log | AO, January 1994 | 13 | 3×5 |
| RSA1 | seas | Level | None | 13 | 3×9 |
| RSA2c | air | Log | Working days, Easter, AO in May 1951 | 9 | 3×3 |
| RSA2c | ao | Log | AO, January 1994 | 13 | 3×5 |
| RSA2c | seas | Level | None | 13 | 3×9 |

## Replay and numerical checks

`validation/reference/capture-x13-refusal-profiles.py --estimated-templates --portable-x86`
takes the same `--jdk` and repeatable `--jar-dir` arguments as the historical
refusal-demand replay. It verifies the input and jar hashes, runs two fresh
Temurin 21.0.11 JVMs and compares each complete capture byte for byte. The
capture SHA-256 is `42be69e16cc2329ed1a7932843e4fa9121497ee384d9b0231e3e22c06560f6a3`.

The numerical checks cover the fitted regression and SARIMA coefficients,
likelihood, 891 residuals, 108 original-scale forecast values and 8,928
extended-domain X-11 values across B1, B7, C7, D7 and D10–D13.
The public final SA, trend and irregular series cover 3,024 observed values,
and the SA and trend forecasts cover a further 216 values. Dates, row
presence, model orders, variables and selector decisions are exact checks.
The existing X-13 table tolerances apply when qualifying a native product.
Each template requires its own native regression and binding qualification.

## Fitted X-11 selection gate

`--estimated-selection --portable-x86` captures the 36 I/C ratios and
Henderson choices and nine MSR decisions after the public X-13 fit.
The capture digest is `ecf3db42c189abaccc71742db4d393533a3e39fc04832a2080933fc660a33261`.
The native regression starts with each public extended B1. It requires bitwise
agreement on those decisions and all 7,812 B7, C7, D7 and D10–D13 values.

## Native RSA0 entry

`x13_estimated_rsa0(observations, year, month)` accepts a complete monthly
series and returns the fitted coefficients, X-11 selections, extended
tables, observed final products and one year of forecasts. It uses the
stock legacy sigma policy. Its fixed-order estimator uses the same final
estimation routine exercised by the automatic model's final cascade.

## RSA2c calendar comparison

The monthly `x13_rsa2c_calendar_test` accepts dated raw observations and the
already selected transformation. As in the stock kernel, the working-day
test considers two models. On the log scale its alternative adjusts February
for leap years before taking logs. On the level scale it includes a separate
leap-year regressor. The chosen branch is the baseline for three Easter fits
of 1, 8 and 15 days. A strict improvement in AICc selects an alternative.
The shared monthly airline optimizer estimates each distinct candidate.
The selected model-scale series, dated design and forward/reverse leap
factors are available for the later outlier and forecast stages.

## Native RSA1 entry

`x13_estimated_rsa1(observations, year, month)` selects log or level with
the stock AICc correction of −2 and scans for additive outliers, level
shifts and transitory changes on the selected scale. Its airline orders
stay fixed. It estimates the final model and returns the same X-11 product
and one-year extension as RSA0. On the three reference series, all six
moving-average coefficients, the selected transformation, the dated outlier
and both Henderson and MSR decisions match the public JDemetra+ fits.

The stock result includes the concentrated log likelihood, effective
observation count, estimated parameter count, residual sum of squares and
full model residuals. The residual calendar starts after regular and seasonal
differencing (13 observations for an airline model). Without regressors the
public model returns the concentrated likelihood innovations. With estimated
regressors it removes their fitted effects from the transformed levels,
differences the adjusted series and filters it through the fitted ARMA.

## RSA2c stock product

`x13_estimated_rsa2c(observations, year, month)` runs the raw scale and
calendar comparisons, outlier detection, calendar significance retest, final
estimation and X-11. Its `stock_calendar` contains the typed selection and
the names and coefficients of the calendar variables in fitted order. The
`variables` vector holds the subsequent outliers. `stock_likelihood` holds
the complete stationary residuals, residual sum of squares and model parameter
count. X-11 returns the same fitted and one-year forecast fields as RSA0
and RSA1, including the separate MSR and Henderson decisions.
