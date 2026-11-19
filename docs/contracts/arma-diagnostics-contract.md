# Automatic-model diagnostics (21-H slice 1)

Unit-qualified transcription of the diagnostics layer of
`jdplus.x13.base.core.x13.regarima.ModelController.accept` for the automatic
model controller: the `AutoCovariances.autoCorrelationFunction` acf, the
`LjungBox(seq, false)` statistic with its `TestsUtility.testOf(statistic,
Chi2, Upper)` p-value (over the [special functions
machine](special-functions-contract.md)), the private `calcLbLength` /
`calcResStat` rules and the accept composition on top of the shared modified
Ljung-Box filter (`src/mlj_engine.hpp`, the 21-C-qualified ML-residual
machine shared verbatim with `src/outlier_scan_engine.cpp`). Bytecode source:
pinned javap dumps of the jdplus 3.9.0 jars (`v390_ModelController`,
`v390_LjungBox`, `AutoCovariances`, `Chi2`, `TestsUtility` under
`/tmp/opencode/jdprobe/item21h/` at slice time); this document and the
goldens are the durable record.

```cpp
auto lb = kalos::ljung_box_test(seq, k, nhp);      // statistic + Upper p
int  k  = kalos::calc_lb_length(freq, neff);
auto ok = kalos::model_controller_accept(req);     // accept(rv|rt|lb) machine
```

Public surface: `include/kalos/arma_diagnostics.hpp` (`auto_correlation`,
`calc_lb_length`, `LjungBoxTest`, `ljung_box_test`, `calc_res_stat_rtval`,
`ModelControllerRequest`, `ModelControllerResult`, `model_controller_accept`).

## The machines

- `auto_correlation(seq, mean, lag)` — the all-finite branch:
  `var = (Σ (x - mean)²) / n`; under `1e-38` (dcmpg: NaN variance takes the
  compute branch and yields NaN) the result is 0; otherwise
  `acc / n / var` with `acc = Σ_{t<n-lag} (x[t] - mean)(x[t+lag] - mean)`
  and +0.0 for `n - lag <= 0`. The pinned divisor is the full length `n`
  (a biased covariance ratio), **not** `n - lag` — a misreading corrected by
  the bit-exact goldens.
- `ljung_box_test(seq, k, nhp)` — `calcLb` semantics: mean-correct (the JVM
  mutates the caller's block via `tmp.sub(mean)`; here a copy), then
  `LjungBox(seq, false)` (`acf(·, 0.0)`, `n = length`, `lag = 1`, `sign = 0`):
  `value = Σ_{i=1..k} r²/(n - i)`; `statistic = value * n * (n + 2)`;
  `df = (double)(k - nhp)` (int subtraction first). `df > 0` fails ⇒
  `built = false` (the JVM `Chi2` constructor `IllegalArgumentException`,
  which `calcLb` catches as `RuntimeException → false`); `p` is the
  `chi2_upper_p(df, statistic)` surface (NaN where the JVM's pvalue catch
  fires).
- `calc_lb_length(freq, neff)` — 12 → 24, 1 → 8, else `4 * freq` with the
  `neff <= 22 && freq == 4 → 6` override, then `k >= neff ⇒ k = neff / 2`
  (integer division).
- `calc_res_stat_rtval(seq)` — `mean / sqrt(var / n)` with `mean = sum / n`,
  `var = ssq / n - mean * mean`; `var / n` under the root.
- `model_controller_accept(request)` — the accept machine on the differenced
  linear-model residuals: expand the SARIMA polynomials
  (`mlj::expand_sarima_polynomials`, the 21-C `getStationaryRepresentation`
  transcription incl. the trailing-zero strip), build
  `ModifiedLjungBox(ar, ma, acvf, psi, m)`, `apply` to get `m + q` ML
  residuals; `rvr = sqrt(ssq(all) / neff)` **before** the drop; keep the
  last `neff` (`tmp.drop(n - neff, 0)`), `rtval = calcResStat(tail)`,
  `k = calcLBLength(annualFrequency, neff)`, `nhp = freeParametersCount`,
  and `accepted = lb.built && !((1 - p) > lbLimit)` — a NaN p-value compares
  false, so the JVM accepts in that case and so does this transcription.
  The JVM keeps `lbtest` / `rvr` / `rtval` as fields for the ODM's
  `plbox0 = 1 - p` and the final mean rule; `ModelControllerResult` exposes
  the same values.

## Qualification

- Capture: `validation/reference/LjungBoxGolden.java` (package-placed to
  reach `LjungBox`; reflection onto the private `calcLbLength` /
  `calcResStat` / `getRTval` of the pinned `ModelController`),
  intrinsic-disabled JVM; nine exact-recurrence input series (no
  transcendentals, replayed bit-identically from the golden table), golden
  data `validation/reference/arma_diagnostics_golden.txt` (1637 rows +
  series table), `tools/gen_arma_diagnostics_golden.py` →
  `tests/jvm_arma_diagnostics.h`.
- `tests/test_arma_diagnostics.cpp`: bit-exact replay of all rows (acf 498
  across both mean variants, lb statistic and p-value 232, calcLbLength
  840-row full grid, rtval 9) plus the 58 `df <= 0` throw rows and the
  identity-MLJ accept wiring check (rvr/rtval/lb/accepted recomposed from
  the pinned scalar machines, zero-coefficient seasonal blocks expanding
  identically, and the `neff > n` fail-closed envelope). Zero failures, OMP
  and serial lanes, ASan/UBSan clean.
- The ML-residual filter itself (`Acvf`, `MaLjungBox`, `ModifiedLjungBox`)
  is qualified by the 21-C corpus ([outlier scoring
  contract](outlier-scoring-contract.md)); the extraction into
  `src/mlj_engine.hpp` is a pure move and the 21-C sweep is unchanged
  (98/98).

## Envelope

Residual sequences finite and non-empty; `neff` in `[1, m + q]`; the sign
filter stays at its default 0 (the usePositive/NegativeAutoCorrelations
surface is unreachable in this path); annual frequency any int the decoder
emits; the non-finite branch of `autoCorrelationFunction` (with-missing
divisor bookkeeping) is not transcribed — inputs with NaN/Inf throw.
`accept()` itself additionally needs the estimation state (`neff`,
free-parameter count), which the slice-2 round driver supplies; nothing here
touches the estimation machine.
