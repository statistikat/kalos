// Full-sweep verification: native x13_allocation against every pinned
// X13Kernel allocation fixture at exact double equality (NaN-aware), plus
// the composed X-11 leg (native selection pass over the preadjusted series
// compared against the captured D10..D13/C17) and the end-to-end restore
// over the native tables. Run from the repository root; an alternative
// fixture directory may be passed as the first argument. The log-additive
// D-table comparison applies the pinned export-stage exp to the delivered
// log-domain pass tables, so it is checked at the transcendental boundary
// with a tight tolerance and the worst relative deviation is reported.
#include <kalos/x11_selection_pass.hpp>
#include <kalos/x13_allocation.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
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
bool parse_bool(const std::string& raw) { return unquote(raw) == "TRUE"; }
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
  return out;
}
std::vector<std::vector<std::string>> read_csv(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::vector<std::vector<std::string>> rows;
  std::string line;
  bool header = true;
  for (std::size_t i = 0; std::getline(in, line); ++i) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (header) { header = false; continue; }
    if (!line.empty()) rows.push_back(fields(line));
  }
  return rows;
}
bool same_nan(double a, double b) {
  return std::isnan(a) || std::isnan(b) ? std::isnan(a) && std::isnan(b) : a == b;
}
// Exact comparison; every call counts as one check.
void expect_equal(const std::vector<double>& got, const std::vector<double>& want,
                  const std::string& label, long& checks, long& bad) {
  ++checks;
  if (got.size() != want.size()) {
    std::printf("  %s: LENGTH %zu vs %zu\n", label.c_str(), got.size(), want.size());
    ++bad;
    return;
  }
  for (std::size_t i = 0; i < got.size(); ++i)
    if (!same_nan(got[i], want[i])) {
      std::printf("  %s: DIFF at %zu %.17g vs %.17g\n", label.c_str(), i, got[i], want[i]);
      ++bad;
      return;
    }
}
// Tolerance comparison across composed transcendental boundaries.
void expect_close(const std::vector<double>& got, const std::vector<double>& want,
                  const std::string& label, double tolerance, double& worst,
                  long& checks, long& bad) {
  ++checks;
  if (got.size() != want.size()) {
    std::printf("  %s: LENGTH\n", label.c_str());
    ++bad;
    return;
  }
  for (std::size_t i = 0; i < want.size(); ++i) {
    if (same_nan(got[i], want[i])) continue;
    const double rel = std::abs(want[i]) > 0.
        ? std::abs(got[i] - want[i]) / std::abs(want[i]) : std::abs(got[i] - want[i]);
    if (rel > tolerance) {
      std::printf("  %s: DIFF at %zu %.17g vs %.17g\n", label.c_str(), i, got[i], want[i]);
      ++bad;
      return;
    }
    if (rel > worst) worst = rel;
  }
}
struct Case {
  std::string id, mode_requested, mode_effective;
  int frequency = 4, first_period = 1, n = 0;
  bool log = false;
  std::vector<double> input;
  std::vector<kalos::X13AllocationVariable> vars;
  std::map<std::string, std::vector<double>> tables;
};
kalos::X13Component component_of(const std::string& s) {
  using C = kalos::X13Component;
  if (s == "Series") return C::series;
  if (s == "Trend") return C::trend;
  if (s == "Seasonal") return C::seasonal;
  if (s == "SeasonallyAdjusted") return C::seasonally_adjusted;
  if (s == "Irregular") return C::irregular;
  if (s == "CalendarEffect") return C::calendar_effect;
  if (s == "Undefined") return C::undefined;
  throw std::runtime_error("unknown regeffect " + s);
}
kalos::X13VariableClass class_of(const std::string& s) {
  using K = kalos::X13VariableClass;
  if (s == "outlier") return K::outlier;
  if (s == "days") return K::days_related;
  if (s == "holiday") return K::moving_holiday;
  if (s == "user") return K::user;
  throw std::runtime_error("unknown variable class " + s);
}
kalos::X11DecompositionMode mode_of(const std::string& s) {
  using M = kalos::X11DecompositionMode;
  if (s == "Additive") return M::additive;
  if (s == "Multiplicative") return M::multiplicative;
  if (s == "LogAdditive") return M::log_additive;
  if (s == "PseudoAdditive") return M::pseudo_additive;
  throw std::runtime_error("unknown mode " + s);
}
} // namespace

int main(int argc, char** argv) {
  const std::string dir =
      argc > 1 ? argv[1] : "validation/fixtures/native/x13-allocation";
  std::vector<Case> cases;
  for (const auto& r : read_csv(dir + "/cases.csv")) {
    Case c;
    c.id = unquote(r[0]); c.frequency = parse_int(r[1]);
    c.first_period = parse_int(r[2]); c.n = parse_int(r[3]);
    c.log = parse_bool(r[4]); c.mode_requested = unquote(r[5]);
    c.mode_effective = unquote(r[6]);
    cases.push_back(c);
  }
  auto case_index = [&](const std::string& id) -> std::size_t {
    for (std::size_t i = 0; i < cases.size(); ++i)
      if (cases[i].id == id) return i;
    throw std::runtime_error("unknown case " + id);
  };
  for (const auto& r : read_csv(dir + "/inputs.csv"))
    cases[case_index(unquote(r[0]))].input.push_back(parse_double(r[2]));
  std::map<std::size_t, std::vector<double>> columns;
  // vars.csv has one row per coefficient; group by (case, index).
  std::map<std::size_t, std::map<std::size_t, std::vector<std::string>>> var_lines;
  std::map<std::size_t, std::map<std::size_t, std::pair<std::string, std::string>>> var_meta;
  for (const auto& r : read_csv(dir + "/vars.csv")) {
    const std::size_t ci = case_index(unquote(r[0]));
    const std::size_t vi = static_cast<std::size_t>(parse_int(r[1]));
    var_lines[ci][vi].push_back(unquote(r[9]));
    var_meta[ci][vi] = {unquote(r[3]), unquote(r[4])};
  }
  for (const auto& ci : var_lines) {
    Case& c = cases[ci.first];
    const std::size_t need = ci.second.rbegin()->first + 1;
    if (c.vars.size() < need) c.vars.resize(need);
    for (const auto& vi : ci.second) {
      kalos::X13AllocationVariable v;
      v.cls = class_of(var_meta[ci.first][vi.first].first);
      v.effect = component_of(var_meta[ci.first][vi.first].second);
      for (const auto& token : vi.second)
        if (!token.empty()) v.coefficient.push_back(std::stod(token));
      c.vars[vi.first] = v;
    }
  }
  for (const auto& r : read_csv(dir + "/columns.csv")) {
    const std::size_t ci = case_index(unquote(r[0]));
    const std::size_t vi = static_cast<std::size_t>(parse_int(r[1]));
    const std::size_t cj = static_cast<std::size_t>(parse_int(r[2]));
    auto& col = columns[ci * 1000 + vi * 10 + cj];
    col.push_back(parse_double(r[4]));
  }
  for (std::size_t ci = 0; ci < cases.size(); ++ci)
    for (std::size_t vi = 0; vi < cases[ci].vars.size(); ++vi)
      for (std::size_t cj = 0; cj < cases[ci].vars[vi].coefficient.size(); ++cj)
        cases[ci].vars[vi].columns.push_back(columns[ci * 1000 + vi * 10 + cj]);
  for (const auto& r : read_csv(dir + "/tables.csv"))
    cases[case_index(unquote(r[0]))].tables[unquote(r[1])].push_back(parse_double(r[3]));

  long checks = 0, bad = 0;
  double worst = 0.;
  for (const auto& c : cases) {
    std::printf("case %s (%s -> %s%s)\n", c.id.c_str(), c.mode_requested.c_str(),
        c.mode_effective.c_str(), c.log ? ", log" : "");
    const auto M = mode_of(c.mode_effective);
    // The pinned mode-forcing rule of X13Kernel.updateSpec.
    const std::string forced = c.log
        ? (c.mode_requested == "LogAdditive" || c.mode_requested == "PseudoAdditive"
               ? c.mode_requested : "Multiplicative")
        : "Additive";
    ++checks;
    if (forced != c.mode_effective) {
      std::printf("  mode forcing violated: %s requested, %s effective\n",
          c.mode_requested.c_str(), c.mode_effective.c_str());
      ++bad;
    }
    kalos::X13PreadjustInput in;
    in.input = c.input;
    in.variables = c.vars;
    in.log = c.log;
    const kalos::X13Preadjustment pre = kalos::x13_preadjust(in);
    ++checks;
    if (pre.status != "available") {
      std::printf("  preadjust %s\n", pre.status.c_str());
      ++bad;
      continue;
    }
    const std::pair<const char*, const std::vector<double>*> pre_tables[13] = {
        {"a1", &pre.a1}, {"a6", &pre.a6}, {"a7", &pre.a7}, {"a8", &pre.a8},
        {"a8t", &pre.a8t}, {"a8s", &pre.a8s}, {"a8i", &pre.a8i}, {"a9", &pre.a9},
        {"a9u", &pre.a9u}, {"a9cal", &pre.a9cal}, {"a9sa", &pre.a9sa},
        {"a9ser", &pre.a9ser}, {"out", &pre.out}};
    // Under a log model every a-table is stored through backTransform's exp;
    // std::exp vs Math.exp is a transcendental boundary (1-ulp hits), so the
    // twelve exp'd buckets compare at close tolerance; a1 and out are pure
    // algebra and compare bit-exactly.
    for (const auto& t : pre_tables) {
      const bool boundary = c.log && t.first != pre_tables[0].first
          && t.first != pre_tables[12].first;
      if (boundary)
        expect_close(*t.second, c.tables.at(t.first), t.first, 1e-15, worst, checks, bad);
      else
        expect_equal(*t.second, c.tables.at(t.first), t.first, checks, bad);
    }
    // The X-11 leg over the preadjusted series with the pinned spec of the
    // capture (all-period S3X9 requests, Henderson 5, sigma 1.5/2.5).
    kalos::MaskedSeries series;
    series.values = pre.out;
    series.missing.assign(pre.out.size(), 0);
    std::vector<kalos::X11SeasonalRequest> requests(
        static_cast<std::size_t>(c.frequency), kalos::X11SeasonalRequest::s3x9);
    const kalos::X11SelectionPass pass = kalos::x11_selection_pass(series,
        c.frequency, c.first_period, requests, 5, kalos::X11SigmaPolicy::legacy,
        kalos::X11MsrIterationPolicy::jdplus, 1.5, 2.5, 0, 0, M);
    ++checks;
    if (pass.status != "available") {
      std::printf("  selection pass %s\n", pass.status.c_str());
      ++bad;
      continue;
    }
    const std::pair<const char*, const std::vector<double>*> dtab[4] = {
        {"d10", &pass.d.d10.values}, {"d11", &pass.d.d11.values},
        {"d12", &pass.d.d12.values}, {"d13", &pass.d.d13.values}};
    // The pinned results-builder stage of X11Kernel (finalResults ->
    // ratioBiasCorrection, the X11Spec default bias) applied ONLY under
    // LogAdditive: with A=exp(d10step), B=exp(d12step), C=exp(d13step),
    // v9=average(A), v13=v9*average(C) (the range is the whole series for a
    // series spanning whole years, as every fixture case does), the stored
    // tables are D10=A/v9, D12=makePositivity(B*v13) (identity here: all
    // positive), D11=input/D10, D13=input/(D10*D12), input = out.
    std::vector<double> s10, s11, s12, s13;
    auto la_results = [&](const std::vector<double>& t10,
                          const std::vector<double>& t12,
                          const std::vector<double>& t13) {
      const std::size_t m = t10.size();
      std::vector<double> A(m), B(m), C(m);
      double sa = 0., sc = 0.;
      for (std::size_t i = 0; i < m; ++i) {
        A[i] = std::exp(t10[i]); B[i] = std::exp(t12[i]); C[i] = std::exp(t13[i]);
        sa += A[i]; sc += C[i];
      }
      const double v9 = sa / static_cast<double>(m),
                   v13 = v9 * (sc / static_cast<double>(m));
      s10.resize(m); s11.resize(m); s12.resize(m); s13.resize(m);
      for (std::size_t i = 0; i < m; ++i) {
        s10[i] = A[i] / v9;
        s12[i] = B[i] * v13;
        s11[i] = pre.out[i] / s10[i];
        s13[i] = (pre.out[i] / s10[i]) / s12[i];
      }
    };
    if (M == kalos::X11DecompositionMode::log_additive)
      la_results(pass.d.d10.values, pass.d.d12.values, pass.d.d13.values);
    for (int j = 0; j < 4; ++j) {
      const char* name = j == 0 ? "d10" : j == 1 ? "d11" : j == 2 ? "d12" : "d13";
      if (M == kalos::X11DecompositionMode::log_additive) {
        const std::vector<double>* r = j == 0 ? &s10 : j == 1 ? &s11 : j == 2 ? &s12 : &s13;
        expect_close(*r, c.tables.at(name), name, 1e-12, worst, checks, bad);
      } else
        expect_equal(*dtab[j].second, c.tables.at(name), name, checks, bad);
    }
    expect_equal(pass.c.c17.values, c.tables.at("c17"), "c17", checks, bad);
    // The finals restore over the captured X-11 tables.
    kalos::X13RestoreInput ri;
    ri.pre = pre;
    ri.mode = M;
    const std::vector<double> capture_d10 = c.tables.at("d10");
    const std::vector<double> capture_d11 = c.tables.at("d11");
    const std::vector<double> capture_d12 = c.tables.at("d12");
    const std::vector<double> capture_d13 = c.tables.at("d13");
    ri.d10 = capture_d10;
    ri.d11 = capture_d11;
    ri.d12 = capture_d12;
    ri.d13 = capture_d13;
    ri.c17 = c.tables.at("c17");
    const kalos::X13Finals fin = kalos::x13_restore(ri);
    ++checks;
    if (fin.status != "available") {
      std::printf("  restore %s\n", fin.status.c_str());
      ++bad;
      continue;
    }
    const std::pair<const char*, const std::vector<double>*> finals[9] = {
        {"d11final", &fin.d11final}, {"d12final", &fin.d12final},
        {"d13final", &fin.d13final}, {"d16", &fin.d16}, {"d18", &fin.d18},
        {"e1", &fin.e1}, {"e2", &fin.e2}, {"e3", &fin.e3}, {"e11", &fin.e11}};
    for (const auto& t : finals)
      expect_equal(*t.second, c.tables.at(t.first), t.first, checks, bad);
    // The end-to-end wiring: restore over the native selection-pass tables
    // (through the pinned results-builder stage under log-additive).
    if (M == kalos::X11DecompositionMode::log_additive) {
      la_results(pass.d.d10.values, pass.d.d12.values, pass.d.d13.values);
      ri.d10 = s10; ri.d11 = s11; ri.d12 = s12; ri.d13 = s13;
    } else {
      ri.d10 = pass.d.d10.values;
      ri.d11 = pass.d.d11.values;
      ri.d12 = pass.d.d12.values;
      ri.d13 = pass.d.d13.values;
    }
    const kalos::X13Finals e2e = kalos::x13_restore(ri);
    const std::pair<const char*, const std::vector<double>*> e2e_finals[9] = {
        {"e2e d11final", &e2e.d11final}, {"e2e d12final", &e2e.d12final},
        {"e2e d13final", &e2e.d13final}, {"e2e d16", &e2e.d16}, {"e2e d18", &e2e.d18},
        {"e2e e1", &e2e.e1}, {"e2e e2", &e2e.e2}, {"e2e e3", &e2e.e3},
        {"e2e e11", &e2e.e11}};
    const char* plain[9] = {"d11final", "d12final", "d13final", "d16", "d18",
        "e1", "e2", "e3", "e11"};
    for (int j = 0; j < 9; ++j)
      if (M == kalos::X11DecompositionMode::log_additive)
        expect_close(*e2e_finals[j].second, c.tables.at(plain[j]), e2e_finals[j].first,
            1e-9, worst, checks, bad);
      else
        expect_equal(*e2e_finals[j].second, c.tables.at(plain[j]),
            e2e_finals[j].first, checks, bad);
  }
  std::printf("%ld checks, %ld failures, transcendental-boundary worst %.3g\n",
      checks, bad, worst);
  return bad == 0 ? 0 : 1;
}
