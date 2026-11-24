# SEATS Wiener-Kolmogorov extraction mirror (24-C.1)

Unit-qualified transcription of the LIVE pinned extraction machine that
`SeatsKernel.process` runs once the 24-A/24-B seams have produced a validated
model and a canonical UCARIMA decomposition: `SeatsModel` construction
(`buildDefinedModel`, fixed-parameter path), `WienerKolmogorovEstimator.
decompose` (the Burman/WK initial surface), `DefaultBiasCorrector.correctBias`
(the final surface) and the `SeatsResults` component views.
`kalos::seats_extract_wk` replays that machine on a
`tramo_seats_model_request`: it re-runs the 24-A validator and the 24-B
decomposition engine (bit-exact, shared code), then mirrors
`UcarimaModel.compact(2, 2)` → `BurmanEstimates` → the 30-add initial
assembly → `fillLevels`/`correctLogs` + the four fill{Fore,Back}casts passes,
emitting the fact grammar of
`validation/reference/SeatsWkExtractionProbe.java` (status, the three
`SeatsResults` domains, the init/final decomposition rows in the fixed
`ComponentType × ComponentInformation` enumeration) line for line.

```cpp
kalos::tramo_seats_model_request req;   // same request the 24-A/24-B units take
auto x = kalos::seats_extract_wk(req);
// x.facts()  - "status ok" | "status error:<class>[:<msg>]", then (ok only)
//              "dom actual|backcast|forecast n N start <year> <pos>", then
//              "init <Type> <Info> n N start <year> <pos> vals <%.17g ...>",
//              then the final rows. Row grammar: " vals " is literal, an
//              EMPTY item prints n 0 with a trailing space after vals.
```

## Envelope (fail-closed refusals, pinned messages)

`std::invalid_argument` with EXACTLY these reasons (check order shown; the
24-A validator runs first and its refusals propagate unchanged):

1. `seats_extract_wk: annual frequency %d is outside the D11 monthly-first
   envelope` — anything but period/frequency 12 (D11).
2. `seats_extract_wk: components_method KalmanSmoother is outside the D1
   envelope (the Kalman estimator is not ported)` — D1, WK-only.
3. `seats_extract_wk: innovation variance 0 selects the JVM
   ConcentratedLikelihood variance path (outside the ported domain)` —
   `SeatsModel.buildDefinedModel` swaps in `ConcentratedLikelihoodComputer.
   DEFAULT` when the spec variance is exactly 0 (the ssq/(dim−nparams)
   estimate needs the full TRAMO likelihood, not ported).
4. `seats_extract_wk: Noisy approximation mode is outside the D6 envelope`
   (24-B parity).
5. `seats_extract_wk: approximation_required decision is outside the qualified monthly retry envelope` for a Legacy retry outside the two fitted-model families described below.

When a monthly (0,1,q)(0,1,BQ) model with q at most one needs approximation, the toolkit keeps its ordinary MA
coefficient and chooses a seasonal MA of −0.2 if the negated ordinary coefficient
is strictly between 0.36 and 0.7, or −0.1 otherwise. An absent or zero ordinary
coefficient produces q=0. The same model recurs at every subsequent retry, so
a second failed decomposition determines the terminal failure. The extraction
uses the original observations, mean setting and innovation variance.

For a mean-free (2,1,0)(1,1,0) model the Legacy seasonal approximation
replaces the seasonal AR with a seasonal MA. The estimator applies the
configured level or log transform to the SEATS input, starts from
Hannan–Rissanen, and fits the three free parameters at precision 1e-7.
The innovation variance is residual SSQ divided by effective observations
less four (three coefficients and variance). The validator then applies the
configured parameter cutoff before the next decomposition.

## Mirror-status rows (data, not refusals)

A decomposition that came back null under mode None is the kernel's
SeatsException BEFORE estimation: the unit emits exactly
`status error:jdplus.tramoseats.base.api.seats.SeatsException:Invalid
decomposition` and no dom/init/final rows (x_stab_none). Mid-run JVM aborts inside the mirrored region are likewise
status rows — probe-observed classes: the prediction-error sink's
`jdplus.toolkit.base.api.eco.EcoException:Negative variance` (x_ar3,
x_stat_ar3), `NullPointerException` (unconditional WKE adds fed by a skipped
null component), `MatrixException`, `DemetraException`, and
`IllegalArgumentException:wrong dimensions` — always terminal (no dom/init/
final follow-up, pinned by the corpus).

## Two innovation variances (pinned, easy to get wrong)

`BurmanEstimates.initModel` reads `ucm.getModel()` — the **UcarimaModel's own
model** (`d model var`, 1.0 on every corpus path): it scales the MA
polynomial, divides the g numerator, and seeds the LU/ExactFc series model.
The WK stdev scaling `innovationStdev` is instead
`Math.sqrt(SeatsModel.getInnovationVariance())` — the spec echo (x_wn: 2.0).
The bm goldens never separate the two (their model iv is always 1.0), so both
wiring points are pinned only by this corpus (x_wn, x_seasonal).

## PED sink (ExactFc)

`ExactArimaForecasts.fcasts` attaches a fresh `PredictionErrorDecomposition
(false)` per call; the mirrored `CkmsFilter.process` loop saves
(error(i), pre-update variance) before every `update(i)` (no missing values
on the ported domain). Only the throw behaviour is live (the Burman path
never reads the accumulated values): `ResidualsCumulator.add` /
`DeterminantalTerm.update` reproduce the dcmpg/dcmpl comparison flavours and
raise `eco_abort` ("Invalid variance" / "Negative variance") or
`demetra_abort` — surfaced as status rows above. No xf/bm golden case
triggers the sink (8321 checks unchanged).

## Capture and corpus

`validation/reference/SeatsWkExtractionProbe.java` (public entry points:
the real `SeatsKernel.process` with the toolkit default wiring; `%.17g`
Locale.ROOT doubles) driven by `capture-seats-wk-extraction.R` (rJava,
OpenJDK 21); 36 cases captured twice with byte-identical csv output under
`validation/evidence/native/seats-wk-extraction/` (`cases.csv`,
`inputs.csv`, `model-facts.csv`, `model-events.csv`, `SHA256SUMS.csv`).
`tools/gen_seats_wk_extraction_golden.py` harvests the replayed subset
(status/dom/init/final rows + request echo + series + start) into
`tests/jvm_seats_wk_extraction.h`; `test_seats_extract_wk` replays all 36
cases, including the four envelope refusals, the classical airline retry and the degenerate fixMa MatrixException. The standalone sweep also reads the raw csv directly.

Eight boundary requests cover both adjacent floating-point values at 0.36 and
0.7, an interior value and an absent ordinary MA. Each reproduces the adjusted
endpoints and all components of the replacement model. To regenerate the small
oracle, compile `SeatsAirlineApproximationProbe.java` together with
`SeatsWkExtractionProbe.java` using the same locked 3.9.0 jars and Java 21.0.11
x86-64. Run its main class twice with
`-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp,_dlog` and compare
standard output with `validation/evidence/native/seats-airline-approximation/thresholds.csv`.

## Facts this unit deliberately does NOT emit

The k.* kernel surface and kevt rows (record-only), the v./model/d/cmp/r
blocks (owned by the 24-A/24-B units), the `CalendarEffect` columns (never
present on the ported domain), and every `dom` row when a status error
aborted the run.

## Open questions / later work

- Other seasonal refits, order reductions and retries that change differencing remain open.
- The `dom` null branch (`TsDomain.DEFAULT_EMPTY` printing "n 0 start 1970
  0") is unreachable inside the envelope (corpus: x_kalman context rows).
- Kalman smoothing (D1), Noisy mode (D6), the quarterly
  frequency path (D11) and the ConcentratedLikelihood variance path stay
  fail-closed by register decision.

## 24-D extraction result for pipeline assembly

The pipeline consumes the initial and bias-corrected decompositions already
returned by the JVM probe. The structured C++ entry points on
`seats_extract_wk_result` are `initial_components()` and `final_components()`.
Each returns the present component series in the probe's component/information
order. Each row identifies its component, information kind, calendar start
(year and one-based month), and original binary64 values. A present row with
zero observations retains its calendar. A missing row is omitted. An extraction
error returns two empty decompositions and its existing status. Request
refusals still throw.

The comparator is the existing double-captured 36-case WK corpus. The exit
gate requires the ordered keys, dates, lengths and every value of both views
to match the JVM rows, with the same status/refusal decisions. Standard
deviations here describe conditional extraction uncertainty. The
`TwoStepsDecomposition.merge` result has a different set of rows and has its
own later gate.
