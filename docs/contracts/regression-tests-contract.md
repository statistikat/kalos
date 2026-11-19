# Regression variable tests and mean rules — the stage-R decision machines (21-G)

Item 21-G qualifies the live stage-R decision code of the pinned jdplus 3.9.0
pipeline as one native unit trio in `include/kalos/
regression_variable_tests.hpp` / `src/regression_variable_tests.cpp`:

* `run_regression_variable_tests()` - a literal transcription of
  `jdplus.toolkit.base.core.regsarima.regular.RegressionVariablesTest.
  process(RegSarimaModelling)`, the decoder-built machine that the pinned
  kernel runs twice per round-0 fit (`regressionTest0` after the first
  outlier-driven re-estimate, `regressionTest1` after auto-modelling);
* `run_mean_controller()` - the transcription of
  `jdplus.x13.base.core.x13.regarima.MeanController.test`, the `checkMu`
  machine the kernel calls with cutoff 1.96 after the round-0 regAIC and
  with 1.6 inside every `redoEstimation` round while the mean is on;
* `final_mean_rule_adds_mean()` - the kernel's last statement before the
  final estimate: `isAutoModelling() && !desc.isMean() && options.checkMu
  && rtval0 > 2.5 -> setMean(true) + clearEstimation()` (rtval0 =
  `ModelController(0.95).getRTval()`).

Together with the transcribed statistics machine (`RegressionTestStatistics::
tstats/tstat`, `derived_test_tstat`, `t_test_accepts`) this covers every
decision the pinned stage R takes over an already-estimated description. The
units feed - and are not yet wired into - the fit seams; the round-structure
wiring (when each machine fires, the re-estimations, the outlier-fix
interaction) is stage Z (21-H). There is no Rcpp boundary; the unit is
C++-only.

## Scope resolution (the slice's NOTE: user variables vs outlier integration)

The slice note offered two candidate meanings of "stage R": (a) user-variable
significance testing/elimination and (b) the outlier-fix integration into the
regression. The bytecode says neither is stage-R code:

* (a) `RegressionVariablesTest.process` filters its three passes with
  `ModellingUtility::isTradingDays`, `::isLengthOfPeriod` and
  `::isMovingHoliday` (invokedynamics #0/#1/#2) and never `isUser`; user
  variables are inert there. User-variable detection is the separate
  `UserVariablesDetectionModule`, a decoder module the roadmap already
  defers to optional 21-I and dead on the default decoder path (no
  `UserVariablesDetectionSpec` in the default `RegArimaSpec`).
* (b) Selected outliers are re-added as `Variable`s inside
  `OutliersDetectionModule.process` (stage O, pinned by 21-C's seam); the
  kernel's outlier/regression interaction (the only live `removeVariable`
  round-path is the `isOutlier(v, true)` sweep in `RegArimaKernel.calc`'s
  post-automodelling lambda) is round structure, owned by 21-H. No live
  stage-R class touches outlier variables.

The headline return item of this slice is therefore the qualification of the
live stage R described above; (a) and (b) stay with 21-I and 21-H.

## Pinned constants and provenance

Everything the machines hold is either required request data (below) or a
transcription constant with the ledger evidence recorded here (standing rule
of item 21: pinned values are transcription constants, never configuration;
roadmap decisions 5 and 6 stay open).

| Constant | Value | Provenance (jdplus 3.9.0) |
| --- | --- | --- |
| initial meanTest/tdTest/mhTest cutoff | 1.96 | `X13SpecDecoder.readRegression`: `tdTest(1.96, true)` iff the TD regression-test spec is active, `movingHolidaysTest(1.96)` iff the Easter test is active, `meanTest(1.96)`; `RegressionTestSpec.DEFAULT` cutoffs |
| final meanTest cutoff | 1.0 | decoder re-uses the same builder and overwrites `meanTest(1.0)` for `regressionTest1` |
| joint TD cutoff ftd | 0 (dead path) | builder defaults all-zero; the decoder never calls `tdJointTest`, so `FRegressionTest` is unreachable at every pinned call site - the unit rejects `td_joint != 0` fail-closed |
| round-0 checkMu cval | 1.96 | `MeanController.CVAL0` (constant pool `1.96d`), `RegArimaKernel.calc` calls `checkMu(m, 1.96)` |
| redoEstimation checkMu cval | 1.6 | `MeanController.CVAL1` (`1.6d`), `redoEstimation` bytecode 16-31 |
| final mean rule cutoff | 2.5 | `MeanController.CVALFINAL` (`2.5d`), `RegArimaKernel.calc` 648-695, strict `>` (dcmpl ifle) |
| mean-added estimation precision | 1.0E-5 | `MeanController.test` field `eps = 1.0E-5d` into `RegArimaUtility.processor(true, eps)`; lives at the request-data boundary (21-H estimation), pinned only to document the row provenance |
| `TRegressionTest` single-arg form | `tlow = thigh = t`, `nsig = 1` | constructor bytecode 4-19; all stage-R tests use it |
| `DerivedRegressionTest` construction | `(ttd, ml = true)` | `RegressionVariablesTest.<init>`; ml=true pins dof = `cl.dim()` |
| derived accept | `abs(tcur) > tsig` | `DerivedRegressionTest.accept` (strict, dcmpl ifle) |
| t accept | `high > 0 \|\| low >= min(nsig, dim)` | `TRegressionTest.accept` 87-111 |

## Request data (required, no defaults)

* `RegressionTestStatistics`: the estimation's `coefficients()` (nx >= 0 -
  the corpus contains estimated models with nx = 0: mean off and no
  regressors), the row-major `unscaledCovariance()` (nx x nx), `ssq()` and
  `dim()`. The transcribed forms are the ones every stage-R call site uses:
  `tstats(0, false)` (dof = dim, no zero guards) and the scalar
  `tstat(0, 0, false)` with its two pinned guards (a zero covariance entry
  makes `ser()` NaN; a zero coefficient makes `ser()` 0 so `tstat()` divides
  0 by 0 - both give a non-significant mean).
* `RegressionVariableLayout`: per `desc.variables()` entry the kind (the
  three `ModellingUtility` predicates; user/other are inert), `position`
  (`desc.findPosition(core)` at process time) and `dim` (`core.dim()`). A
  negative findPosition (-1, never reachable at live call sites) is a
  static request error instead of the JVM's AIOOBE.
* Description state: `mean` (`isMean()`; mean on implies nx >= 1),
  `adjusted` (`isAdjusted() = log && lpTransformation != None`) and
  `preadjustment` (0 None, 1 LeapYear, 2 GoodFriday).
* `MeanControllerRequest`: the same statistics of the estimation the machine
  consumes - the current one when the mean flag is on, otherwise the
  mean-added one through `RegArimaUtility.processor(true, 1.0E-5)` (the
  21-F precedent: a call-site estimation arrives as data; the estimation
  itself is 21-H wiring) - plus `mean` and the cutoff `cval`.
* `final_mean_rule_adds_mean`: the boolean flags plus the scalar `rtval`
  (the `ModelController` residual-mean z-statistic; its computation is
  21-H round-structure machinery).

## Pinned semantics (the transcription)

`process` reads `cl.tstats(0, false)` once (per accept call in the bytecode;
the values are pure functions of the same statistics, so the transcription
shares the vector). Pass 1 (trading days): a variable whose width-`dim` t
test fails gets a second life through `DerivedRegressionTest(ttd, true)`
when dim > 1; surviving (either way) sets the b flag, otherwise the core is
queued in `removals`. Pass 2 (length of period, tested with width 1): pass
sets the c flag, failure queues the core. Pass 3 (moving holidays, width 1):
failures set `changed` and remove the variable immediately, independent of
the gate. The queued TD/LoP removals fire as a batch only when the queue is
non-empty and b and c were never set (all-fail-or-none); the batch resets
the preadjustment to None when the description is adjusted. The mean is
dropped only when meanTest != null, the mean flag is on and the width-1
test at column 0 fails. `changed` implies `clearEstimation()` and
ProcessingResult.Changed.

`MeanController.test`: sig = `|tstat(0,0,false)| > cval` (strict); `sig ==
mean0` returns Unchanged without touching the description; otherwise
`setMean(sig)` + `clearEstimation()` + Changed.

Final mean rule: `auto_modelling && !mean && check_mu && rtval > 2.5`.

## Documented deviations

* Static validation replaces JVM exceptions: malformed statistics, layout or
  cutoffs throw `std::invalid_argument` before the machine runs; the dead
  `td_joint != 0` branch is rejected fail-closed instead of silently ignored
  (bytecode: `ttd <= 0 && ftd > 0 -> FRegressionTest`, unreachable for the
  pinned decoder). `ProcessingLog` push/pop and the "mean significant
  (added)"/"mean not significant (removed)" remarks are logging, not
  decisions, and are out of scope.
* The JVM `isScalingFactor()` guard (EcoException on scaled statistics)
  becomes a request-data property of qualified inputs; captured corpora are
  all unscaled.
* `findPosition` ordering (`sortVariables`) is a 21-H description concern;
  the unit consumes the JVM-observed positions as data.

## Tiers

There are none: every stage-R decision is bytecode-ordered double arithmetic
over the captured statistics (a left fold for `DoubleSeq.sum`/
`FastMatrix.sum`, one multiply, one divide, one sqrt, one final divide), so
the sweep demands bit identity for all statistics and exactness for all
decisions. IEEE-strict operations and `Math.sqrt` make the C++ transcription
bit-equivalent; the NaN orderings (dcmpl/dcmpg sites) match IEEE relational
operators.

## Corpus and capture

`validation/evidence/native/regression-tests/` (captured by
`validation/reference/capture-regression-tests.R` through the public-API
observer `RegressionTestsProbe.java` - no decision rule is re-implemented in
the capture): 18 double-captured cases (byte-identical CSVs across two
independent OpenJDK 21.0.11 JVM processes, intrinsics disabled per the 21-D.2
ground-truth convention) over period {4, 12}; TD4/TD2 calendar columns
exactly as `X13ModelBuilder` builds them, LoP(LeapYear), one/two Easter
windows (PreComputed correction, endPosition -1), user columns, mean on/off,
log and preadjusted states, d up to 1. Tables: `cases.csv` (inputs, states,
`ModelController` rtval/rvr), `cl.csv` + `ucov.csv` (statistics, %.17g),
`vars.csv` (post-sort layout, positions from `findPosition`), `rvt.csv`
(process outcomes per builder configuration), `obs.csv` (the pinned T/Derived
forms called directly for observation), `mean.csv` (MeanController decisions
plus the MEANSTAT statistics rows, cur and mean-added), `metadata.R`,
`SHA256SUMS.csv`.

The sweep `tools/verify_regression_variable_tests.cpp` replays all of it:
**494 checks, 0 failures** - the `tstats(0, false)` vector per case (bit),
the mean `tstat(0, 0, false)` on 36 MEANSTAT rows (bit), the derived value
and statistic on the 8 OBS rows where the JVM ran the derived form (bit; all
captured derived tests reject, so the derived-rescue branch b = true is
qualified by the unit test), all JVM OBS accept decisions, the four
process configurations per case (changed/mean/preadjustment/remaining, exact,
plus unit-vs-OBS derived identity), the MeanController decisions for both
pinned cutoffs, and the strict-`>` final rule on the 18 captured rtvals.
Decision coverage: all-calendar batch removal (m_allfail, m_derived), the
b-gate with immediate MH removal (m_tdpass), keep-all (m_passall), the LoP
pass/gate combination (m_i2_cal), the adjusted preadjustment reset
(m_adj cfg0/1 vs 2/3), user-variable inertness (m_td2users, m_wn60_two),
mean add (m_meanadd), mean keep (m_meanton), mean removal (m_log_meanoff and
others) and quarterly cases (q_*). The unit test
(`tests/test_regression_variable_tests.cpp`, 56th ctest suite) replays the
m_allfail statistics/decision and the m_meanadd mean-added statistic
bit-exactly and isolates the branches the corpus only reaches in
combination.

## Replay

```sh
cmake -S . -B <build> -DCMAKE_BUILD_TYPE=Release && cmake --build <build> -j
ctest --test-dir <build> -R regression_variable          # unit
c++ -O2 -std=c++17 -Iinclude <build-dir>/generated tools/verify_regression_variable_tests.cpp \
    <build>/libkalos.a <OpenBLAS> -lgomp -lpthread -o /tmp/verify_rvt && /tmp/verify_rvt
# re-capture (OpenJDK 21, intrinsics disabled; two runs into fresh dirs,
# second with --repro=<first> for the byte-identity assertion):
JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 \
_JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog" \
Rscript validation/reference/capture-regression-tests.R --output=<dir>
```
