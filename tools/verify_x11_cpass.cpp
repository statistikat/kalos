// Full-sweep verification: native x11_c_pass against every pinned X11CStep
// fixture case at exact double equality (NaN-aware). Run from the repository
// root; an alternative fixture directory may be passed as the first argument
// (the multiplicative sweeps run on validation/fixtures/native/x11-cpass-mult,
// whose optional cases.csv `mode` column selects the decomposition mode).
// fixture (composed like X11Kernel: native B pass -> native C pass). Every
// table of every case at exact double equality (NaN-aware).
#include <kalos/x11_c_pass.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
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
double parse_double(const std::string& raw) {
  const std::string s = unquote(raw);
  if (s == "NaN" || s == "nan") return std::nan("");
  return std::stod(s);
}
int parse_int(const std::string& raw) { return std::stoi(unquote(raw)); }
std::vector<int> parse_list(const std::string& raw) {
  std::vector<int> out;
  std::istringstream in(unquote(raw));
  int v;
  while (in >> v) out.push_back(v);
  return out;
}
std::vector<std::string> split(const std::string& line) {
  std::vector<std::string> out;
  std::string field;
  bool quoted = false;
  for (const char c : line) {
    if (c == '"') { quoted = !quoted; field.push_back(c); }
    else if (c == ',' && !quoted) { out.push_back(field); field.clear(); }
    else field.push_back(c);
  }
  out.push_back(field);
  return out;
}
std::vector<std::vector<std::string>> read_csv(const std::string& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("cannot open " + path);
  std::vector<std::vector<std::string>> rows;
  std::string line;
  std::getline(file, line);  // header
  while (std::getline(file, line))
    if (!line.empty()) rows.push_back(split(line));
  return rows;
}
std::map<std::string, std::size_t> read_csv_header(const std::string& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error("cannot open " + path);
  std::string line;
  std::getline(file, line);
  std::map<std::string, std::size_t> out;
  const auto fields = split(line);
  for (std::size_t i = 0; i < fields.size(); ++i) out[unquote(fields[i])] = i;
  return out;
}
kalos::X11DecompositionMode parse_mode(const std::string& raw) {
  const auto s = unquote(raw);
  if (s == "Multiplicative") return kalos::X11DecompositionMode::multiplicative;
  if (s == "Additive") return kalos::X11DecompositionMode::additive;
  if (s == "LogAdditive") return kalos::X11DecompositionMode::log_additive;
  throw std::runtime_error("unexpected fixture mode " + s);
}
bool equal_nan(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::isnan(a[i]) != std::isnan(b[i])) return false;
    if (!std::isnan(a[i]) && a[i] != b[i]) return false;
  }
  return true;
}
struct Case {
  std::string id;
  int frequency = 0, n = 0, start_period = 0, trend = 13;
  double lsig = 1.5, usig = 2.5;
  std::vector<int> initial, final;
  long flagged_rc = 0, corrected_rc = 0;
  kalos::X11DecompositionMode mode = kalos::X11DecompositionMode::additive;
};
}  // namespace

int main(int argc, char** argv) {
  const std::string root = argc > 1 ? argv[1] : "validation/fixtures/native/x11-cpass/";
  const auto cols = read_csv_header(root + "cases.csv");
  const auto mode_col = cols.find("mode");  // absent in the additive fixtures
  std::vector<Case> cases;
  for (const auto& f : read_csv(root + "cases.csv")) {
    Case c;
    c.id = unquote(f[0]);
    c.frequency = parse_int(f[2]);
    c.n = parse_int(f[3]);
    c.start_period = parse_int(f[4]);
    c.trend = parse_int(f[5]);
    c.lsig = parse_double(f[6]);
    c.usig = parse_double(f[7]);
    c.initial = parse_list(f[9]);
    c.final = parse_list(f[10]);
    c.flagged_rc = std::stol(unquote(f[13]));
    c.corrected_rc = std::stol(unquote(f[14]));
    if (mode_col != cols.end()) c.mode = parse_mode(f[mode_col->second]);
    cases.push_back(c);
  }
  std::map<std::string, kalos::MaskedSeries> series;
  for (const auto& f : read_csv(root + "inputs.csv")) {
    auto& y = series[unquote(f[0])];
    const auto row = static_cast<std::size_t>(std::stoull(unquote(f[1])));
    if (y.values.size() <= row) {
      y.values.resize(row + 1);
      y.missing.resize(row + 1, 0);
    }
    y.values[row] = parse_double(f[2]);
    y.missing[row] = unquote(f[3]) == "TRUE" ? 1 : 0;
  }
  std::map<std::string, std::map<std::string, std::vector<double>>> tables;
  for (const auto& f : read_csv(root + "tables.csv")) {
    auto& t = tables[unquote(f[0])][unquote(f[1])];
    const auto row = static_cast<std::size_t>(std::stoull(unquote(f[2])));
    if (t.size() <= row) t.resize(row + 1, std::nan(""));
    t[row] = parse_double(f[3]);
  }
  int failures = 0, passed = 0;
  const std::vector<std::pair<const char*, kalos::MaskedSeries kalos::X11CPass::*>> names = {
      {"C1", &kalos::X11CPass::c1},   {"C2", &kalos::X11CPass::c2},
      {"C4", &kalos::X11CPass::c4},   {"C5", &kalos::X11CPass::c5},
      {"C5a", &kalos::X11CPass::c5a}, {"C6", &kalos::X11CPass::c6},
      {"C7", &kalos::X11CPass::c7},   {"C9", &kalos::X11CPass::c9},
      {"C10", &kalos::X11CPass::c10}, {"C10a", &kalos::X11CPass::c10a},
      {"C11", &kalos::X11CPass::c11}, {"C13", &kalos::X11CPass::c13},
      {"C17", &kalos::X11CPass::c17}, {"C20", &kalos::X11CPass::c20}};
  for (const auto& c : cases) {
    bool clean = true;
    const auto& y = series[c.id];
    if (y.values.size() != static_cast<std::size_t>(c.n)) {
      std::printf("FAIL %s: input length\n", c.id.c_str());
      ++failures;
      continue;
    }
    kalos::X11CPass pass;
    try {
      const auto b = kalos::x11_b_pass(y, c.frequency, c.start_period, c.initial,
          c.final, c.trend, kalos::X11SigmaPolicy::legacy, c.lsig, c.usig, c.mode);
      pass = kalos::x11_c_pass(y, c.frequency, c.start_period, c.initial,
          c.final, c.trend, kalos::X11SigmaPolicy::legacy, b, c.lsig, c.usig);
    } catch (const std::exception& e) {
      std::printf("FAIL %s: threw %s\n", c.id.c_str(), e.what());
      ++failures;
      continue;
    }
    if (pass.mode != c.mode) {
      std::printf("FAIL %s: mode echo\n", c.id.c_str());
      ++failures;
      continue;
    }
    if (pass.status != "available" ||
        pass.c2drop != static_cast<std::size_t>(c.frequency) / 2) {
      std::printf("FAIL %s: status/c2drop\n", c.id.c_str());
      ++failures;
      continue;
    }
    for (const auto& [name, member] : names) {
      const auto& got = (pass.*member).values;
      const auto& want = tables[c.id][name];
      if (!equal_nan(got, want)) {
        ++failures;
        clean = false;
        std::printf("FAIL %s: table %s (len %zu vs %zu)\n", c.id.c_str(), name,
            got.size(), want.size());
        for (std::size_t i = 0; i < std::min(got.size(), want.size()); ++i)
          if ((std::isnan(got[i]) != std::isnan(want[i])) ||
              (!std::isnan(got[i]) && got[i] != want[i])) {
            std::printf("   first row %zu: got %.17g want %.17g\n", i, got[i], want[i]);
            break;
          }
      }
    }
    // Correction history: the delivered C17/C20 are the round's weights and
    // factors and the pinned metadata counts match.
    if (!equal_nan(pass.round.sigma.weights, tables[c.id]["C17"]) ||
        !equal_nan(pass.round.replacements.correction_factors, tables[c.id]["C20"]) ||
        !equal_nan(pass.round.detection.values, tables[c.id]["C13"]) ||
        pass.round.analysis_position != c.start_period - 1 ||
        pass.round.policy != kalos::X11SigmaPolicy::legacy) {
      std::printf("FAIL %s: correction history linkage\n", c.id.c_str());
      ++failures;
      continue;
    }
    long flagged = 0, corrected = 0;
    const double dflt =  // the pinned mode default (DummyFilter/correction factor)
        c.mode == kalos::X11DecompositionMode::multiplicative ? 1. : 0.;
    for (const auto w : pass.c17.values) if (w != 1.) ++flagged;
    for (const auto f : pass.c20.values) if (f != dflt) ++corrected;
    if (flagged != c.flagged_rc || corrected != c.corrected_rc) {
      std::printf("FAIL %s: counts flagged %ld/%ld corrected %ld/%ld\n", c.id.c_str(),
          flagged, c.flagged_rc, corrected, c.corrected_rc);
      ++failures;
      continue;
    }
    if (clean) ++passed;
  }
  std::printf("cases=%zu passed=%d failures=%d\n", cases.size(), passed, failures);
  return failures == 0 ? 0 : 1;
}
