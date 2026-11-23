#!/bin/bash
# ci-shim-invariance.sh - machine-independence acceptance battery (LEG C).
#
# Builds the two LD_PRELOAD diagnostic interposers (tools/diagnostics/) and
# runs the bit-claim suites three ways per binary:
#   plain     - no interposer (reference output + exit status)
#   ledger100 - libm_shim mode=ledger-all density=100: worst-case 1-ulp noise
#               on EVERY distinct reached host-libm ledger argument.
#   lapack1   - lapack_shim mode=lapack1 density=100: 1-ulp noise on every
#               distinct LAPACK/BLAS top-level OUTPUT value.
#
# Verdicts:
#   * plain must exit 0 (bit claims hold on this machine unaided).
#   * ledger100 must match plain BYTE-IDENTICALLY once the CI-FDLIBM ledger
#     is fully routed (vendored hostlineage + fdlibm kernels): any remaining
#     reaction exposes an unmigrated raw host-libm dependency. LEDGER-CELL
#     FAILURES FAIL THE BATTERY.
#   * lapack1 cells are informational: provider noise cannot be vendored
#     away; the suite outputs are diffed and any divergence is REPORTED
#     (exit status unaffected) for honest headroom tracking.
#
# Usage: tools/ci-shim-invariance.sh <ctest-build-dir> [out-dir]
# Requires Linux (LD_PRELOAD + dladdr). Outputs: $OUT/<suite>.<cell>.log and
# a summary table on stdout. LD_LIBRARY_PATH is cleared INTERNALLY by run()
# (every suite process runs under env -u LD_LIBRARY_PATH), so an outer export
# is harmless - the historical LAPACKE_dsyev_work phantoms came from suites
# invoked WITHOUT that clearing, which this script performs itself.
set -u
BUILD=${1:?usage: ci-shim-invariance.sh <build-dir> [out-dir]}
BUILD=$(cd "$BUILD" && pwd)
OUT=${2:-$BUILD/shim-invariance}
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$OUT" || exit 1

echo "== building diagnostic shims"
c++ -O2 -fPIC -shared -std=gnu++17 -o "$OUT/libm_shim.so" \
    "$REPO/tools/diagnostics/libm_shim.cpp" -ldl || exit 1
c++ -O2 -fPIC -shared -std=gnu++17 -o "$OUT/lapack_shim.so" \
    "$REPO/tools/diagnostics/lapack_shim.cpp" -ldl || exit 1

# Bit-claim suites: the units whose goldens pin libm/provider-sensitive bits.
# KALOS_SHIM_SUITES may override (whitespace-separated) for targeted runs.
SUITES_DEFAULT="
test_estimation_control
test_x13_automatic test_x13_automatic_oracle
test_arma_identification test_initial_arma_identification
test_decompose_ucarima
test_seats_linear_filters test_seats_extract_wk
test_calendar_generation
test_tramo_arma test_tramo_differencing test_tramo_model_builder
test_tramo_final_estimation
test_tramo_outlier_scan test_tramo_seats_model test_tramo_transform_decision
test_tramo_regression_tests
test_tramo_kernel_rounds
test_tramo_fit
test_calendar_detection
test_x11_log_additive test_x13_allocation test_x11_musgrave
test_preparation test_outlier_scoring test_diffuse_state_space test_x13_fit
"
SUITES=${KALOS_SHIM_SUITES-$SUITES_DEFAULT}

run() { # label binary [extra env assignments for env(1)...]
  local label=$1 bin=$2
  shift 2
  ( cd "$BUILD" && env -u LD_LIBRARY_PATH KALOS_NUM_THREADS=2 OMP_NUM_THREADS=2 \
      OPENBLAS_NUM_THREADS=1 "$@" "./$bin" \
      > "$OUT/$bin.$label.log" 2>&1 )
  echo $? > "$OUT/$bin.$label.rc"
}

declare -a FAILS
for bin in $SUITES; do
  [ -x "$BUILD/$bin" ] || { echo "SKIP $bin (not in build dir)"; continue; }
  run plain "$bin"
  run ledger100 "$bin" KALOS_LIBM_SHIM=ledger-all KALOS_LIBM_DENSITY=100 \
      LD_PRELOAD="$OUT/libm_shim.so"
  run lapack1 "$bin" KALOS_LIBM_SHIM=lapack1 KALOS_LIBM_DENSITY=100 \
      LD_PRELOAD="$OUT/lapack_shim.so"

  rc_plain=$(cat "$OUT/$bin.plain.rc")
  rc_led=$(cat "$OUT/$bin.ledger100.rc")
  rc_lap=$(cat "$OUT/$bin.lapack1.rc")
  led=PASS lap=info
  if [ "$rc_plain" != 0 ]; then led="PLAIN-FAIL(rc=$rc_plain)"; lap="-"
    FAILS+=("$bin plain(rc=$rc_plain)")
  elif [ "$rc_led" != 0 ] || ! cmp -s "$OUT/$bin.plain.log" "$OUT/$bin.ledger100.log"; then
    led="FAIL(rc=$rc_led)"; FAILS+=("$bin ledger100")
  fi
  if [ "$rc_plain" = 0 ] && { [ "$rc_lap" != 0 ] || ! cmp -s "$OUT/$bin.plain.log" "$OUT/$bin.lapack1.log"; }; then
    lap="DIVERGES(rc=$rc_lap)"
  fi
  printf '%-38s plain=%s ledger100=%s lapack1=%s\n' "$bin" "$rc_plain" "$led" "$lap"
done

echo "== logs + rcs in $OUT"
if [ "${#FAILS[@]}" -gt 0 ]; then
  echo "BATTERY: FAIL - ledger-noise reactions:"; printf '  %s\n' "${FAILS[@]}"
  exit 1
fi
echo "BATTERY: ledger-cells INVARIANT (lapack1 divergences reported above, informational)"
