# Seasonal adjustment evaluation and performance plan

## 1. Establish a comparable baseline

Use the existing pinned JDemetra+ (JD+) 3.9.0 reference environment. Record the
actual JAR hashes, JVM, R and wrapper versions, Kalos and rkalos commits, release
build flags, BLAS provider, CPU, OS and thread settings. Keep historical fixtures
and tolerance files intact. Store new inputs, manifests and results under
`validation/benchmarks/`, or link to immutable artifacts there for large runs.
Use public JD+ inputs and outputs for live comparisons and frozen fixtures for
offline correctness checks.

Start with the delivered fixed-order SARIMA fit and initial additive X-11 tables.
The archived 64-case fit-only matrix took 9.006 s native, 0.195 s JD+ X-13 and
0.181 s JD+ TRAMO. For the six initial additive X-11 tables, 24 short calls took
0.00568 s native versus 0.02913 s JD+, and eight 100,003-row calls took 0.12625 s
versus 0.08464 s. These measurements cover different operations and establish
priorities for remeasurement after the split. Full seasonal adjustment becomes a
benchmark once the corresponding Kalos path is available.

## 2. Grow the comparison with the implementation

| Milestone | Matched comparison | Qualification before timing |
| --- | --- | --- |
| Delivered | Fixed-order SARIMA likelihood and fit, forecast and interpolation, initial additive X-11 tables | Match likelihood conventions, optimizer controls, output fields and requested horizons. Check the existing frozen cases against the installed builds. |
| 22b.3a/b | Moving dispersion, sigma weights and same-phase replacements | Compare windows, counts, weights, replacement rows and correction values. Report zero or tiny dispersion and threshold cases by explicit policy. |
| 22b.4 | Fixed additive X-11 decomposition with B/C/D passes | Feed both decomposers the same preprocessed series and extension values. Compare intermediate tables, endpoints, chosen filters and final component identities. |
| 22b.5 / 22c | Restricted fixed-model X-13 seasonal adjustment | Start both engines from the same raw series and fixed specification. Compare preprocessing, regressor effects, transformation, decomposition, forecasts, diagnostics and requested output together. Add each supported decomposition mode separately. |
| Later | Automatic X-13 and TRAMO/SEATS | Use separate method-specific suites for selected models, transformations, outliers, filters, decision scores and final components. Add diagnostics and benchmarking outputs as their contracts qualify. |

Build a small named matrix for frequent runs and a broader qualification matrix.
Include synthetic series with known components and redistributable real series
with recorded provenance. Cover monthly and quarterly series, short spans near
eligibility thresholds, ordinary production lengths, and longer stress inputs
within both engines' supported domains. Include stable seasonality, changing
trend, calendar effects, outliers, internal and endpoint gaps, near-unit ARMA
roots, regressor rank loss and forecast/backcast extensions. Add panels of many
short series to measure batch throughput separately from single-series latency.
Give each case a stable ID and version its inputs, specification, requested
outputs, expected policies and applicability per milestone.

For every comparison, align the calendar and row domain first. Require exact
agreement on output availability, masks and discrete decisions where the policies
match. Apply the existing field-specific absolute and relative budgets to numeric
values, with separate summaries for endpoints, interior and forecasts. Check
reconstruction identities and independently derived controls beside JD+ parity.
Record deliberate policy differences, such as accurate versus legacy reporting,
with the affected rows and an independent expectation. Report failed and skipped
cases explicitly. Ranking timings requires matched output, equivalent controls
and successful qualification on both sides.

## 3. Measure elapsed time at distinct boundaries

| Scope | Start and finish |
| --- | --- |
| Kernel or stage | Prepared inputs to equivalent stage outputs. Use C++ measurements for Kalos optimization. Use public JD+ stage timings only where the stage and output can be matched. |
| Warm engine | Installed rkalos and rjd3 calls in a warmed R session. Measure setup plus execution and execution with reusable setup separately. Include equivalent conversion and result materialization. |
| End to end | Raw input and specification to a fully materialized requested result. Measure cold first call in a fresh process separately from warm latency. Add a Persephone workflow measurement when it can invoke both full paths. |
| Batch | Fixed manifest of independent series. Measure wall time, series/s, per-series latency and peak memory at the same concurrency and thread budget. |

Build optimized artifacts and run on an idle host with fixed affinity where
available. Establish a one-thread baseline and then measure explicit Kalos
OpenMP, provider and batch configurations. Control the effective thread budget
and check for nested parallelism. Warm both engines, record JVM/GC behavior,
alternate execution order in paired rounds and calibrate batches above timer
resolution. Use at least seven paired rounds for new baseline runs and repeat
across processes when results overlap the noise floor. Measure profiling and
allocation counters in separate runs from the headline timings.

Publish one raw row per timed batch with case, engine, scope, run and round,
repetition count, elapsed seconds, threads, status and output checksum or
validation reference. Show median and spread of per-case times, p95 latency
for individual calls where sampled, peak RSS, paired speed ratio with uncertainty
and coverage. State the ratio as Kalos elapsed divided by JD+ elapsed. Report
absolute milliseconds or seconds for both engines alongside the ratio. Keep
timeouts and unsupported cases visible and summarize by workload group before
aggregating. Keep cold startup, native-only profiling and warm comparisons distinct.

## 4. Profile, then optimize the measured bottleneck

1. Profile the fixed-order fit on the archived slow cases and the X-11 initial
   tables on both short and long series. Sample native stacks and use scoped
   timers around conversion, preparation, initialization, filtering, optimization,
   decomposition and output assembly. Count objective and gradient evaluations,
   factorizations, allocated bytes and copies. On JD+, collect available JVM
   sampling, allocation and GC evidence separately from the timing rounds.
2. If fixed-order fitting dominates, separate cost per objective evaluation from
   the number of evaluations. Start with reuse of invariant model preparation,
   scratch storage or repeated factorizations where the validity key is explicit.
   Consider initializer, derivative or optimizer changes only after measuring the
   evaluation count and checking convergence, likelihood and model decisions on
   hard cases.
3. If X-11 dominates, measure arithmetic and memory traffic across centered
   filters, endpoints, seasonal phases and intermediate tables. Test rolling
   sums or reusable buffers only with explicit boundary, missingness and rounding
   checks. Measure optional intermediate output and R copies separately. Preserve
   the requested results for every timed comparison.
4. Use OpenMP where iterations or series are independent. Measure crossover
   lengths and speedup at fixed thread counts before changing thresholds.
   Compare parallel results with serial outputs and track memory growth. Test
   series-level concurrency against within-series parallelism for the same total
   worker budget.
5. Rerun the full supported benchmark scopes after each accepted change. Once
   the fixed-model X-13 path exists, use its end-to-end profile to reprioritize
   work. A faster isolated stage may account for little of the complete process.

## 5. Deliver and accept work in measured steps

1. **Baseline commit.** Reuse the reference harness to register benchmark cases
   and capture elapsed times on the installed split builds. Start with the
   [fixed-order fits](validation/reference/arma-fit-notes.md#runtime-comparison)
   and [initial-table verifier](validation/reference/verify-x11-initial.R).
   Publish explicit scope and provenance with the new baseline.
2. **Profiling commit.** Add repeatable profile commands and one report ranking
   measured costs on the representative slow cases and the batch workload.
   Pick the first optimization based on its expected end-to-end contribution.
3. **Optimization commits.** Change one performance mechanism per commit.
   Compare baseline, candidate and pinned JD+ on the same cases. Accept an
   improvement only after correctness gates pass and repeated paired measurements
   show a benefit above run-to-run variation on the target workload. Investigate
   regressions in short-series latency, p95, batch throughput and memory before
   adopting the change. Keep a qualified fallback where useful.
4. **Continuous qualification.** Run output parity and a small smoke matrix in
   ordinary CI. Collect performance trends on a stable benchmark host with
   variance measured before enabling timing regression thresholds. Run the full
   matrix for performance changes and before releases. Update
   [implementation status](IMPLEMENTATION_STATUS.md) with the scope, measured
   gains or regressions, numerical evidence and next bottleneck.
