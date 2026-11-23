# Tramo kernel round-driver contract (item 23-F, F.3 layer)

Unit: `kalos/tramo_kernel_rounds.{hpp,cpp}` — the round-driver seam of the
pinned `jdplus.tramoseats.base.core.tramo.TramoKernel` (rjd3 3.9.0,
`jdplus-tramoseats-base-core-3.9.0.jar`): the `ami()` ten-iterate loop with
its guards, the change accumulator, the `estimateModel()` retry ladder, the
round-0 reference snapshot, the `refStats` gate (`pass2`), the seasonality-
controller pass3 reset and the controller sweep. The heavy stages
(differencing selection, arma selection, the outlier scan, the five-round
`estimate()`, the regression tests, the controllers) ride into the unit as
ordered `TramoKernelSeam` FACTS and stay owned by their own landed units;
the refStats gate and the estimateModel ladder run on the landed 23-E
machines (`tramo_ref_stats_gate`, `tramo_estimate_model_retry`) UNCHANGED.
The bytecode decode lives in `tmp_opencode/kalos-m5-pinning.md` SECTIONS 25,
26.4, 30.8 and 31; the corpus is
`validation/evidence/native/tramo-kernel-rounds/` (Scratch23r: the real
`TramoKernel.process` and a bytecode-faithful manual driver in ONE JVM,
35/35 identity on the event stream, the final model bits and the aggregate).

## Scope and refusals (fail-closed matrix)

| input | behavior |
|---|---|
| `annual_frequency != 12` | `std::invalid_argument` — the D11 monthly envelope; `kr_q_air` is the replayed refusal witness |
| `observations <= 0`, negative controller count, empty seam list | `std::invalid_argument` before any state |
| non-finite spec/curva/pcr/stats/parameter facts | `std::invalid_argument` (the pinned flow never carries NaN here) |
| seam facts that disagree with the recomputed control flow (wrong kind at a site, `taken` vs the estimation state, a gate `revert`/skip echo vs the landed machine, an impossible `iterate` end, leftover or exhausted facts) | `std::invalid_argument` — the driver is fail-closed against fabricated requests; every throw message names the site |

## Pinned semantics (bytecode-verified; corpus witnesses named)

* `initProcessing(n)`: `curva = va`; `calcCv(n)` runs ONLY when outlier
  detection is on AND `dcmpl(curva, 0.0) == 0` (the user left the critical
  value at 0; a NaN curva does NOT trigger, SECTION 25 correction). `n` is
  the ACTUAL (non-missing) observation count — `kr_missing` pins it 94 for
  a 96-point series (init bits 3.4099999999999997). `needAutoModelling` is
  initialised FALSE by `initProcessing` even when `usingAutoModel` is on
  (the first iterate skips diff/arma; the round-0 block re-arms it).
* iterate head logs `step "round " + pass` (not `round` — `q_air`'s pass-3
  entry prints `round 3` while the ladder counts iterate #4; the monthly
  `kr_gs2` third head prints `round 2`).
* guards (`needDifferencing`/`needAutoModelling`/`needOutliers`, bytecode
  1251/1311/1300): `g = round == 2 && outliers empty`;
  differencing `= needAuto && !g && (d < 2 || bd >= 1)`;
  auto-modelling `= needAuto && !g`;
  outliers `= isOutliersDetection() && needOut` — the outliers site has NO
  round-2 guard. `kr_tr1_air` (autoModel 0) witnesses the whole skip set
  (no diff/arma/gate/scontroller and an EMPTY controller list, scontroller
  null — the constructor else-branch).
* change accumulator: the differencing result is an ASSIGNMENT
  (`changed = res == Changed`), the arma site OVERWRITES it with
  `!(spec.equals(orders0) && mean == mean0)`, the scan result ORs in
  (`changed |= res == Changed`). The pass-1 rewrite of the difference order
  additionally removes the outlier regressors and invalidates the
  estimation (`diffdrop` fact; 19 corpus rows).
* the scan site first clears the outlier regressors whenever the model
  carries any (26.5 — every corpus round feeds the scan a clean design);
  the scan fact must carry the kernel's current `curva`.
* `estimateModel(m)`: the ladder keeps ONE persistent counter across the
  re-estimates of a single call (`while (true) { ++i; … }`, retry sites
  `retry1..`; 37 corpus rows exercise the mean test at the pinned 1.96).
  A failed estimation returns false, with the pass-1
  `curva = reduceVa(curva)` arm fired only under `outliers && ami`
  (implemented; `estmodel ret 0` never fires in the monthly corpus —
  documented-qualified by the landed retry machine). `pass3 || pass != 0`
  short-circuits true after a successful estimation.
* round-0 block (`round == 0 && (ami || od)`): re-arms `needOut`/`needAuto`
  from the spec, `++round; ++pass`, snapshots `refAuto`/`refStats`, then the
  landed `tramo_round0_accepts(lbp, pcr, acceptAirline)` rule; accept ends
  the driver SILENTLY (no `iterate done` — `kr_accept_air`, the only
  accept=1 row).
* the refStats gate is entered as `pass2(!changed, m)` — the negated
  accumulator is the operand (witness `q_air` pass 2: a changed arma model
  with `needAuto = 1` still passes `false`). It runs only when
  `pass <= 3 && !pass3 && usingAutoModel`. All gate arithmetic (rules A–D,
  the shape guards, the skip on `nr > nb`, the pcr ladder, the pass-1
  reduceVa remark, the last-chance rewrite) is the landed 23-E
  `tramo_ref_stats_gate` verbatim; the replay cross-checks the reverted bit,
  the `skip-nrnb` echo (2 rows: `kr_ml`, `kr_tc09`) and the four gate-line
  operand bits (`lbp se rlbp rse`, 34 rows) against it. Dated correction
  (SECTION 31): the Scratch23r manual driver transcribes rules C/D with a
  deviation that no corpus row can discriminate (all 34 monthly gate rows
  revert 0); the unit keeps the 23-E adjudication, which fresh javap
  re-confirmed byte-correct — the corpus bytes are untouched.
* a gate KEEP refreshes `refAuto`/`refStats` from the fresh row; the last-
  chance rewrite (pass > 2) logs `warn "last chance model"`, rewrites the
  SPECIFICATION (never the arima operand the guards read — the driver
  applies the landed `tramo_last_solution_orders` to the specification),
  clears the outliers, pins `round` to 1 and invalidates the estimation
  (`q_air` pass-3 re-estimate witness; implemented, unwitnessed monthly).
  A gate REVERT restores `refAuto` (implemented, never fires monthly — all
  34 echoes keep).
* the raw mean test (`tsig` from the spec) failing sets `pass = 4` and
  kills both `needAuto`/`needOut` and continues (regfail end; implemented,
  unwitnessed monthly).
* the seasonality controller runs only under `usingAutoModel`; its Changed
  answer arms pass3 (`pass3 = true; pass = 1; needAuto = true;
  needOut = od`) and continues with the `pass3reset` end (`kr_gs2`,
  `kr_shortfail`). Its context writes (`seasonal = 1`,
  `originalSeasonalityTest = 3`) ride as fact fields.
* the controller sweep applies the constructor list (scontroller +
  `RegularUnderDifferencingTest`/`SeasonalUnderDifferencingTest`/
  `ModelBenchmarking` under `amiCompare`, `TradingDaysController` under
  `tdAuto`, the two `*Test2` passes) and the kernel IGNORES every result;
  only the seasonality controller mutates the round state (`kr_td_auto`
  witnesses the 6-controller sweep and its trailing remark).
* events: the kernel owns `push "tramo" 0`, `push "automatic model
  identification" 1`, `step "round N" 2`, the gate remark
  `reduction of the critical value for outliers detection` and the
  last-chance warning; every module block (`differencing selection`,
  `arma selection`, `outliers selection`, `full estimation`, the
  pre-stage blocks, the controller-sweep prints) rides on its seam fact
  verbatim, wrapper `push 2`/`pop 2` included.

## Witness vs fact (per seam kind)

| seam | unit RECOMPUTES (witness) | rides as fact | JVM echo cross-checked |
|---|---|---|---|
| estimation | taken = estimation-absent state | precision | taken bit |
| differencing | guard entry, assignment, pass-1 drop + outlier clear + invalidation | result, orders/mean/outliers, block events | — |
| arma | guard entry, overwrite operand, pass-3 q:=1 rewrite shape | result, orders/mean | — |
| scan | guard entry, outlier pre-clear, `changed |=`, cv identity | result, state, cv, block events | cv bit |
| estimation_model | ladder shape, `pass3 || pass != 0` short-circuit, failure arm + reduceVa, persistent retry counter | return, pass at call, nnsig, orders, outliers, pars | pass-at-call bit |
| regression_test | site (retry vs iterate), retry index, ladder stop rule, iterate-site `pass = 4` kill | result, mean, tsig, t-stats | tsig + retry index |
| round0 | re-arm, ++round/++pass, accept rule (landed), silent-converge end | refStats row (+ e), accept echo | accept bit, lbp bit |
| pass2_gate | gate entry guard, operand negation, ALL rule arithmetic via the landed 23-E machine, log structure, state transitions | fresh row (+ e), arima orders, arima pars, gate-line operand bits | revert, skip-nrnb, lbp/se bits |
| pass2_return | accepted flag, evolved pcr | ret + pcr echo | both bits |
| seasonality_controller | guard entry, pass3 arm | result, state, ctx after, events | — |
| model_controller | count = spec list length; results ignored | result + state + sweep prints | — |
| iterate_end | end kind from the driver's own path | end echo | end token |

The estimate/needEstimation state machine (build → absent; estimation,
diff-drop, scan removal, lastSolution, common-roots failure → transitions)
is recomputed and checked against EVERY `estimate taken` fact (38 taken-0
and taken-1 rows agree).

## D3 flip (closes the 23-F.2 SECTION 28.2 flip table)

The three 23-B `divergent = 1` final-state rows become POSITIVE
multi-round assertions on this unit (asserted in
`tests/test_tramo_kernel_rounds.cpp`, replayed in both gate lanes):

| 23-B row | F.3 witness | replayed shape |
|---|---|---|
| `sc_so` (emptied picks) | `kr_scso` | 2 iterates (round-0 gate → pass-1 rewrite → done) |
| `sc_spanout` | `kr_spanout` | 2 iterates |
| `sc_spannone` (collapsed scan) | `kr_spannone` | 2 iterates |

plus the F.2 divergent witnesses riding the same ladder: `kr_gs2` and
`kr_shortfail` close their first pass with the pass3 reset and finish
3-iterate runs. Roadmap decision D3 executes option A exactly.

## Implemented, unwitnessed monthly (documented-qualified)

`armaq1` (pass >= 3 zero-parameter q:=1 rewrite), the `estmodel ret 0`
failure arm and its pass-1 reduceVa, the gate `first` form (refAuto null),
the gate revert/restore, the last-chance rewrite, the regfail and
estmodelfail ends — each fires in the bytecode per the SECTION 25/26.4/30.8
decode (and several in the quarterly `q_air` trace, which the unit refuses
by D11) but never inside the 34 monthly replays. The fail-closed throws
qualify them replay-wise: a fabricated fact at any of these sites throws.

## Gates

Fresh ctest **78/78 in BOTH lanes** on exclusive fresh trees (lane-OMP
2854.46 s, true-serial `KALOS_OPENMP=OFF` 2892.61 s; TIMEOUT 3600;
`test_tramo_kernel_rounds` Passed both). ASan/UBSan dedicated
`KALOS_SANITIZERS=ON` build: full replay green, zero sanitizer reports.
All twenty-two existing verify sweeps rc=0 at their recorded counts (zero
drift) plus this sweep: `verify_tramo_kernel_rounds` PASS, 35 cases,
1641 checks, 0 failures — raw-corpus cross-qualification (identity flags,
init bits, every jar event, every rsnap field, the seam census, the
gate/round0/pass2 echo bits, the stats census), then the full replay and
the bit-exact Ljung-Box recompute from the captured e-arrays (65/65).
`check-manifest.py` 185 core / 112 fixture / 320 evidence;
`verify-migration.py` unchanged at 406; the evidence `SHA256SUMS.csv`
replays 10/10 (corpus not re-captured); shim-invariance battery post-lane
**25/25 ledger cells plain=0 ledger100=PASS INVARIANT** with
`test_tramo_kernel_rounds` as the 25th cell (lapack1 informational
divergences unchanged from the declared profile).
