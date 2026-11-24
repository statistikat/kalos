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
