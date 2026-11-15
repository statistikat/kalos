# Extended-precision scratch and range-guard contract

Added 2026-11-15 in response to the Linux x86_64 baseline qualification, which
showed that several underflow guards fired on x86-64 (80-bit `long double`) but
were structurally unreachable on the qualified macOS ARM64 baseline
(`long double` is binary64). This document fixes one platform-independent
semantics for every range guard in the core. No other contract's numeric
tolerances or failure statuses change.

## Decision: binary64 is the numeric contract; extended precision is scratch

1. **Data domain.** Every model input, stored result and public numeric
   quantity is binary64. `long double` is used only as wider accumulation
   scratch inside kernels (dot products, filter recursions, likelihood
   sums) to reduce rounding error. It is never a numeric contract, never
   observable, and never selects a different branch than binary64 would.

2. **Guarded conversion rule (flush-to-zero at binary64 granularity).** A
   guarded conversion `checked(x)` of an extended-precision scratch value
   returns the correctly rounded binary64 value and throws
   `std::overflow_error` (or the kernel's documented range exception) only
   when that rounded value is non-finite. A nonzero extended-precision
   intermediate whose correct binary64 rounding is zero **is zero**; it does
   not raise underflow. This reproduces the qualified macOS behavior exactly:
   there, `static_cast<double>` is the identity and the former
   `value != 0 && result == 0` test was unreachable, so all 37 qualified
   fixtures and reference values were produced under precisely this rule.

3. **Underflow guards observe named binary64 quantities.** Every explicit
   underflow contract (a documented rejection, not a conversion side effect)
   is evaluated on binary64 granularity: a nonzero binary64 operand and a
   guard result of zero *after rounding the expression to binary64*. The
   expression itself may be evaluated in wider scratch and then rounded once;
   for exact products of two binary64 values this equals the single-rounded
   binary64 product, so the guard fires on every platform in exactly the
   cases where it fired on the qualified baseline. Examples that remain
   enforced: `diffuse state-space square underflows`, `regression residual
   square underflows`, `regression coefficient covariance underflows`,
   `finite innovation variance underflows`, `profiled variance underflows`,
   `ARMA prediction variance underflows`, and the forecast/interpolation
   covariance scaling guards.

4. **Overflow semantics are unchanged.** A guard result that is non-finite in
   binary64 remains an error (`std::overflow_error`, `std::range_error`, or
   the kernel's documented status) on every platform. Relative-margin tests
   (symmetry, projection and rank tolerances) are scale-relative and already
   platform-independent.

5. **Accumulation stays in extended scratch.** Dot products and sums continue
   to accumulate in `long double` and convert once, per rule 2. The scratch
   width can change the final rounding by at most its own error term; it can
   never change exponent-range decisions after rule 2, because conversion
   flushes sub-binary64 magnitudes to zero identically everywhere. No fixture
   tolerance was changed to accommodate this.

## Rejected alternatives

- **Let the extended-width underflow throw stand** (guard on the raw scratch
  value): rejected because it changes fitted *values*, not just diagnostics.
  The macOS-qualified evidence carries exact double 0 where the x86-64 80-bit
  intermediate carried ≈6.91e-334 (period-12 seasonal AR diffuse filter) or
  ≈1e-400 (regression likelihood), and those runs continue successfully;
  throwing there on x86-64 diverges from the qualified baseline and is what
  broke `test_diffuse_state_space`, `test_forecast_arma_information` and
  `test_regression_likelihood`.
- **Force all accumulation to binary64 (plain or Kahan-compensated):**
  rejected as unnecessary and more invasive. It would perturb summation
  rounding of every already-qualified kernel, whereas the observed divergence
  came solely from guard *branching* on scratch width, which rule 2 fixes at
  the guard boundary.

## Enforced site inventory (conversion rule 2 unless noted)

| File | Guard | Change |
| --- | --- | --- |
| `src/finite_state_factor.cpp` | `checked()` | underflow throw removed |
| `src/diffuse_state_space.cpp` | `checked()` | underflow throw removed |
| `src/diffuse_state_space.cpp` | `square()` (rule 3) | guard now tests the binary64 product |
| `src/sparse_history.cpp` | `checked()` | underflow throw removed |
| `src/regression_likelihood.cpp` | `checked()` | underflow throw removed |
| `src/regression_likelihood.cpp` | whitened residual square (rule 3) | guard and accumulation observe the binary64 square |
| `src/state_space.cpp` | standardized innovation square (rule 3) | guard and accumulation observe the binary64 square |
| `src/prediction_arma_information.cpp` | `finite_prediction()` | underflow throw removed |
| `src/forecast_arma_information.cpp` | `finite_prediction()` | underflow throw removed |
| `src/interpolation_arma_information.cpp` | `finite_prediction()` | underflow throw removed |
| `src/arma_residuals.cpp` | `legacy_checked()` | underflow throw removed |

Guards comparing a binary64 operand against a binary64 result
(`scaled_covariance`, covariance diagonals, prediction-variance diagonals,
covariance scalings in `forecast_information.cpp` /
`interpolation_information.cpp` / `arma_information.cpp`, the
standardized-innovation and whitening divisions, and `prediction_moments`
`!(result>0)`) already evaluate at binary64 granularity on every platform and
were not changed.

## Qualification evidence

- The three previously failing programs (`test_diffuse_state_space`,
  `test_forecast_arma_information`, `test_regression_likelihood`) pass on
  Linux x86_64 with the pinned OpenBLAS 0.3.34 provider.
- Full standalone suite: **37/37** in the Release OpenMP build and **37/37**
  in the serial (`-DKALOS_OPENMP=OFF`) build, both with zero warnings under
  `-Wall -Wextra -Wpedantic`; the tests' internal serial/threaded equality
  assertions hold in both builds, and cross-build program outputs are
  identical.
- No fixture, oracle, tolerance or test file was modified; the change is
  source-level and provider-independent. See IMPLEMENTATION_STATUS.md
  (2026-11-15) for the delivery record.
