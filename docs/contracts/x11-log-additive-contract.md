# Log-additive decomposition mode (22b.5 M3b-i)

```cpp
kalos::X11BPass b = kalos::x11_b_pass(series, 12, /*first_period=*/1,
    initial, final, /*trend_length=*/13, kalos::X11SigmaPolicy::legacy,
    1.5, 2.5, kalos::X11DecompositionMode::log_additive);
kalos::X11CPass c = kalos::x11_c_pass(series, 12, 1, initial, final, 13,
    kalos::X11SigmaPolicy::legacy, b);            // mode inherited from b
kalos::X11SelectionPass r = kalos::x11_selection_pass(series, 12, 1,
    requests, 0, kalos::X11SigmaPolicy::legacy,
    kalos::X11MsrIterationPolicy::jdplus, 1.5, 2.5, 0, 0,
    kalos::X11DecompositionMode::log_additive);
```

This slice delivers the **log-additive decomposition mode** of the pinned
X-11 pipeline (`jdplus.x13.base.core.x11.X11{B,C,D}Step` and `X11Kernel`
under `X11Context` mode `LogAdditive`, pinned rjd3 3.9.0) across the item
22b.4 S1–S4 fixed and selection passes. The delivered surface is exactly
the item 22b.4/22b.5 surface with `mode = log_additive`: the last optional
argument of every pass entry point, echoed on `X11BPass::mode` and
inherited by the C/D/selection passes.

**Delivered semantics.** `X11Kernel.process` applies `DoubleSeq.log()` to
the input *before* constructing the plain (non-pseudo) step classes; the
decoded truth table `isMultiplicative() == (mode == Multiplicative || mode
== PseudoAdditive)` makes LogAdditive **not** multiplicative, so every
decoded step site — `X11Context.remove`, the sigma measurement centre, the
correction-factor form and mode default, the `MsrTable.of` relative flag
and the `makePositivity` guards — keeps its **additive** form and runs on
the transformed sequence. The delivered tables are therefore the
step-getter **log-domain** tables of that pipeline: `B1` is the natural
log of the input, every difference identity (`B6 = B1 - B5`, `C1 = Y -
B20`, `D13 = D11 - D12`, …) holds over the log-domain tables, and no trend
table is positivity-repaired (no `makePositivity` site is gated on
LogAdditive). `DoubleSeq.log()` is `fn(Math::log)` (bytecode-verified: the
bootstrap method targets `java/lang/Math.log:(D)D`); a probe over all
6,701 fixture input values confirmed Java `Math.log` is bit-identical to
C `log` on this machine, which is what makes the bit-exact qualification
below possible.

## Pinned mode-driven behaviour (bytecode-verified against rjd3 3.9.0)

* **`X11Kernel.check()`** (`detail::check_decomposition`): the positivity
  requirement is gated on `mode == Multiplicative || mode == LogAdditive`
  and fails with the pinned **constant message**
  `"multiplicative decomposition of non positive series"` for *both* gated
  modes (the string says "multiplicative" even in log-additive mode —
  pinned verbatim). The gate runs on the raw present rows before any
  transform; flagged-missing rows are skipped at the fixed-pass level and
  the selection pass keeps its separate completeness rejection.
* **`X11Kernel.process` input transform** (`detail::log_domain`): the
  plain additive steps run on `fn(Math::log)` of the input. Each pass
  entry applies the same transform to its own series parameter after the
  raw-value gate (the C/D B-pass and C-pass input echo checks compare
  against the *transformed* predecessor tables), and the selection pass's
  manual C-stage composition consumes the transformed series; masked rows
  keep their finite placeholders — only present rows are transformed and
  the internal passes read flagged rows as NaN exactly like the pinned
  NaN-carrying sequence (a masked placeholder never reaches `log`).
* **Additive-form context** (verified unchanged from item 22b.4):
  `X11Context.remove` stays the difference at every site; the sigma
  weights measure against the centre 0; the B20/C20 correction default and
  the dummy-filter default are 0; `X11Utility.defaultMsrTable` takes the
  additive branch (`obs = x − mov`, `MsrTable.of` relative flag `false`)
  and `MsrFilterSelection` passes its strict `mode == Multiplicative`
  flag — so the whole MSRE/MSR apparatus is the absolute form;
  `AutomaticHenderson.calcICR` uses the difference remove with absolute
  lag-1 variations (`detail::pinned_ic_ratio`, mult flag `false`).
* **No `makePositivity` sites**: all four decoded guards test
  `ctx.isMultiplicative()`, which LogAdditive fails, so the delivered
  trend tables are the raw Henderson/MSRE-endpoint outputs (which is why
  the log fixtures carry no repair columns and a log-domain trend row is
  not positivity-asserted).
* **Contrast with the multiplicative run** (pinned in the selection
  fixture): under Multiplicative the constant-series `const-msr` contrast
  is decided (I/C ratio 3.515625); under LogAdditive the constant series
  is exactly constant in the log domain, so both `const-msr` and
  `const-fixed` degenerate to the NaN I/C ratio and are refused with
  `unsupported:trend-b7`, like the additive fixture.
* **Export stage not delivered**: `X11Kernel.buildResults/finalResults`
  apply `prepare(seq) = seq.exp()` only to the D10..D13 *results* and skip
  the bias correction exactly when `mode == LogAdditive && seasonal &&
  bias == None`. That behaviour lives in the results builder, not in any
  step class (no step getter contains `log`/`exp`); this contract claims
  no export-domain value and none is delivered.

## Explicitly unsupported (no improvised decisions)

`pseudo_additive` fails every entry point with
`<pass>: decomposition mode is not delivered` (its qualification is item
22b.5 M3b-ii). Non-positive present rows under `log_additive` fail with
the pinned constant message above. The results-builder `exp` of D10..D13
and the LogAdditive bias-correction skip are export-stage behaviour and
are not claimed; cross-validation, the period-specific corrector, the
mean-endpoint fallback and ARIMA-regression inputs remain undelivered.

## Oracle and exactness

All four capture scripts
(`validation/reference/capture-x11-{bpass,cpass,dpass,selection}.R`) take
`[--mode=Additive|Multiplicative|LogAdditive] --output=<fresh empty
directory>`; the `Additive` and `Multiplicative` paths are unchanged (the
mode-driven schema columns — the `mode` column, now also written for the
log fixtures — keep the additive schemas byte-exact, and the repair-count
columns stay multiplicative-only). For the log mode the capture feeds
`log(values)` to the pinned steps (or the raw `ts` to `X11Kernel.process`,
which logs internally), the `inputs.csv` files keep the **raw** strictly
positive values and every captured table is a log-domain step table; the
capture-side `stopifnot` guards re-derive every identity in the additive
form on the log-domain tables (rjd3 3.9.0, JVM 21). Installed fixtures:
`validation/fixtures/native/x11-{bpass,cpass,dpass}-log` (24/24/28 cases,
32,656/23,224/31,616 table rows, the shared positive multiplicative-shape
inventory including the masked and four `ext-` extended-calendar cases,
the D fixture with the additive-form MSRE diagnostic) and
`x11-selection-log` (the complete 15-case kernel inventory, 28,382 table
rows, per-stage selection records and per-attempt MSR traces — the dpass
capture cross-links its C13/C17/C20 against the installed `x11-cpass-log`
fixture).

`test_x11_log_additive.cpp` (ctest, `PASS: 32 pinned log-additive-mode
cases`) embeds verbatim at 17 significant digits: five quarterly B cases,
five C cases, seven D cases (two extended calendars) and the complete
15-case selection inventory — bit-exact log-domain tables, mode echoes
through B→C→D, the `B1 = Math.log(input)` transform identity, difference
decomposition identities recomputed with the same IEEE operations, the
additive-form staged I/C ratios and absolute-form MSRE tables, both
constant-series degenerate refusals, the five-year-cap truncation of the
19-attempt log walk, serial/OpenMP equality and the full interruption
boundary sweep. `test_x11_multiplicative.cpp` was updated in the same
slice: the gate block now refuses `pseudo_additive` alone and proves the
full pinned `isMultiplicative()` truth table (log-additive false,
pseudo-additive true). `tools/verify_x11_{bpass,cpass,dpass}` parse the
`LogAdditive` mode name and sweep the full log fixture directories.

Validation on this machine (pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`): Release OpenMP build
**45/45**, true serial build **45/45**; the log sweeps run **24/24, 24/24,
28/28** bit-exact next to the unchanged additive sweeps **18/18, 18/18,
22/22** and mult sweeps **24/24, 24/24, 28/28** (OMP and serial binaries);
the three log fixture directories re-swept at `KALOS_NUM_THREADS` 1, 2 and
8 (9/9 bit-exact); the log unit and the mult unit are diagnostic-free
under gcc 13.3 `-fsanitize=address,undefined` with leak detection enabled.
No fixture, oracle, tolerance, default or capability was promoted.
