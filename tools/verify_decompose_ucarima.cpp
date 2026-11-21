// Full-sweep verification: the native item 24-B decomposition-seam mirror
// against the pinned jdplus 3.9.0 corpus
// (validation/evidence/native/decompose-ucarima; an alternative corpus
// directory may be passed as the single argument). Unlike the golden unit
// this tool replays the csv corpus DIRECTLY (the generated header stays a
// derived artifact): every case is driven through the shared 24-A request
// parser (tests/tramo_seats_model_replay.h, the same code the unit test
// uses) and the returned fact lines must equal the seam grammar of
// model-facts.csv line for line (key, order and %.17g payload bit-exact).
//
// Sections: (1) the 32 non-refused cases, byte-identical fact replay plus a
// determinism double-run; (2) the refusal matrix, the 16 Legacy
// approximation_required and the single Noisy request failing closed with
// the EXACT pinned std::invalid_argument reason (D6 envelope) and every
// replay case never throwing; (3) a cross-case audit tying the fact
// grammar to the cases.csv metadata (d-null <=> the SeatsException status
// line under mode None, r.count agreeing with the emitted r-block cN keys,
// the refusal classification reproducing from the captured columns).
// Tiering (docs/contracts/decompose-ucarima-contract.md): everything this
// unit emits is decision- and bit-tier - the golden was double-captured
// byte-identical, so a line mismatch is always a mirror divergence.
#include <kalos/decompose_ucarima.hpp>

#include "../tests/tramo_seats_model_replay.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string unquote(const std::string& s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    return s.substr(1, s.size() - 2);
  return s;
}

std::vector<std::string> fields(const std::string& line) {
  // Split one CSV line honouring quoted cells with embedded commas.
  std::vector<std::string> out;
  std::string cell;
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (quoted) {
      if (ch == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') { cell += '"'; ++i; }
        else quoted = false;
      } else cell += ch;
    } else if (ch == '"') quoted = true;
    else if (ch == ',') { out.push_back(cell); cell.clear(); }
    else cell += ch;
  }
  out.push_back(cell);
  for (std::string& c : out) c = unquote(c);
  return out;
}

// The pinned refusal reasons (src/decompose_ucarima.cpp); index = code.
const char* kReasons[3] = {
    "",
    "decompose_ucarima: approximation_required decision is outside the D6 "
    "envelope",
    "decompose_ucarima: Noisy approximation mode is outside the D6 envelope",
};

const char* kSeatsInvalid =
    "error:jdplus.tramoseats.base.api.seats.SeatsException:Invalid decomposition";

bool is_seam_key(const std::string& key) {
  static const char* kSeam[] = {"d.decompose", "d.excep", "d",
                                "cmp.excep",   "cmp",    "r.decompose",
                                "r.count",     "r",      "r.excep"};
  for (const char* s : kSeam)
    if (key == s) return true;
  return false;
}

struct CaseRec {
  std::string id, mode, status, ddecompose, rdecompose;
  int refusal = 0;  // 0 replay, 1 approximation_required, 2 Noisy
  std::vector<std::string> req;
  std::vector<std::string> facts;
};

int g_checks = 0, g_failures = 0;
std::vector<std::string> g_problems;

void fail(const std::string& id, const std::string& what) {
  ++g_failures;
  if (g_problems.size() < 40) g_problems.push_back(id + ": " + what);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir =
      argc > 1 ? argv[1] : "validation/evidence/native/decompose-ucarima";

  std::map<std::string, CaseRec> cases;
  std::vector<std::string> order;
  auto load_cases = [&](std::istream& f) {
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
      if (line.empty()) continue;
      const std::vector<std::string> t = fields(line);
      if (t.size() < 12) continue;
      CaseRec& c = cases[t[0]];
      c.id = t[0];
      c.status = t[5];
      c.mode = t[7];
      c.ddecompose = t[10];
      c.rdecompose = t[11];
      order.push_back(t[0]);
    }
  };
  {
    std::ifstream f(dir + "/cases.csv");
    if (!f) { std::cerr << "missing " << dir << "/cases.csv\n"; return 2; }
    load_cases(f);
  }
  {
    std::ifstream f(dir + "/inputs.csv");
    if (!f) { std::cerr << "missing " << dir << "/inputs.csv\n"; return 2; }
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
      if (line.empty()) continue;
      const std::vector<std::string> t = fields(line);
      if (t.size() != 2) continue;
      if (t[1].rfind("req ", 0) == 0 && cases.count(t[0]))
        cases[t[0]].req.push_back(t[1]);
    }
  }
  {
    std::ifstream f(dir + "/model-facts.csv");
    if (!f) { std::cerr << "missing " << dir << "/model-facts.csv\n"; return 2; }
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
      if (line.empty()) continue;
      const std::vector<std::string> t = fields(line);
      if (t.size() != 3) continue;
      if (!cases.count(t[0])) continue;
      const std::string& key = t[1];
      if (is_seam_key(key))
        cases[t[0]].facts.push_back(key + " " + t[2]);
      else if (key == "status" && t[2] == kSeatsInvalid)
        cases[t[0]].facts.push_back("status " + t[2]);
    }
  }

  // Refusal classification, data-driven exactly as the corpus was captured:
  // Noisy is always refused; a Legacy request is refused exactly when the
  // validator flagged approximation_required (live decompose null).
  int n_replay = 0, n_refused = 0;
  for (const std::string& id : order) {
    CaseRec& c = cases[id];
    if (c.mode == "Noisy") c.refusal = 2;
    else if (c.mode == "Legacy" && c.ddecompose == "null") c.refusal = 1;
    if (c.refusal) ++n_refused;
    else ++n_replay;
  }
  std::printf("corpus: %zu cases (%d replay, %d refused)\n", order.size(),
              n_replay, n_refused);

  // Section 1: byte-identical seam-fact replay + determinism.
  int n_lines = 0;
  for (const std::string& id : order) {
    CaseRec& c = cases[id];
    if (c.refusal) continue;
    kalos::tramo_seats_model_request req;
    std::string err;
    std::vector<const char*> reqp;
    for (const std::string& r : c.req) reqp.push_back(r.c_str());
    golden_tsm::Case proxy;
    proxy.id = c.id.c_str();
    proxy.status = proxy.mode = "";
    proxy.legacy_approx = proxy.reject = 0;
    proxy.nreq = c.req.size();
    proxy.req = reqp.data();
    proxy.nfacts = 0;
    proxy.facts = nullptr;
    proxy.nevents = 0;
    proxy.events = nullptr;
    if (!replay_tsm::parse_request(proxy, req, err)) {
      fail(id, "request parse: " + err);
      continue;
    }
    std::vector<std::string> got, again;
    try {
      got = kalos::decompose_ucarima(req).facts();
      again = kalos::decompose_ucarima(req).facts();
    } catch (const std::exception& e) {
      fail(id, std::string("threw: ") + e.what());
      continue;
    }
    if (got.size() != c.facts.size())
      fail(id, "line count: got " + std::to_string(got.size()) + ", want " +
                   std::to_string(c.facts.size()));
    const std::size_t common =
        got.size() < c.facts.size() ? got.size() : c.facts.size();
    for (std::size_t j = 0; j < common; ++j) {
      ++n_lines;
      if (got[j] != c.facts[j])
        fail(id, "line " + std::to_string(j) + ":\n  got  " + got[j] +
                     "\n  want " + c.facts[j]);
    }
    if (got != again) fail(id, "determinism: double-run differs");
    ++g_checks;
  }
  std::printf("section 1: %d cases replayed, %d fact lines exact, "
              "determinism double-run OK\n",
              g_checks, n_lines);

  // Section 2: refusal matrix.
  int checks1 = g_checks;
  for (const std::string& id : order) {
    CaseRec& c = cases[id];
    if (!c.refusal) continue;
    kalos::tramo_seats_model_request req;
    std::string err;
    std::vector<const char*> reqp;
    for (const std::string& r : c.req) reqp.push_back(r.c_str());
    golden_tsm::Case proxy;
    proxy.id = c.id.c_str();
    proxy.status = proxy.mode = "";
    proxy.legacy_approx = proxy.reject = 0;
    proxy.nreq = c.req.size();
    proxy.req = reqp.data();
    proxy.nfacts = 0;
    proxy.facts = nullptr;
    proxy.nevents = 0;
    proxy.events = nullptr;
    if (!replay_tsm::parse_request(proxy, req, err)) {
      fail(id, "request parse: " + err);
      continue;
    }
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
    if (!invalid || msg != kReasons[c.refusal])
      fail(id, "refusal: got [" + msg + "] want [" + kReasons[c.refusal] + "]");
  }
  std::printf("section 2: %d refusals exact-reason, %d replay cases never threw\n",
              g_checks - checks1, n_replay);

  // Section 3: fact-grammar / metadata audit over the captured grammar.
  checks1 = g_checks;
  for (const std::string& id : order) {
    CaseRec& c = cases[id];
    if (c.refusal) continue;
    bool dnull = false, dline = false, seats = false;
    std::map<std::string, int> rcount_emitted;
    for (const std::string& l : c.facts) {
      if (l == "d.decompose null") dnull = dline = true;
      if (l == "d.decompose ok") dline = true;
      if (l.rfind("status ", 0) == 0) seats = true;
      if (l.rfind("r c", 0) == 0) {
        // "r cN <getter> ..." -> component key "cN"
        const std::size_t sp = l.find(' ', 3);
        if (sp != std::string::npos)
          ++rcount_emitted[l.substr(2, sp - 2)];
      }
    }
    if (!dline) fail(id, "no d.decompose line");
    if (dnull != seats)
      fail(id, "d-null/SeatsException status disagree (d-null=" +
                   std::to_string(dnull) + ", seats=" + std::to_string(seats) +
                   ")");
    // cases.csv records the SAME d-state the facts show.
    if (c.ddecompose == "null" && !dnull) fail(id, "cases.csv d-null vs facts");
    if (c.ddecompose == "ok" && dnull) fail(id, "cases.csv d-ok vs facts");
    for (const auto& kv : rcount_emitted)
      if (kv.second < 4)
        fail(id, "r-block component " + kv.first + " has only " +
                     std::to_string(kv.second) + " getter lines");
    ++g_checks;
  }
  std::printf("section 3: %d audit checks (grammar vs cases.csv)\n",
              g_checks - checks1);

  std::printf("SUMMARY: checks=%d failures=%d\n", g_checks, g_failures);
  for (const std::string& p : g_problems) std::cout << "FAIL " << p << "\n";
  return g_failures ? 1 : 0;
}
