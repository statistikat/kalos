# Pass-level X-11 selection flow (22b.4 S4)

```cpp
kalos::X11SelectionPass run = kalos::x11_selection_pass(
    series, 4, /*first_period=*/1,
    /*requests=*/{Msr, Msr, Msr, Msr}, /*trend_length=*/0 /* automatic */,
    kalos::X11SigmaPolicy::legacy);
if (run.status == "available") {
    int b7 = run.b7.length;                 // X11TrendStage records
    auto seas = run.d.seas_filter;          // pinned seasFilter fill
    const kalos::X11DPass& d = run.d;       // selected-state D tables
}
```

This is the pass-level additive selection flow of the pinned `X11Kernel`
(item 22b.4, slice S4): the staged seasonal requests resolved through the
qualified item 22b.3d seasonal plan, the automatic Henderson choices at B7,
C7, D7 and the D final step through the qualified item 22b.3c comparison-
ratio rule, and the global-MSR D-final seasonal decision through the
qualified item 22b.3c band rule with the pinned attempt loop, composed onto
the item 22b.4 S1–S3 fixed-pass arithmetic. **The slice decides nothing
new**: every selection rule is the qualified contract function applied to
the pinned I/C ratios and MSR tables. Cross-validation (including the
pinned `getCrossvalidationFilter` D-final variant), multiplicative and
non-seasonal modes, the period-specific corrector, the mean-endpoint
fallback below the trend length and ARIMA-regression inputs are not
delivered or claimed.

## Public surface and validation

`x11_selection_pass(series, frequency, first_period, requests, trend_length,
policy, msr_policy, lower_sigma, upper_sigma, backcast, forecast, interrupt)`
returns the finished `X11BPass`/`X11CPass`/`X11DPass` of the selected state
plus the four `X11TrendStage` records (`b7`, `c7`, `d7`, `final`), the
`X11MsrStage` decision and the per-stage plans. `requests` are the pinned
`SeasonalFilterOption` values; `trend_length` is an odd Henderson length
>= 5 or 0 for the pinned automatic selection. The sigma policy is required
and nothing is promoted; `msr_policy` selects the qualified iteration
policy (default `jdplus`). Validation: complete observations (the pinned
`X11Kernel.check` refuses missing values), frequency 4/12, 1-based
first period, the fixed-pass arithmetic floors (`n >= 3*(frequency/2) +
frequency`, `n >= trend_length` including the length selected at each
stage), non-negative horizons with at least four cycles of original
calendar, and one request per calendar period (the plan rejects others).
`status` is `"available"` or `unsupported:<stage>` (see below); unsupported
runs keep the stage records and plans but no tables.

## Pinned selection points (bytecode-verified against rjd3 3.9.0)

* **B7** (`X11BStep.b7Step`): the ratio is `AutomaticHenderson.calcICR`
  over B6 and the length follows the pinned **inline branch** —
  `ratio >= 1` takes `period + 1`, otherwise `selectFilter(ratio, period)`
  — *not* `selectFilter` alone. Quarterly cases with ratios in [3.5, inf)
  therefore select 5 where `selectFilter` would say 7, and monthly ratios
  >= 1 select 13. The selection is independent of the probe length because
  B6 does not depend on the trend filter, so the automatic case probes the
  fixed B pass at length 5 and reruns at the selected length.
* **C7/D7**: `selectFilter(calcICR(C6/D6), period)`; the C7 length may
  differ from B7 (the pinned never requires agreement), which is why the
  automatic composition builds the C tables from the shared pinned
  machinery instead of chaining the fixed C pass.
* **D final** (`dFinalStep`): `iCRatio = calcICR(D11bis)` is always
  computed and stored, also in fixed mode (the fixture pins the scalar for
  fixed-trend cases); the length is `selectFilter(iCRatio, period)` only
  when automatic.
* `calcICR` (`detail::pinned_ic_ratio`): pilot Henderson of length
  `period + 1` with the pinned symmetric weights over the interior span
  only, irregular = aligned input − pilot, explicit extensions dropped
  from both windows, ratio of lag-1 mean absolute changes with the pinned
  `calcAbsMeanVariation` divisor `count − 1` and the pinned multipliers
  (×3 quarterly, ×6 biannual).

## Seasonal-request coupling

The fixed passes receive the qualified plan arrays: Msr and X11Default map
to 3×3 at every initial stage and to the provisional 3×5 at every final
stage, exactly mirroring the pinned `X11Context.getInitialSeasonalFilter`
/`getFinalSeasonalFilter` getters (a mixed-MSR final request stays on the
provisional per-period values; only the all-MSR context engages the
selection). `dFinalStep` copies the final array, and when the context is
all-MSR fills the copy with the doMSR result (`Arrays.fill` on the D-local
copy — the getter returns a fresh array, so the context keeps the
provisional values and the D10 normalization re-reads those; the fill only
reaches D10bis, verified in the bytecode). The delivered `d.seas_filter`
and `d.msr_iterations` echo the pinned `getSeasFilter()` and
`getD9msriter()` (0 when the context never engaged). The pinned
`X11Context.isD9default()` is a spec-level flag captured as false in every
engaged case; the method default flag is `MsrFilterSelection.isUsingDefault`
and is what `X11MsrStage::using_default` carries.

## The global-MSR walk

Over the D9bis series, the pinned `MsrFilterSelection.doMSR` loop: drop the
explicit horizons, trim the trailing partial calendar year at
`getPosition(backcast)`, and while at least five calendar years remain
evaluate the pinned `MsrTable` (the FMEND-7 pilot and the S3-qualified
additive `defaultMsrTable` branch) over the **leading** window — ambiguous
bands remove one year from the trailing edge. The global ratio is
`sum(ri·count)/sum(rs·count)` over the per-phase table; the qualified
`x11_msr_band` cutoffs decide and the qualified policies stop the loop:
`jdplus` continues until decisive or until another evaluation would have
fewer than five years (method default 3×5 with reason
`insufficient_remaining_history`, or `insufficient_initial_history` when
the window never held five years), `x11_five_year_cap` additionally
defaults to 3×5 after five year removals (reason `five_year_cap`). Every
evaluation is retained as an `X11MsrAttempt` row (window count, years
removed, global ratio, band), bit-exact against the pinned attempt trace
including a 19-evaluation case. The loop is finite by construction (one
year per iteration), so the qualified standalone budget never applies
here.

## Explicit unsupported statuses (no improvised decisions)

The qualified rules refuse the pinned degenerate choices, and the pass
records them instead of reproducing them: a NaN comparison ratio makes the
pinned take the longest filter (the `>= 1` test fails under `dcmpg`) and
`x11_henderson_select` rejects NaN; a NaN MSR table makes the pinned select
3×9 and `x11_msr_band` rejects NaN. Such a stage yields
`unsupported:trend-b7|trend-c7|trend-d7|trend-final|msr-d-final` at the
first occurrence and the run stops with records only. The constant-series
fixture cases pin both contrasts (pinned length 7, pinned MSR 3.844… → 3×5)
as controls that the native pass deliberately does not claim.

## Oracle and exactness

`validation/reference/capture-x11-selection.R` drives the pinned kernel
through `X11Kernel.process` for 15 cases (fixed and automatic Henderson;
stable/S3×3/S3×5/S3×9/X11Default/Msr requests; monthly and quarterly; two
hand-extended calendars; exhausted and zero-iteration windows; a 19-
evaluation walk; two NaN controls) and captures `inputs.csv`, `cases.csv`,
`selections.csv` (per-stage pinned ratio, `selectFilter` value and chosen
length), `msr.csv` (per-attempt walk trace from `MsrFilterSelection` plus
standalone cross-checks), `tables.csv` (28,382 rows: B7/B13/B20, C7/C13/
C20 and all sixteen D tables of the selected state) and `metadata.R`, all
at 17 significant digits with `SHA256SUMS.csv`. Capture-side `stopifnot`
guards re-verify per stage `calcICR`/`selectFilter` on the kernel context,
the standalone `doMSR` on `D9_g_bis` and a pinned-component attempt walk
(FMEND-7 pilot + `MsrTable.of` per attempt) reproducing filter, iterations,
default flag and global ratio of every engaged case.

`test_x11_selection_pass.cpp` embeds all 15 cases' decision records and six
full 22-table sets (auto+MSR, fixed+MSR, exhausted, zero-iteration,
extended calendar, monthly) verbatim: stage records bit-exact including the
inline-branch contrast (`selectFilter` is additionally re-evaluated from
the captured ratios), attempt traces bit-exact, the five-year-cap policy
truncated against the same pinned 19-evaluation trace with the invariant
that the prefix tables B..D9_g_bis are identical while D10-on diverge, the
decomposition and MSRE identities, serial/OpenMP/4-thread bit equality,
argument validation and the full interruption boundary sweep.

Validation on this machine (pinned OpenBLAS 0.3.34): Release OpenMP build
**43/43**, true serial build **43/43**, and the B/C/D fixtures re-verified
**18/18, 18/18, 22/22** bit-exact (4-thread and serial) after the shared
header gained `pinned_ic_ratio`; ASan/UBSan spot runs of the selection and
all three fixed-pass units are diagnostic-free. No fixture, oracle,
tolerance, default or capability was promoted.
