// Item 23-E replay test: drives the transcribed tramo regression-test layer
// (kalos::tramo_model_statistics, kalos::tramo_test_tstats / tramo_t_accepts,
// kalos::run_tramo_regression_tests, kalos::tramo_ref_stats_gate and the
// AmiOptions helpers) over the generated golden JVM corpus
// (tests/jvm_tramo_regression_tests.h, corpus
// validation/evidence/native/tramo-regression-tests, double capture
// byte-identical). Every replay case rebuilds the request from the captured
// hand-over echoes and compares each consumed ModelStatistics row, both
// cl.tstats arms, every mean-test run decision, the full pass2 ladder
// after-state and the checkMu witnesses bit-for-bit (tests/
// tramo_regression_replay.h). Seam rows pin the fail-closed refusals: the
// quarterly MS row (D11), the Fast ami-td/ami-lp/ami-easter rows and the
// Default calendar-field rows (item 21-I.3). The synthetic block qualifies
// the pieces the corpus cannot reach (retry-ladder shapes, round-0 rule,
// reduceVa/lastSolution arms, the NaN T-bucket semantics, envelope
// refusals); see docs/contracts/tramo-regression-tests-contract.md.
#include "tramo_regression_replay.h"

#include <cstdio>

int main() {
    kalos::replay::Reporter rep;
    kalos::replay::replay_all(rep);
    if (rep.failures != 0) {
        std::printf("test_tramo_regression_tests: %d FAILURES of %d checks\n",
                    rep.failures, rep.checks);
        return 1;
    }
    std::printf("test_tramo_regression_tests: %d checks verified\n",
                rep.checks);
    return 0;
}
