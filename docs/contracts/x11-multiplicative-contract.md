# Multiplicative decomposition mode (22b.5 M3a)

```cpp
kalos::X11BPass b = kalos::x11_b_pass(series, 12, /*first_period=*/1,
    initial, final, /*trend_length=*/13, kalos::X11SigmaPolicy::legacy,
    1.5, 2.5, kalos::X11DecompositionMode::multiplicative);
kalos::X11CPass c = kalos::x11_c_pass(series, 12, 1, initial, final, 13,
    kalos::X11SigmaPolicy::legacy, b);            // mode inherited from b
kalos::X11SelectionPass r = kalos::x11_selection_pass(series, 12, 1,
    requests, 0, kalos::X11SigmaPolicy::legacy,
    kalos::X11MsrIterationPolicy::jdplus, 1.5, 2.5, 0, 0,
    kalos::X11DecompositionMode::multiplicative);
```

This slice delivers the **multiplicative decomposition mode** of the pinned
X-11 pipeline (`jdplus.x13.base.core.x11.X11{B,C,D}Step` and `X11Kernel`
under `X11Context` mode `Multiplicative`, pinned rjd3 3.9.0) across the
item 22b.4 S1–S4 fixed and selection passes. `kalos::X11DecompositionMode`
(include/kalos/x11_decomposition.hpp) mirrors `jdplus.sa.base.api.
DecompositionMode` in pinned declaration order without `Undefined` (the
pinned steps treat it as additive in every decoded branch and no pinned
spec default uses it). Every pass entry point takes the mode as its last
optional argument — the default `additive` leaves the item 22b.4 surface
byte-for-byte unchanged — and echoes it on `X11BPass::mode` (the C/D/
selection passes inherit or echo the same value). Nothing else about the
pipeline is new: the seasonal plans, sigma policies, corrector structure,
band rule and selection stages are the qualified contracts applied under
the pinned multiplicative arithmetic.

## Pinned mode-driven behaviour (bytecode-verified against rjd3 3.9.0)

* **`X11Context.remove(x, y)`** (`detail::remove_pinned`): the multiplicative
  branch is the elementwise quotient `x / y` at every composition site (B3,
  B4d, B6, B8, B11, B13, C1, C4, C6, C9, C11, C13, D1, D4, D6, D8, D9bis,
  D11, D11bis, D13, the normalization input and the MSRE irregular); the
  additive branch stays the difference. NaN rows propagate through the
  quotient like the pinned `DoubleSeq`.
* **`X11Kernel.check()`** (`detail::check_decomposition`): the positivity
  requirement applies only under `mode == Multiplicative || mode ==
  LogAdditive` — any present row with `!(x > 0)` (pinned `dcmpg(x, 0) > 0`
  test, NaN comparisons false) fails with the pinned message
  `"multiplicative decomposition of non positive series"`. Flagged-missing
  rows are skipped at the fixed-pass level (the masked mult fixtures run
  through all three fixed passes); the selection pass keeps its separate
  pinned completeness rejection of missing values regardless of mode.
* **`makePositivity`** (`detail::make_positivity`) repairs the *trend*
  tables after the Henderson filter, exactly at the four decoded sites:
  `b7Step` (B7, the one `X11BStep` site), `c7Step` (C7), `d7Step` (D7) and
  `dFinalStep` (D12). A non-positive row takes the mean of its nearest
  non-repaired neighbours (either side alone when only one exists), the
  scan stops at a positive value or a NaN, repairs run in ascending order
  so later scans see repaired values, and with no usable neighbour on
  either side the pinned reference throws (delivered as
  `std::invalid_argument("negative series")`); NaN rows are skipped. The
  delivered tables are the *repaired* pinned tables; the fixtures carry
  per-case repair counts (record-only) proving the branch is live: B7 2
  rows on both spikeend cases and 2/1 on the wideband cases, C7 2/1 on
  wideband, D7 2/1 on wideband and D12 1 on the quarterly wideband case.
  Every delivered trend-table row is strictly positive.
* **Extreme-value analysis and corrector** (pinned `X11SigmaLimitsM` /
  `DefaultExtremeValuesCorrector` under a multiplicative context): sigma
  weights are measured against the **center 1** instead of 0
  (`x11_sigma_weights(..., multiplicative)`), the correction factor is
  `x / (1 + w·(x − 1))` below unit weight (`x·(1 − w)` additive), and the
  dummy/uncorrected default of B20/C20 and of the corrected series is the
  **mode default 1** (additive 0).
* **`X11Utility.defaultMsrTable` and `MsrTable.of`** (`detail::
  pinned_msr_table`): the pinned `Multiplicative` branch takes
  `obs = x / mov` and passes the **relative-form flag `true`** to
  `MsrTable.of` — the per-phase deltas become `(s2 − s1) / s1` for both
  arguments — while the additive (and pseudo-additive `x − mov + 1`,
  flag `false`) branches keep plain differences. The flag is set exactly
  when the mode is `Multiplicative` (`MsrFilterSelection` passes the same
  strict mode test; the flag is `false` under `PseudoAdditive`, one of the
  reasons the ratio arithmetic alone does not deliver that mode). The
  global ratio form `sum(ri·count)/sum(rs·count)` is unchanged. The D-pass
  fixture `msr.csv` carries the relative-form per-phase table and global
  value bit-exact (including the pinned `Inf`/`NaN` outcomes on degenerate
  windows).
* **`MsrFilterSelection.calcComponents`** (the D-final MSR walk): the FMEND-7
  pilot is unchanged, the irregular is `x / seas` under the mode, and the
  `MsrTable.of` relative flag follows the rule above — the pinned attempt
  traces (global MSR per attempt, iteration counts, band decisions) are
  bit-exact in all eight engaged mult selection cases.
* **`AutomaticHenderson.calcICR`** (`detail::pinned_ic_ratio`, mode-aware):
  the pilot is the length `period + 1` Henderson over the interior span,
  the irregular is the **ratio** remove of the pilot, and
  `X11Utility.calcAbsMeanVariations(x, lag, mult)` under the flag makes
  every lag-1 change **relative**, `|(x[i] − x[i−lag]) / x[i−lag]|`, in
  BOTH the pilot and the irregular window before the divisor `count − 1`
  and the pinned ×3/×6 multipliers. This is a new pinned fact decoded for
  this slice: it turns the additive NaN degenerate contrast into a decided
  ratio in the multiplicative `const-msr` fixture case (I/C ratio exactly
  3.515625 at b6/c6/d6 → the inline branch takes 5 at B7), while
  `const-fixed` (constant D11bis) stays NaN and is refused with the usual
  `unsupported:trend-b7`.
* **Mode-free sites** (verified, unchanged): the D9 mask compares the raw
  `D9bis`/`D8` pair (`dcmpg` on the plain difference in both modes — the
  `d9Step` carries no decomposition switch); the seasonal normalizer, the
  seasonal filters, the Henderson weights and the replacement window
  machinery are mode-blind apart from the remove fed into them.

## Explicitly unsupported (no improvised decisions)

`log_additive` and `pseudo_additive` fail every entry point with
`<pass>: decomposition mode is not delivered`: the pinned log-additive
pipeline is additive arithmetic on a `log()` transform whose `exp` belongs
to the `X11Kernel.fill` **export stage** of D10..D13 (decoded, not
delivered), and the pinned pseudo-additive pipeline runs the dedicated
`X11{B,C,D}StepPseudoAdd`/`PseudoAddUtility` classes whose two decoded
paths differ (MSRFSEL ratio-remove with flag `false` vs `defaultMsrTable`
`x − mov + 1` with flag `false`) and are undecoded in full.
`x11_mode_is_multiplicative` exposes the pinned `isMultiplicative()`
predicate (true for both `Multiplicative` and `PseudoAdditive`) and the
passes gate on the **full mode**, never on the predicate alone. Non-positive
present rows under `multiplicative` fail with the pinned kernel message;
the fixed-pass mask semantics and the selection completeness rule are as
described above.

## Oracle and exactness

All four capture scripts
(`validation/reference/capture-x11-{bpass,cpass,dpass,selection}.R`) take
`[--mode=Additive|Multiplicative] --output=<fresh empty directory>`; the
default `Additive` reproduces the item 22b.4 fixtures byte-identically
(re-captured and `cmp`-verified against every installed additive CSV,
including all five selection files). The multiplicative-only `cases.csv`
columns (`mode`, `b7_repair`, `c7_repair`, `d7_repair`, `d12_repair`) are
appended last so the additive schemas stay byte-exact, and the capture
`stopifnot` guards re-derive every table through the pinned ratio
arithmetic, the `makePositivity` rebuild of the trend tables and the
mode-aware MSR/ICR cross-checks on the pinned context (JVM 21). Installed
fixtures: `validation/evidence/native/x11-{bpass,cpass,dpass}-mult` (24/24/28
quarterly+monthly cases, 32,656/23,224/31,616 table rows: the nine additive
fixture categories at positive levels plus `deep`, `spikeend` and
`wideband`, the last two carrying the positivity repairs; the D fixture
additionally carries the same four `ext-` extended-calendar cases as the
additive D fixture) and
`x11-selection-mult` (the complete 15-case kernel inventory, 28,382 table
rows, per-stage selection records and per-attempt MSR traces).
`validation/evidence/native/x11-bpass-mult-cases.R` defines the shared
positive-valued case family.

`test_x11_multiplicative.cpp` (ctest, `PASS: 32 pinned multiplicative-mode
cases`) embeds verbatim at 17 significant digits: five quarterly B cases
(including both repair-carrying categories and the masked case), five C
cases, seven D cases (two extended calendars), and the complete 15-case
selection inventory — bit-exact tables, mode echoes, relative-form MSRE
tables and self-consistency, the mode-aware staged I/C ratios (fixed
stages store NaN like the additive test), ratio decomposition identities
recomputed with the same IEEE operations, the `const-fixed` refusal and
`const-msr` decided contrast, the five-year-cap truncation, repair-driven
strict positivity of the delivered trend tables, serial/OpenMP equality
and the full interruption boundary sweep. The gate tests pin the
`log_additive`/`pseudo-additive` refusals, the positivity message, the
masked-row hole semantics and the unchanged additive default.
`tools/verify_x11_{bpass,cpass,dpass}` read the optional `mode` column by
header name (additive fixtures unchanged) and sweep the full mult fixture
directories.

Validation on this machine (pinned OpenBLAS 0.3.34): Release OpenMP build
**44/44**, true serial build **44/44**; the mult sweeps run **24/24,
24/24, 28/28** bit-exact next to the unchanged additive sweeps **18/18,
18/18, 22/22**; all six fixture directories × {serial, 4-thread} sweeps are
bit-exact (12/12); the embedded mult unit and the B/C/D/selection units are
diagnostic-free under gcc 13.3 `-fsanitize=address,undefined`
(`detect_leaks=0`), as are sanitizer-built mult and additive D sweeps. No
fixture, oracle, tolerance, default or capability was promoted.
Cross-validation, the period-specific corrector, log-additive and
pseudo-additive modes, the mean-endpoint fallback and ARIMA-regression
inputs remain undelivered and unclaimed.
