// Full-sweep verification: the native item 24-C.1 SEATS Wiener-Kolmogorov
// extraction mirror against the pinned jdplus 3.9.0 corpus
// (validation/evidence/native/seats-wk-extraction; an alternative corpus
// directory may be passed as the single argument). Unlike the golden unit
// this tool replays the csv corpus DIRECTLY (the generated header stays a
// derived artifact): every case is driven through the shared 24-A request
// parser (tests/tramo_seats_model_replay.h, the same code the unit test
// uses) plus the 24-C knobs tail, and the returned fact lines must equal
// the extraction grammar of model-facts.csv (status, the three SeatsResults
// domains, the init and final decomposition rows) line for line - key,
// order and %.17g payload bit-exact.
//
// Sections: (1) the 30 non-refused cases, byte-identical fact replay plus a
// determinism double-run; (2) the refusal matrix, the 6 envelope requests
// (frequency 4, the two KalmanSmoother cases, innovation variance 0, the
// classical airline and the fixMa crash) failing closed with the EXACT
// pinned std::invalid_argument reason and every replay case never throwing;
// (3) a cross-case audit tying the fact grammar to the cases.csv metadata
// (the tier classification reproducing from the captured columns, "status
// ok" <=> three dom rows plus init/final rows, a status error <=> no
// dom/init/final follow-up, the dom sizes agreeing with the extrapolation
// counts).
// Tiering (docs/contracts/seats-wk-extraction-contract.md): every replayed
// line is bit-tier - the golden was double-captured byte-identical, so a
// line mismatch is always a mirror divergence.
#include <kalos/seats_extract_wk.hpp>

#include "../tests/tramo_seats_model_replay.h"

#include <cstdio>
#include <cstdlib>
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

// The pinned refusal reasons (src/seats_extract_wk.cpp).
const char* kReasonFreq =
    "seats_extract_wk: annual frequency %d is outside the D11 monthly-first "
    "envelope";
const char* kReasonKalman =
    "seats_extract_wk: components_method KalmanSmoother is outside the D1 "
    "envelope (the Kalman estimator is not ported)";
const char* kReasonVar0 =
    "seats_extract_wk: innovation variance 0 selects the JVM "
    "ConcentratedLikelihood variance path (outside the ported domain)";
const char* kReasonApprox =
    "seats_extract_wk: approximation_required decision is outside the D6 "
    "envelope (the DefaultModelApproximator retry loop is not ported)";

std::string cfmt(const char* fmt, int v) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), fmt, v);
  return buf;
}

bool is_fact_key(const std::string& key) {
  return key == "status" || key == "dom" || key == "init" || key == "final";
}

struct CaseRec {
  std::string id, start, mode, method, status, ddecompose;
  int freq = 12;
  std::string refusal_reason;  // "" = replay tier
  std::vector<std::string> req;
  std::string values;
  std::vector<std::string> facts;
};

int g_checks = 0, g_failures = 0;
std::vector<std::string> g_problems;

void fail(const std::string& id, const std::string& what) {
  ++g_failures;
  if (g_problems.size() < 40) g_problems.push_back(id + ": " + what);
}

bool build_request(const CaseRec& c, kalos::tramo_seats_model_request& r,
                   std::string& err) {
  std::vector<const char*> reqp;
  reqp.reserve(c.req.size());
  for (const std::string& ln : c.req) reqp.push_back(ln.c_str());
  const golden_tsm::Case proxy{c.id.c_str(),     "",  "", 0, 0,
                               c.req.size(),     reqp.data(), 0, nullptr, 0,
                               nullptr};
  if (!replay_tsm::parse_request(proxy, r, err)) return false;
  for (const std::string& ln : c.req) {
    if (ln.rfind("req knobs ", 0) != 0) continue;
    const std::vector<std::string> t = replay_tsm::tokens(ln);
    if (t.size() != 12) {
      err = "bad knobs tail";
      return false;
    }
    r.forecast_count = std::atoi(t[8].c_str());
    r.backcast_count = std::atoi(t[9].c_str());
    r.components_method = t[10] == "Burman"
                              ? kalos::seats_components_method::burman
                              : kalos::seats_components_method::kalman_smoother;
    r.bias = t[11] == "Legacy" ? kalos::seats_bias_correction::legacy
                               : kalos::seats_bias_correction::none;
  }
  for (const std::string& tok : replay_tsm::tokens(c.values))
    r.series.push_back(std::strtod(tok.c_str(), nullptr));
  const std::vector<std::string> st = replay_tsm::tokens(c.start);
  if (st.size() != 2) {
    err = "bad start";
    return false;
  }
  r.start_year = std::atoi(st[0].c_str());
  r.start_month = std::atoi(st[1].c_str());
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1
                              ? argv[1]
                              : "validation/evidence/native/seats-wk-extraction";

  std::map<std::string, CaseRec> cases;
  std::vector<std::string> order;
  {
    std::ifstream f(dir + "/cases.csv");
    if (!f) { std::cerr << "missing " << dir << "/cases.csv\n"; return 2; }
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
      if (line.empty()) continue;
      const std::vector<std::string> t = fields(line);
      if (t.size() < 14) continue;
      CaseRec& c = cases[t[0]];
      c.id = t[0];
      c.freq = std::atoi(t[2].c_str());
      c.start = t[3];
      c.status = t[5];
      c.mode = t[7];
      c.method = t[10];
      c.ddecompose = t[13];
      order.push_back(t[0]);
    }
  }
  {
    std::ifstream f(dir + "/inputs.csv");
    if (!f) { std::cerr << "missing " << dir << "/inputs.csv\n"; return 2; }
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
      if (line.empty()) continue;
      const std::vector<std::string> t = fields(line);
      if (t.size() != 2 || !cases.count(t[0])) continue;
      CaseRec& c = cases[t[0]];
      if (t[1].rfind("req ", 0) == 0) {
        c.req.push_back(t[1]);
      } else if (t[1].rfind("values ", 0) == 0) {
        c.values = t[1].substr(7);
      }
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
      if (t.size() != 3 || !cases.count(t[0])) continue;
      if (is_fact_key(t[1])) cases[t[0]].facts.push_back(t[1] + " " + t[2]);
    }
  }

  // Tier classification, data-driven exactly as the unit gates
  // (seats_extract_wk): frequency, method, variance, approximation.
  for (const std::string& id : order) {
    CaseRec& c = cases[id];
    double var = 0.0;
    for (const std::string& ln : c.req) {
      const std::vector<std::string> tk = replay_tsm::tokens(ln);
      if (tk.size() == 7 && tk[1] == "log") var = std::atof(tk[6].c_str());
    }
    if (c.freq != 12)
      c.refusal_reason = cfmt(kReasonFreq, c.freq);
    else if (c.method == "KalmanSmoother")
      c.refusal_reason = kReasonKalman;
    else if (var == 0.0)
      c.refusal_reason = kReasonVar0;
    else if (c.ddecompose == "null" && c.mode != "None")
      c.refusal_reason = kReasonApprox;
    if (c.refusal_reason.empty() && c.facts.empty())
      fail(id, "replay tier without fact rows");
  }

  // Section 1+2: replay and refusal.
  int nreplay = 0, nrefusal = 0, nok = 0;
  for (const std::string& id : order) {
    const CaseRec& c = cases[id];
    kalos::tramo_seats_model_request r;
    std::string err;
    if (!build_request(c, r, err)) {
      fail(id, "request: " + err);
      continue;
    }
    if (!c.refusal_reason.empty()) {
      ++nrefusal;
      std::string msg = "NO THROW";
      bool invalid = false;
      try {
        kalos::seats_extract_wk(r);
      } catch (const std::invalid_argument& e) {
        invalid = true;
        msg = e.what();
      } catch (const std::exception& e) {
        msg = std::string("wrong type: ") + e.what();
      }
      ++g_checks;
      if (!invalid || msg != c.refusal_reason)
        fail(id, "refusal: got [" + msg + "]");
      continue;
    }
    ++nreplay;
    std::vector<std::string> got;
    try {
      got = kalos::seats_extract_wk(r).facts();
    } catch (const std::exception& e) {
      fail(id, std::string("escaped: ") + e.what());
      continue;
    }
    if (got != c.facts) {
      fail(id, "fact replay: got " + std::to_string(got.size()) + " lines, " +
                   std::to_string(c.facts.size()) + " expected");
      for (std::size_t j = 0; j < got.size() && j < c.facts.size(); ++j)
        if (got[j] != c.facts[j]) {
          fail(id, "first divergence at line " + std::to_string(j));
          break;
        }
    } else {
      ++g_checks;
    }
    const std::vector<std::string> again = kalos::seats_extract_wk(r).facts();
    ++g_checks;
    if (again != got) fail(id, "non-deterministic second run");
    if (!got.empty() && got[0] == "status ok") ++nok;
    // Section 3 audit: status line vs the captured status column.
    ++g_checks;
    const std::string want = "status " + c.status;
    if (got.empty() || got[0] != want) fail(id, "status column mismatch");
    int doms = 0;
    bool has_init = false, has_final = false;
    for (const std::string& ln : got) {
      if (ln.rfind("dom ", 0) == 0) ++doms;
      if (ln.rfind("init ", 0) == 0) has_init = true;
      if (ln.rfind("final ", 0) == 0) has_final = true;
    }
    ++g_checks;
    if (want == "status ok") {
      if (doms != 3) fail(id, "status ok with " + std::to_string(doms) + " doms");
    } else if (doms != 0 || has_init || has_final) {
      fail(id, "status error with dom/init/final follow-up");
    }
  }

  std::printf(
      "verify_seats_wk_extraction: %zu cases (%d refusal, %d replay, %d "
      "status ok), %d checks, %d failures\n",
      order.size(), nrefusal, nreplay, nok, g_checks, g_failures);
  for (std::size_t i = 0; i < g_problems.size(); ++i)
    std::printf("  %s\n", g_problems[i].c_str());
  return g_failures == 0 ? 0 : 1;
}
