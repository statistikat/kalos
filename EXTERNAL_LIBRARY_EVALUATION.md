> Ownership update, 2026-11-14: this evaluation belongs to Kalos. The repository
> split does not adopt these candidate algorithms. All experiments remain
> separate, with the original accuracy and matched-timing gates. The implemented
> baseline now extends through 22b.2; the historical survey below is retained.

# External numerical library evaluation

Recorded: **2026-11-13**. Status (updated **2026-11-26**): the candidates below have now been
profiled and benchmarked against this core under the procedure at the end of this
document; decision records per candidate are in the
M9 report (`M9_REPORT.md` at the family workspace root, with the qualification dossiers
listed there). **One candidate is now adopted**: the SLICOT SB03MD discrete-Lyapunov
solver ships as a vendored, license-pinned minimal closure behind an m ≥ 8 size gate
(M11 in [implementation status](IMPLEMENTATION_STATUS.md)), with the pivoted-QR route
retained below the gate and as the documented `KALOS_WITH_SLICOT=OFF` build. The
optimizer candidates were rejected on measured grounds; the evaluation also located a
larger internal lever (LM eligibility widening) outside this document's scope. Each
candidate section below now carries its dated disposition; the protocol steps at the
end were executed per candidate, with the platform-qualification step (step 6) open
on macOS/Windows as a recorded hosted-CI obligation of the SLICOT adoption.

Related documents: [implementation status](IMPLEMENTATION_STATUS.md),
[repository split plan](KALOS_REPOSITORY_SPLIT_PLAN.md), and
[numerical provenance](tools/reference/NATIVE_PROVENANCE.md).

## Purpose and current baseline

Evaluate existing numerical software where it can reduce execution time,
improve starting estimates, or provide an independently implemented optimizer.
Keep the current implementation and its statistical contracts as the comparison
baseline. A dependency's advertised speed or a different fitted answer does not
establish an improvement for this engine.

The recorded numerical delivery now includes **20c.1a observed information**.
The fitted timing baseline remains **20b.2b.2** (`9cd42b3`), with matched API
timing added at commit `4142955edb74d16999e13e4a52108acf181c9be0`. The core already
implements bounded fixed-order ARMA optimization over a concentrated
regression-SARIMA likelihood, stable/invertible parameter maps, exact fixed
coefficients, and structural transfer diagnostics. It also provides separately
labelled profile-ML and conditional-regression/residual-df observed-information
calculations. The latter is qualified against pinned X-13; TRAMO covariance and
joint regression uncertainty remain open. Follow the
[information contract](tools/native-tests/arma-information-contract.md) and
implementation status for their precise support limits. Freeze the actual
reviewed source revision before evaluating a candidate; the timing baseline
alone does not represent all current functionality.

OpenBLAS **0.3.34** has already been selected for standalone C++ and Python in
the split plan. The candidates below are additional, separate evaluations.
`rkalos` continues to use R's configured BLAS/LAPACK provider.

## Recommended order

1. **Profile stationary initialization and evaluate SLICOT's Lyapunov solver.**
   This targets the cost of every likelihood evaluation without changing the
   statistical estimation method.
2. **Evaluate RhysU/ar's Burg estimator for AR starting values.** Measure whether
   better starts reduce total fitting time and failed runs.
3. **Compare dlib BOBYQA with the existing BFGS optimizer on the same objective.**
   Keep both success criteria and model constraints explicit.
4. **Keep LBFGS++ as a secondary optimizer candidate.** Prioritize it if profiling
   or future derivative work makes a gradient-based replacement attractive.
5. **Treat CTSA as a source of specific candidate routines and comparisons.**
   Defer wholesale adoption; inspect the selected code and its dependency closure.
   StatsForecast is also useful for comparison, but its current ARIMA fitting
   path is not a standalone C++ library.

## Candidates and evaluation boundaries

| Candidate | Relevant capability | Dependency/license evidence | Proposed role |
| --- | --- | --- | --- |
| [SLICOT](https://github.com/SLICOT/SLICOT-Reference) | Discrete Lyapunov equations and conditioning estimates | Fortran library callable from C++; BLAS/LAPACK dependencies; current source declares BSD-3-Clause | Prototype a replacement for the stationary initialization solve |
| [RhysU/ar](https://github.com/RhysU/ar) | Header-only C++ Burg estimation over a sequence of AR orders | Core header uses the C++ standard library; MPL-2.0 | Generate candidate AR starts, retaining the existing final likelihood |
| [dlib](https://dlib.net/optimization.html) | Derivative-free BOBYQA, BFGS, and scalar optimization | C++; BOBYQA source declares Boost Software License and uses dlib matrix support | Alternative optimizer experiment |
| [LBFGS++](https://lbfgspp.statr.me/doc/) | L-BFGS and box-constrained L-BFGS-B | Header-only C++, Eigen dependency, MIT license | Secondary gradient-based optimizer experiment |
| [CTSA](https://github.com/rafat/ctsa) | Yule-Walker, Burg, Hannan-Rissanen, ARIMA and SARIMAX | ANSI C callable from C++; top-level BSD declaration with additional component notices/licenses | Selected initializers or external comparison, subject to source review |
| [StatsForecast](https://github.com/Nixtla/statsforecast) | C++ ARIMA numerical kernels within a forecasting package | Project declares Apache-2.0; inspected ARIMA code uses Eigen, pybind11 and C++20 `std::span`; fitting orchestration uses Python/SciPy | Comparative implementation; defer extraction into the C++17 core |

### SLICOT: reduce the cost per likelihood evaluation

The current [stationary initialization](src/state_space.cpp) constructs an
`m^2` by `m^2` Kronecker system and solves two right-hand sides with dense QR.
The factorization has approximately `O(m^6)` work. SLICOT's
[SB03MD](https://www.slicot.org/objects/software/shared/doc/SB03MD.html)
solves the discrete Lyapunov equation using an `O(m^3)` algorithm. This is a
performance hypothesis from the algorithms, not a measured speedup.

Evaluate the covariance equation `P - T P T' = Q` and the stability certificate
`S - T S T' = I`, reusing a Schur factorization where supported. Handle the
solver's returned scaling and failure status explicitly. Preserve the existing
scaled residual checks, symmetry checks, positive-definite stability certificate,
and conditioning policy. A solver-specific separation estimate does not silently
replace the current certificate condition measure.

Test stable, near-boundary, nonstationary, nonnormal, and rank-deficient-noise
models, including state sizes 1, 4, 8 and 16. Compare covariance, certificate,
likelihood and fitted results with the existing solver and independent identities.
Keep interruption checks around the external call and test successful reuse after
cancellation. This experiment concerns stationary initialization, not replacement
of the exact diffuse filter or its initial-coordinate normalization.

**Adopted (M11, 2026-11-26).** The vendored closure is SLICOT 5.9.1-dev at revision
`5eeb845d84f5a60f6069754b7819dad468951933` (BSD-3-Clause), the eight-member
link-map-minimal SB03MD set under `vendor/slicot/` with `UPSTREAM.md` provenance
and a sha256 manifest; every external BLAS/LAPACK symbol resolves against the
pinned OpenBLAS provider. `stationary_initialization` uses it for state order ≥ 8
and keeps the pivoted-QR route below the gate and in `KALOS_WITH_SLICOT=OFF`
builds. The contracts above ship verbatim: SCALE and INFO are inspected explicitly
and a failed solve surfaces (no silent fallback), the SEP estimate stays
informational and does not replace the certificate condition measure, interruption
checkpoints bracket the opaque Fortran calls, and reuse after cancellation is
qualified. Decision records: `tmp_opencode/m9-perf/seam/SEAM-QUALIFICATION-2026-11-25.md`
and `tmp_opencode/m9-perf/seam-r/RQUAL-2026-11-25.md`.

### RhysU/ar: inexpensive starting coefficients

The [standalone header](https://github.com/RhysU/ar/blob/master/ar.hpp)
implements Burg estimation and can produce estimates for several AR orders.
Use it first on complete, regular, eligible AR inputs. Verify coefficient signs,
centering, lag order, residual-scale conventions, and the conversion to the
existing parameter map before using a result as a start.

Preserve supplied fixed coefficients and run the existing stability checks.
An initializer that cannot produce an admissible candidate must report that
outcome; it must not clip coefficients or silently change the requested model.
Missing-data cases retain the existing start until a separate missing-aware
initialization policy is qualified. Do not remove gaps and treat the compressed
observations as consecutive times.

Burg and Yule-Walker estimate pure AR models; their output is not the final
exact diffuse regression-SARIMA estimate. Assess final objective quality,
convergence failures, total likelihood evaluations, and total elapsed time,
including the initialization cost. Keep the current deterministic start as a
control. Retain the [MPL notice](https://github.com/RhysU/ar/blob/master/LICENSE)
and record the exact selected source revision if adopted.

**Disposition (2026-11-26): subsumed, not adopted.** The capability targets
evaluation count, which the core's own qualified Hannan–Rissanen starts and the
landed LM eligibility route (M10-D, decision record
`tmp_opencode/m9-perf/lmext/LMBLOCK-QUALIFICATION-2026-11-25.md`: airline
86→11 evaluations, 2.4–3.4×) already deliver without new third-party code; the
measured effect of internal starts alone was 1.29–1.68× (decision record
`tmp_opencode/m9-perf/evals/STARTS-QUALIFICATION-2026-11-25.md`). RhysU/ar was
therefore not microbenchmarked against the internal start. Open residue
recorded there as *option B* (explicit client opt-in for the HR start, with
widened qualification) is not implemented; it remains available as a small
slice.

### dlib and LBFGS++: compare optimizers without changing the model

Run optimizer comparisons against the same prepared concentrated objective,
parameter constraints, starts, scale, and evaluation budgets. Separate this
experiment from the starting-value experiment so that their effects can be
measured independently.

[BOBYQA](https://dlib.net/optimization.html#find_min_bobyqa) avoids numerical
gradient evaluations, which may help when likelihood calls dominate runtime.
It requires at least two variables; one-variable controls need a scalar solver.
Its box constraints do not by themselves implement the core's mixed fixed/free
stationarity and invertibility constraints. Begin with cases where the admissible
parameterization can be represented faithfully. Any adapter must preserve
unavailable-candidate statuses and must not substitute arbitrary finite penalties.
Cases that an adapter cannot support remain explicitly outside its comparison.

[LBFGS++](https://lbfgspp.statr.me/doc/) expects an objective and gradient.
Account for the cost of qualified numerical gradients if those are used; changing
the optimizer does not make derivatives of the existing double/LAPACK likelihood
automatic. Limited-memory updates are not automatically advantageous for the
small parameter counts in current SARIMA models.

For either candidate, validate convergence independently of the library's success
flag, retain rejection and evaluation counts, and preserve cancellation and
bounded execution. Optimizer inverse-Hessian approximations remain optimization
aids and are not substituted for the separate information/uncertainty contract.

**Disposition (2026-11-26): rejected on measured grounds; no head-to-head
comparison was run.** The 300-sample stack attribution (decision record
`tmp_opencode/m9-perf/SLICOT-scoping-2026-11-25.md`, table in §4) puts the
optimizer loop itself at ≈1% of fit time, so the document's own step-2 profiling
gate disposes of both before any comparison: even a zero-cost optimizer bounds
below the timing-variability floor of step 7, and BOBYQA's derivative-free
character would *raise* evaluation counts on the dominant likelihood cost. The
doc's small-parameter-count caveat for LBFGS++ is thus confirmed by
measurement, and its trigger condition ("if profiling makes a gradient-based
replacement attractive") does not fire.

### CTSA and StatsForecast: broader implementations

CTSA's [initialization source](https://github.com/rafat/ctsa/blob/master/src/initest.c)
contains Yule-Walker, Burg and Hannan-Rissanen routines. Its broader package may
also supply useful comparative fits. Before extraction, review selected routines,
their transitive dependencies, numerical conventions, and error handling.
Concrete findings from the inspected source are:

- [dist.c](https://github.com/rafat/ctsa/blob/master/src/dist.c) contains MIT and
  LGPL-3.0-or-later component declarations alongside the BSD file declaration.
- Some attribution/public-domain comments require checking against the original
  source; the top-level BSD label alone is insufficient evidence for every routine.
- [emle.c](https://github.com/rafat/ctsa/blob/master/src/emle.c) contains process-exit
  and console-output error paths that cannot be imported unchanged into R/Python.

StatsForecast's [C++ ARIMA kernels](https://github.com/Nixtla/statsforecast/blob/main/src/arima.cpp)
are coupled to pybind11/Eigen, while its
[Python ARIMA module](https://github.com/Nixtla/statsforecast/blob/main/python/statsforecast/arima.py)
still invokes SciPy optimization. Treat reuse as an extraction/qualification
project, not a drop-in C++17 fitting dependency.

**Disposition (2026-11-26): not pursued.** The one capability of interest here
(Yule-Walker/Burg/Hannan–Rissanen initialization) already exists internally,
qualified and pinned (see the RhysU/ar disposition above), so extraction would
import the license and error-path findings recorded above for no measured gain;
both projects remain comparative references only, per their source review.

## Common benchmark and acceptance procedure

1. Freeze the current source revision, build settings, and reference hashes.
   Pin each candidate's source revision and record all selected components,
   dependencies, and notices in the existing provenance workflow.
2. Profile the current fitted workload before implementation. Reuse
   [the prospective fit matrix](inst/tinytest/fixtures/native/arma-fit-cases.R),
   [fit qualification](tools/reference/verify-arma-fits.R), and
   [matched API timing](tools/reference/fit-timing.R). The existing timing helper
   assumes native/JD+ engines; extend or wrap it for candidate comparisons while
   preserving its original comparison and frozen artifacts.
3. Compare one change at a time. Cover monthly/quarterly data, permitted seasonal
   orders, short/long series, complete/gapped observations, fixed/free coefficients,
   log/windows, weak regressors, near-unit roots, and redundant factors within
   current support bounds. Candidate-specific unsupported cases must be visible.
4. Run numerical qualification before timing. Preserve existing tolerances and
   independent identities. Compare final objective, coefficients where identified,
   counts, domains, and failure categories; preserve the information calculation's
   method labels and availability gates. Do not select only successful fast runs.
5. Record warmed repeated timings, setup versus fit-only cost, memory/allocations,
   objective/gradient evaluations, and failures. Record CPU, OS, compiler flags,
   BLAS/LAPACK provider/version/integer ABI, and both BLAS and Kalos thread counts.
   Keep startup and conversion costs separately visible. Prevent nested-thread
   oversubscription in controlled comparisons.
6. Validate on Linux, macOS, and Windows, with sanitizer and cancellation/reuse
   coverage where applicable. Source portability is not platform qualification.
7. Adopt only after a repeatable end-to-end benefit beyond timing variability, or
   a documented robustness improvement with acceptable cost, and unchanged
   statistical acceptance. Otherwise retain the current implementation.

The evaluation deliverable is a reproducible results table and a short decision
record per candidate: adopted, deferred, or rejected; supported cases; measured
benefit; integration cost; source/license inventory; and remaining limitations.
No universal fastest-library claim follows from these recommendations.
