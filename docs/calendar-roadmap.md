# Calendar roadmap — item 21-I (calendar, Easter, and user-variable detection)

Planning decomposition of the deferred 21-I surface of item 21 (and of the
calendar branches it blocks in items 23/24) into deliverable slices, in the
shape of [item21-roadmap.md](item21-roadmap.md) and
[tramo-seats-roadmap.md](tramo-seats-roadmap.md). Everything below is
grounded in the pinned rjd3 3.9.0 jar family (`jdplus-{toolkit,sa,x13,
tramoseats}-base-{api,core}-3.9.0.jar`) via `javap -p -c` dumps
(`tmp_opencode/jdprobe/i21/`, 66 dumps) and LIVE JVM probes run 2026-11-22
(`tmp_opencode/i21probe/`, every capture double-produced in two independent
OpenJDK 21 JVMs, byte-identical; session record `tmp_opencode/
kalos-21i-pinning.md`). The R-lane template bytes were re-verified the same
day (rjd3x13 `x13_spec` × 2 JVM processes, byte-equal to the pinned
`m4gap-dec-A/proto-*.txt` and to the jar-side `X13Spec.fromString` bytes).
Nothing here is implemented — this is the plan, its dependency order, its
per-slice commit boundaries, and the decision boundaries needing maintainer
sign-off. It resolves the open 21 roadmap decision 9 ("calendar-detection and
user-variable modules inside item 21 or separate") toward a separate 21-I
queue, and gives the 23/24 blocking table (tramo-seats-roadmap §Dependencies)
its long-sought content: WHAT the 21-I surface is, machine by machine.

## The two calendar machines (probe-verified)

The pinned stack contains TWO independent calendar-detection families that
share only the column-generation layer:

**A. X-13 regAIC battery (sa jar, drives rsa4/rsa5c through
`X13SpecDecoder → RegArimaKernel.calendarTest/easterTest → regAIC`)** — an
AICc-comparator search, NOT F-based:

- `CalendarEffectsDetectionModule` (builder carries `AICcComparator` with the
  proto-spec margin `RegressionSpec.getAicDiff()`): compares the no-calendar
  base model against exactly one candidate — `td2` for rsa4, `td7` for rsa5c —
  plus the spec's LoP/Easter handling per `TEST_REMOVE`/`TEST_ADD` semantics
  (M4-pinned: under `TEST_REMOVE` the td columns are dropped after selection
  but leap-year/easter remain ESTIMATED; `TEST_ADD` = 3-candidate search).
- `EasterDetectionModule`: candidates `{easter [1], easter [8], easter [15]}`
  against the no-easter base (`getLl0()`), best-of via the same comparator;
  `best=−1` means keep none.
- Both re-fire inside every estimation round: rsa4 on a TD-effect series
  emits 3×`AutomaticTradingRegressionModule$Info` + 3×`EasterDetectionModule
  $Info` events (round 0, after AMI, final); on a calendar-free series 1+1
  (nothing kept → no re-round).
- Live class-load proof (rsa4 AND rsa5c, clean and TD-effect series):
  `CalendarEffectsDetectionModule`, `EasterDetectionModule`,
  `AutomaticTradingRegressionModule`, `AICcComparator` LOADED;
  `jdplus.toolkit.base.core.dstats.F` NEVER loaded;
  `TradingDaysController`/`Automatic{F,Wald}RegressionTest` NEVER loaded.

**B. TRAMO controllers (tramoseats jar, a FORK of family A)** — the FTEST
machine, driven by `TramoKernel` + `TradingDaysController` + the AMI's
`DefaultRegressionTest`/`FastRegressionTest`:

- `TramoKernel` ctor adds `new TradingDaysController(TramoModelBuilder.td(
  spec, DayClustering.TD2, ctx), spec.getProbabilityForFTest())` whenever the
  TD spec `isAutomatic()` (TRfull: `automaticMethod=FTEST`, p=0.01); the
  controller gates on `Ols(...).Ftest().getPvalue()` vs `ptd`.
- `AutomaticFRegressionTest` emits the raw decision oracle
  `$Info{nhp, ss0, ss1, ss6, ssmc1, ssmc6, pftd1, pftd6, dof0, dof1, dof6}`
  with `ssmc1=ss1/(dof1−nhp)`, `F1=(ss0−ss1)/ssmc1`, `F6=(ss0−ss6)/
  (ssmc6·6.0)`, `pftd = F(Fx, dof−nhp, Lower)` (guard F≤0 → p=0), select six contrasts when `pftd6 > pftd1` and `pftd6 > 1 − fpvalue`;
  otherwise select one contrast when `pftd1 >= 1 − fpvalue`. `nhp` = `ArimaSpec.freeParametersCount()` of the base
  test model (measured 2).
- Live discrimination confirmed on the probe corpus: TD-effect series →
  pftd1=pftd6=1.0 → td kept (`td` dim=1 + `lp` in the final model); clean
  series → pftd1=0.8798…, pftd6=0.3065… < 0.99 → all calendar removed.
- `AutomaticTRegressionTest`/`AutomaticWaldRegressionTest` are the sibling
  auto-methods (Wald pvalue via `ConcentratedLikelihoodWithMissing.tstat
  (I,I,Z)`; X-13-side twins `AutomaticTradingDaysRegressionTest`/
  `AutomaticTradingDaysWaldTest` use thresholds `autoPvalue1`/`autoPvalue2`).
  The pvalue kernel is the SAME `dstats.F`/`SpecialFunctions.FProbability`
  that kalos 23-C.2 already mirrors as `detail::f_upper_p` (Lower tail =
  1 − f_upper_p).

**Column generation (shared, toolkit jar)** — pinned live matrices for the
whole surface: `GenericTradingDaysFactory` (raw `tdCount` 7 columns,
`meanDays` 12×7 long-run means), contrasts/raw for all nine `DayClustering`s
(TD2/2c/2d/3/3c/4/4c/6/7 + TDuser, dims 1/1/1/2/2/3/3/5/6), `LengthOfPeriod`
(LeapYear = +0.75 in Feb of leap years; LengthOfPeriod = days − domain mean),
`Easter.easter/easter2/julianEaster/probEaster` 1985–2015, `EasterVariable`
durations {1,5,8,15} × corrections {Simple (builder DEFAULT), None},
`JulianEasterVariable` (gregorian/julian mixture), `Ramp` (sign convention:
−1 before start, linear to 0 at end — the NEGATIVE staircase), and
`UserVariables` (out-of-domain cells fill 0.0, no length check, description =
group name). NAME conventions pinned: regression name always `td`/`lp`
(`ITradingDaysVariable.defaultName(k)` ignores k), candidate tags
`td2…td7/tduser`, `easter [8]`, `julian easter [d]`, `ramp a / b`.

**User-variable/holiday DETECTION does not exist in the jar**: there is no
`UserVariablesDetectionModule` class in any 3.9.0 jar (jar-listed); user
variables and holidays are pure pass-through columns. 21-I therefore retires
the "user detection" half of the 21-I label as a no-op spec statement, and
holiday-corrected TD (`HolidaysCorrectedTradingDays`, stock days, TDuser)
stays fail-closed unless a corpus demands it.

## Liveness matrix (class-load + event evidence, double-captured)

| run (input) | CEDM | EasterDet | ATD module | AICcComparator | TradingDaysCtrl | AutoFRegressionTest | dstats.F | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| X-13 rsa4 clean (96 obs) | loaded | loaded | loaded+Info | loaded | — | — | **not loaded** | AICc battery, all rejected (best 0/−1) |
| X-13 rsa4 tdeff (156 obs) | loaded | loaded | loaded+Info ×3 | loaded | — | — | **not loaded** | td2 kept, re-fires 3× |
| X-13 rsa5c clean/tdeff | loaded | loaded | loaded+Info | loaded | — | — | **not loaded** | [no td, td7] battery |
| TRfull tdeff | — | — | — | — | **loaded** | **loaded** | **loaded** | FTEST keeps td+lp |
| TRfull clean | — | — | — | — | **loaded** | **loaded** | **loaded** | FTEST drops all calendar |
| TR4 / TR2 tdeff | — | — | — | — | — | — | loaded | no controller; AMI `DefaultRegressionTest`/`FastRegressionTest` path |
| TR5 tdeff | — | — | — | — | — | — | loaded | pinned-jar NPE in `ModelComparator.compare` (AMI stage, not calendar; decision I-6) |

Inventories: 473–786 jdplus classes per run under `-Xlog:class+load`
(`tmp_opencode/i21probe/out/cl-*.txt`); `dstats.T`/`Chi2` load everywhere
(`ModellingUtility`/regression-test machinery), `dstats.F` distinguishes the
families exactly. Probe caveat: the OTHER family's `$Info` classes appear in
every inventory (probe `instanceof` preloading) — only the main-class and
`dstats` rows above are load-path evidence.

## Reuse / fork table

| Machine | kalos status | 21-I disposition |
| --- | --- | --- |
| AICc + likelihood comparison | LANDED (`transformation_decision.hpp`, `arma_refinement.hpp`, `regression_likelihood`) | REUSE — family A needs only candidate registration + comparator margin `getAicDiff()` |
| F upper-tail pvalue | LANDED 23-C.2 (`detail::f_upper_p`, incBeta) | REUSE for family B (Lower = 1 − f_upper_p); family A needs NO F at all |
| Wald statistic | not landed | DECODED 2026-11-23 (21-I.3 probe, pinning §9): `ConcentratedLikelihoodWithMissing.tstat(I,I,Z)` is pure algebra — `sqrt((vcov·ssq)/dof)`; no distribution kernel. Every p-value across all six calendar machines reduces to `SpecialFunctions.FProbability` = incBeta = the landed 23-C.2 `f_upper_p` (FTEST pftd = 1−upper-tail). **Zero new distribution kernels in 21-I.3**; t-thresholds compare |t| directly (no t-CDF referenced anywhere) |
| OLS + `Ftest()` assembly | LANDED (23-C surfaces) | REUSE; only the `AutomaticFRegressionTest$Info` bookkeeping is new |
| TD/LoP/Easter/Ramp/User columns | NOT landed (23-A refuses everything but static TD2) | NEW unit (21-I.1), byte-for-byte from the gen capture |
| Builder registration (`ami`/`regeffect` attrs, names) | pattern landed (23-A) | FOLLOW pattern, pin measured names (`td`, `lp`, `easter [8]`, …) |
| TRAMO vs X-13 modules | — | FORK-PRESERVING: two module families in two namespaces; do NOT unify (decision I-3) |

## 21-I slice ledger (recommended order: 21-I.1 → 21-I.2 → 21-I.3 → 21-I.4)

| Slice | Unit (planned) | JVM oracle entry point | Corpus (pinned) | Size estimate |
| --- | --- | --- | --- | --- |
| 21-I.1 generation | `calendar_generation` (tdCount/meanDays/contrasts+raw for all clusterings, LoP, Easter computus + EasterVariable/JulianEasterVariable, Ramp, UserVariables pass-through, names) | `I21Probe gen` (`GenericTradingDaysFactory`, `Easter`, `Regression.matrix` raw getters) | `gen-.out` 4128 lines: 108-row tdCount, 9×(contrast+raw) full matrices, LoP 2×108, Easter 1985–2015, 4 durations ×2 corrections + julian, ramp/ramp2, user 0-fill, quarterly td2 | ~900 lines new unit + ~1200 oracle checks (matrix replay is data-driven from the capture) |
| 21-I.2 X-13 regAIC calendar battery | `calendar_effects_detection` (CalendarEffectsDetectionModule + EasterDetectionModule candidates, AICcComparator margin, per-round re-fire, TEST_REMOVE/TEST_ADD semantics) | pinned proto bytes → `X13.specOf` → `X13Kernel.process(observer)` | `x13run-rsa4/5c-{clean,tdeff}.out` (141/141/227/230 lines): ATD/EAS Info ll/ssq/bicc arrays + best indices + final variable lists | ~600 lines + replay of the 8 Info events (ll0 + per-candidate arrays, `%.17g`) |
| 21-I.3 TRAMO calendar controller | `tramo_trading_days_controller` (`TradingDaysController` gate, `Automatic{F,T,Wald}RegressionTest` builder, FTEST Info math on the landed OLS + `f_upper_p`) | `TramoSpec.fromString("TRfull")` → `TramoKernel.process(observer)` | `tramo-TRfull-{tdeff,clean}.out` (213/141 lines): full FT Info raw doubles, gate outcomes, final variable lists; TR2/TR4 controls (no controller) | ~500 lines + 2×9 doubles bit-exact Info replay + gate decisions |
| 21-I.4 integration + refusal retirement | rsa4/rsa5c template calendar-block expansion (248-byte rows), end-to-end detection replay, `persephone3` refusal ladder | both lanes (R-lane bytes re-verified identical) | template bytes (m4gap + `x13tpl-.out` 193 lines) + the four x13run captures as end-to-end rows | ~300 lines + template byte checks; retires refusals per decision I-8 ladder |

**Probe dossier 2026-11-28** ([`item21-calendar-detection-probe-2026-11-28.md`](item21-calendar-detection-probe-2026-11-28.md)):
live three-layer refusal inventory plus a nine-fit JVM decision battery
(`rsa3/rsa4/rsa5c × clean/tdeff/air`, double captures byte-identical) that
re-confirms I-2 (`dstats.F` never loads) and CORRECTS the 21-I.2 replay
statistic: `AICcComparator` compares `getAICC()` with strict `<` at
`aicDiff = 0.0` (bytecode-proven; the row's "bicc arrays" are probe-printed
BICc, which is NOT the selection statistic — it mis-ranked the td7 candidate
on `rsa5c/air`). Measured margins on AICC: TD decisions robust (all |margin|
≥ 2.0 raw units), Easter on a thin surface (min |margin| 0.769, with a
measured within-fit out→in→out round flip on `rsa5c/tdeff`); calendar-free
`clean` landings are bit-identical to rsa3 while calendar-firing landings move
every component. Four priced options (close / I-8-step-1 / TD-staged / full)
and per-slice acceptance budgets await maintainer choice; option (iii) alone
cannot admit rsa4/rsa5c (both templates always request Easter).

**21-I.1 LANDED 2026-11-22** as `kalos::calendar_generation`
(`include/kalos/calendar_generation.hpp`, ctest #75
`test_calendar_generation`). The envelope-shape oracle `gen2-.out` was added
and double-captured next to the primary `gen-.out`; both now live as the
committed evidence `validation/evidence/native/calendar-generation/`. All
five 21-I.0 recommendations that this slice could apply were applied per the
register: I-5 (generation covers julian + all duration/loP/quarterly shapes;
the {none, 1, 8, 15} detection batteries stay out of scope) and I-9 (user
0-fill and the ramp sign convention pinned; the "reversed" ramp is the
ALL-ZERO column — see the contract's machine list). Raw/contrast bit-identity
and every machine correction are recorded in
`docs/contracts/calendar-generation-contract.md`.

Non-goals: holiday-corrected TD / stock days / TDuser columns (no detection
module exists; keep the 23-A refusals), `DayClustering.toString()` (jar bug,
throws — never port), user-variable *detection* (does not exist), quarterly
Easter/TD beyond pinning the one measured quarterly row (D11 precedent).

## Decision register draft (sign-off before the owning slice)

| # | Decision | Slice | Recommendation |
| --- | --- | --- | --- |
| I-1 | Ratify item-21 decision 9 as "separate 21-I queue" with this ledger | all | yes — the decoder modules are separable and both engines share the column layer |
| I-2 | X-13 rsa4/rsa5c calendar detection = AICc battery ONLY (no F/Wald anywhere in the regAIC calendar path) | 21-I.2 | **APPROVED 2026-11-22** — `dstats.F` never loads on these paths (liveness matrix) |
| I-3 | Preserve the TRAMO/X-13 module FORK (two namespaces, shared column layer, no second TD implementation — honors 23/24 decision D10) | 21-I.2/3 | yes |
| I-4 | Auto-method TD tests beyond FTEST (T/Wald; X-13 `AutomaticTradingDays{,Wald}Test` with autoPvalue1/2) inside the first envelope? | 21-I.3 | **MAINTAINER RULING 2026-11-22 — OVERRIDE: T/Wald IN SCOPE NOW.** Port the `AutomaticTradingDays{,Wald}Test` controllers + the `ConcentratedLikelihoodWithMissing.tstat(I,I,Z)` Wald/T kernel in 21-I.3 rather than fail-closing them; FTEST still lands first within the slice, T/Wald with their own double-captured oracles in the same slice (not deferred). Rationale: avoid a second re-entry into the controller code path; the small Wald/T kernel qualifies cheaply alongside F |
| I-5 | Julian Easter and durations other than the template-pinned {1,8,15} candidates | 21-I.1/2 | generation may land them (matrices pinned); detection candidates stay {none,1,8,15}; julian detection refused (no live caller) |
| I-6 | TR5 + strong-TD input class: pinned jar NPEs in `ModelComparator.compare` (AMI, not calendar) | 21-I.3 (record for 23) | **APPROVED 2026-11-22** — do NOT emulate the crash; record and let the 23 envelope fail-closed on the input class |
| I-7 | Detection re-fires per estimation round (3 rounds observed) — integration contract replays per-round, not once per fit | 21-I.2/4 | yes — pin the round-conditional firing |
| I-8 | Refusal-retirement ladder for the `21-I` refusal in `persephone3` ("x13 automatic fit defers calendar/easter/TD/user detection") | 21-I.4 | **APPROVED 2026-11-22** — retire in three steps: calendar-free rsa4/5c passes → TD-effect passes → full parity flag; holidays/user-TD/julian stay refused |
| I-9 | UserVariables out-of-domain 0-fill and the negative-Ramp sign convention are pinned semantics (accept, document, unit-test) | 21-I.1 | yes — measured, deterministic |
| I-10 | TRAMO auto-methods AIC/BIC/WALD on strong-TD inputs: pinned jar NPEs in `RegularUnderDifferencingTest2.process` (post-ATD, AMI family — same class as the I-6 TR5 crash; reproduced byte-identical in two JVMs, 21-I.3 decode §9.6) | 21-I.3 | recommend per I-6 precedent: do NOT emulate the crash; fail-closed on the input class with pinned reason — pending maintainer confirmation 2026-11-23 |

Decode errata (2026-11-23, 21-I.3 probe, full record in `tmp_opencode/kalos-21i-pinning.md` §9): X-13 `autoPvalue1/2` spec defaults are **0.01/0.1** (earlier working notes said 0.9/0.001 — wrong; jar evidence in §9); TRAMO Wald `pconstraint` is hardcoded 0.1; TRAMO auto-methods override `estimationPrecision` to the builder default 0.001 (AmiOptions.intermediatePrecision); TRfull `lptype` is `LeapYear`; the TRAMO/X-13 alltd candidate lists differ in ORDER (sa puts TD2c first) — fork-preserving per I-3.

## Monthly TRAMO F-test candidate comparison

`tramo_calendar_f_test` estimates the base model and both calendar alternatives
for monthly observations on the model's estimation scale. Each candidate uses
an airline SARIMA with two free MA coefficients, with or without a mean. The
entry point accepts the first observation's year and month, a significance
level and estimation precision. It returns the three residual sums of squares,
the two candidates' degrees of freedom, mean squares and lower-tail probabilities,
and the selected trading-day grouping. The six-contrast test is strict at the
threshold; the one-contrast test includes equality and wins a tie between two
significant candidates.

The reference runs the public `AutomaticFRegressionTest` module on four dated
series and reads both its likelihoods and its output model. Its 14 calls include
both mean settings, two exact cutoffs with neighbouring probabilities, a tie
at probability one, and all three selection outcomes. Reproduce the reference
with `validation/reference/capture-tramo-seats-bridge.py --stage calendar-ftest`
and the pinned x86-64 Java 21.0.11 and 3.9.0 jar directories.
[Calendar F-test evidence](../validation/evidence/native/tramo-calendar-ftest/panel.txt)

The stock `tramo_full_initial_calendar` entry also performs the selected
model's leap-year, Easter and mean tests. It starts with the unadjusted
monthly airline description and its mean, optionally log-transforms the
observations, and uses the template's estimated Easter column (six days,
end position zero, simple centering). The selected TD model includes the
leap-year column in the final refit. With no trading days, the base
model supplies the mean and Easter tests. The thresholds are 1.96 for
the mean, 2.0 for leap year and 2.2 for Easter. The module's processing
status also matches the reference when adding trading days alone.

The stock F-test processor from 3.9.0 uses 1e−5 precision. The separate
configurable comparison above was qualified at 1e−3. The 20 level/log
stock-template cases reproduce the candidate likelihoods, probabilities,
selected-model t statistics, retained variables and processing status.
The `trfull-calendar` capture stage replays the locked template factory
and its real regression module on two JVMs.
[Stock TRfull calendar evidence](../validation/evidence/native/tramo-calendar-ftest/full.txt)

The late `TradingDaysController` tests the fitted model's residuals for remaining
working-day effects. `tramo_residual_trading_days` aligns those residuals to the
end of the estimation domain and fits the single TD2 contrast by OLS without an
intercept. It returns the F statistic, upper-tail probability and decision to
refit. A probability exactly at the threshold leaves the current fit in place.
The 32 reference tests include both sides of that boundary across eight fitted
descriptions and four calendar starts. A zero residual vector produces no test.
[Residual trading-day decisions](../validation/evidence/native/tramo-calendar-ftest/residual-td.txt)

For a monthly airline with optional mean and outlier detection disabled,
`tramo_airline_trading_days_controller` also fits the starting model, fits the
TD2 candidate after a positive residual test, retests the mean at 1.96 and uses
the default model comparator to retain or reject the candidate. The 32 cases
exercise skipped refits, rejected candidates, accepted candidates and a second
refit after dropping the mean. The 272-month cases also exercise overlapping
samples in the residual stability tests. Inputs with existing regressors and
automatic outlier detection, and end-to-end stock TRfull, remain open.
[Candidate fits and selected models](../validation/evidence/native/tramo-calendar-ftest/td-controller.txt)

## What this unblocks (downstream map)

- **23-E calendar branch** (TRfull `td(spec, TD2)` + controller): unblocked by
  21-I.1 + 21-I.3 — the FTEST machine is fully specified with raw-value
  oracles.
- **23-G TRfull-FTEST part**: same; the "F-based trading days tests" trace
  insertion is 21-I.3 replay evidence.
- **24-E rsafull leg**: needs 23-E/23-G, hence 21-I.1/.3.
- **X-13 rsa4/rsa5c gap (M4 refusal)**: needs 21-I.1 + 21-I.2 (+ template
  bytes, already pinned) — the calendar half of the refusal becomes
  implementable end to end; the automatic-TD *auto-method* half stays gated
  per I-4.
- **21-H full-auto consumer**: the calendar/easter branches of regAIC land
  with 21-I.2 without touching the landed spine.

## Acceptance rules (item-level, inherited)

Every 21-I unit is pinned against the double-captured JVM evidence in
`tmp_opencode/i21probe/out/capA/` (shas in the pinning notes §6): raw getter
doubles bit-exact at `%.17g`, matrix rows byte-for-byte, decision indices and
gate outcomes exact, class-load facts cited (never assumed). Corpus replays
run in both rkalos lanes; no new tunables — thresholds arrive as spec data
(`getAicDiff()`, `probabilityForFTest`), never as core constants.
