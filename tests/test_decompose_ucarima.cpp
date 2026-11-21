// Item 24-B golden test: replays the 49-case decompose-ucarima corpus (the
// double-captured jdplus 3.9.0 evidence) through kalos::decompose_ucarima.
// Refused requests (16 Legacy approximation_required, 1 Noisy) must throw
// std::invalid_argument with the EXACT pinned reason; every other case must
// return the decomposition-seam fact lines byte-for-byte (key, order and
// %.17g payload) with a determinism double-run. The request protocol parses
// through the shared 24-A replay layer (tests/tramo_seats_model_replay.h).
#include <kalos/decompose_ucarima.hpp>

#include "jvm_decompose_ucarima.h"
#include "tramo_seats_model_replay.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_checks = 0, g_failures = 0;
std::vector<std::string> g_problems;

void fail(const std::string& id, const std::string& what) {
  ++g_failures;
  g_problems.push_back(id + ": " + what);
}

}  // namespace

int main(int argc, char** argv) {
  const bool verbose = argc > 1 && std::strcmp(argv[1], "-v") == 0;
  std::size_t n = 0;
  const golden_du::Case* cs = golden_du::table(n);
  for (std::size_t i = 0; i < n; ++i) {
    const golden_du::Case& c = cs[i];
    kalos::tramo_seats_model_request req;
    std::string err;
    const golden_tsm::Case proxy{c.id,  "",  "", 0, 0, c.nreq, c.req,
                                 0,     nullptr, 0, nullptr};
    if (!replay_tsm::parse_request(proxy, req, err)) {
      fail(c.id, "request parse: " + err);
      continue;
    }
    if (c.refusal != 0) {
      std::string msg = "NO THROW";
      bool invalid = false;
      try {
        kalos::decompose_ucarima(req);
      } catch (const std::invalid_argument& e) {
        invalid = true;
        msg = e.what();
      } catch (const std::exception& e) {
        msg = std::string("wrong type: ") + e.what();
      }
      ++g_checks;
      if (!invalid || msg != c.reason) fail(c.id, "refusal: got [" + msg + "]");
      continue;
    }
    std::vector<std::string> got;
    try {
      got = kalos::decompose_ucarima(req).facts();
    } catch (const std::exception& e) {
      fail(c.id, std::string("threw: ") + e.what());
      continue;
    }
    if (got.size() != c.nfacts) {
      fail(c.id, "line count: got " + std::to_string(got.size()) + ", want " +
                     std::to_string(c.nfacts));
    }
    const std::size_t common =
        got.size() < c.nfacts ? got.size() : c.nfacts;
    for (std::size_t j = 0; j < common; ++j) {
      ++g_checks;
      if (got[j] != c.facts[j])
        fail(c.id, "line " + std::to_string(j) + ":\n  got  " + got[j] +
                       "\n  want " + c.facts[j]);
    }
    for (std::size_t j = common; j < c.nfacts; ++j)
      fail(c.id, "missing line " + std::to_string(j) + ": " + c.facts[j]);
    for (std::size_t j = common; j < got.size(); ++j)
      fail(c.id, "extra line " + std::to_string(j) + ": " + got[j]);
    // determinism: the unit is pure, a second run must be identical.
    const std::vector<std::string> again =
        kalos::decompose_ucarima(req).facts();
    ++g_checks;
    if (again != got) fail(c.id, "non-deterministic re-run");
  }
  for (std::size_t i = 0; i < g_problems.size(); ++i) {
    if (verbose || g_problems[i].find("checks, 0 failures") == std::string::npos)
      std::printf("%s\n", g_problems[i].c_str());
  }
  std::printf("decompose-ucarima: %zu cases, %d checks, %d failures\n", n,
              g_checks, g_failures);
  return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
