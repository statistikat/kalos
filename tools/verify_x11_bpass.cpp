// Full-sweep verification: native x11_b_pass against every pinned X11BStep
// fixture case at exact double equality (NaN-aware). Run from the repository
// root; an alternative fixture directory may be passed as the first argument
// (the multiplicative sweeps run on validation/fixtures/native/x11-bpass-mult,
// whose optional cases.csv `mode` column selects the decomposition mode).
// Compile against the installed library headers, e.g.
//   g++ -O2 -std=c++17 -fopenmp -Iinclude tools/verify_x11_bpass.cpp
//       -L<build> -lkalos -lopenblas -o /tmp/verify_x11_bpass
#include <kalos/x11_b_pass.hpp>

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
  if (s == "PseudoAdditive") return kalos::X11DecompositionMode::pseudo_additive;
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
  kalos::X11DecompositionMode mode = kalos::X11DecompositionMode::additive;
};
}  // namespace

int main(int argc, char** argv) {
  const std::string root = argc > 1 ? argv[1] : "validation/fixtures/native/x11-bpass/";
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
  const std::vector<std::pair<const char*, kalos::MaskedSeries kalos::X11BPass::*>> names = {
      {"B1", &kalos::X11BPass::b1},   {"B2", &kalos::X11BPass::b2},
      {"B3", &kalos::X11BPass::b3},   {"B4", &kalos::X11BPass::b4},
      {"B4a", &kalos::X11BPass::b4a}, {"B4anorm", &kalos::X11BPass::b4anorm},
      {"B4d", &kalos::X11BPass::b4d}, {"B4g", &kalos::X11BPass::b4g},
      {"B5", &kalos::X11BPass::b5},   {"B6", &kalos::X11BPass::b6},
      {"B7", &kalos::X11BPass::b7},   {"B8", &kalos::X11BPass::b8},
      {"B9", &kalos::X11BPass::b9},   {"B9g", &kalos::X11BPass::b9g},
      {"B10", &kalos::X11BPass::b10}, {"B11", &kalos::X11BPass::b11},
      {"B13", &kalos::X11BPass::b13}, {"B17", &kalos::X11BPass::b17},
      {"B20", &kalos::X11BPass::b20}};
  for (const auto& c : cases) {
    bool clean = true;
    const auto& y = series[c.id];
    if (y.values.size() != static_cast<std::size_t>(c.n)) {
      std::printf("FAIL %s: input length\n", c.id.c_str());
      ++failures;
      continue;
    }
    kalos::X11BPass pass;
    try {
      pass = kalos::x11_b_pass(y, c.frequency, c.start_period, c.initial,
          c.final, c.trend, kalos::X11SigmaPolicy::legacy, c.lsig, c.usig, c.mode);
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
    const std::pair<const char*, const kalos::MaskedSeries*> history[] = {
        {"B4d", &pass.rounds[0].detection}, {"B9d", &pass.rounds[1].detection},
        {"B13", &pass.rounds[2].detection}};
    for (const auto& [name, ser] : history) {
      const auto& want = tables[c.id][name];
      if (!equal_nan(ser->values, want)) {
        ++failures;
        clean = false;
        std::printf("FAIL %s: round detection %s (len %zu vs %zu)\n", c.id.c_str(), name,
            ser->values.size(), want.size());
        for (std::size_t i = 0; i < std::min(ser->values.size(), want.size()); ++i)
          if ((std::isnan(ser->values[i]) != std::isnan(want[i])) ||
              (!std::isnan(ser->values[i]) && ser->values[i] != want[i])) {
            std::printf("   first row %zu: got %.17g want %.17g\n", i, ser->values[i], want[i]);
            break;
          }
      }
    }
    if (clean) ++passed;
  }
  std::printf("cases=%zu passed=%d failures=%d\n", cases.size(), passed, failures);
  return failures == 0 ? 0 : 1;
}
