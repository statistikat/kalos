# X-13 deterministic-effect allocation staging (22c, slice M3c)

```cpp
kalos::X13Preadjustment pre = kalos::x13_preadjust(in);   // initialStep seam
kalos::X13Finals fin = kalos::x13_restore(ri);            // finals seam
```

This slice delivers the **deterministic-effect allocation staging** of the
pinned X-13 pipeline — the preadjustment (`X13Kernel.initialStep`) that
produces the series fed to X-11 and the restore (`X13Kernel.finals`) that
returns the deterministic effects — decoded from the pinned rjd3 3.9.0
bytecode and qualified bit-exactly against the pinned kernel on
`validation/evidence/native/x13-allocation`. The pinned kernel offers **no
fixed-coefficient entry point**: every `X13Kernel` run estimates the ARIMA
part and the missing values, so the qualified contract is the exact seam the
capture proves — given the complete input series and, as **inputs**, the
pinned model-description variable order with each variable's pinned
regression columns and its coefficient values, the staging is pure algebra
and this header reproduces it. Everything upstream of the seam (coefficient
and ARIMA estimation, outlier detection, trading-day tests) and everything
the `LengthOfPeriodType.None` staging never reaches (missing-row
interpolation, the length-of-period transform, benchmarking, diagnostics) is
outside this contract; the ARIMA backcast/forecast extension that a nonzero
X-11 horizon triggers was delivered here with item 21-H.4 (the
`X13PreadjustInput.backcast/forecast/arima` and
`X13RestoreInput.backcast/forecast` fields, qualified by
[arima-forecasts-contract.md](arima-forecasts-contract.md) and the automatic
horizon captures). See [x13-allocation-scope-22c.md](x13-allocation-scope-22c.md) for
the scoping memo.

## Capture and evidence

`validation/reference/capture-x13-allocation.R` compiles the independently
authored observer `X13AllocationProbe.java` against the pinned jars and runs
`X13Kernel.of(spec, ModellingContext.getActiveContext()).process(ts,
DefaultProcessingLog)` per case on fully specified fixed-coefficient models
(zero ARIMA-differencing coefficients still run the kernel's estimation
machinery, but every captured table depends only on the input, the pinned
regression columns and the given coefficients). The observer recomposes the
stored tables from the pinned public pieces — `GeneralLinearModel.
deterministicEffect`/`backTransform`, `RegArimaDecomposer.deterministicEffect`,
the `TsData` folds and `X11Utility.correctSeries` in the exact forms
transcribed from the bytecode — and compares **22 tables per case
bit-exactly** (a1..a9ser, out against the run's `B1`, d11final/d12final/
d13final/d16/d18/e1/e2/e3/e11); the capture refuses to write on any
disagreement, on any removed variable, or on a mode-forcing violation. The
seven cases pin the additive model with the full variable mix, the empty
model, the forced-additive Multiplicative request, the log model forced to
Multiplicative, the log-additive and pseudo-additive finals, and the monthly
frequency; 154 recomposition checks passed and the CSV corpus is deterministic
across reruns (all values written at `%.17g` and byte-identical between
independent captures).

## Bucket model (initialStep)

Each model variable carries its pinned class (`ModellingUtility` predicates,
mutually exclusive for spec-built models) and the EXACT `regeffect`
attribute (`SaVariable.isRegressionEffect`: attribute equals the
`ComponentType` name; an **absent** attribute is the Undefined bucket):

| class | buckets filled |
|---|---|
| `moving_holiday` | `a7` — regeffect ignored |
| `days_related` | `a6` — regeffect ignored |
| `outlier` | `a8t`/`a8s`/`a8i` on the exact regeffect `Trend`/`Seasonal`/`Irregular`; **any other attribute (e.g. the captured Undefined outlier) contributes to no table** |
| `user` | `a8t`/`a8s`/`a8i` (Trend/Seasonal/Irregular) plus `a9sa`/`a9ser`/`a9cal`/`a9u` (SeasonallyAdjusted/Series/CalendarEffect/Undefined) |

Accumulation follows the pinned `GeneralLinearModel.deterministicEffect`:
a zero column accumulated **in model-description order**, per regression
column `DataBlock.addAY(coef, column)` with the pinned rule (`coef == 0`
skips the column entirely — its values are never read; `coef == 1` adds it
undamped; otherwise `x[i] = x[i] + coef*column[i]` with plain multiply then
add, no fma). The bucket sums fold left-associatively per row in the pinned
order: `a8t=(v13+v16)`, `a8=((a8t+a8s)+a8i)`, `a9=((a9cal+a9sa)+a9ser)`,
`v26=(((a6+a7)+a8)+a9)`; `a9u` joins neither `a9`, `v26`, `out` nor the
finals (it only feeds the unreachable ARIMA route). Every stored table is
`backTransform`'ed — under a log model the exponential of the log-space
bucket (`lengthOfPeriod` is `None`, so the boolean flag is a no-op) — while
`out = input − backTransform(v26)` (additive) or `input / backTransform(v26)`
(divide iff `isLogTransformation`, independent of the decomposition mode:
the log-additive capture subtracts nothing here because its input scale is
the level domain). `a1` is the interpolated series, which under the complete
input of this contract equals the input **bit-exactly** (the log model does
not pass `a1` through log/exp). Under a log model the exponentials are a
transcendental boundary against `Math.exp` (1-ulp hits); the fixture
qualifies them at relative 1e-15, everything else bit-exactly.

`X13Kernel.updateSpec` forces the X-11 mode: a non-log model is forced
`Additive`; a log model becomes `Multiplicative` unless `LogAdditive` or
`PseudoAdditive` was requested (`PseudoAdditive` is never forced); the
horizons arrive through the request and drive the 21-H.4 extension route.
The rule is asserted from the case
metadata in the test and sweep.

## Restore (finals)

With `a6`..`a9sa` the **stored** tables and `d10..d13`/`c17` the tables as
`X11Results` stores them for the run on `out` (mode = forced mode):

```
d18      = invOp(invOp(a6,  a7),    a9cal)
d16      = invOp(invOp(d10, a8s),   d18)
d11final = invOp(invOp(invOp(d11, a8t), a8i), a9sa)
d12final = invOp(d12, a8t);  d13final = invOp(d13, a8i)
v28  = op(a1, a8i);  v24b = op(d11final, a8i)
e1   = correct(v28,  c17, op(a1, d13final))
e2   = correct(v24b, c17, d12)
e3   = correct(d13,  c17, mean)          // the RAW d13, scalar mean
e11  = correct(v24b, c17, invOp(d12, op(v28, e1)))
```

with the pinned truth table (verified against `DecompositionMode.
isMultiplicative() == (Multiplicative || LogAdditive)` and pinned empirically
by the log-additive fixture):

| mode | `op` | `invOp` | `mean` |
|---|---|---|---|
| additive | − | + | 0 |
| multiplicative | / | × | 1 |
| log_additive | − | × | 1 |
| pseudo_additive | / | × | 1 |

(`op` tests `mode == Multiplicative || mode == PseudoAdditive` explicitly,
so log-additive **subtracts**; `invOp` tests `isMultiplicative() || mode ==
PseudoAdditive`, so log-additive **multiplies**.) `correct` is the pinned
`X11Utility.correctSeries`: a copy of `x` whose entry `i` takes `y[i]`
(yielded exactly where `w[i] == 0` — negative zero included, NaN weights
kept; probe-verified against the pinned method). The `fitToDomain` calls of
the pinned finals are the identity under the shared-calendar contract.

## Results tables the restore consumes

`X13RestoreInput` consumes `d10..d13`/`c17` **as the pinned `X11Results`
stores them** — for additive, multiplicative and pseudo-additive runs that is
the raw step pipeline (`X11Kernel.finalResults` applies no bias correction
outside log-additive; bit-exact in the captures). Under log-additive the
results-builder stage (`finalResults` → `ratioBiasCorrection`, the
`X11Spec` default bias) rescales the exp'd step tables: with
`A=exp(d10step)`, `B=exp(d12step)`, `C=exp(d13step)`, `v9=average(A)` and
`v13=v9*average(C)` (the range spans the whole series for a series covering
whole years, as every case does), the stored tables are `D10 = A/v9`,
`D12 = makePositivity(B·v13)` (the identity on the all-positive captured
table), `D11 = out/D10` and `D13 = (out/D10)/D12`. That stage belongs to the
M3b-i export scope and is not delivered here; the test and the sweep tool
reconstruct it from the qualified log-domain pass tables and compare at the
transcendental boundary (worst relative deviation 1.88e-16), then feed the
reconstruction to `x13_restore` end to end.

## Fail-closed inputs

`x13_preadjust` throws `std::invalid_argument` on an empty series, a missing
(non-finite) observation, a non-positive observation under a log model, a
variable without one coefficient per column, a column not spanning the input
calendar, or a non-finite coefficient. `x13_restore` throws unless the
preadjustment is `available` and every supplied table shares its length.
Non-positive series values under log modes are inputs the pinned `check()`
refuses upstream; they are not reachable here because the input of a log
model must be positive. Missing inputs, non-zero horizons and
`lengthOfPeriod != None` have no representation in this surface — the
scoping memo lists them as the unsupported set that later 22c slices must
pin before extending it.

## Qualification

* `test_x13_allocation.cpp` (ctest `#47`) embeds all seven cases verbatim at
  17 significant digits and proves per case: the mode-forcing rule; the
  native preadjust against the stored a-tables and `out` (bit-exact; under a
  log model the exp'd buckets at 1e-15); the native selection pass over the
  preadjusted series against the captured `D10..D13`/`C17` (bit-exact; under
  log-additive through the reconstructed results-builder stage at 1e-12);
  and the native restore against the captured finals over both the captured
  and the native X-11 tables — 252 table checks per thread leg at pinned
  thread settings 2, 1 and 8. The fail-closed shapes, the empty-model rule
  (all-zero buckets, `out == a1`), the `addAY` damping rules (zero-coefficient
  NaN columns skipped) and the log back-transform then run through the
  public surface. 756 pinned table checks pass in every leg.
* `tools/verify_x13_allocation.cpp` sweeps the corpus from the CSVs:
  **280 checks, 0 failures**, identical at `KALOS_NUM_THREADS` 1, 2 and 8.
* Full suites: Release OpenMP build **47/47**, true serial build (`KALOS_OPENMP=OFF`)
  **47/47**, ASan/UBSan Debug build **47/47**; the unit and the sweep run
  diagnostic-free under gcc 13.3 `-fsanitize=address,undefined` with leak
  detection.

Rules and semantics: this file (the pinned-flow transcription is mirrored
here); scope: [x13-allocation-scope-22c.md](x13-allocation-scope-22c.md).
