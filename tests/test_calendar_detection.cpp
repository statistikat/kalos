// Item 21-I.3 replay test: drives the transcribed calendar-detection
// machines (kalos::calendar_best_model / calendar_wald_test,
// kalos::run_tramo_auto_td_test, kalos::run_sa_auto_td_test,
// kalos::run_tramo_ftest_td_test, kalos::run_tramo_fast_calendar_step,
// kalos::run_tramo_default_calendar_step and kalos::calendar_joint_test)
// over the generated golden JVM corpus (tests/jvm_calendar_detection.h,
// corpus validation/evidence/native/calendar-detection, double capture
// byte-identical). Every golden double rides its raw bit pattern; the
// comparator algebra, both ladder machines (decision, best, mean echo and
// the reconstructed post-state variable lists), the FTEST Info math and
// branch, the Fast/Default step machines (including the joint-test echo
// and the dim == 1 -> wdTest routing witness), the public JointTest form
// and the pipeline tdspec witnesses are compared bit-for-bit in
// tests/calendar_detection_replay.h; see
// docs/contracts/calendar-detection-contract.md.
#include "calendar_detection_replay.h"

#include <cstdio>

int main() {
    kalos::replay::Reporter rep;
    kalos::replay::replay_cmp(rep);
    kalos::replay::replay_ladder(rep);
    kalos::replay::replay_ftest(rep);
    kalos::replay::replay_fast_step(rep);
    kalos::replay::replay_default_step(rep);
    kalos::replay::replay_joint(rep);
    kalos::replay::replay_pipe(rep);
    if (rep.failures != 0) {
        std::printf("test_calendar_detection: %d FAILURES of %d checks\n",
                    rep.failures, rep.checks);
        return 1;
    }
    std::printf("test_calendar_detection: %d checks verified\n", rep.checks);
    return 0;
}
