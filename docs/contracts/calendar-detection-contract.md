# Calendar-detection machines (21-I.3)

Transcription of the automatic calendar-detection layer of the pinned
jdplus 3.9.0 stack: the shared ladder machine
`TradingDaysRegressionComparator` (ladder layout as a pure selection over
hand-over rows, `bestModel`, `waldTest`), the TRAMO machines
`AutomaticRegressionTest` (AIC/BIC), `AutomaticWaldRegressionTest` and
`AutomaticFRegressionTest` (FTEST Info math plus the three-branch walk),
the calendar branches of `FastRegressionTest` / `DefaultRegressionTest`
(the 23-E seam rows, activated here), and the X-13 side machines
`AutomaticTradingDaysRegressionTest` (AIC/BIC) and
`AutomaticTradingDaysWaldTest`, plus the public `JointTest` quadratic form
behind the Default machine's join arm. The bytecode record is
`tmp_opencode/kalos-21i-pinning.md` §§9–10 (§10.1 is the fresh-decode
errata list — "DUMP NOT TRUST": every rule below was re-read from live
`javap` of the pinned jars, not from the earlier working dump).
Everything numeric is bit-exact: decisions AND values.

Boundary (fork-preserving, `docs/calendar-roadmap.md` registers I-3/I-4):
the TRAMO machines read `TramoModelBuilder.calendarAMI` variables while
the sa machines read `ModelBuilder.calendarAMI` ones, and the two
families mutate the tested description with DIFFERENT grammars — both are
transcribed side by side, no fork is merged, and no second trading-day
column implementation exists: calendar columns come from
`calendar_generation.hpp`, estimation results are REQUIRED REQUEST DATA
(21-G precedent) handed over as `TramoTestStatistics` rows, and only the
decision grammar plus pure algebra (Info math, Wald ratios, t/F statistics
over hand-overs, the `JointTest` form, Cholesky/solveLx operand order)
runs here. Every p-value reduces to the landed 23-C.2
`detail::f_upper_p` — **zero new distribution kernels**; the t-thresholds
compare |t| directly (no t-CDF anywhere).

## Coordinate map (module → unit)

| JVM (pinned) | unit entry point |
| --- | --- |
| `TradingDaysRegressionComparator.bestModel` | `kalos::calendar_best_model` |
| `TradingDaysRegressionComparator.waldTest` | `kalos::calendar_wald_test` |
| `AutomaticRegressionTest.test` (TRAMO, AIC/BIC) | `kalos::run_tramo_auto_td_test` |
| `AutomaticWaldRegressionTest.test` (TRAMO, wald) | `kalos::run_tramo_auto_td_test` (selector wald) |
| `AutomaticFRegressionTest.test` (TRAMO, FTEST) | `kalos::run_tramo_ftest_td_test` |
| `AutomaticTradingDaysRegressionTest.test` (sa, AIC/BIC) | `kalos::run_sa_auto_td_test` |
| `AutomaticTradingDaysWaldTest.test` (sa, wald) | `kalos::run_sa_auto_td_test` (selector wald) |
| `FastRegressionTest.test` calendar branch | `kalos::run_tramo_fast_calendar_step` |
| `DefaultRegressionTest.test` calendar branch | `kalos::run_tramo_default_calendar_step` |
| `JointTest.variableSelection().build()` | `kalos::calendar_joint_test` |

The public `calendar_joint_test` entry mirrors the jar's PUBLIC class
exactly as the probe constructs it (`JointTest(cl)
.variableSelection(first, dim).hyperParametersCount(nhp).build()`); the
Default step calls the same internal form on its join arm. The split
matters because the step's bytecode routes a `dim == 1` variable to
`wdTest` EVEN under `joinTest` (fresh javap, offsets 149–166), while the
corpus witnesses a dim-1 form (`ds_join`) built directly — the public
entry consumes it without distorting the machine.

## Capture and corpus

`validation/reference/CalendarDetectionProbe.java` observes only public
entry points (the six machines via their builders, the comparator rows
re-estimated on the live sequence, `JointTest`, and the TRfull/X13
pipeline decoders) against the pinned 17-jar classpath.
`validation/reference/capture-calendar-detection.R` runs ONE JVM PER CASE
(fresh kernel state; the five `loadlog-*` witnesses pin which machine
classes each route actually loads), guards the JVM build (OpenJDK 21) and
the intrinsic ground-truth recipe (`-XX:+UnlockDiagnosticVMOptions
-XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog`, verified against
`StrictMath` bits), and re-emits every golden double through
`Double.doubleToRawLongBits` hex: the pinned JVM's `String.format("%.17g")`
proved NOT bit-faithful against glibc for equal doubles (errata recorded
in the pinning doc), so decimal rendering is trusted only for the
threshold constants in `inputs.csv`, which cross into the golden by
`strtod` bit patterns and are re-cross-checked by the sweep.

The corpus `validation/evidence/native/calendar-detection/` was
DOUBLE-CAPTURED in independent JVM processes; the repro gate reports all
50 data files byte-identical (`SHA256SUMS.csv` verified). 43 cases: 4
comparator (`cm_*`, row aicc/bic keys + `bestModel`/`waldTest` echoes), 7
TRAMO ladder (`ta_*`), 5 FTEST (`ft_*`, Info raw doubles + branch), 7 sa
ladder (`sa_*`, incl. the `saw` keep/swap witness vectors), 4 Fast steps
(`fs_*`), 6 Default steps (`ds_*`, incl. the join arms), 3 `JointTest`
forms (`jt_*`), 4 TRfull pipeline (`pt_*`) and 3 X13 pipeline (`px_*`)
wiring witnesses.

`tools/gen_calendar_detection_golden.py` turns the corpus into the
bit-pattern tables of `tests/jvm_calendar_detection.h` (335 raw-bit
pools); `tests/calendar_detection_replay.h` (shared by ctest
`test_calendar_detection` — the 80th — and the sweep) rebuilds every
request from the captured hand-overs and compares 229 checks
bit-for-bit, including the reconstructed jar post-state variable lists
(`vars_after`) and the `det ATD best=` / `det FT` journal echoes.
`tools/verify_calendar_detection.cpp` adds 320 raw-corpus cross-checks
(inventory and family counts, decimal thresholds re-parsed into bits,
`res` result/mean/est tokens, `det ATD`, `det FT` hex maps, `jt` echoes,
`tdspec` lines) so a corpus edit fails the sweep before the replay runs.

## Decision laws (bytecode → unit; full text in the frozen header)

- `bestModel`: skip leading absent rows, replace while
  `compare(ests[best], ests[cand]) > 0` over present rows (ties keep the
  earlier index); the all-absent `-1` return is unreachable dead code (the
  jar AIOOBEs in the scan loop; the mirror throws `std::out_of_range`,
  machines translate to `failed`).
- `waldTest`: reference is the LAST row (`dof = last.dof - 2`,
  `s2 = last.ssq/dof`); p-loop then the descend/j-loop with the pinned
  dcmpg/dcmpl polarities (NaN enters every comparison as documented);
  a needed p-value that `f_upper_p` cannot produce is the jar's
  `DStatException` → `failed`.
- TRAMO ladders (AR/Wald): rows = comparator layout (row 0 copyOf+airline
  +mean, row 1 +plain lp, rows i+2 +plain td_i); selection guard
  `best < 2 → null, else td[best-2]`, `best < 1 → null, else lp` (§10.1.4);
  update = unconditional `addVariable("td", tdVar, calendarAMI)` when
  selected (Wald branch passes the WD variable), mean test (dcmpl), lp
  behind the selected flag, easter tstat UNGUARDED (pos < 0 →
  IndexOutOfBounds → `failed`); `canPre = adjust && log`.
- FTEST: the machine's OWN `createTestModel` does NOT force airline/mean
  (§10.1.1); Info rows carry NO lp (arg2→ll1=wd, arg3→ll6=td, swap
  confirmed, §10.1.2); branch walk `A: pftd6 > pftd1 && pftd6 > 1-fp`
  (dcmpl, NaN→A) → TD; else `pftd1 >= 1-fp` (dcmpg) → WD swap; else NONE
  (§10.1.3 — the §9 B/C labels were swapped vs the bytes; no dof guard
  anywhere); update models are SECOND estimations WITH lp per branch.
- sa ladders: `alltd` order `[TD2c, TD2, TD3, TD3c, TD4, TD7]` (TRAMO is
  `[TD2, TD2c, ...]` — fork-preserving, §10.1.8); `has_lp` is always true
  (the sa comparator never leaves row 1 absent); update is the
  keep/swap/add/remove walk over the existing `"td"` variable with
  core-equality and the `update()` early-return quirk, an lp branch with
  preadjustment, NO mean test and NO easter test.
- Fast/Default calendar steps: the 23-E seam refusals activate — the
  calendar arms run the transcription (variable lookup +
  `isAutomaticallyIdentified`, per-field t/F tests, the `addVariable`
  grammar incl. `lp` riding on `t0td` in Default, the mean test,
  `setPreadjustment` vs `addVariable` for lp, `m.set(desc, null)` /
  `clearEstimation()` on change). Default join arm: `JointTest` with
  `nhp = descTest.getArimaSpec().freeParametersCount()`; the
  `|p| < eps (eps = 0.05, hardcoded ctor)` dcmpg law means a NaN p-value
  ACCEPTS. `lcholesky`/`solveLx` mirror `SymmetricMatrix.lcholesky`
  (`lapack.Cholesky` provider, tol 0) and `LowerTriangularMatrix.solveLx`
  operand order bit-for-bit; `m_err_chol`/`m_err_sing`/`DStatException`
  propagate as `std::domain_error`.

## Errata (fresh-bytecode, full record in pinning §10.1 + header)

The §9 working dump was wrong on: FTEST `createTestModel` shape
(no forced airline/mean), the FTEST branch labels, the ladder selection
guards (TRAMO-AR `best<1` note wrong), `bestModel` all-absent being a
`-1` return, the candidate-side `mapping()` in `createTestModel`, the sa
`alltd` order, and the constructor `nsig` values. The formatter erratum
(`%.17g` not bit-faithful) forces the raw-bit corpus grammar. The
dim==1→wdTest routing under `joinTest` is a fresh javap finding dated this
session (offsets 149–166).

## Gates

ctest `test_calendar_detection` (#80, TIMEOUT 3600) passes in the plain
and sanitizer lanes over the frozen corpus; the sweep
`verify_calendar_detection` reports `0 failures` with the recorded
229 replay + 320 cross-check counts. The 23-E calendar-seam refusal rows
keep pinning the fork boundary from the other side.
