# Native numerical foundation checks

The C++ core is independently authored and contains no translated JDemetra+
source. It currently supplies time indexing, differencing polynomials,
convolution, finite filters, transformations, LAPACK adapters and a stationary
fixed-parameter innovations likelihood, with a separate exact diffuse kernel
for integrated SARIMA models, plus concentrated regression likelihood with
fixed ARMA parameters. A bounded fixed-order ARMA optimizer builds on these
kernels. It does **not** fit seasonal-adjustment models.

Run from the repository root with any supported C++17 compiler:

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -Isrc src/numerics.cpp tools/native-tests/test_numerics.cpp -o /tmp/persephone-numerics-test
/tmp/persephone-numerics-test
```

On a toolchain supporting AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
c++ -std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer -g -Isrc src/numerics.cpp tools/native-tests/test_numerics.cpp -o /tmp/persephone-numerics-sanitized
/tmp/persephone-numerics-sanitized
```

The installed-package tests in `inst/tinytest/test-native-primitives.R` exercise
the Rcpp boundary and compare filters with independent R operations. Missing
input and out-of-range active filter taps produce an explicit mask and `NA`
outputs; zero-weight taps do not propagate missingness. No endpoint extension
is performed. A nonfinite coefficient or arithmetic overflow is an error.

## Integration conventions

Two conventions resolve the boundary-friction items accumulated across
M2–M3 (each contract states its own instances):

* **Struct growth is contract surface.** The echo members of the pass and
  fit result structs (`first_period`, `reference_series`, `mode`, the
  provenance block) are part of the composition contract: a downstream
  result view must rebuild companions faithfully, so adding, removing or
  reordering a member is a **schema change** for every consumer, not
  merely a new getter. Contracts state each growth event where it happened
  (the S2→S3 C-pass growth, the mode echo of M3a) and proposed growths
  carry the same warning (the fit-result fingerprint).
* **Verbatim statuses and messages are pinned.** Unsupported statuses and
  the pinned validation/error messages propagate verbatim through
  downstream boundaries and are pinned by downstream tests; the core does
  not reword them unilaterally. Improvements that would change a pinned
  string (e.g. folding the pass label and mode into
  `check_decomposition`'s message) or add a member to a pinned result
  struct (e.g. the `X11SelectionPass` mode echo) are recorded in the
  contract as **coordinated-change-needed** and applied only in a
  coordinated change with the affected boundaries.

## Linear algebra (19b)

`src/core/linear_algebra.hpp` owns column-major memory without Rcpp types.
The implementation uses R's declared Fortran interfaces to DPOTRF/DPOTRS,
DPOCON, DGELSY and DGEMM, linked by the package's portable Makevars flags.
No LAPACK implementation is copied. Cholesky reports a 1-norm reciprocal
condition **estimate** and accepts symmetry differences only within 64 machine
epsilons of the largest matrix entry, averaging accepted pairs.

Pivoted QR uses the caller's positive rank threshold (default `1e-10`) and
returns minimum-norm solutions for rank-deficient or underdetermined systems.
Coefficients retain original column order; pivots are diagnostic. Residuals
are recomputed as `B - A %*% coefficients`. Rank is an estimated numerical
rank, not a promise of coefficient identifiability; no covariance or QR
condition estimate is fabricated. Zero matrix dimensions, missing/nonfinite
values, integer-range overflow, and non-SPD Cholesky inputs are explicit errors.

`inst/tinytest/test-native-linear-algebra.R` compares with independent analytic,
R solve and SVD cases. `test_linear_algebra.cpp` adds storage/factor corruption,
bounds and interruption/reuse checks outside Rcpp. Sanitizing these wrappers
does not instrument the prebuilt R BLAS/LAPACK libraries themselves.

For a Unix R installation, the standalone linear-algebra harness can be run
with its installed LAPACK/BLAS (override `CXX` for your C++17 toolchain):

```sh
${CXX:-c++} -std=c++17 -Wall -Wextra -Wconversion -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Isrc \
  $(R CMD config --cppflags) src/linear_algebra.cpp \
  tools/native-tests/test_linear_algebra.cpp \
  $(R CMD config LAPACK_LIBS) $(R CMD config BLAS_LIBS) \
  $(R CMD config FLIBS) $(R CMD config --ldflags) -o /tmp/persephone-linear-algebra
/tmp/persephone-linear-algebra
```

## Transforms and regressor domains (19c)

The elementary `none`/`log` transformations preserve explicit missing masks.
Log requires positive observed values and returns each observation's log
Jacobian (`-log(y)`), so a future likelihood can select the correct estimation
span before summation. Inverse log is pointwise exp; it does not apply a
lognormal forecast-mean adjustment. Nonfinite input or exponential overflow/
underflow is rejected. Internal Rcpp transformation defaults select `none`.

`native_prepare_inputs()` validates a monthly/quarterly univariate `ts` and
a uniquely named list of regressor series or an `mts`. It aligns by integer
period, requires finite regressors throughout the entire requested observed,
backcast and forecast domain, and crops only unused extra supplied periods.
Missing responses are allowed; missing regressors, padding, recycling and
implicit extrapolation are not. Output is a serializable plain-data contract.
Estimation spans and coefficient constraints are connected by 20a.3/20b.1 below;
context-object conversion and automatic transformation selection remain later work.

The standalone transform harness uses no R headers or libraries:

```sh
c++ -std=c++17 -Wall -Wextra -Wconversion -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Isrc \
  src/numerics.cpp src/transforms.cpp tools/native-tests/test_transforms.cpp \
  -o /tmp/persephone-transforms
/tmp/persephone-transforms
```

## Stationary likelihood (20a.1)

The fixed-parameter stationary state-space/ARMA pilot, mathematical conventions,
scale normalization, missing-data behavior and explicit numerical limits are
documented in [state-space-contract.md](state-space-contract.md).
The standalone `test_state_space.cpp` harness links `src/numerics.cpp`,
`src/linear_algebra.cpp` and `src/state_space.cpp` with the same R LAPACK/BLAS
flags shown above. It exercises callback cancellation at every boundary and
subsequent successful reuse. Installed R tests compare closed forms, an
independent multivariate Gaussian density, and R's public Kalman/ARIMA APIs.
These establish the stated kernel contract, not JD+ likelihood conventions or
full SARIMA/seasonal-adjustment parity.

## Exact diffuse likelihood (20a.2)

The internal `.native_diffuse_likelihood()` accepts finite and diffuse initial
loadings explicitly. `.native_sarima_likelihood()` constructs a monthly/quarterly
fixed-parameter SARIMA model with initial observation history treated diffusely.
The core keeps finite and diffuse uncertainty separate throughout filtering; no
arbitrary large initial variance is used. Its initial-state coordinate measure,
effective sample count and numerical rejection rules are documented in
[diffuse-state-space-contract.md](diffuse-state-space-contract.md).

At most 32 total states are supported, including at most 16 stationary ARMA
states. Ordinary and seasonal differencing orders range from zero to two,
subject to that total bound. The common monthly airline model uses 27 states.
The scalar mean for integrated models stays zero; 20b.1 below supplies drift as
an explicit regressor. Unresolved diffuse states produce
an explicit unavailable likelihood, not a successful objective for optimization.

`test-native-diffuse.R` uses closed forms, differencing and an independent joint
Gaussian calculation integrated over the initial-state coordinates. Optional
external oracle scripts exercise public KFAS and pinned JD+ APIs; neither
package is required by the native kernel or its regular test suite. The
standalone sanitizer harness is `test_diffuse_state_space.cpp`; compile it with
`numerics.cpp`, `linear_algebra.cpp`, `state_space.cpp`, `sparse_history.cpp`,
`finite_state_factor.cpp`,
`diffuse_state_space.cpp` and the R LAPACK/BLAS flags above.

## Fixed-model windows and transformations (20a.3)

`native_fixed_likelihood()` selects an explicit inclusive estimation window,
prepares identity/log inputs and evaluates the fixed SARIMA model with a fresh
prior at that window's start. It preserves domains, missing positions and row
maps, and reports transformed and adjusted objectives separately. The log
adjustment follows the named pinned JD+ direct-window calendar convention;
it is not inferred from diffuse pivot positions. See
[fixed-likelihood-contract.md](fixed-likelihood-contract.md) and the independent
identities in `inst/tinytest/test-native-fixed-likelihood.R`. Regression and drift
are added through the separate 20b.1 contract below; ARMA estimation and native
seasonal adjustment remain unavailable.

## Optional OpenMP (19d)

Independent filter, convolution and transformation outputs may use bounded
OpenMP regions when the C++17 compile/link/runtime probe succeeds. Serial
fallback is automatic. `PERSEPHONE_NUM_THREADS` requests 1..64 threads (default
two), capped by the OpenMP runtime. See [openmp-contract.md](openmp-contract.md)
for thread safety, deterministic arithmetic, cancellation and standalone checks.

## Fixed-SARIMA regression (20b.1)

`native_regression_likelihood()` connects named fixed/free regressors, estimated
stationary mean or integrated drift, shared-gain whitening, rank-aware GLS and
explicit ML/residual-df covariance scales. It retains window/log/context semantics
and reports nonidentifiable coefficients without dropping columns. The exact
contract and numerical limits are in
[regression-likelihood-contract.md](regression-likelihood-contract.md).

Compile `test_regression_likelihood.cpp` with `numerics.cpp`,
`linear_algebra.cpp`, `state_space.cpp`, `sparse_history.cpp`,
`finite_state_factor.cpp`, `diffuse_state_space.cpp`, `regression_likelihood.cpp`
and the R LAPACK/BLAS flags above. Run both serial and `-fopenmp` builds where
supported; `PERSEPHONE_REQUIRE_OPENMP=1 OMP_NUM_THREADS=4 OMP_DYNAMIC=FALSE`
requires actual workers in the latter. The harness checks identical coefficients,
whitening, covariance and likelihood, plus main-thread cancellation/reuse.

## ARMA parameter/objective contract (20b.2a)

The internal parameter map uses bounded reflections for fully free factors and
checked coefficient coordinates for mixed fixed/free factors. A reusable plain
R objective contract evaluates the existing concentrated regression likelihood
at one admissible proposal, with explicit unavailable/failure statuses.
See [arma-objective-contract.md](arma-objective-contract.md) for signs,
constraints, numerical margins and the optimizer boundary.

`test_arma_parameters.cpp` compiles with `src/arma_parameters.cpp` alone, without
R headers/libraries. `test_arma_objective.cpp` adds `src/arma_parameters.cpp` and
`src/arma_objective.cpp` to the regression harness's source/link list above.
Use the same strict-warning and ASan/UBSan options. Mandatory R tests compare
independent polynomial roots and dense Gaussian AR/MA objective profiles.

## Deterministic ARMA optimization (20b.2b.1)

`native_arma_optimize()` accepts a prepared objective and runs bounded C++ BFGS
with feasible backtracking and checked numerical derivatives. It retains fixed
parameters, explicit failure/convergence statuses and the final likelihood
without uncounted reevaluation. The contract, working-coordinate convention,
controls and remaining identification/reference gates are in
[arma-optimizer-contract.md](arma-optimizer-contract.md).

Compile `test_optimizer.cpp` with `src/optimizer.cpp` alone; neither needs R.
For `test_arma_optimizer.cpp`, add `src/optimizer.cpp` and
`src/arma_optimizer.cpp` to the objective harness source/link list above.
Both support the same strict-warning/ASan/UBSan workflow. The R suite uses
independent dense Gaussian minima for scalar, joint, mixed and seasonal models.

## Fitted-model qualification and transfer sensitivity (20b.2b.2)

The [fit qualification notes](../reference/arma-fit-notes.md) describe 64
monthly/quarterly cases, persistent-AR controls, independent density minima,
alternate starts and pinned direct comparisons. These exposed a precision loss
with large drift terms; affine regression centering before whitening resolves
it without changing the model or optimizer tolerances.

`native_arma_identification()` and the optimizer's `parameter_diagnostics`
screen exact/near parameter redundancy using an algebraic transfer Jacobian and
column-normalized SVD. The [diagnostic contract](arma-identification-contract.md)
distinguishes this structural screen from finite-sample identification and
uncertainty. Compile `test_arma_identification.cpp` with `numerics.cpp`,
`linear_algebra.cpp`, `arma_parameters.cpp`, `arma_identification.cpp` and the
R LAPACK/BLAS flags above; strict warnings and ASan/UBSan cover all callbacks.

## Observed information and uncertainty

`native_arma_information()` computes physical-coefficient observed curvature
separately from optimization, with labelled full-profile ML and fixed-regression
residual-df covariance. The [information contract](arma-information-contract.md)
defines accuracy, structural, stationarity and curvature gates. Independent
Gaussian tests exercise all 64 fitted cases and nuisance Schur complements.
Standalone `test_information.cpp` and `test_arma_information.cpp` cover analytic
identities, signed/weak/rank-deficient information and exhaustive cancellation.
The [pinned comparison](../reference/arma-information-notes.md) qualifies the
X-13 convention; the separate TRAMO covariance contract is described below.

### TRAMO residual information and legacy replay

`test-native-tramo-information.R` and `test-native-tramo-legacy.R` exercise the
accurate default and explicit numeric-state replay without Java. The independent
dense oracle and additive recorded states are installed under `fixtures/native`.
`test_residual_information.cpp` adds analytic residual-information, rank/domain,
budget and interruption tests; `test_arma_information.cpp` also covers both TRAMO
modes. Include `src/residual_information.cpp` in the existing ARMA harness link.
See [the complete contract](tramo-information-contract.md).

### Estimated-ARMA regression uncertainty

`test-native-regression-information.R` checks conditional GLS inference against
independent dense covariance blocks across the 64 fitted cases and boundary/unit
changes. `test_regression_information.cpp` independently checks closed OLS
covariance, residual df, range handling and cancellation. Link the same source
set as `test_arma_information.cpp`. The
[contract](regression-information-contract.md) distinguishes this conditional
covariance from the joint profile-ML estimator.

`test-native-joint-information.R` now covers the optional joint profile-ML
estimator against full independent Gaussian Hessians. The standalone
`test_vector_jacobian.cpp` checks analytic vector derivatives, and
`test_joint_regression_information.cpp` checks closed AR(1) GLS derivatives,
full joint curvature, budgets, conditioning and exhaustive cancellation. Add
`src/vector_jacobian.cpp` and `src/joint_regression_information.cpp` to the ARMA
harness source list. See the same regression-information contract for the
Schur identity, estimator distinction and versioned timing procedure.

## Initial additive X-11 tables

The standalone `test_x11_initial.cpp` links the C++ numerical/filter sources
listed in [x11-initial-contract.md](x11-initial-contract.md), without R/Rcpp,
BLAS or LAPACK. Serial and owned-OpenMP sanitizer runs exercise closed additive
controls, masks/ranges, caller-only interruption and reuse. Mandatory R tests
add public intermediate-table comparisons, independent complete operators,
calendars, missing-support audits and serialization. These are initial B tables;
they do not expose a full native seasonal-adjustment fit.

## Extended-precision scratch and range guards

All range guards (underflow/overflow rejection) follow
[extended-precision-range-contract.md](extended-precision-range-contract.md):
binary64 is the numeric contract, `long double` is accumulation scratch only,
guarded conversions flush sub-binary64 magnitudes to zero identically on every
platform, and every explicit underflow contract is evaluated on binary64
quantities. This fixed the three Linux x86_64 standalone failures recorded in
the 2026-11-15 baseline qualification without touching any fixture.

## Moving five-year dispersion and sigma weights

`test_x11_sigma.cpp` and the 125-case pinned fixture qualify the moving
five-year dispersion windows and two-pass sigma weights of the default X-11
extreme-value stage (item 22b.3a) for both delivered rules — the exact pinned
legacy rules and the scale-limit alternative. The maintainer resolved the
default on 2026-11-17: `legacy` is the product default, applied at the
binding and product layers; the core keeps the policy a required argument.
See [x11-sigma-contract.md](x11-sigma-contract.md).

## Same-phase replacements and correction factors

`test_x11_replacement.cpp` and the 22-case pinned fixture qualify the
same-phase replacement values, neighbor provenance, endpoint extension,
insufficient-neighbor period-average fallback, projection masking, correction
factors and apply-corrections identity of the default X-11 extreme-value stage
(item 22b.3b) on top of the item 22b.3a weights. B17/B20 table placement and
the CV correction variant are not delivered or claimed. See
[x11-replacement-contract.md](x11-replacement-contract.md).

## Complete fixed additive B pass

`test_x11_b_pass.cpp` and the 18-case pinned fixture qualify the complete
fixed additive B pass of the default X-11 decomposition (item 22b.4 S1): all
20 public tables bit-exact on their pinned spans, the three correction rounds
with their detection series and analysis positions, the pinned fixed
Henderson trend filter (local-polynomial weights, mmsre asymmetric endpoints)
and the pinned seasonal processing conventions reproduced file-locally. The
C/D passes, automatic filter choice, cross-validation, multiplicative and
non-seasonal modes and the mean-endpoint fallback below the trend length are
not delivered or claimed. See [x11-bpass-contract.md](x11-bpass-contract.md).

## Complete fixed additive C pass

`test_x11_c_pass.cpp` and the 18-case pinned fixture qualify the complete
fixed additive C pass of the default X-11 decomposition (item 22b.4 S2): all
14 public tables bit-exact on their pinned spans and the final extreme-value
analysis on C13 with its correction history, composed from the item 22b.4 S1
B pass exactly like the pinned X11Kernel (C1 = series - B20) and enforcing
that coupling in the API. The D pass, automatic filter choice,
cross-validation, multiplicative modes and ARIMA-regression inputs are not
delivered or claimed. See [x11-cpass-contract.md](x11-cpass-contract.md).

## Complete fixed additive D pass

`test_x11_d_pass.cpp` and the 22-case pinned fixture qualify the complete
fixed additive D pass of the default X-11 decomposition (item 22b.4 S3): all
16 public tables bit-exact on their pinned spans, the final decomposition
identities, the D9 MSRE diagnostic table and the original/extended calendar
behavior (four hand-extended fixture cases pin the context horizons and the
dropped-original-calendar diagnostic), composed from the item 22b.4 S2 C
pass exactly like the pinned X11Kernel (D1 = series - C20) and enforcing
that coupling in the API. The MSRE filter selection, the automatic Henderson
choice, cross-validation, the period-specific corrector, multiplicative
modes and ARIMA-regression inputs are not delivered or claimed. See
[x11-dpass-contract.md](x11-dpass-contract.md).

## Pass-level X-11 selection flow

`test_x11_selection_pass.cpp` and the 15-case pinned fixture qualify the
pass-level additive selection flow of the pinned X11Kernel (item 22b.4 S4):
the staged seasonal requests through the qualified 22b.3d plan, the
automatic Henderson choices with the pinned inline B7 branch, the global-
MSR D-final decision with both qualified iteration policies and the full
attempt trace, and the selected-state B/C/D tables — while the pinned
degenerate NaN choices stay explicit unsupported statuses instead of being
reproduced. Cross-validation, multiplicative and non-seasonal modes, the
period-specific corrector and ARIMA-regression inputs are not delivered or
claimed. See [x11-selection-pass-contract.md](x11-selection-pass-contract.md).

## Multiplicative decomposition mode

`test_x11_multiplicative.cpp` and the 91-case pinned multiplicative fixtures
qualify the multiplicative decomposition mode (item 22b.5 M3a) across the
S1–S4 passes: the pinned ratio remove at every composition site, the
`X11Kernel.check` positivity gate with the pinned message, the
`makePositivity` repair of B7/C7/D7/D12 (fixture repair counts prove the
branch live), the mode-default-1 sigma/corrector arithmetic, the
relative-form `MsrTable.of` flag set exactly for `Multiplicative`, the
mode-aware `doMSR` irregular and the new pinned `calcICR` fact that the
flag makes the lag-1 variations relative in both windows (it decides the
additive-NaN `const-msr` contrast at 3.515625). The other modes are now
all delivered too: log-additive with its own qualification in item 22b.5
M3b-i and pseudo-additive in item 22b.5 M3b-ii (see below). See
[x11-multiplicative-contract.md](x11-multiplicative-contract.md).

## Log-additive decomposition mode

`test_x11_log_additive.cpp` and the 91-case pinned log-additive fixtures
qualify the log-additive decomposition mode (item 22b.5 M3b-i) across the
S1–S4 passes: the pinned `X11Kernel.process` input transform
(`DoubleSeq.log()` = `fn(Math::log)`, bit-identical to C `log` on every
fixture value), the additive-composed step arithmetic on the log-domain
input (`isMultiplicative()` is false: difference removes, centre-0 sigma,
0 mode default, absolute-form MSRE/MSR, no `makePositivity` guards), the
positivity gate shared with `Multiplicative` through the pinned constant
message on the raw present rows, log-domain step tables verified bit-exact
through the fixed passes, the extended calendars and the complete 15-case
kernel-selection inventory (both constant-series controls degenerate like
the additive fixture, the documented contrast with the decided
multiplicative `const-msr`). The results-builder `exp` of D10..D13 and the
LogAdditive bias-correction skip are export-stage behaviour and are not
delivered or claimed. See
[x11-log-additive-contract.md](x11-log-additive-contract.md).

## Pseudo-additive decomposition mode

`test_x11_pseudo_additive.cpp` and the 91-case pinned pseudo-additive
fixtures qualify the pseudo-additive decomposition mode (item 22b.5 M3b-ii)
across the S1–S4 passes: the pinned `pseudoadd.X11{B,C,D}StepPseudoAdd` and
`MsrFilterSelectionPseudoAdd` pipeline that `X11Kernel` selects for mode
`PseudoAdditive`. Because `isMultiplicative()` is true, the whole context
runs the ratio arithmetic (ratio `remove`, centre-1 sigma, 1 mode default,
`makePositivity` trend repairs proven live on the spikeend/wideband cases,
relative-form `calcICR` lag-1 variations), while the subclasses override
exactly twelve composition sites with the bytecode-exact `PseudoAddUtility`
forms (`removeSeasonalAdjusted(a, b, c) = a − c·(b − 1)`, `adjustRefSeries
(ref, corr, s, r) = ref + s·(r/corr − r)`, `removeIrregular(a, b) = a − b +
1`, `removeSeasonalAdjustedWithBorders` keeping the ratio at the borders and
`x − f·(s − 1)` inside) and the global-MSR walk measures `x − seas + 1` with
the `MsrTable.of` relative flag staying the strict `mode == Multiplicative`
test (false). Unlike `multiplicative`/`log_additive`, `X11Kernel.check()`
imposes no positivity requirement, so non-positive inputs are legal; the
`const-msr` contrast is decided (I/C 3.515625, one decisive 3x9 evaluation)
like the multiplicative run while `const-fixed` stays NaN-degenerate, and
the 15-attempt `q-longmsr` walk decides at its fifteenth so the five-year
cap and the full run deliver identical tables with differing MSR metadata.
See [x11-pseudo-additive-contract.md](x11-pseudo-additive-contract.md).

## X-13 deterministic-effect allocation staging

`test_x13_allocation.cpp` and the 7-case pinned x13-allocation fixtures
qualify the deterministic-effect allocation staging of the pinned
`X13Kernel` (item 22c slice M3c): the fixed-coefficient preadjust
(`initialStep`) that allocates the model variables into the stored a-tables
and the `out` series fed to X-11, and the finals restore (`finals`) that
re-adds the effects, with the `X11Spec` mode forcing in between. The pinned
kernel gives no fixed-coefficient entry point, so the capture runs the full
kernel and validates a compositional recompute seam: every stored table is
recomposed bit-exactly from the pinned `deterministicEffect`/`backTransform`/
`TsData`-fold/`correctSeries` pieces (22 checks per case, refuse-on-
disagreement). The fixtures pin the class predicates with the EXACT
`regeffect` attribute match (an outlier outside Trend/Seasonal/Irregular
contributes to no table; `a9u` enters neither `out` nor the finals), the
`DataBlock.addAY` damping rule, the pinned left-associative fold orders, the
exp back-transform boundary (a1 stays the complete input bit-exactly), the
`updateSpec` forcing (non-log → additive, log → multiplicative unless
log_additive/pseudo_additive), the op/invOp/mean truth table with the
log-additive subtract-then-multiply asymmetry, and the `correctSeries`
zero-weight replacement (negative zero included, NaN kept). The X-11 leg
over the preadjusted series and the end-to-end restore run through the
qualified selection passes; the log-additive results-builder rescale
(ratio bias correction, the `X11Spec` default) is reconstructed for
verification only, at the transcendental boundary. Estimation, the ARIMA
extension, missing-row interpolation, length-of-period transforms,
benchmarking and diagnostics stay outside the surface (fail closed). See
[x13-allocation-contract.md](x13-allocation-contract.md) and
[x13-allocation-scope-22c.md](x13-allocation-scope-22c.md).

## Restricted fixed-model X-13 fit

`test_x13_fit.cpp` and the pinned x13-fit fixtures qualify `x13_fit`, the
restricted fixed-model X-13 fit (item 22c M3-finale): one entry point
composing the two allocation seams and the pass-level selection run in the
pinned `X13Kernel.process` order — `initialStep`, the X-11 run over `out`
under the `updateSpec`-forced mode, the `X11Results` tables as `finals`
consumes them (the log-additive results-builder stage — `finalResults` →
`ratioBiasCorrection`, the `X11Spec` `Ratio` default — delivered as fit
composition) and the finals restore — with the final components exposed as
the pinned final-components triplet `D11final/D12final/D13final` (no
separate final-trend or final-irregular table exists in the pinned jars).
The pinned fixed-path horizon domain is exactly `{0, 0}`: `initialStep`
engages `ExactArimaForecasts` (a1a/a1b, the extended out calendar, the
split finals) for any nonzero horizon — negatives becoming
`|h| · annual frequency` — so every nonzero request answers the explicit
unsupported status, as do non-fixed seasonal requests, the automatic
Henderson length and the missing required sigma policy; shapes throw
through the seams. The fixtures were captured end to end through the
`x13_fast` entry point on fully fixed monthly specs (11 cases: 9
fixed-route covering all four forced modes with Stable/S3X1/S3X9/S3X15 and
Henderson 5/13, 2 nonzero-horizon route-evidence cases), with the capture
recomposing all 22 stored tables per case at bit precision (198 checks) and
one case re-executed bit-exactly through the R `x13_fast` result path
(proto round trip, 14 tables). 16 embedded cases, 1440 checks; OMP, serial
and sanitized builds 48/48. See
[x13-fit-contract.md](x13-fit-contract.md).

## Fixed-order estimation control

`test_estimation_control.cpp`, the pinned estimation-control corpora
(17 Zero/Default cases plus 10 HannanRissanen/Multiple cases) and
`tools/verify_estimation_control.cpp` qualify `estimate_sarima_fixed`, the
estimation-CONTROL layer of the pinned `RegSarimaComputer` (item 21-A) over
the qualified 20b.2 objective/optimizer kernels: the pinned Zero/Default
starting-point fills on the probe-verified sign convention (rjd3 raw
`1 + phi(B)` versus Kalos `1 - phi(B)`), the transcribed
`HannanRissanenInitializer` (item 21-A.2, qualified separately by
`test_hannan_rissanen.cpp`), the pinned `Multiple` two-pass
internal-precision policy and its strict-likelihood selection, the
`eps`-precision pass, the pinned likelihood-only guard, the
`ur_regular`/`ur_seasonal` unit-root cancellation gates, deflations and
strict-improvement acceptance with the chronological per-pass decision
records (chain shape and selection bit asserted per starting point, bit-exact
record reuse per thread count). Control fields are required request data (no
defaults); the Hannan-Rissanen scope limits (masked series, fixed design
coefficients) and the `ml = false` form are explicit rejections. Agreement
with the pinned Levenberg-Marquardt oracle is at fit tolerance (2e-3
coefficients, 5e-4 likelihood, guards at 1e-9; measured agreement over the
27 cases 6.2e-4/7.25e-6). The score and information, the Rcpp boundary, any
promotion of pinned values to configuration and qualification of the pinned
behaviour on degenerate unbounded-likelihood inputs are not delivered or
claimed. See [estimation-control-contract.md](estimation-control-contract.md).

## TRAMO model-builder decision mirror

`test_tramo_model_builder.cpp`, the double-captured 22-case JVM corpus under
`validation/evidence/native/tramo-model-builder/` and
`tools/verify_tramo_model_builder.cpp` qualify `kalos::build_tramo_model`
and `kalos::tramo_template` (item 23-A): the LIVE pinned
`TramoModelBuilder.build` decisions — start rule (autoModel airline plus
the post-`initializeVariables` mean overwrite versus
`withPeriod(12)`), the `initializeMean` fixed-coefficient quirk, the
derived `RegressionSpec.isUsed` gate, the `AverageInterpolator` missing
plan (interior `(left + right) / 2.0`, forward-filled leading run,
left-filled trailing run, ascending missing list), the `sortVariables`
order with the `regeffect`/`ComponentType` attribute vocabulary, and the
processing-log warnings — replayed as byte-identical probe lines (the
driver `tests/tramo_model_builder_replay.h`, Java-`%.17g` shortest-decimal
formatter, raw-bit series). All seven templates register NO calendar
variables (the decode finding); every unproven path (stock/holidays/user
trading days, TD types other than TD2, julian/include Easter, non-monthly
frequency, declared periods other than 0/12, resolved user variables) is
refused fail-closed, plus the template-name matrix (`rsa*`/`Trfull`
rejected). Design algebra (day clustering, LoP and Easter columns), the
log/level decision, the `check()` collinearity pass, automatic outlier
identification and the Rcpp boundary are not delivered or claimed. See
[tramo-model-builder-contract.md](tramo-model-builder-contract.md).

## SEATS model admissibility and decomposition mirror

`test_tramo_seats_model.cpp`, the double-captured 35-case JVM corpus under
`validation/evidence/native/tramo-seats-model/` and the replay driver
`tests/tramo_seats_model_replay.h` qualify
`kalos::validate_tramo_seats_model` (item 24-A): the LIVE pinned
`DefaultModelValidator` (simplifyModel trim with cutOff, the xl<1
`stabilizeMA` projection versus the xl≥1 `fixMaUnitRoots` unit-circle
projection with its degree-1 −1 quirk), the `SeatsToolkit` selector chain
(`TrendCycleSelector`/`SeasonalSelector` wired from the DecompositionSpec
knobs, the greedy D1 division), the `RootDecomposer` linear system
(`LULinearSystemSolver2` bit-exact symmetric filters, `checkSpecialCases`)
and the `setVarianceMax` minima-sum decomposability decision
(`Spectrum$Minimizer` by-roots/by-grid), plus the kernel flags and the
`ProcessingLog` frame — all golden numerics IEEE-754 bit-exact (raw sar/ur/
sfd, validator parameters; no measured band at landing). The `kevt` frame is
exact, exact-prefix for the 13 `legacy_approx` rows whose live continuation
into `DefaultModelApproximator` is register D6 (stays unimplemented: the unit
reports `approximation_required` and stops). Spectral factorization
(one-sided MA/variance), the variance allocation and `k.ucar` facts are
24-B scope; five QR-cascade corpus rows (degree > 2 `roots()`: `d_d2`,
`q_airline`, `q_d2`, `v_cut_bq2`, `v_simplify`) were refusal-asserted at
landing; since the 24-A.1 QR-cascade port they replay as positive
assertions (the factorizer chain itself is 24-B, see
[decompose-ucarima-contract.md](decompose-ucarima-contract.md)). Parameter-estimation uncertainty is 24-C scope. See
[tramo-seats-model-contract.md](tramo-seats-model-contract.md).

## SEATS decomposition seam mirror

`test_decompose_ucarima.cpp`, the double-captured 49-case JVM corpus under
`validation/evidence/native/decompose-ucarima/`, the replay tool
`tools/verify_decompose_ucarima.cpp` and the generator
`tools/gen_decompose_ucarima_golden.py` qualify `kalos::decompose_ucarima`
(item 24-B): the LIVE pinned decomposition seam — raw selector-chain split,
`UcarimaModel.setVarianceMax(-1, false)` and
`SeatsModel.compactUcarimaModel(false, false)` — through the spectral
factorization chain (`SymmetricFilter.factorize`: constant shortcut →
`EigenValuesDecomposer2` → `EigenValuesDecomposer`, the z = x + 1/x
companion + `balance`/`hessenbergQR`, the real-pair / unit-circle
classification, leja + `fromComplexRoots`), the `Spectrum$Minimizer`
by-roots/by-grid dispatch (`2*degree+1 <= 6`), the compact reduction and
the SeatsException status row. All 1620 fact lines are IEEE-754 bit-exact
(no band at landing); JVM abort classes are captured as data (the dangling-
prefix `maerr`/`varr`/`sfdr` lines, `d.decompose null`); the 16
`approximation_required` and 1 Noisy requests fail closed with the pinned
D6 reasons (the crashing `DefaultModelApproximator` stays unimplemented,
register D6 APPROVED). The fast/robust factorizer tiers, the k.* kernel
flags and the post-seam estimation statuses are record-only or out of
envelope; the WK-only downstream caveat is D1. See
[decompose-ucarima-contract.md](decompose-ucarima-contract.md).

## TRAMO pre-stage decision mirrors

`test_tramo_transform_decision.cpp` / `test_tramo_outlier_scan.cpp` and the
double-captured JVM corpora under
`validation/evidence/native/tramo-transform/` and
`validation/evidence/native/tramo-outlier-scan/` qualify
`kalos::decide_tramo_transform` and `kalos::scan_tramo_outliers` (item 23-B):
the `TramoUtility.calcCv` grid, the `TramoSeasonalityDetector` machine and
the `LogLevelModule` pair through the CI-FDLIBM-routed estimation layer
(26-case bit-exact replay), and the ROUND-0 observation of the
`internal.OutliersDetectionModule` (the `make()` gate, the design pre-stage
with the `SimplifyingTool.simplifyExact` least-squares quotient columns, the
OlsComputer→HR→stabilize→warm-optimize estimateModel chain, the
FastKalmanFilter initmodel, the per-factory T/coefficient tables,
`searchMax` and the `!(|maxT| < cv)` gate; all 22 numerically observable
cases bit-exact at IEEE-754 level). The multi-round driver and the
`curva`/`reduceVa`/pcr policy stay open under D3 (faithful port ruled for
23-F); `sc_so`/`sc_spanout`/`sc_spannone` are the registered D3 evidence and
`sc_pexao47` pins the warm-seam fail-closed throw. See
[tramo-transform-decision-contract.md](tramo-transform-decision-contract.md)
and [tramo-outlier-scan-contract.md](tramo-outlier-scan-contract.md).

## TRAMO differencing decision mirror

`test_tramo_differencing.cpp`, the double-captured 25-case JVM corpus under
`validation/evidence/native/tramo-differencing/` and the generator
`tools/gen_tramo_differencing_golden.py` qualify
`kalos::decide_tramo_differencing` (item 23-C): the LIVE round-1
`internal.DifferencingModule.process` — the `comespd`/autlar size gate with
the airline fallback, the step0/nextstep/initstep/maincondition/cond1/
finalcond ladder with its per-step Hannan-Rissanen start and the warm
maximum-likelihood seam, the `searchur` unit-root scans (the
`FastEigenValuesSolver` closed forms plus the scaled `ComplexType.abs`, the
root cause of the 1-ulp `rmax` drift against the DGEEV companion), the
`FastKalmanFilter.fastFilter` tmean machine (the sticky Durbin-Magenau
transcription `pinned_fast_filter_residuals`) and the `isMeanCorrection`
length table. All 120 module fact lines of the 22 replayed cases are
IEEE-754 bit-exact, including the `(-1)^(d + bd)` differencing-filter
orientation pin and the fdlibm log channel. Item 23-C.2 ACTIVATED the
battery stage: `kalos::decide_tramo_differencing_battery` replays the
`SeasonalityController` passes, the `RegularUnder(+2)`/`SeasonalUnder(+2)`
loop, the `ModelComparator`, the per-state `SeasonalOverDifferencingTest`
codes and the `SpectralPeaks` gate bit-exactly over the pinned
estimation-state facts of
`validation/evidence/native/tramo-differencing-battery/`, with the
`SeasonalFTest` p-values recomputed through the new bit-exact
`detail::f_upper_p` incomplete-beta kernel (golden
`validation/reference/fcdf_golden.txt`, double capture byte-identical); the
multi-round driver and the io = y cases stay 23-F. See
[tramo-differencing-decision-contract.md](tramo-differencing-decision-contract.md).

## TRAMO arma selection mirror

`test_tramo_arma.cpp`, the double-captured 16-case JVM corpus under
`validation/evidence/native/tramo-arma-selection/` and the generator
`tools/gen_tramo_arma_golden.py` qualify `kalos::select_tramo_arma` (item
23-D): the LIVE `internal.ArmaModule.process` — the `comespa`/`maxInic`
size gate with the default-model and already-airline branches, the
`calcmaxspec` candidate envelope, the three-stage `ArmaModelSelector` grid
(seasonal AR-block, ARMA, seasonal), the per-candidate Hannan-Rissanen +
`isStable(true)` + `FastKalmanFilter.fastProcessing` fast-BIC sort, the
`getPreferred` pick, the `mergeInto` watermark merge into the five-row
`hrModels` panel and the eleven-gate `select()` tie-break (alpha tiers
{0.003→0.0625, 0.03→0.25, else 1}, budgets {0.01, 0.0025, 0.0075},
per-accept relief). The selector input is the `olsResiduals` hand-over
(request data, length `n − (d + period·bd)`, `nobs` carried separately for
the gate). All 14 replayed cases are IEEE-754 bit-exact (the `selected
model:` colon, the five `dpanel` BICs and the full ProcessingLog event
sequence), the quarterly case pins the D11 refusal and the constant series
pins the upstream `olsResiduals` NPE (record-only); a `-Xlog:class+load`
witness proves the HR + fast-conditional-BIC engine (HannanRissanen +
`core.arima.estimation.FastKalmanFilter` + `core.arima.AutoCovarianceFunction`
loaded, `RegSarimaComputer`/
`GlsSarimaComputer`/`internal.HannanRissanenInitializer`/
`LevenbergMarquardtMinimizer` zero-loaded — no ML). The unit reuses the
qualified `hannan_rissanen_pipeline`, `pinned_fast_filter_residuals` and
`check_roots` units bit-for-bit and adds only the FastBIC panel machine.
`tools/verify_tramo_arma_selection.cpp` re-drives the raw corpus (14
replayed, 1 refusal, 1 record-only, 0 failures); shim-invariant. See
[tramo-arma-selection-contract.md](tramo-arma-selection-contract.md).

## TRAMO regression tests, no-calendar branch (23-E)

`test_tramo_regression_tests.cpp` (#76), the double-captured 55-case JVM
corpus under `validation/evidence/native/tramo-regression-tests/`, the
generator `tools/gen_tramo_regression_tests_golden.py`, the shared replay
`tests/tramo_regression_replay.h` and the sweep
`tools/verify_tramo_regression_tests.cpp` (663 replay + 464 corpus
cross-checks) qualify `kalos/tramo_regression_tests.hpp`: the
`ModelStatistics.of` rows the refStats gate consumes (Ljung-Box + seasonal
pair via the 23-C.2 kernels, `se`, fdlibm `bic`, the `df <= 0` un-built
surface), `ConcentratedLikelihood.ser/tstats` both arms with the TRAMO
addmu-site denominator, the dcmpl-bucket `TRegressionTest.accept` (NaN
counts in NEITHER bucket), the full `TramoKernel.pass2` refStats gate
(rules A-D + shape guards, pcr ladder, unconditional pass-1 remark,
`needAutoModelling = !changed`, last-chance rewrite) and the calendar-free
`FastRegressionTest`/`DefaultRegressionTest` mean-rule branch, plus the
`AmiOptions` checkMu / round-0 / `reduceVa` / `lastSolution` /
estimate-model retry members. Every calendar-kernel request (Fast ami
td/lp/easter, Default calendar builder fields, moving-holiday) is refused
fail closed naming item 21-I.3; the quarterly ModelStatistics row pins the
D11 refusal. Bit-exact decisions and values; the retry ladder is qualified
as transcribed control flow (no live witness, see the contract).
Shim-invariant (23rd ledger cell). See
[tramo-regression-tests-contract.md](tramo-regression-tests-contract.md).

## SEATS Wiener-Kolmogorov extraction mirror

`test_seats_extract_wk.cpp`, the double-captured 36-case JVM corpus under
`validation/evidence/native/seats-wk-extraction/`, the replay tool
`tools/verify_seats_wk_extraction.cpp` and the generator
`tools/gen_seats_wk_extraction_golden.py` qualify `kalos::seats_extract_wk`
(the live `SeatsKernel` extraction machine: `UcarimaModel.compact(2, 2)`,
`BurmanEstimates`, the `WienerKolmogorovEstimator` initial surface and the
`DefaultBiasCorrector` final surface, plus the PED sink in the ExactFc
filter). 28 full runs, 3 mirror-status rows (the mode-None SeatsException
and two mid-run EcoException prediction-error aborts) and 6 pinned refusals
(period 4 D11, the two KalmanSmoother cases D1, innovation variance 0, the
classical airline and the fixMa crash D6); the two-variance split
(`ucm.getModel()` var 1 vs the spec-echo stdev) is pinned at x_wn /
x_seasonal. See [seats-wk-extraction-contract.md](seats-wk-extraction-contract.md).

## Calendar design generation mirror

`test_calendar_generation.cpp`, the double-captured JVM corpora under
`validation/evidence/native/calendar-generation/` (`gen.out` primary +
`gen2.out` envelope shapes), the replay tool
`tools/verify_calendar_generation.cpp` and the generator
`tools/gen_calendar_generation_golden.py` qualify `kalos::calendar_generation`
(the live design-generation layer of 23-A's registered variables:
`GenericTradingDaysFactory` counts/meanDays/cluster contrasts, `LPFactory`,
the `Easter` computus with `EasterVariable`/`JulianEasterFactory`,
`RampFactory`, `TsVariablesFactory` placement and the pinned name
conventions). 137 golden cases replayed bit-for-bit (17916 captured IEEE
patterns; the raw/contrast and default/Simple twin identities are pinned as
double replays); envelope refusals cover annual TD/Easter domains (the jar's
silent zero-fill is refused, not emitted), duration bounds [1, 28] /
PreComputed [1, 25] and unaligned domains or user data. Decisions I-5 and I-9
applied per the 21-I register. See
[calendar-generation-contract.md](calendar-generation-contract.md).
