// Full-sweep verification: native x11_d_pass against every pinned X11DStep
// fixture case at exact double equality (NaN-aware). Run from the repository
// root; an alternative fixture directory may be passed as the first argument.
// fixture (composed like X11Kernel: native B pass -> native C pass -> native
// D pass). Every table of every case, including the MSRE diagnostics and the
// four hand-extended calendar cases, at exact double equality (NaN-aware).
#include <kalos/x11_d_pass.hpp>
#include <kalos/detail/x11_pinned.hpp>

#include <algorithm>
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
bool equal_nan(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::isnan(a[i]) != std::isnan(b[i])) return false;
    if (!std::isnan(a[i]) && a[i] != b[i]) return false;
  }
  return true;
}
bool same_scalar(double a, double b) {
  return std::isnan(a) || std::isnan(b) ? std::isnan(a) && std::isnan(b) : a == b;
}
struct Case {
  std::string id;
  int frequency = 0, n = 0, start_period = 0, trend = 13;
  double lsig = 1.5, usig = 2.5;
  std::vector<int> initial, final;
  int back = 0, fc = 0, orig_n = 0, orig_start = 0;
  double global_msr = 0;
  long d9_nonnan = 0, corrected_rc = 0;
};
}  // namespace

int main(int argc, char** argv) {
  const std::string root = argc > 1 ? argv[1] : "validation/fixtures/native/x11-dpass/";
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
    c.back = parse_int(f[11]);
    c.fc = parse_int(f[12]);
    c.orig_n = parse_int(f[13]);
    c.orig_start = parse_int(f[14]);
    c.global_msr = parse_double(f[15]);
    c.d9_nonnan = std::stol(unquote(f[16]));
    c.corrected_rc = std::stol(unquote(f[17]));
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
  struct Msr { std::vector<double> ri, rs, msr; std::vector<long> count; };
  std::map<std::string, Msr> msr;
  for (const auto& f : read_csv(root + "msr.csv")) {
    auto& m = msr[unquote(f[0])];
    const auto phase = static_cast<std::size_t>(std::stoul(unquote(f[1])));
    if (m.ri.size() <= phase) {
      m.ri.resize(phase + 1); m.rs.resize(phase + 1);
      m.msr.resize(phase + 1); m.count.resize(phase + 1);
    }
    m.ri[phase] = parse_double(f[2]);
    m.rs[phase] = parse_double(f[3]);
    m.msr[phase] = parse_double(f[4]);
    m.count[phase] = std::stol(unquote(f[5]));
  }
  int failures = 0, passed = 0;
  const std::vector<std::pair<const char*, kalos::MaskedSeries kalos::X11DPass::*>> names = {
      {"D1", &kalos::X11DPass::d1},     {"D2", &kalos::X11DPass::d2},
      {"D4", &kalos::X11DPass::d4},     {"D5", &kalos::X11DPass::d5},
      {"D6", &kalos::X11DPass::d6},     {"D7", &kalos::X11DPass::d7},
      {"D8", &kalos::X11DPass::d8},     {"D9", &kalos::X11DPass::d9},
      {"D9bis", &kalos::X11DPass::d9bis}, {"D9_g_bis", &kalos::X11DPass::d9bis},
      {"D10", &kalos::X11DPass::d10},   {"D10bis", &kalos::X11DPass::d10bis},
      {"D11", &kalos::X11DPass::d11},   {"D11bis", &kalos::X11DPass::d11bis},
      {"D12", &kalos::X11DPass::d12},   {"D13", &kalos::X11DPass::d13}};
  for (const auto& c : cases) {
    bool clean = true;
    const auto& y = series[c.id];
    if (y.values.size() != static_cast<std::size_t>(c.n)) {
      std::printf("FAIL %s: input length\n", c.id.c_str());
      ++failures;
      continue;
    }
    kalos::X11DPass pass;
    try {
      const auto b = kalos::x11_b_pass(y, c.frequency, c.start_period, c.initial,
          c.final, c.trend, kalos::X11SigmaPolicy::legacy, c.lsig, c.usig);
      const auto cp = kalos::x11_c_pass(y, c.frequency, c.start_period, c.initial,
          c.final, c.trend, kalos::X11SigmaPolicy::legacy, b, c.lsig, c.usig);
      pass = kalos::x11_d_pass(y, c.frequency, c.start_period, c.initial, c.final,
          c.trend, kalos::X11SigmaPolicy::legacy, cp, c.back, c.fc);
    } catch (const std::exception& e) {
      std::printf("FAIL %s: threw %s\n", c.id.c_str(), e.what());
      ++failures;
      continue;
    }
    if (pass.status != "available" ||
        pass.d2drop != static_cast<std::size_t>(c.frequency) / 2 ||
        pass.backcast != c.back || pass.forecast != c.fc ||
        pass.msr_iterations != 0 ||
        pass.seas_filter != c.final) {
      std::printf("FAIL %s: status/echo\n", c.id.c_str());
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
    // MSRE diagnostics: per-phase arrays and the global ratio.
    const auto& m = msr[c.id];
    const auto& d = pass.msr_table;
    if (!equal_nan(d.mean_irregular_evolutions, m.ri) ||
        !equal_nan(d.mean_seasonal_evolutions, m.rs) ||
        !equal_nan(d.msr, m.msr) ||
        d.counts.size() != m.count.size() ||
        !std::equal(d.counts.begin(), d.counts.end(), m.count.begin(),
            [](std::int64_t a, long bb) { return a == bb; }) ||
        !same_scalar(d.global_msr, c.global_msr)) {
      std::printf("FAIL %s: msr table\n", c.id.c_str());
      // Debug: recompute straight from the fixture D9_g_bis slice.
      {
        const auto& full = tables[c.id]["D9_g_bis"];
        std::vector<double> slice(full.begin() + c.back,
            full.begin() + static_cast<std::ptrdiff_t>(full.size() - c.fc));
        std::vector<double> ri2, rs2;
        std::vector<std::int64_t> n2;
        const std::size_t f = static_cast<std::size_t>(c.frequency);
        const std::size_t pos =
            (static_cast<std::size_t>(c.start_period - 1) + static_cast<std::size_t>(c.back)) % f;
        kalos::detail::pinned_msr_table(slice, c.frequency, pos, ri2, rs2, n2);
        std::printf("   fixture-input ri:");
        for (auto v : ri2) std::printf(" %.17g", v);
        std::printf("\n   fixture-input rs:");
        for (auto v : rs2) std::printf(" %.17g", v);
        std::printf("\n   fixture-input n :");
        for (auto v : n2) std::printf(" %lld", static_cast<long long>(v));
        std::printf("\n");
      }
      for (std::size_t i = 0; i < d.mean_irregular_evolutions.size(); ++i)
        if (!equal_nan({d.mean_irregular_evolutions[i]}, {m.ri[i]})) {
          std::printf("   phase %zu: ri %.17g want %.17g\n", i,
              d.mean_irregular_evolutions[i], m.ri[i]);
          break;
        }
      ++failures;
      continue;
    }
    // D9 mask count equals the correction count of the consumed C round.
    long nonnan = 0;
    for (const auto v : pass.d9.values) if (!std::isnan(v)) ++nonnan;
    if (nonnan != c.d9_nonnan || nonnan != c.corrected_rc) {
      std::printf("FAIL %s: d9 counts %ld vs %ld/%ld\n", c.id.c_str(), nonnan,
          c.d9_nonnan, c.corrected_rc);
      ++failures;
      continue;
    }
    if (clean) ++passed;
  }
  std::printf("cases=%zu passed=%d failures=%d\n", cases.size(), passed, failures);
  return failures == 0 ? 0 : 1;
}
