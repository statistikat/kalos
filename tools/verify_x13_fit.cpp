// Full-sweep verification of the restricted fixed-model X-13 fit: native
// x13_fit against every pinned fit fixture at exact double equality
// (NaN-aware), across both corpora — the x13-allocation seam cases (driven
// end to end with the uniform fixed S3X9/Henderson-5 specification) and the
// x13-fit corpus (monthly fixed specs captured end to end through the pinned
// x13_fast entry point). Per fixed-route case: the available status and the
// provenance echo, the 13 stored a-tables and out (bit-exact; under a log
// model every table downstream of backTransform's exp is checked at the
// transcendental boundary, where Java's Math.exp and the native libm can
// flip a final ulp), the results tables
// as finals consumed them (bit-exact outside the log models, across the
// delivered results-builder stage inside them), c17, the nine finals (bit-exact
// outside the log models, across the composed transcendental boundary inside
// it) and the final-components triplet identity. Horizon-evidence cases
// (route "arima") must answer the zero-horizon refusal the fit boundary
// pins. Run from the repository root; alternative fixture directories may be
// passed as further arguments.
#include <kalos/x13_fit.hpp>

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
bool parse_bool(const std::string& raw) { return unquote(raw) == "TRUE"; }
std::vector<std::string> fields(const std::string& line) {
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
struct Csv {
  std::map<std::string, std::size_t> col;
  std::vector<std::vector<std::string>> rows;
  const std::string& at(const std::vector<std::string>& r, const char* name) const {
    const auto it = col.find(name);
    if (it == col.end() || it->second >= r.size()) return empty;
    return r[it->second];
  }
  bool has(const char* name) const { return col.count(name) != 0; }
  static const std::string empty;
};
const std::string Csv::empty;
Csv read_csv(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  Csv csv;
  std::string line;
  bool header = true;
  for (std::size_t i = 0; std::getline(in, line); ++i) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    auto f = fields(line);
    if (header) {
      header = false;
      for (std::size_t j = 0; j < f.size(); ++j) csv.col[unquote(f[j])] = j;
      continue;
    }
    csv.rows.push_back(std::move(f));
  }
  return csv;
}
bool same_nan(double a, double b) {
  return std::isnan(a) || std::isnan(b) ? std::isnan(a) && std::isnan(b) : a == b;
}
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
kalos::X11SeasonalRequest filter_of(int code) {
  using R = kalos::X11SeasonalRequest;
  switch (code) {
    case 0: return R::stable;
    case 1: return R::s3x1;
    case 3: return R::s3x3;
    case 5: return R::s3x5;
    case 15: return R::s3x15;
    default: return R::s3x9;
  }
}
struct Case {
  std::string id, mode_requested, mode_effective, route = "fixed";
  int frequency = 12, first_period = 1, n = 0;
  int trend_length = 0, backcast = 0, forecast = 0;
  double lower = 1.5, upper = 2.5;
  std::vector<kalos::X11SeasonalRequest> seasonal;
  bool log = false;
  std::vector<double> input;
  std::vector<kalos::X13AllocationVariable> vars;
  std::map<std::string, std::vector<double>> tables;
};
void sweep(const std::string& dir, long& checks, long& bad, double& worst) {
  const Csv cases_csv = read_csv(dir + "/cases.csv");
  std::vector<Case> cases;
  std::map<std::string, std::size_t> index;
  for (const auto& r : cases_csv.rows) {
    Case c;
    c.id = unquote(cases_csv.at(r, "case"));
    c.frequency = parse_int(cases_csv.at(r, "frequency"));
    c.first_period = parse_int(cases_csv.at(r, "first_period"));
    c.n = parse_int(cases_csv.at(r, "n"));
    c.log = parse_bool(cases_csv.at(r, "log"));
    c.mode_requested = unquote(cases_csv.at(r, "mode_requested"));
    c.mode_effective = unquote(cases_csv.at(r, "mode_effective"));
    if (cases_csv.has("route")) c.route = unquote(cases_csv.at(r, "route"));
    if (cases_csv.has("henderson")) c.trend_length = parse_int(cases_csv.at(r, "henderson"));
    if (cases_csv.has("lower")) c.lower = parse_double(cases_csv.at(r, "lower"));
    if (cases_csv.has("upper")) c.upper = parse_double(cases_csv.at(r, "upper"));
    if (cases_csv.has("backcast")) c.backcast = parse_int(cases_csv.at(r, "backcast"));
    if (cases_csv.has("forecast")) c.forecast = parse_int(cases_csv.at(r, "forecast"));
    const std::string filters = cases_csv.has("filters")
        ? unquote(cases_csv.at(r, "filters")) : "";
    std::size_t p = 0;
    while (p <= filters.size()) {
      const std::size_t comma = filters.find(',', p);
      const std::string tok = filters.substr(p,
          comma == std::string::npos ? std::string::npos : comma - p);
      if (!tok.empty()) c.seasonal.push_back(filter_of(std::stoi(tok)));
      if (comma == std::string::npos) break;
      p = comma + 1;
    }
    if (c.seasonal.empty())
      c.seasonal.assign(static_cast<std::size_t>(c.frequency),
                        kalos::X11SeasonalRequest::s3x9);
    if (c.trend_length == 0) c.trend_length = 5;
    index[c.id] = cases.size();
    cases.push_back(c);
  }
  auto case_index = [&](const std::string& id) -> std::size_t {
    const auto it = index.find(id);
    if (it == index.end()) throw std::runtime_error("unknown case " + id);
    return it->second;
  };
  {
    const Csv csv = read_csv(dir + "/inputs.csv");
    for (const auto& r : csv.rows)
      cases[case_index(unquote(r[0]))].input.push_back(parse_double(r[2]));
  }
  {
    const Csv csv = read_csv(dir + "/vars.csv");
    std::map<std::size_t, std::map<std::size_t, std::vector<std::string>>> var_lines;
    std::map<std::size_t, std::map<std::size_t, std::pair<std::string, std::string>>> var_meta;
    for (const auto& r : csv.rows) {
      const std::size_t ci = case_index(unquote(r[0]));
      const std::size_t vi = static_cast<std::size_t>(parse_int(r[1]));
      var_meta[ci][vi] = {unquote(csv.at(r, "class")), unquote(csv.at(r, "effect"))};
      var_lines[ci][vi].push_back(unquote(csv.at(r, "coefficient")));
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
  }
  {
    const Csv csv = read_csv(dir + "/columns.csv");
    std::map<std::size_t, std::vector<double>> columns;
    for (const auto& r : csv.rows) {
      const std::size_t ci = case_index(unquote(r[0]));
      const std::size_t vi = static_cast<std::size_t>(parse_int(r[1]));
      const std::size_t cj = static_cast<std::size_t>(parse_int(r[2]));
      columns[(ci * 1000 + vi) * 100 + cj].push_back(parse_double(r[4]));
    }
    for (std::size_t ci = 0; ci < cases.size(); ++ci)
      for (std::size_t vi = 0; vi < cases[ci].vars.size(); ++vi)
        for (std::size_t cj = 0; cj < cases[ci].vars[vi].coefficient.size(); ++cj)
          cases[ci].vars[vi].columns.push_back(columns[(ci * 1000 + vi) * 100 + cj]);
  }
  {
    const Csv csv = read_csv(dir + "/tables.csv");
    for (const auto& r : csv.rows)
      cases[case_index(unquote(r[0]))].tables[unquote(r[1])].push_back(parse_double(r[3]));
  }
  for (const Case& c : cases) {
    std::printf("case %s (%s%s)\n", c.id.c_str(), c.mode_requested.c_str(),
        c.route == "arima" ? ", horizon evidence" : "");
    kalos::X13FitRequest rq;
    rq.input = c.input;
    rq.variables = c.vars;
    rq.log = c.log;
    rq.frequency = c.frequency;
    rq.first_period = c.first_period;
    rq.seasonal = c.seasonal;
    rq.trend_length = c.trend_length;
    rq.sigma = kalos::X11SigmaPolicy::legacy;
    rq.lower_sigma = c.lower;
    rq.upper_sigma = c.upper;
    rq.mode = mode_of(c.mode_requested);
    rq.backcast = c.backcast;
    rq.forecast = c.forecast;
    if (c.route == "arima") {
      // The pinned route evidence: the fit must refuse exactly where the
      // pinned engine leaves the fixed path.
      ++checks;
      const kalos::X13FitResult fit = kalos::x13_fit(rq);
      if (fit.status != "unsupported:x13 fit requires zero horizons") {
        std::printf("  horizon boundary: %s\n", fit.status.c_str());
        ++bad;
      }
      continue;
    }
    const kalos::X13FitResult fit = kalos::x13_fit(rq);
    ++checks;
    if (fit.status != "available") {
      std::printf("  status %s\n", fit.status.c_str());
      ++bad;
      continue;
    }
    ++checks;
    if (fit.mode_effective != mode_of(c.mode_effective)) {
      std::printf("  mode_effective mismatch\n");
      ++bad;
    }
    const std::pair<const char*, const std::vector<double>*> pre_tables[13] = {
        {"a1", &fit.preadjustment.a1}, {"a6", &fit.preadjustment.a6},
        {"a7", &fit.preadjustment.a7}, {"a8", &fit.preadjustment.a8},
        {"a8t", &fit.preadjustment.a8t}, {"a8s", &fit.preadjustment.a8s},
        {"a8i", &fit.preadjustment.a8i}, {"a9", &fit.preadjustment.a9},
        {"a9u", &fit.preadjustment.a9u}, {"a9cal", &fit.preadjustment.a9cal},
        {"a9sa", &fit.preadjustment.a9sa}, {"a9ser", &fit.preadjustment.a9ser},
        {"out", &fit.preadjustment.out}};
    for (const auto& t : pre_tables) {
      // Under a log model every bucket and out crosses backTransform's exp
      // (Java's Math.exp and the native libm can flip a final ulp); only
      // a1 is pure algebra.
      const bool boundary = c.log && std::string(t.first) != "a1";
      if (boundary)
        expect_close(*t.second, c.tables.at(t.first), t.first, 1e-15, worst, checks, bad);
      else
        expect_equal(*t.second, c.tables.at(t.first), t.first, checks, bad);
    }
    const bool la = fit.mode_effective == kalos::X11DecompositionMode::log_additive;
    const std::pair<const char*, const std::vector<double>*> consumed[4] = {
        {"d10", &fit.d10}, {"d11", &fit.d11}, {"d12", &fit.d12}, {"d13", &fit.d13}};
    for (const auto& t : consumed)
      if (la || c.log)
        expect_close(*t.second, c.tables.at(t.first), t.first, 1e-12, worst, checks, bad);
      else
        expect_equal(*t.second, c.tables.at(t.first), t.first, checks, bad);
    if (c.log)
      expect_close(fit.c17, c.tables.at("c17"), "c17", 1e-12, worst, checks, bad);
    else
      expect_equal(fit.c17, c.tables.at("c17"), "c17", checks, bad);
    const std::pair<const char*, const std::vector<double>*> finals[9] = {
        {"d11final", &fit.finals.d11final}, {"d12final", &fit.finals.d12final},
        {"d13final", &fit.finals.d13final}, {"d16", &fit.finals.d16},
        {"d18", &fit.finals.d18}, {"e1", &fit.finals.e1}, {"e2", &fit.finals.e2},
        {"e3", &fit.finals.e3}, {"e11", &fit.finals.e11}};
    for (const auto& t : finals)
      if (la || c.log)
        expect_close(*t.second, c.tables.at(t.first), t.first, 1e-9, worst, checks, bad);
      else
        expect_equal(*t.second, c.tables.at(t.first), t.first, checks, bad);
    // The final-components triplet identity (same bytes).
    expect_equal(fit.finalSA, fit.finals.d11final, "finalSA", checks, bad);
    expect_equal(fit.finalTrend, fit.finals.d12final, "finalTrend", checks, bad);
    expect_equal(fit.finalIrregular, fit.finals.d13final, "finalIrregular", checks, bad);
    // The R x13_fast guard corpus, when present: the f_empty tables as the
    // R fast result path returned them must equal the fit as well.
    std::ifstream probe(dir + "/x13fast.csv");
    if (probe.good()) {
      probe.close();
      const Csv csv = read_csv(dir + "/x13fast.csv");
      std::map<std::string, std::vector<double>> fast;
      for (const auto& r : csv.rows)
        fast[unquote(r[0]) + "/" + unquote(r[1])].push_back(parse_double(r[3]));
      const std::map<std::string, const std::vector<double>*> look = {
          {"d11final", &fit.finalSA}, {"d12final", &fit.finalTrend},
          {"d13final", &fit.finalIrregular}, {"d16", &fit.finals.d16},
          {"d18", &fit.finals.d18}, {"e1", &fit.finals.e1}, {"e2", &fit.finals.e2},
          {"e3", &fit.finals.e3}, {"e11", &fit.finals.e11}, {"d10", &fit.d10},
          {"d11", &fit.d11}, {"d12", &fit.d12}, {"d13", &fit.d13},
          {"a1", &fit.preadjustment.a1}};
      for (const auto& e : look) {
        const auto it = fast.find(c.id + "/" + e.first);
        if (it != fast.end()) expect_equal(*e.second, it->second, "x13fast " + e.first, checks, bad);
      }
    }
  }
}
} // namespace

int main(int argc, char** argv) {
  long checks = 0, bad = 0;
  double worst = 0.;
  std::vector<std::string> dirs;
  for (int i = 1; i < argc; ++i) dirs.emplace_back(argv[i]);
  if (dirs.empty())
    dirs = {"validation/evidence/native/x13-allocation",
            "validation/evidence/native/x13-fit"};
  for (const auto& dir : dirs) sweep(dir, checks, bad, worst);
  std::printf("%ld checks, %ld failures, transcendental-boundary worst %.3g\n",
      checks, bad, worst);
  return bad == 0 ? 0 : 1;
}
