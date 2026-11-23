# TRAMO regression tests, no-calendar branch (23-E)

Transcription of the calendar-free part of the tramo regression-test layer of
the pinned jdplus 3.9.0 stack: the `ModelStatistics.of` rows the refStats
gate consumes (with the Ljung-Box/se/BIC machines behind them), the mean-rule
layer (`AmiOptions.checkMu`, the round-0 `acceptAirline` rule, the
`reduceVa`/`lastSolution`/`estimateModel` control members) and the
non-automatic members of `FastRegressionTest` / `DefaultRegressionTest`.
The bytecode record is SECTION 25 of `tmp_opencode/kalos-m5-pinning.md`
(with three session-start corrections dated 2026-11-22, see "Pinned
semantics" below). Everything numeric is bit-exact: decisions AND values.

Boundary (maintainer ruling of 2026-11-22, `docs/calendar-roadmap.md` I-4):
every request that would drive a CALENDAR-variable kernel — the Fast
machine's auto-identified trading-day / length-of-period / Easter tests and
the Default machine's calendar builder fields and calendar accept paths —
belongs to item 21-I.3. This unit never approximates them: it refuses such
requests fail closed with reasons naming 21-I.3. Calendar-free requests run
the transcription in full. The `TradingDaysController` T/Wald machinery and
the `Automatic{,Wald}RegressionTest` families are likewise 21-I.3-owned and
not ported here.

## Coordinate map (module → unit)

| JVM (pinned) | unit entry point |
| --- | --- |
| `TramoKernel.readAmiOptions` (checkMu) | `kalos::tramo_ami_check_mu` |
| `TramoKernel.iterate` round-0 rule | `kalos::tramo_round0_accepts` |
| `TramoKernel.reduceVa` | `kalos::tramo_reduce_va` |
| `TramoKernel.lastSolution` (orders rewrite) | `kalos::tramo_last_solution_orders` |
| `TramoKernel.estimateModel` retry loop | `kalos::tramo_estimate_model_retry` |
| `ModelStatistics.of` (consumed rows) | `kalos::tramo_model_statistics` |
| `ConcentratedLikelihood.ser/tstats` (both arms) | `kalos::tramo_test_tstats(stats, addmu)` |
| `TRegressionTest(tlow, thigh, nsig).accept` | `kalos::tramo_t_accepts` |
| `TramoKernel.pass2` (refStats gate) | `kalos::tramo_ref_stats_gate` |
| `FastRegressionTest.test` / `DefaultRegressionTest.test` | `kalos::run_tramo_regression_tests` |

Likelihood hand-overs are REQUIRED REQUEST DATA (21-G precedent): the units
consume the coefficients, `unscaledCovariance` diagonal, `ssq`, `dim`,
`degreesOfFreedom`, `sigma2` and free-parameter counts of an already-completed
estimation; nothing here runs an estimation. The pass2 gate takes both
ModelStatistics-derived rows (current side: build with
`tramo_model_statistics`; reference side: the stored row from the previous
pass) as they stand.

## Capture and corpus

`validation/reference/TramoRegressionTestsProbe.java` observes only public
entry points (`ModelStatistics.of`, `ConcentratedLikelihood.tstats`,
`FastRegressionTest.test`, `DefaultRegressionTest.test`,
`TramoKernel.pass2` reflectively driven with its fields set, `AmiOptions`
via `readAmiOptions`) against the pinned jars.
`validation/reference/capture-tramo-regression-tests.R` runs one JVM per
case (fresh kernel state, class-load witnesses), guards the JVM build and the
routed-semantics recipe (OpenJDK 21, `-XX:DisableIntrinsic=_dexp
-XX:DisableIntrinsic=_dlog`), and fails on any ground-truth drift. The
corpus `validation/evidence/native/tramo-regression-tests/` was
DOUBLE-CAPTURED in independent JVM processes; the repro gate reports all
nine data files byte-identical (`SHA256SUMS.csv` verified; 55 cases: 10
ModelStatistics, 12 Fast, 10 Default, 16 pass2 gate, 7 AmiOptions witnesses,
plus load witnesses pinning the two machines and `ModelStatistics` — the
21-I.3-owned controller classes appear in the load log ONLY on the seam
cases).

`tools/gen_tramo_regression_tests_golden.py` turns the corpus into the
bit-pattern tables of `tests/jvm_tramo_regression_tests.h`; every double
rides as raw IEEE-754 bits. `tests/tramo_regression_replay.h` (shared by
ctest `test_tramo_regression_tests` and the sweep) rebuilds each request
from the hand-over echoes and compares bit-for-bit. The `drb/drbb/drbc`
hand-overs are the Default machine's own test-model estimation re-run by the
probe (`RegArimaUtility.processor(true, 1e-5)` over `copyOf(desc)`; every
captured case uses precision 1e-5, matching the probe's echo pass — the
deterministic re-estimation is part of the hand-over contract, not the
unit's job).

## Decision laws (bytecode → unit)

- `checkMu = usingAutoModel || meanIsTest`. Corpus `am_*` witnesses replay
  it through the pinned `TramoSpec` decomposition (jshell witness:
  `isUsingAutoModel()` is true exactly for TR3/TR4/TR5/TRfull;
  `RegressionSpec.getMean().isTest()` is false for every spec constant) plus
  the full 4-combination truth table.
- `tramo_round0_accepts`: `acceptAirline && dcmpg(1 - pvalue, limit) < 0`;
  dcmpg pushes +1 for NaN, so a NaN p-value does NOT accept.
- `reduceVa`: `if (dcmpg(curva, 2.0) == 0) return false;` else
  `curva = Math.max(2.0, curva * (1 - reduceVa))` — `Math.max` propagates
  NaN (unlike `std::max`), witnessed by the crafted NaN case through the
  synthetic block.
- `lastSolution` orders: `p := 3; if (bd > 0) bp := 0; q := 1;`
  `if (seasonal) bq := 1;` d/bd pass through; the caller owns
  `removeVariable(isOutlier)` and `round := 1` (both in the gate result).
- `estimateModel` retry ladder: pure decision over the finite request
  sequences (see qualification note under "Gates").
- `ModelStatistics.of`: `lb_length = LjungBox.defaultAutoCorrelationsCount`
  (reuses 23-C.2 `calc_lb_length`), `value = Σ r²/(n−i)` over
  `auto_correlation(residuals, 0.0, i)` (23-C.2 kernel), statistic
  `value·n·(n+2)`, p via `detail::chi2_upper_p(df = m − k)` (no new
  distribution kernel); `df <= 0` throws inside the builder with no catch —
  the unit returns `built == false` with every double NaN (no row invented,
  witnessed by `ms_craft_dfneg`); `bic = log(sigma2) + (k+nx)·log(dim)/dim`
  with the routed fdlibm log; `se = sqrt(ssq/(dof − k))`; the seasonal pair
  is ALWAYS computed for freq > 1 with fixed df 2 (never a throw), an
  out-of-range lag contributing a zero autocorrelation (witnessed live by
  `slb = 0, slbp = 1` at n = 7); `observations = estimation-domain length`,
  `effobs = cl.dim()`, `outliersCount` = number of ami-flagged outlier
  VARIABLES (distinct from the pass2 `nb` = `countRegressors`, which sums
  `core.dim()` — the gate request carries both). The model comparator also uses absolute
  skewness and the skewness, mean stability and variance stability probabilities.
  `tramo_model_comparison_panel` builds all its fields from this same likelihood.
  For the two sample comparisons, JDemetra drops up to 120 months at each end.
  Each sample variance uses a known zero mean. The pooled t test uses both sample
  means, and the upper-tail F test uses the second variance divided by the first.
- `tramo_test_tstats`: operation-for-operation `ConcentratedLikelihood.
  ser/tstats` with `nobs = free_parameters` — every TRAMO accept site calls
  `tstats(k, true)`, the ADDMU arm with denominator `dim − nx − k`; the
  plain arm (`dim`) rides along for the echoes. `sqrt` is the exact IEEE
  operation (`std::sqrt` bit-for-bit).
- `tramo_t_accepts`: `high` counts `dcmpl(|t|, thigh) >= 0`, the remaining
  `low` counts `dcmpl(|t|, tlow) >= 0`, accept iff
  `high > 0 || low >= min(nsig, dim)`. A NaN `|t|` gets `dcmpl = -1` and
  counts in NEITHER bucket.
- `tramo_ref_stats_gate` (pass2): rules evaluated in order, first hit
  reverts (`restore`: orders/mean/outlier tally := reference,
  `lbp := rlbp`; keep: the stored reference row := current):
  `A: lbp<.95 ∧ rlbp<.75 ∧ rse<se` (dcmpg); `B: pass==1 ∧ lbp>=.95 ∧
  rlbp<.95` (dcmpl for lbp — a NaN lbp is EXCLUDED); `C: A's guards ∧
  rlbp<lbp ∧ rse<1.0·se` (dead subset of A in the pinned build, transcribed
  anyway); `D: lbp>=.95 ∧ rlbp<.95 ∧ rse<1.0·se`; `shapeA: d=0 ∧ bd=1 ∧ p=1 ∧
  phi(1)<=−.82 ∧ q<=1 ∧ bp=0 ∧ bq=1`; `shapeB: d=1 ∧ bd=0 ∧ p=0 ∧ q=1 ∧ bp=1
  ∧ bphi(1)<=−.65 ∧ bq<=1`; `nr > nb` skips ALL rules and lands on keep;
  `nr = −1` marks `refAuto == null` (rule block skipped, stored row :=
  current). Then `pcr += .025` (pass 1) / `.015` (pass ≥ 2); accept iff
  `dcmpg(lbp, pcr) <= 0` (NaN continues); on continue, the pass-1 arm runs
  `reduceVa` and the remark "reduction of the critical value for outliers
  detection" UNCONDITIONALLY (bytecode 489-501 pops the return), `round++`,
  `pass++`; `pass ≤ 2`: `needAutoModelling = !changed` (bytecode 534-544 —
  the session-start pin note said `changed`; corrected, corpus witnesses
  `pg_null_ref`/`pg_remark`), `needOutliers = outliersDetection`;
  `pass > 2`: `lastSolution` rewrite + `outliers := 0` (removeVariable),
  `round := 1`, `needAutoModelling := false`, `needOutliers :=
  outliersDetection`, warning "last chance model". The surviving stored row
  (current when the reference was replaced, the stored one when reverted) is
  cross-checked against the `pg refLbp/refSe` echoes in every gate case.
- Machines: Fast runs the mean test `T(tmean)`-shaped accept
  (`tlow = thigh = tmean`, `nsig = 1`) over coefficient 0 when `testMean`
  and `regarima().isMean()`; rejection sets `mean := false` and clears the
  estimation (`m.set(newDesc, null)`); plain (non-ami) calendar-kind
  variables are inert echoes in the pinned filters (witnessed, accepted).
  Default is `Unprocessed` when no calendar field is set (refused otherwise)
  and no mean test is wired; its mean branch consumes the TEST-MODEL
  estimation (hand-over), sets the mean flag on the live description and
  calls `clearEstimation()` on change. `joinTest`/`adjust` without calendar
  fields never build their F kernel (inert, corpus-witnessed, accepted).

Pinned-semantics corrections landed with this item (SECTION 25 carries the
dated notes): (1) `dcmpl` pushes −1 for NaN, so a NaN `|t|` counts in
NEITHER T bucket and a NaN `lbp` is excluded from rules B/D — the
session-start notes "NaN → HIGH" and "NaN enters B/D" had the dcmpl/dcmpg
polarity swapped and are superseded; there is NO divergence from the landed
21-G `t_test_accepts` (`a >= t` is C++-false for NaN, same buckets).
(2) `needAutoModelling = !changed` on the pass-1/2 continue arm.
(3) the pass-1 `log.remark` fires unconditionally after `reduceVa()`.

## Fail-closed matrix

| Request | Behaviour |
| --- | --- |
| `annual_frequency != 12` (ModelStatistics) | `std::invalid_argument` (D11, monthly-first) |
| `period != 12` (machines) | `std::invalid_argument` (D11) |
| empty / non-finite residual block, mismatched or negative hand-over counts | `std::invalid_argument` (envelope) |
| Fast request with ami auto-td / auto-lp / auto-Easter variable | refused, reason names item 21-I.3 |
| Fast request with auto moving-holiday variable | refused fail closed (no filter exists in the pinned machine, no corpus witness) |
| Default request with trading-days / length-of-period / Easter builder field | refused, reason names item 21-I.3 |
| `df = m − k <= 0` in ModelStatistics | NO throw: `built == false`, every double NaN (JVM throw surface modelled as an unusable row) |
| exhausted request sequences in `tramo_estimate_model_retry` | `std::invalid_argument` (request data, not a JVM path) |

Corpus pins for the refusals: `ms_quarter` (JVM computes the quarterly row;
the corpus records it, the unit refuses), `fr_seam_td` / `fr_seam_td_lp` /
`fr_seam_easter` (Fast ami variables), `dr_seam_td` / `dr_seam_lp` /
`dr_seam_easter` (Default calendar fields) — the load logs show the
controller classes loading exactly there.

## Not delivered or claimed

- Calendar kernels (21-I.3): TradingDaysController T/Wald, the automatic
  test families, the Default calendar branch, adjusted/joined series.
- `estimate_model_retry` is PARTIALLY qualified: the loop control flow is
  integer-only (no arithmetic to pin) and has NO live witness (reaching it
  needs a full `estimateModel`, out of this boundary). The reduceVa failure
  arm shares the corpus-qualified `tramo_reduce_va` and the Fast mean test
  is the corpus-qualified machine; the ladder shape itself is qualified by
  the synthetic request-sequence block only (five-failure completion,
  short-circuit, pass-gating, exhaustion throw). Treat it as transcribed
  control flow, not corpus-qualified numerics.
- The Default test-model estimation itself is not re-run by the unit; it is
  supplied as the hand-over (21-G precedent), captured here at precision
  1e-5 as every corpus case uses.
- Quarterly/annual operation anywhere (D11).

## Gates

- ctest `test_tramo_regression_tests` (#76, TIMEOUT 3600): 663 replay
  checks across 55 corpus cases + the synthetic block, green in the
  OpenMP-on, serial and ASan/UBSan lanes.
- Sweep `tools/verify_tramo_regression_tests.cpp`: 663 replay checks + 464
  corpus cross-checks (golden vs raw `rt-facts.csv` tokens, both coupling
  directions), 0 failures.
- Shim-invariance battery: `test_tramo_regression_tests` is the 23rd ledger
  cell; the replay consumes only the routed kernels (`chi2_upper_p`,
  `auto_correlation`, `fdlibm_log`) and exact IEEE `sqrt`, so the ledger100
  cell is byte-identical to plain by construction (verified in the battery).
- `check-manifest.py`: 178 core / 112 fixture / 286 evidence.
