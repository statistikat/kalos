# Special functions and Chi-square p-values (21-H slice 1)

Unit-qualified transcription of `jdplus.toolkit.base.core.dstats.
SpecialFunctions` (logGamma / IGSeries / IGContFract / incGamma / chiSquare)
and of the `Chi2.getProbability(x, Upper)` surface consumed by
`TestsUtility.pvalue` — the p-value machine behind every Ljung-Box acceptance
decision in the automatic model controller ([arma-diagnostics
contract](arma-diagnostics-contract.md)). Bytecode source: `javap -c -p` of
the pinned jdplus 3.9.0 runtime jars (the local `rjd3jars` set, jdplus
3.9.0), captured under `/tmp/opencode/jdprobe/item21h/` at slice time; this
document and the goldens below are the durable record.

```cpp
auto p = kalos::detail::chi2_upper_p(df, stat);      // NaN on JVM throw surface
auto g = kalos::detail::special_inc_gamma(a, x);     // nullopt on JVM throw
```

Public surface: `include/kalos/detail/special_functions.hpp`
(`special_log_gamma`, `special_ig_series`, `special_ig_cont_fract`,
`special_inc_gamma`, `chi2_upper_p`); implementation `src/special_functions.cpp`
over the 21-D-qualified fdlibm kernels (`include/kalos/detail/fdlibm.hpp`).

## The machines

- `logGamma(a)` (guard `!(a > 0)` throws): `t = a + 5.5`;
  `series = (a + 0.5) * log(t) - t`; `tmp = 1.000000000190015` plus six
  Lanczos terms `c_gammaconst[j] / (a + j + 1)` with
  `c_gammaconst = {76.18009172947146, -86.50532032941678, 24.01409824083091,
  -1.231739572450155, 0.001208650973866179, -5.395239384953E-6}`;
  `series + (log(tmp) + log(2.5066282746310007)) - log(a)` in that order.
  (An earlier session note recorded a G1..G5 polynomial variant; that is not
  this runtime's method — the bytecode above and the goldens settle it.)
- `IGSeries(a, x)` (guard `a <= 0 || x < 0` throws, `dcmpg` so NaN slips the
  guards and meets the iteration throw): `del = sum = 1/a`; 1000-round loop
  `ap += 1; del = del * (x / ap); sum += del;` returning
  `sum * exp(((a * log x) - x) - logGamma(a))` once
  `|del| < |sum| * 1e-16`; the throw maps onto `nullopt`.
- `IGContFract(a, x)`: modified Lentz with `tiny = 1e-65`,
  `b = (x + 1) - a`, `c = 1/tiny`, `d = 1/b`, `h = d`; per round
  `ana = (-i) * (i - a)`, `b += 2`, `d = ana*d + b` (floored), `c = b + ana/c`
  (floored), `d = 1/d`, `del = d*c`, `h *= del`, convergence at
  `|del - 1| <= 1e-16` — where a NaN `del` keeps turning (the JVM `dcmpg`
  compares NaN as greater), returning `exp((a*log x - x) - logGamma(a)) * h`.
- `incGamma(a, x)`: series branch for `x < a + 1`, `1 - IGContFract`
  otherwise; NaN takes the continued-fraction branch.
- `chiSquare(x, df)` guards `x < 0` only (`dcmpg ifge`: NaN passes) and
  returns `incGamma(df / 2, x / 2)`; `Chi2.getProbability(x, Upper)` returns
  `1 - that`. `TestsUtility.pvalue` catches every exception onto NaN, which
  is `chi2_upper_p`'s nullopt surface. The `Chi2` constructor (`df > 0` else
  `IllegalArgumentException`) is the caller's concern, reproduced in
  `ljung_box_test` (see the [arma-diagnostics
  contract](arma-diagnostics-contract.md)).

## Qualification

- Capture: `validation/reference/SpecialFunctionsGolden.java` (same-package
  access to the package-private `IGSeries` / `IGContFract`), intrinsic-disabled
  JVM (`-XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog`, the corpus
  convention); golden data `validation/reference/special_functions_golden.txt`
  (2261 rows, raw-bits format `tag aHex bHex vHex`), replayable
  `tools/gen_special_golden_header.py` → `tests/jvm_special_functions.h`.
- `tests/test_special_functions.cpp`: bit-exact replay of all 2261 rows
  (loggamma 37, igseries 400, igcf 400 of which the 46 divergent-`x = 0`
  throw rows, incgamma 400, chi2upper 1024) plus the guard-surface probes
  (logGamma(0/-1/NaN), `chi2_upper_p(df, -x | NaN) = NaN`,
  `chi2_upper_p(df, 0) = 1`). Zero failures, OMP and serial lanes, ASan/UBSan
  clean.
- The intrinsic-off capture flag matters: 608 of the 2261 rows change when
  the JIT `StubRoutines` `dexp`/`dlog` stubs are enabled; the intrinsic-on
  variant is *not* the qualified surface.

## Envelope

Every argument combination the Ljung-Box chain reaches is inside the golden
grid's shape (a = df/2 in (0, 20.5], x = stat/2 in [0, 150], plus the guard
probes). Inputs outside the captured shapes still execute the literal
machine — the transcription is total — but only the grid is JVM-pinned.
