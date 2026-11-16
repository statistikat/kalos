# Pseudo-additive decomposition mode (22b.5 M3b-ii)

```cpp
kalos::X11BPass b = kalos::x11_b_pass(series, 12, /*first_period=*/1,
    initial, final, /*trend_length=*/13, kalos::X11SigmaPolicy::legacy,
    1.5, 2.5, kalos::X11DecompositionMode::pseudo_additive);
kalos::X11CPass c = kalos::x11_c_pass(series, 12, 1, initial, final, 13,
    kalos::X11SigmaPolicy::legacy, b);            // mode inherited from b
kalos::X11SelectionPass r = kalos::x11_selection_pass(series, 12, 1,
    requests, 0, kalos::X11SigmaPolicy::legacy,
    kalos::X11MsrIterationPolicy::jdplus, 1.5, 2.5, 0, 0,
    kalos::X11DecompositionMode::pseudo_additive);
```

This slice delivers the **pseudo-additive decomposition mode** of the pinned
X-11 pipeline (`jdplus.x13.base.core.x11.pseudoadd.X11{B,C,D}StepPseudoAdd`
and `pseudoadd.MsrFilterSelectionPseudoAdd`, selected by `X11Kernel` under
`X11Context` mode `PseudoAdditive`, pinned rjd3 3.9.0) across the item
22b.4 S1–S4 fixed and selection passes. The delivered surface is exactly
the item 22b.4/22b.5 surface with `mode = pseudo_additive`: the last
optional argument of every pass entry point, echoed on `X11BPass::mode` and
inherited by the C/D/selection passes.

**Delivered semantics.** The decoded truth table `isMultiplicative() ==
(mode == Multiplicative || mode == PseudoAdditive)` makes PseudoAdditive
**multiplicative in the whole context**: `X11Context.remove` is the ratio at
every site the step subclasses do **not** override, the sigma measurement
centre and the correction-factor mode default are 1,
`AutomaticHenderson.calcICR` takes the ratio remove with relative lag-1
variations, and all four `makePositivity` trend guards engage — while the
`X11{B,C,D}StepPseudoAdd` subclasses replace exactly twelve composition
sites with the `PseudoAddUtility` forms and `X11Kernel` swaps the global-MSR
filter selection for `MsrFilterSelectionPseudoAdd`. Unlike Multiplicative
and LogAdditive the pinned `X11Kernel.check()` imposes **no positivity
requirement** on PseudoAdditive: non-positive inputs are legal and never
reach a positivity message.

## Pinned mode-driven behaviour (bytecode-verified against rjd3 3.9.0)

* **`PseudoAddUtility` forms** (all lambda capture orders decoded from the
  invokedynamic bootstrap records):
  `removeIrregular(x, y) = x − y + 1`;
  `removeSeasonalAdjusted(x, s, f) = x − f·(s − 1)` (the lambda captures
  `(arg0, arg2, arg1)`);
  `removeSeasonalAdjustedWithBorders(x, s, f, drop)` keeps the ratio `x/s`
  at the border rows and takes `x − f·(s − 1)` on the dropped interior rows
  (the filter `f` already lives on the cropped span at every call site);
  `adjustRefSeries(ref, corr, s, r) = ref + s·(r/corr − r)` (lambda args
  `(arg0, arg2, arg3, arg1)`); `removeSeasonalAdjustedFinal(x, y) = x − y`
  (no caller in the qualified surface). Every form propagates NaN rows
  through the same union-of-inputs mask as `remove_pinned`.
* **The twelve overridden sites** (everything else is the plain
  ratio-context arithmetic of the base class):
  B pass — `b4d = B3 − B4anorm + 1`, `b6 = withBorders(B1, B5, B2, drop)`,
  `b9d = B8 − s9 + 1`, `b11 = B1 − B7·(B10 − 1)`;
  C pass (constructor carries the B pass `B7`/`B13`) —
  `c1 = adjustRefSeries(Y, B20, B7, B13) = Y + B7·(B13/B20 − B13)`,
  `c6 = withBorders(C1, C5, C2, drop)`, `c11 = Y − C7·(C10 − 1)`;
  D pass (constructor carries the C pass `C7`/`C13`/`C20`) —
  `d1 = Y + C7·(C13/C20 − C13)`, `d6 = withBorders(D1, D5, D2, drop)`,
  `d8 = (D1/D7)·C20` (the ratio remove scaled by the correction),
  `d11 = Y − D12·(D10 − 1)`, `d11bis = D1 − D7·(D10 − 1)`.
  Inherited ratio identities stay ratio: `B3 = crop(B1)/B2`, `B8 = B1/B7`,
  `B13 = B11/B7`, `C4/C9/C13`, `D4 = D1/D2`, `D9bis = D1/D7`,
  `D13 = D11/D12`.
* **Multiplicative context**: the B20/C20 correction factors and the dummy
  filter default to 1; the sigma weights measure the multiplicative form;
  the four `makePositivity` guards (b7Step, c7Step, d7Step, dFinalStep)
  repair the trend tables — live in the fixture inventory (B7 on the
  spikeend and wideband cases, C7 and D7/D12 on the wideband cases) and the
  delivered trend rows are exactly the repaired pinned tables.
* **`X11Kernel.check()`**: gated on `mode == Multiplicative || mode ==
  LogAdditive` only — PseudoAdditive passes ungated, so this mode accepts
  non-positive inputs (the fixtures run on the strictly positive
  multiplicative-shape inventory because the *ratio* sites still divide by
  trend values, not because the gate demands it).
* **`X11Utility.defaultMsrTable` SWITCHMAP**: PseudoAdditive is its own
  branch (`case 1`): the observation residual is
  `obs = x − mov + 1` (`removeIrregular`) while `MsrTable.of` is called
  with relative flag `FALSE` — the phase evolutions stay
  absolute-difference.
* **`MsrFilterSelectionPseudoAdd`** (`X11DStepPseudoAdd` overrides
  `getMsrFilterSelection()`): `calcIrregular` is
  `PseudoAddUtility.removeIrregular(x, seas) = x − seas + 1` and `doMSR`
  keeps the base-class **strict** `mode == Multiplicative` test as the
  `MsrTable.of` relative flag — `FALSE` under PseudoAdditive. The whole
  attempt loop (FilteredMeanEndPoints pilot, band rule, iteration caps) is
  the qualified item 22b.4 S4/M3a loop over the pseudo irregular.
* **Contrast with the multiplicative and log-additive runs** (pinned in the
  selection fixture): the constant-series `const-msr` contrast is
  **decided** in the ratio context like the multiplicative run (I/C ratio
  3.5156250000000027 at every stage, one decisive 3x9 MSR evaluation) while
  `const-fixed` stays NaN-degenerate and is refused; the long `q-longmsr`
  walk runs **15** pinned jdplus evaluations (19 under log-additive) and
  **decides** at its fifteenth with the decisive-ratio reason — under the
  qualified five-year cap the run truncates at six, takes the method
  default 3x5 with the `five_year_cap` reason and, since the pinned walk
  settles on the same 3x5, **every delivered table of the two runs is
  identical and only the MSR metadata differs**.

## Explicitly unsupported (no improvised decisions)

Non-positive present rows are **not** an error in this mode (unlike
`multiplicative`/`log_additive`, which keep failing with the pinned
constant message). The results-builder export stage, cross-validation, the
period-specific corrector, the mean-endpoint fallback and ARIMA-regression
inputs remain undelivered for every mode as before.

## Oracle and exactness

All four capture scripts
(`validation/reference/capture-x11-{bpass,cpass,dpass,selection}.R`) accept
`--mode=PseudoAdditive` next to the unchanged
`Additive|Multiplicative|LogAdditive` paths; the mode-driven schema columns
extend (`mode` and the repair-count columns are now also written for the
padd fixtures). For the pseudo mode the fixed captures instantiate
`X11BStepPseudoAdd` / `X11CStepPseudoAdd(B7, B13)` /
`X11DStepPseudoAdd(C7, C13, C20)` exactly like `X11Kernel` composes them
(the B pass feeds the C step, the C pass feeds the D step) and the
selection capture runs the kernel itself, which selects the pseudo classes
internally; the capture-side `stopifnot` guards re-derive every captured
table from the captured inputs and predecessor tables through the bytecode
forms above, and the pseudo captures additionally cross-check the twelve
overrides against the pinned `PseudoAddUtility` statics (rjd3 3.9.0, JVM
21). The standalone `MsrFilterSelectionPseudoAdd.doMSR` and a
pinned-component attempt walk (`FilteredMeanEndPoints` + `MsrTable.of` with
the pseudo irregular and flag `FALSE`) reproduce every global-MSR
iteration. Installed fixtures: `validation/evidence/native/
x11-{bpass,cpass,dpass}-padd` (24/24/28 cases, 32,656/23,224/31,616 table
rows, the shared positive multiplicative-shape inventory including the
masked and four `ext-` extended-calendar cases, the D fixture with the
removeIrregular-form/absolute-evolution MSRE diagnostic) and
`x11-selection-padd` (the complete 15-case kernel inventory, 28,382 table
rows — the dpass capture cross-links its C-pass inputs against the
installed `x11-cpass-padd` fixture).

`test_x11_pseudo_additive.cpp` (ctest, `PASS: 32 pinned pseudo-additive-mode
cases`) embeds verbatim at 17 significant digits: five quarterly B cases,
five C cases, seven D cases (two extended calendars) and the complete
15-case selection inventory — bit-exact tables, mode echoes through B→C→D,
the `B1 = input` no-transform identity, the twelve pseudo forms and the
inherited ratio identities recomputed with the same IEEE operations on the
delivered tables, the strictly-positive repaired trend tables, the
`MsrFilterSelectionPseudoAdd` walks including the 15-attempt `q-longmsr`
with its cap contrast and the decided `const-msr`/refused `const-fixed`
pair, serial/OpenMP equality and the full interruption boundary sweep.
`test_x11_multiplicative.cpp` and `test_x11_log_additive.cpp` were updated
in the same slice: the not-delivered-mode assertions are gone (the message
is now unreachable for every entry point) and both gate blocks prove the
four-mode `isMultiplicative()` truth table, the positivity gate holding for
`multiplicative`/`log_additive` alone, and pseudo results that differ from
their own pipelines' tables. `tools/verify_x11_{bpass,cpass,dpass}` parse
the `PseudoAdditive` mode name (and the cpass sweep takes the mode default
from `x11_mode_is_multiplicative`) and sweep the full padd fixture
directories.

Validation on this machine (pinned OpenBLAS 0.3.34 provider,
`env LD_LIBRARY_PATH=/tmp/opencode/openblas/lib`): Release OpenMP build
**46/46**, true serial build **46/46**; the padd sweeps run **24/24, 24/24,
28/28** bit-exact next to the unchanged additive sweeps **18/18, 18/18,
22/22**, mult sweeps **24/24, 24/24, 28/28** and log sweeps **24/24, 24/24,
28/28** (OMP and serial binaries, 24/24 sweeps); the three padd fixture
directories re-swept at `KALOS_NUM_THREADS` 1, 2 and 8 (9/9 bit-exact); the
pseudo, log and mult units are diagnostic-free under gcc 13.3
`-fsanitize=address,undefined` with leak detection enabled. No fixture,
oracle, tolerance, default or capability was promoted.
