# TRAMO model-builder decision mirror (23-A)

Unit-qualified transcription of the LIVE pinned TRAMO builder:
`jdplus.tramoseats.base.core.tramo.TramoModelBuilder.build(TsData,
ProcessingLog)` — the package-private builder that `TramoKernel` drives (the
public constructor `new TramoModelBuilder(spec, new ModellingContext())` and
`build` are reached reflectively by the oracle probe; `DefaultModelBuilder`
and `TramoSelection` are dead classes on the live path and are not
transcribed). The unit mirrors the builder's DECISIONS and registered
variable metadata — start rule, initial SARIMA spec, log/LoP/mean flags,
the `AverageInterpolator` missing plan, the `sortVariables` variable list
with the `regeffect` attribute vocabulary, and the `ProcessingLog`
warnings. It does NOT compute calendar design algebra (day-clustering
contrasts, LoP and Easter column values — deferred to the 21-I joint
surface per register D10), run the log/level decision (that is the
`TransformationType.Auto` consumer in 21-H/23-F; the builder treats Auto as
levels), or perform the pinned `check()` collinearity pass (a Householder
QR over the design matrices; the qualified corpus is full-rank and the unit
makes no collinearity claim).

Bytecode sources: javap decodes `TramoModelBuilder`, `ModelDescription`,
`SarimaSpec`, `Parameter`, `Variable`, `ModellingUtility` (ami),
`AverageInterpolator`, `RegressionSpec` under
`/data/home/kowa/seasonaladjustment/tmp_opencode/jdprobe/{m5,cur}/` at slice
time; the double-captured corpus is the durable behavioural record.

```cpp
const kalos::tramo_template_fields* t = kalos::tramo_template("TR3");
kalos::tramo_build_request req;              // expand the template yourself:
req.using_auto_model = t->using_auto_model;  // template rows arrive as data,
/* ... transform, arima, mean, calendar, delta_tc ... */
req.values = series;                          // monthly, NaN = missing
req.start_year = 1990; req.start_month = 1;
auto b = kalos::build_tramo_model(req);
// b.arima (start rule), b.log_transformation, b.preadjustment, b.mean,
// b.missing + b.interpolated, b.variables (sortVariables order), b.warnings
```

## Capture and corpus

`validation/reference/TramoModelBuilderProbe.java` (the public-getter
observer: raw getter values, `%.17g` doubles, enum constant names, never a
display `toString` — pinning notes 6.1/6.2) driven by
`validation/reference/capture-tramo-model-builder.R` (rJava, OpenJDK 21, the
G2 intrinsic-disable recipe with a live `Math.exp != StrictMath.exp` guard),
22 cases captured twice from independent JVM processes with the `--repro`
byte-identity gate. Corpus: `validation/evidence/native/tramo-model-builder/`
(`cases.csv`, `inputs.csv` with the protocol `src`/request-echo `req`/
`values` lines, `builder-facts.csv`, `builder-events.csv`, `metadata.R`,
`SHA256SUMS.csv`). The generator `tools/gen_tramo_model_builder_golden.py`
harvests the corpus into `tests/jvm_tramo_model_builder.h` (series as raw
IEEE-754 bits); the shared driver/formatter
`tests/tramo_model_builder_replay.h` and `kalos::build_tramo_model` are
compared line-for-line against the captured grammar by
`tests/test_tramo_model_builder.cpp` and `tools/verify_tramo_model_builder.cpp`.

Two JVM numeric identities the replay must reproduce: `String.format(
"%.17g", d)` renders the SHORTEST round-tripping decimal padded to 17
significant digits (Java 0.7 → `0.70000000000000000`, C
`%#.17g` → `0.69999999999999996`), and `Parameter.isFree()` is
`type != Fixed`, so Undefined-rank parameters render as `R:0.0000000000000000`.

## Decision laws (bytecode → unit)

* **Start rule** (`initializeArima`): `usingAutoModel` → `setAirline(freq !=
  1)` (monthly: undefined airline `p0d1q1P0D1Q1 per12`) plus
  `setMean(true)` AFTER `initializeVariables` (the `mean_auto_off` row: a
  `MeanSpec.none()` TR3 build still ends with mean on); else
  `setArimaSpec(spec.getArima().withPeriod(12))` (`SarimaSpec` orders ARE
  the parameter-array lengths; `withPeriod(1)` drops the seasonal block —
  unreachable monthly).
* **Mean quirk** (`initializeMean`): unused → mean off; used with a FIXED
  coefficient → a `TrendConstant(spec.arima.getD(), getBd(),
  estimationDomain.start)` named `const` with `regeffect=Undefined` and the
  fixed coefficient, mean off; used otherwise → mean on. `MeanSpec.isTest`
  is never read (bytecode).
* **Regression gate**: `RegressionSpec.isUsed()` is DERIVED (mean used,
  calendar used, or any non-empty variable list); false skips the whole
  `initializeVariables` (the `mean_off` row: TR1 + `MeanSpec.none()` leaves
  mean default-off with nothing registered).
* **Transform**: `log = (function == Log)` (Auto stays levels here);
  `setPreadjustment(adjust)` always.
* **Missing plan** (`AverageInterpolator.cleanMissings`, right-to-left):
  interior run → `(left + right) / 2.0` for every slot; leading run → the
  first finite value; trailing run → the finite value to its left; the
  missing list is `Arrays.sort`ed ascending; all-missing is a refusal.
* **Outliers** (`initializeOutliers`, exact-case switch): AO →
  `AdditiveOutlierFactory`/Irregular, LS →
  `LevelShiftFactory.FACTORY_ZEROENDED`/Trend, SO →
  `PeriodicOutlierFactory(freq, true)`/Seasonal, TC →
  `TransitoryChangeFactory(deltaTC)`/Irregular. `withCore` keeps the request
  name and attributes; the factory `regeffect` is set only when absent (a
  preset attribute survives — `outliers_manual`). Any other code (case
  sensitive) is skipped in silence. The names match the 22c allocation
  contract `ComponentType` vocabulary (G3 probed: LS → Trend, SO →
  Seasonal).
* **Calendar**: TD registers only when `used && !test && !automatic`
  (`td` + `lp` when the LoP rule is set, both `regeffect=CalendarEffect`);
  Easter registers when `used && !test` (`easter`, `EasterVariable`,
  endPosition −1, Simple correction). ALL SEVEN templates are gated away
  from calendar registration (TR2/TR4/TR5 tested; TRfull automatic-FTEST +
  tested IncludeEaster) — every template build carries zero variables, the
  key decode finding for 23-B/23-E.
* **Users**: `TsContextVariable.instantiateFrom(new ModellingContext(),
  name)` is null in-envelope → the pinned warning `"<name>: not found
  (removed from the model)"` and no registration.
* **Sort** (`ModelDescription.sortVariables`, six stable passes):
  TrendConstant → user category (users, interventions, ramps in
  registration order) → days-related (td, lp) → moving holidays (easter) →
  manual outliers sorted by `IOutlier.getPosition` → auto-identified
  outliers sorted (the `ami` attribute layer belongs to 23-B).

## Template table and fail-closed matrix

`tramo_template(name)` accepts exactly `tr0 tr1 tr2 tr3 tr4 tr5 trfull
TR0..TR5 TRfull` (the `fromString` spellings); every other name — `Trfull`,
all `rsa*` — is refused (nullptr). Rows are the pinned getter values
(autoModel on TR3+; TR0 with all outlier-detection flags off; TR0–TR3 the
trend-constant mean, TR4/TR5/TRfull DEFAULT_UNUSED; calendar data per
above). `build_tramo_model` refuses (`std::invalid_argument`): non-monthly
frequency (the annual/quarterly corpus rows are JVM-qualified RECORD cases
the unit rejects per D11), empty or all-NaN series, infinite values,
bad months, declared SARIMA periods other than 0/12, resolved user
variables (21-I), stock/holidays/user-trading-day calendars, TD types other
than TD2, registered TD blocks without a LeapYear LoP rule, julian/include
Easter registrations. A derived-unused regression block is NOT a refusal.
The request `used` flags must equal the pinned equals-vs-DEFAULT derivation
of the expanded spec (the derivation lives in the 23-B spec layer).

## Implementation-actual deltas at landing (23-A)

* **The M5-open decode note "leading NaNs STAY NaN" is refuted by the
  capture**: `cleanMissings` forward-fills the leading run with the first
  finite value and back-fills the trailing run with the left value; the
  `missing_lead`/`missing_trail` rows pin both.
* **TR1's transform function is `Auto`, not `None`** (req echo; the
  template matrix sketch was wrong). The knob lines (`function`/`adjust`)
  REPLACE the whole `TransformSpec` — a lone `adjust` line drops the
  template's function to `None` (the probe's `TransformSpec.builder()`
  semantics, pinned by `calendar_fixed`).
* **`RegressionSpec.isUsed`/`CalendarSpec.isUsed` are derived** (equals vs
  DEFAULT / component ORs), which is what makes the `mean_off` row (a TR1
  build with NO registered mean at all) the correct pinned behaviour.
* `MeanSpec` coefficient null vs Undefined rank render identically through
  the probe (`R:` prefix); the unit keeps both states distinct in the
  request (only `Fixed` matters to the builder).
* The stock/holidays/user-TD, julian/include-Easter, TD3/4/6/7 and
  `LengthOfPeriod`-rule paths are unproven by the corpus and refused;
  extending the envelope is a recapture, not a reinterpretation.
* The probe reaches the package-private builder through
  `Constructor.setAccessible` (public constructor + `build`, no
  instrumentation) — the roadmap's "public entry point" holds.

## Gates

`test_tramo_model_builder`: 22 corpus cases (request echo + facts + events,
byte-identical lines) + the template-name matrix + the rejection matrix +
the interpolation-plan structural case, bit-exact in BOTH the OpenMP and
serial lanes (65/65 each, fresh trees).
`tools/verify_tramo_model_builder.cpp` re-drives the same corpus from the
raw csv files: 22/22 PASS. ASan/UBSan build of unit and sweep: clean.
Existing suite unchanged: the nine sweeps byte-identical, the full automatic
sweep 71401 checks 0 bad (13/13), `verify-migration.py` 406.
