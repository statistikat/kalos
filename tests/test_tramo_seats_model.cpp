// Item 24-A golden test: replays the 35-case tramo-seats-model corpus (the
// double-captured jdplus 3.9.0 evidence) through the replay layer and fails
// on any mismatch in the validator facts, the raw decomposition structure,
// the decomposability decision, the kernel flags or the event frame (exact,
// or exact-prefix for the Legacy D6 continuation).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tramo_seats_model_replay.h"

int main(int argc, char** argv) {
  const bool verbose = argc > 1 && std::strcmp(argv[1], "-v") == 0;
  replay_tsm::Summary s = replay_tsm::replay_all(verbose);
  for (std::size_t i = 0; i < s.problems.size(); ++i) {
    if (verbose || s.problems[i].find("checks, 0 failures") == std::string::npos)
      std::printf("%s\n", s.problems[i].c_str());
  }
  std::printf("tramo-seats-model: %zu cases, %d checks, %d failures\n", s.cases, s.checks,
              s.failures);
  return s.failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
