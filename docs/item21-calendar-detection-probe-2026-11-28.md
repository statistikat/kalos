# Item 21-I — calendar & trading-day detection on the native automatic lane: refusal-surface inventory and measured decision margins, probe slice (2026-11-28)

Measurement-and-design dossier for the deferred 21-I surface, in the
[`item29`/`item30` probe-dossier style](item30-k0-routing-probe-2026-11-28.md).
This slice ships **no production code and no evidence-package movement**: it
(a) re-inventories the live refusal surface across all three layers
(`persephone3` wrapper → `rkalos` envelope → kalos core) with live fits,
(b) runs a fresh nine-fit JVM reference battery over
`rsa3/rsa4/rsa5c × clean/tdeff/air`, corrects the 21-I.2 selection statistic
from pinned bytecode, and measures the decision margins and landing
divergences the un-refusal decision turns on, and (c) prices the four options
open to the maintainer with acceptance budgets per slice. Any implementation
stays gated on the maintainer's option choice (§7); the
[calendar roadmap](calendar-roadmap.md) ledger carries the outcome as an
annotation in the same commit as this dossier.

Revisions and lanes. The native measurements run on the prebuilt R lane
(`tmp_opencode/m15-p3if/lib-run`, `rkalos` 0.1.0.9035) compiled at core
`1e2e3c8`; repo tip `64c5804` touches only docs/validation/tools and
`git diff --stat 1e2e3c8 HEAD -- src include tests` is empty, so the refusal
surface and fit timings below are the tip's behavior verbatim. The JVM side
runs against the pinned 3.9.0 jars in `tmp_opencode/jarcache` with
`JAVA_HOME` 21.0.x, in the bridge numeric domain
(`_JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp
-XX:DisableIntrinsic=_dlog"`), the same domain as the committed capture set.
Scratch transcripts and probes live under `tmp_opencode/m16-p7/` and are
fingerprinted in §9; §7 asks whether to freeze them under
`validation/evidence/native/`.

## 1. The refusal surface today, live at every layer

**Core gate.** `src/x13_automatic.cpp:2257-2263`: any request with
`use_calendar_detection || use_easter_detection || automatic_trading_days ||
use_user_variables` returns the status string, verbatim,

```cpp
  if (request.use_calendar_detection || request.use_easter_detection ||
      request.automatic_trading_days || request.use_user_variables) {
    out.status =
        "unsupported:x13 automatic fit defers calendar/easter/TD/user "
        "detection (21-I)";
    return out;
  }
```

and the contract pins it (`docs/contracts/x13-automatic-pipeline-contract.md`
:208, :218, :249).

**Envelope behavior nuance (measured, `native-flag-probe.out`).** The
`rkalos` primitive `native_x13_fit_automatic` does **not** throw on the four
deferred flags (`R/native_x13_automatic_fit.R:72-74` signature; the flags
travel as request data into `src/rcpp_x13_automatic.cpp:482-485,544-547`).
Each single-flag request returns the pinned status object — complete request
provenance echo, `note = "the provenance echo is complete; the pinned
fail-closed matrix refuses the request"`, `records_only` semantics, **no**
diagnostics node — while the all-false control returns `status = available`
with `td.ftest.sa`/`td.ftest.i`/`combined_seasonality` present. The only
layer that *errors* is the wrapper, by template name.

**Wrapper refusal table.** `persephone3/R/backend_native.R:77-89`
(`backend_native_auto_refusals`) carries `rsa4` and `rsa5c`, each citing the
core string verbatim and the template contents ("TD2/leap-year (test REMOVE)
and Easter (test ADD)" for rsa4; TD7 for rsa5c); dispatch at :107-114 refuses
at spec construction; the rsa3 decode table at :158-189 is the one accepted
shape. Live: `perX13(AirPassengers, "rsa4")` fails at spec construction with
the table's message; a decode-table (rsa3-shape) fit proceeds.

**Fit-shape anchors on this lane.** AirPassengers (n = 120) through the full
automatic lane: `status available`, **1.337 s** wall (14 chain stages),
outliers-off **0.734 s**, automodel-off **0.80 s**; delivered diagnostics
`td.ftest.sa` value **2.8406525858195382** / p **0.012278267499684494** and
`td.ftest.i` value **2.5553926372670133** / p **0.022389206585029246** (the
21-G attach on the working log scale, `preadjustment$log = TRUE`); applying
the certified primitive directly to the delivered levels components gives
3.7268788028505782 / 2.535243302739365 — the levels-vs-log pair the
residual-TD ledger already documents (rkalos `IMPLEMENTATION_STATUS.md`
:690-703). The stock estimated lane costs RSA0 0.011 s, RSA1 0.97–1.50 s,
RSA2c 0.68–0.79 s — the stock fixed calendar battery inside RSA2c is below
the run-to-run noise of the pipeline, i.e. *column-level* calendar work is
cheap; the price of 21-I is the **candidate-estimation** rounds, priced in §2
and optioned in §4.

## 2. The JVM reference battery, and the selection-statistic correction

**Harness.** `tmp_opencode/m16-p7/P7Probe.java` (sha256 `554fc5d2…`) replays
the repo-pinned `validation/reference/I21Probe.java` mechanics: pinned template
hex → `X13Spec.fromString` → `X13Kernel.process(observer)`, an observer that
dumps every `ProcessingInfo` (ATD/EAS `names`/`best` + full
`LikelihoodStatistics[]` arrays), the landed regression variables, the final
model facts, the X-11 component finals `d11final/d12final/d13final/d18` and
the elapsed wall time. Nine fits (`rsa3/rsa4/rsa5c × clean/tdeff/air`), each
run twice: **all nine double captures are byte-identical modulo the timing
line** (the `rsa4/clean` diff is the single `elapsed_ms` word), re-proving
same-domain determinism next to the committed I21Probe `capA == capB` fact.
The re-verified pinned line counts of the original battery (141/227/141/230
for `x13run-rsa4/5c-{clean,tdeff}`) still hold. Detection inflates the event
stream: 23 events on `rsa3/air` vs 35 (`rsa4/air`), 36 (`rsa5c/air`), 57
(`rsa4/tdeff`).

**Selection statistic — bytecode-corrected.** The 21-I.2 ledger row says
"AICcComparator margin … Info ll/ssq/**bicc** arrays". Disassembly of the
pinned jar (`javap/AICcComparator.txt`, `javap/CEDM.txt`,
`javap/X13SpecDecoder.txt`, all in the scratch record) shows:

* `AICcComparator.compare(x, y)` returns `0` **iff**
  `y.statistics().getAICC() − aicDiff < x.statistics().getAICC()` (strict `<`,
  both operands `getAICC()`), else `−1`;
* `X13SpecDecoder` constructs `new AICcComparator(regression.getAicDiff())`
  (decoder javap :342-347) and hands the one comparator to both
  `CalendarEffectsDetectionModule.builder().modelComparator(…)` (:446) and
  `EasterDetectionModule.builder().modelComparator(…)` (:519); the pinned
  rspec fixtures carry no `regression.aicdiff` (default **0.0**) — the proto's
  `aicdiff: -2.0` sits under `transform` and belongs solely to the log-level
  test (decoder javap :77);
* the CEDM's ATD step estimates the td candidate and the no-td baseline and
  switches to the candidate **iff** `compare(est_no_td, est_with_td) == 0`;
  the Info `LikelihoodStatistics[]` order is `[no td, with td]`. The LoP
  candidate rule in the same method (offsets 46–162): the baseline never
  carries LoP (preadjustment reset to `None` / column removed); the td
  candidate takes LoP as **preadjustment** when `log && adjust ≠ None` and as
  a regression **column** otherwise. This subtle fork must be replicated
  byte-mechanically by any native port.

The practical consequence, witnessed in this battery: on `rsa5c/air` round 1
the td7 candidate is *worse* on the probe-printed BICc (+3.87e−02) yet is
selected, because its **AICC** is better by 10.86. A native replay that
compares BICC-style statistics — as an earlier working table in this session
did — mis-ranks candidates and will report phantom non-determinism. The
acceptance replay in 21-I.2 must pin `getAICC` arrays, not BICc.

**Decision stream (all nine fits).** ATD/EAS `best` per round, with the landed
regression part:

| corpus | spec | detection rounds (ATD/EAS best) | landed variables |
| --- | --- | --- | --- |
| clean | rsa3 | — | AO(1996-06), AO(1996-10), AO(1997-08) |
| clean | rsa4 | ATD keep / EAS keep | identical to rsa3 |
| clean | rsa5c | ATD keep / EAS keep | identical to rsa3 |
| tdeff | rsa3 | — | AO(1999-01) (non-log, coef −114.95) |
| tdeff | rsa4 | 3 rounds: TD2 in / EAS keep | td2 |
| tdeff | rsa5c | 3 rounds: TD7 in / EAS **out→in→out flip** | const, td7 |
| air | rsa3 | — | (empty regression part) |
| air | rsa4 | 2 rounds: TD2 in / easter[1] in | td2, easter, AO(1951-05) |
| air | rsa5c | 2 rounds: TD7 in / easter[1] in | td7, easter, AO(1951-05) |

**Margins on the true criterion (raw AICC; positive = candidate wins).**
From `runC/` with `battery_margins_aicc.py`:

| fit | ATD round margins | EAS round margins |
| --- | --- | --- |
| rsa4 clean | keep, best cand −2.033 | keep, −2.046 |
| rsa4 tdeff | +347.7 / +325.3 / +356.4 | keep −1.615 / −1.692 / −1.471 |
| rsa4 air | +18.33 / +30.00 | +3.777 / +7.528 |
| rsa5c clean | keep, −10.294 | keep, −2.046 |
| rsa5c tdeff | +919.8 / +969.7 / +879.9 | keep −2.088 / **+21.05** / keep −0.769 |
| rsa5c air | +10.86 / +23.90 | +3.958 / +7.083 |

The TD decisions are robust on this corpus (every |margin| ≥ 2.0, most ≥ 10).
The Easter decisions live on a thin surface: the `rsa5c/tdeff` chain is a
genuine round flip (out at −2.09, in at +21.1, out again at −0.77) *inside one
fit*, and the smallest margin anywhere is 0.769 raw AICC units — about seven
orders of magnitude above the observed numeric sensitivities (double-capture
bit-identity; 1–2 ulp coefficient / small-ulp component drift between the
bridge and default domains with **all decision bits stable** across the pinned
captures; native-vs-JVM scalar drift 6.9e−09..3.05e−08 on the certified TD
statistics), but only one fit-width away from flipping on a harder corpus.
Easter is the risk class of 21-I, not trading days.

**Landing divergence, un-refusal priced on real models** (`rsa4`/`rsa5c`
minus `rsa3` per corpus, max/mean abs delta):

| corpus | d11 | d12 | d13 | d18 |
| --- | --- | --- | --- | --- |
| clean | 0 / 0 (both specs) | 0 / 0 | 0 / 0 | 0 / 0 |
| tdeff | 1.52e+02 / 4.3e+01, 1.56e+02 / 4.5e+01 | 5.9e+01, 6.1e+01 | 2.0e+02, 2.0e+02 | 1.04, 1.06 |
| air | 9.86 / 2.0, 11.3 / 2.3 | 2.92, 3.36 | 5.1e−02, 5.8e−02 | 2.4e−02, 3.1e−02 |

`clean` shows that when detection correctly declines, the calendar-enabled
templates land the *same* model bit for bit (coefficients bit-identical) — the
un-refusal is a no-op on calendar-free data. `tdeff`/`air` show that when
detection fires, every published component moves enormously — these are
decision-driven differences (different models), not numeric drift, and they
are what a downstream consumer of a newly-accepted rsa4/rsa5c request will
see. The final-model coefficient sets agree bit-exactly across all three
specs on `clean` and diverge on `tdeff`/`air` for the same reason.

**Class liveness (fresh, `-Xlog:class+load`).** `out/cl2-rsa4-tdeff.log`
(2482 loads) and `out/cl2-rsa5c-clean.log` (2486) show the machines that
actually run: `sa.base.core.regarima.CalendarEffectsDetectionModule` (the
class that executes both the ATD and the calendar battery),
`AutomaticTradingRegressionModule` (+`$Info`, interface constants only),
`EasterDetectionModule`, `ModelBuilder`, `regression.AICcComparator`,
`regsarima.regular.RegressionVariablesTest`, `dstats.Chi2` — while
`core.dstats.F`, `sa.base.core.regarima.AutomaticTradingDaysRegressionTest`/
`WaldTest`, the TRAMO `tramo.TradingDaysController`/`AutomaticRegressionTest`
family, and the legacy `modelling.CalendarEffects` are **never loaded**. This
re-confirms decision I-2 on a new corpus and pins the 21-I.2/21-I.3 fork: the
X-13 ATD step runs inside CEDM; the F/T/Wald controllers are TRAMO-side only.

**JVM timing anchors** (`elapsed_ms` around `X13Kernel.process`, bridge
domain; `runA`, wall-clock jitter to ±40% on re-runs — a clean repeat came in
517/608): rsa3 354 (air) / 365 (clean) / 2231 (tdeff); rsa4 545 / 416 / 1349;
rsa5c 666 / 441 / 1857. The detection overhead is +191/+312 ms (air),
+51..+91 ms (clean) for 10 candidate estimations per fit on air — ~10–31 ms
per candidate-estimation wall. On tdeff the rsa3 baseline is *slower* than
its calendar-enabled siblings (a non-log model with a −114.9 AO coefficient
dominates its AMI path), so that column prices the rounds, not the delta.
Scaling to the native lane (a full automatic fit costs 0.73–1.34 s and a
candidate-free pipeline stage is ~10 ms), option (iii) below should be
expected to add O(0.1–1 s) to a hard automatic fit — a design consequence,
not a measurement of the port.

## 3. Downstream movement: what un-refusal touches

**Test pins that flip the moment behavior changes.** Wrapper:
`persephone3/inst/tinytest/test-backend.R:41` (`backend_default_spec` refusal)
and `:146` (`expect_error(perX13(AirPassengers, "rsa4"), "does not yet
implement")`). Envelope: `rkalos/inst/tinytest/test-native-x13-automatic.R`
:372 (`af$deferred` cites 21-I), :437 and :453 (the status string pinned for
all four flags, refusal-before-horizon ordering included), :533 (wrapper
pass-through). **M4-gap stack pin**: `persephone3/inst/tinytest/
test-parity.R:1019-1028` re-derives against the LIVE JVM the fixtures under
`fixtures/native/refusals-m4gap/` — `rspec`/proto captures for **all six**
templates (rsa4/rsa5c included) but demand runs only for the three
automodel-OFF templates — precisely because rsa4/rsa5c have no native side to
compare. On any un-refusal step this panel gains a demand leg (rsa4/rsa5c
demand captures exist and double-JVM byte-identity was the capture protocol),
and the refusal fixtures become landing-acceptance oracles, so their hashes
are the first manifest movement to expect.

**Ledger entries that carry the deferral today.** `rkalos/
IMPLEMENTATION_STATUS.md`:774-780 (the D8 fail-soft panel: the automatic route
can deliver identically-zero TD compositions which the pinned machine declines
to fit) and :798 ("the refusal half keeps the 21-I calendar deferral
provenance-only"); `kalos/IMPLEMENTATION_STATUS.md`:444-447 (the sensitive
panels — `test-native-x13-estimated`, `test-native-residual-trading-days`,
the `test-native-x13-automatic` D8 panel — must re-certify byte-identical
under any core re-vendor; detection code makes that a live requirement, not a
expectation); `persephone3/IMPLEMENTATION_STATUS.md`:310 ("`rsa4/rsa5c` stay
refused (21-I)"); the contract rows cited in §1. Note for the record: no
ledger section literally titled "Pass A deferrals" or "TD-capture-drift"
exists in any of the three status files; the nearest measured items are the
suite-pass deferrals at `persephone3/IMPLEMENTATION_STATUS.md`:171, :384-389
(not calendar-related) and the attach-convention drift entry at
`rkalos/IMPLEMENTATION_STATUS.md`:690-703 (RSA2c estimated components drifting
up to 0.10 log units across a decomposition switch — the canonical warning
that *decomposition identity*, not floating point, is what moves landings).

## 4. Options, priced with the measurements above

**(i) Close permanently.** Zero code; the M4 refusal, the §3 pins and the
M4-gap panel stay as they are forever; the roadmap 21-I line closes with the
refusal as a documented product boundary. The clean-corpus zero-divergence
result (§2) shows nothing on calendar-free data is being lost; everything
lost is on data with real calendar effects, where §2 shows the entire landing
moves.

**(ii) Minimal un-refusal, decision I-8 step 1 only** (calendar-free
rsa4/rsa5c requests pass; any request still touching detection stays refused).
No detection machine lands. Movement is plumbing: the core gate narrows to the
four request flags; the wrapper decode table admits the two template shapes
with the calendar blocks *stripped* and the refusal narrowed to flagged
requests; §3's pins become positive assertions; the M4-gap panel gains the
strip-and-fit leg. On the measured clean corpus the landing is bit-identical
to today's rsa3 model, so the qualification risk is the smallest of all
moving options; the user-visible gain is template-shape compatibility only.

**(iii) TD-only staged** (port the CEDM ATD step + LoP rule + round re-fires;
Easter stays refused). Measured support: every TD decision on this corpus sits
≥ 2.0 raw-AICC from its boundary (§2) — this is the robust half. But
rsa4/rsa5c always request Easter (TEST_ADD), so **template-level acceptance
of rsa4/rsa5c cannot complete under this option**; it only unblocks the
envelope's `trading_days = TRUE` flag path and TD-only shapes, and adds the
`easter` refusal asymmetry to the wrapper table. Price this as an
internal-milestone option, not an M4-gap fix.

**(iv) Full 21-I.2 + 21-I.4 per the existing ledger** (X-13 calendar battery:
ATD step + Easter battery + comparator on `getAICC` + per-round re-fire, then
the refusal ladder). This is the only option that closes the M4 gap. Cost
anchor: 10 candidate-estimations per automatic fit at ~10–31 ms JVM wall
(§2) against the landed native candidate machinery — expect O(0.1–1 s) added
per hard fit, dominated by re-fire rounds, with the clean-corpus fit paying
nearly nothing (one round, both batteries decline). Risk anchor: the Easter
keep/reject surface (min margin 0.769, with a measured within-fit round flip)
must be pinned by exact replay of the capture event stream, not by
"same decision on our corpus" testing.

## 5. Acceptance budgets (draft, for the chosen moving option)

* **Tier 1 — bit-exact.** `calendar_generation` columns are already
  bit-exact-certified (21-I.1, ctest #75). The decision *stream* — candidate
  counts, per-round `best` indices, landed variable lists — is bit-exact
  replayable against frozen captures: same-domain double captures are
  byte-identical here and at I21Probe. The ATD/Info `getAICC` arrays replay
  to the pinned decimal strings (21-I.2's "replay of the Info events,
  `%.17g`" row stands, with the §2 statistic correction);
* **Tier 2 — certified-scalar tier** for any exposed detection statistic
  comparable to the landed 21-G units: the existing field tolerances
  (`validation/fixtures/backend/field-tolerances.csv`:112-115, 1e−10/1e−07
  provisional) with the 6.9e−09-inside-9.3e−09 witness as the precedent
  floor. Given min margin 0.769 raw AICC, a corpus qualifies for decision-bit
  pinning only if every detection margin is ≥ 0.1; below that the pin must
  record the flip or exclude the case;
* **Tier 3 — landings: no fixture-bit-exactness claimed**, following the
  residual-TD product precedent (`rkalos/IMPLEMENTATION_STATUS.md`:784-792):
  wiring-identical + finiteness + consistency, with the measured floors as
  the honest statement — same-domain double capture bit-identical;
  bridge-vs-default-domain moves coefficients by 1–2 ulp and small component
  ulps with all decision bits stable; decomposition-identity changes (the
  0.10-log-unit RSA2c entry) are out-of-contract, not tolerance cases;
* **Corpus floor.** Three corpora are a probe, not a qualification. The
  landing acceptance runs against the 13-case x13-automatic oracle panel and
  the horizon corpus (existing qualification assets), the M4-gap fixtures
  become the rsa4/rsa5c demand oracles, and the wrapper suites re-certify.

## 6. Risk register (measured)

* **R1 — decision-flip sensitivity (the class risk).** Easter keep/reject
  margins reach 0.769 raw AICC units and flip across rounds inside one fit
  (`rsa5c/tdeff` out −2.09 → in +21.1 → out −0.77). Every measured
  numeric sensitivity is far smaller — double-capture bit-identity,
  1–2 ulp cross-domain coefficients, 6.9e−09..3.05e−08 certified-scalar
  drift — so a *native* flip requires native landing drift inside ~1e−7 of a
  boundary; none observed on three corpora, but that is a sample of three and
  the margin distribution's left tail is the open exposure. Mitigation: the
  §5 margin floor plus exact event-stream replay (Tier 1), not corpus-level
  decision agreement.
* **R2 — criterion substitution (BICC for AICC).** Witnessed twice during
  this probe: BICC-proxy margins mis-rank td7-class candidates and manufacture
  phantom non-determinism. The replay contract is `getAICC()` strings only;
  the 21-I.2 ledger row carries the correction annotation.
* **R3 — the LoP fork.** The CEDM ATD step applies LoP as *preadjustment*
  when `log && adjust ≠ None` and as a regression column otherwise (bytecode
  offsets 46–162). A port that implements only the column arm silently changes
  the td-candidate design on log transformations — exactly the corpus class
  (`air`) where detection fires.
* **R4 — round-conditional re-fire (I-7).** Detection re-fires per AMI round
  (2–3 observed) and per-round decisions differ; intermediate model states
  feed the AMI path, so a port that decides once per fit can land the same
  final variables through a different trajectory and diverge anyway.
* **R5 — decomposition identity, not precision, moves landings.** The
  calendar-firing corpora move *every* component (d11 max 9.9–1.6e+02; §2)
  and the RSA2c attach-convention entry (rkalos `IMPLEMENTATION_STATUS.md`
  :690-703, 0.10 log units) is the precedent that out-of-contract
  re-decompositions look like drift disasters if the contract is silent.
  Downstream consumers (D8 fail-soft panel, the 21-G `td.ftest` attach,
  horizon envelopes) re-qualify before any refusal step executes.
* **R6 — domain-sensitivity floors are measured, not bounded.**
  Bridge-vs-default stability holds on the captured corpus; the acceptance
  protocol must therefore *state* the numeric domain, per the frozen-capture
  convention ("the oracle capture domain is the bridge flag profile").
* **R7 — qualification-surface movement is guaranteed.** The M4-gap fixtures
  become demand oracles, the §3 pins flip polarity, and the byte-identical
  re-certification expectation for the three sensitive panels
  (`kalos/IMPLEMENTATION_STATUS.md`:444-447) becomes a live gate under any
  core change of this size.

## 7. Decision asks

1. **Choose (i)–(iv).** The measurements price each; nothing beyond them is
   needed to choose.
2. If moving: ratify the 21-I.2 statistic correction (AICC, strict `<`,
   aicDiff 0.0 — this dossier) as the replay contract, and the LoP
   preadjustment-vs-column fork as a required port subtlety.
3. Ratify freezing `tmp_opencode/m16-p7` (probe `P7Probe.java`, nine+three
   captures, `javap/` disassemblies, class-load logs — sha256 in §9) under
   `validation/evidence/native/x13-calendar-detection/` as a follow-up
   evidence commit. This dossier deliberately moved **no** manifest.
4. Confirm the §5 corpus floor before any refusal retirement executes.

## 8. Reproduction

JVM battery: `javac -cp 'classes:tmp_opencode/jarcache/*' P7Probe.java`;
`java -cp … P7Probe <rsa3|rsa4|rsa5c> <clean|tdeff|air>` under the bridge
`_JAVA_OPTIONS` above (`run_p7_battery.sh` drives all nine, twice); margins
via `battery_margins_aicc.py`; divergence via `divergence.py`. Native lane:
`.libPaths(c("tmp_opencode/m15-p3if/lib-run",
"tmp_opencode/m12-persephone/lib/nojava-iso", .libPaths()))`, then
`native_inventory.R`, `native_flag_probe.R`, `native_calendar_cost.R` under
`/opt/R/4.5.1/bin/Rscript`. Class liveness: rerun any battery cell with
`-Xlog:class+load:file=…`.

## 9. Scratch fingerprints (sha256, first 16 hex)

`P7Probe.java` `554fc5d25512ca00` · `javap/AICcComparator.txt`
`5e57eaad41f74804` · `javap/CEDM.txt` `46022e34f13c4d08` ·
`javap/X13SpecDecoder.txt` `48641bc065394cb1` · `native-flag-probe.out`
`30e58afb5548d830` · `native-inventory.out` `d1e6b6982788f35f` · nine
`runA/*.out` captures: `3710f129cea567c4` (rsa3-air) `60edee78b2db0eae`
(rsa3-clean) `8284efd0829bdae5` (rsa3-tdeff) `de9494277b7d8f81` (rsa4-air)
`3af9dba0f932f5e5` (rsa4-clean) `bfdf78992377a1a3` (rsa4-tdeff)
`1ac34e9fa5a64716` (rsa5c-air) `50c869c9df24e281` (rsa5c-clean)
`f6fc1ca7e76510fe` (rsa5c-tdeff).
