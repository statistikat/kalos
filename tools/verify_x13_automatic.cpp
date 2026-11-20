// Full-sweep verification: the native automatic-spec pipeline driver (item
// 21-H, x13_automatic_fit) against the JVM oracle corpus captured by
// validation/reference/capture-x13-automatic.R (evidence at
// validation/evidence/native/x13-automatic; an alternative corpus directory
// may be passed as the first argument, optional further arguments filter by
// case id).
//
// The oracle is the REAL decoder-shaped pipeline: the pinned X13Spec
// scalars mirrored into the request (ub1/ub2 are the exact JVM reciprocals,
// cancel/precision/grid/round scalars as pinned in the pinning notes
// SECTION 16), the request run through X13Kernel.process with a
// ProcessingLog observer, and the double-JVM byte-identical facts/events
// as truth.
//
// Tiers (docs/contracts/x13-automatic-pipeline-contract.md, slice-3 policy):
// every decision the observer exposes is asserted bit-exact - the final
// description (log/mean/orders/variables/mode), the differencing (d, bd,
// mean), the ODM selections (positions, types) and their cv, the ARMA
// selection orders, the merged-panel membership and the cascade final
// spec.  Landing quantities are measured tiers with recorded root causes:
// tmean at the 21-D qualified band (1e-3, trial-fit class), panel BIC2 at
// the 21-E kPanelBand (1e-5), the AICC pair at the observer's %.6g display
// precision, the landing ARMA/regression coefficients and statistics at
// the 2e-3 basin band (adapter row 4, never bit-claimed), and the
// downstream tables at the fit-sweep families widened by the landing drift
// propagation (measured, reported as the worst relative deviation).
// Diagnostics: KALOS_DUMP_ODM=1 prints the driver's per-round ODM picks,
// t-values and stop reasons beside the comparison lines.
#include <kalos/x13_automatic.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ---- CSV plumbing (the sweep-tool convention) ----------------------------

std::string unquote(const std::string& s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    return s.substr(1, s.size() - 2);
  return s;
}
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
double real(const std::string& raw) { return std::strtod(unquote(raw).c_str(), nullptr); }
int integer(const std::string& raw) { return std::stoi(unquote(raw)); }
bool boolean(const std::string& raw) { return unquote(raw) == "true"; }
std::vector<double> semicolon_reals(const std::string& raw) {
  std::vector<double> out;
  std::string s = unquote(raw);
  if (s.empty()) return out;
  std::stringstream parts(s);
  std::string item;
  while (std::getline(parts, item, ';')) out.push_back(std::strtod(item.c_str(), nullptr));
  return out;
}

// ---- evidence model -------------------------------------------------------

struct Ev {
  int seq = 0, depth = 0;
  std::string kind, msg, detail;
};
struct Case {
  std::string id, note, status;
  std::string mode_requested, mode_effective;
  int n = 0;
  bool log = false, mean = false;
  int p = 0, d = 0, q = 0, bp = 0, bd = 0, bq = 0;
  int nvars = 0, nevents = 0;
  std::map<std::string, std::string> input;              // inputs.csv lines
  std::vector<Ev> events;                                 // oracle-events.csv
  std::vector<std::pair<std::string, std::string>> facts; // oracle-facts.csv
  std::string fact(const std::string& key) const {
    for (const auto& f : facts)
      if (f.first == key) return f.second;
    return "";
  }
  std::vector<std::string> facts_of(const std::string& key) const {
    std::vector<std::string> out;
    for (const auto& f : facts)
      if (f.first == key) out.push_back(f.second);
    return out;
  }
};

// ---- family scoreboard ----------------------------------------------------

struct Family {
  long checks = 0, bad = 0, exact = 0;
  double worst = 0.;
  std::string worst_where;
};
std::map<std::string, Family> g_families;
long g_cases = 0, g_cases_failed = 0;
bool g_dump_odm = false;

void tally(const std::string& name, bool ok, bool bit, double dev,
           const std::string& where) {
  Family& f = g_families[name];
  ++f.checks;
  if (bit) ++f.exact;
  if (!ok) ++f.bad;
  if (dev > f.worst) { f.worst = dev; f.worst_where = where; }
}

bool bit_equal(double a, double b) {
  return std::memcmp(&a, &b, sizeof(double)) == 0;
}
bool same_nan(double a, double b) {
  return (std::isnan(a) && std::isnan(b)) || a == b;
}

void bit_d(const std::string& fam, double got, double want, const std::string& where) {
  const bool ok = bit_equal(got, want);
  tally(fam, ok, ok, 0., where);
  if (!ok)
    std::printf("  %s: BIT %s %.17g vs %.17g\n", where.c_str(), fam.c_str(), got, want);
}
void bit_i(const std::string& fam, long got, long want, const std::string& where) {
  const bool ok = got == want;
  tally(fam, ok, ok, 0., where);
  if (!ok) std::printf("  %s: BIT %s %ld vs %ld\n", where.c_str(), fam.c_str(), got, want);
}
void bit_s(const std::string& fam, const std::string& got, const std::string& want,
           const std::string& where) {
  const bool ok = got == want;
  tally(fam, ok, ok, 0., where);
  if (!ok) std::printf("  %s: BIT %s '%s' vs '%s'\n", where.c_str(), fam.c_str(),
                       got.c_str(), want.c_str());
}
// Relative deviation (absolute at want == 0), pass/fail against the band.
void close_d(const std::string& fam, double got, double want, double band,
             const std::string& where) {
  ++g_families[fam].checks;
  if (same_nan(got, want)) {
    tally(fam, true, bit_equal(got, want), 0., where);
    return;
  }
  const double rel = std::abs(want) > 0.
      ? std::abs(got - want) / std::abs(want) : std::abs(got - want);
  tally(fam, rel <= band, false, rel, where);
  if (rel > band)
    std::printf("  %s: TIER %s %.17g vs %.17g (rel %.3g > %.3g)\n",
                where.c_str(), fam.c_str(), got, want, rel, band);
}

// ---- decoders for the oracle payload vocabulary ---------------------------

struct OdmEvent {
  std::string types;
  std::vector<std::pair<int, int>> picks; // (position, code)
  double va = 0.;
};
OdmEvent parse_odm(const std::string& detail) {
  // "odm AO,LS,TC,SO [48;0][66;1] 3.8007..."
  OdmEvent out;
  std::string s = detail.substr(4);
  std::size_t k = s.find(' ');
  out.types = s.substr(0, k);
  std::size_t last = s.rfind(']');
  std::string tail = last == std::string::npos ? s : s.substr(last + 1);
  out.va = std::strtod(tail.c_str(), nullptr);
  std::size_t i = s.find('[');
  while (i != std::string::npos) {
    std::size_t j = s.find(']', i);
    if (j == std::string::npos) break;
    const std::string cell = s.substr(i + 1, j - i - 1);
    const std::size_t semi = cell.find(';');
    out.picks.emplace_back(std::stoi(cell.substr(0, semi)),
                           std::stoi(cell.substr(semi + 1)));
    i = s.find('[', j);
  }
  return out;
}
struct Orders4 { int a = 0, b = 0, c = 0, d = 0; };
Orders4 parse_pair4(const std::string& s, std::size_t at) {
  // "(a,b)(c,d)" starting at '('
  Orders4 o;
  if (s.size() < at + 11 || s[at] != '(')
    throw std::runtime_error("bad orders at " + s.substr(at, 16));
  if (std::sscanf(s.c_str() + at, "(%d,%d)(%d,%d)", &o.a, &o.b, &o.c, &o.d) != 4)
    throw std::runtime_error("bad orders " + s.substr(at, 11));
  return o;
}
struct ArmaEvent {
  Orders4 sel;
  std::vector<std::pair<Orders4, double>> panel; // spec, BIC2
};
ArmaEvent parse_arma(const std::string& detail) {
  // "arma sel=(a,b)(c,d) [spec:bic]..."
  ArmaEvent out;
  const std::size_t sel = detail.find("sel=");
  if (sel == std::string::npos) throw std::runtime_error("bad arma event " + detail);
  out.sel = parse_pair4(detail, sel + 4);
  std::size_t i = detail.find('[', sel);
  while (i != std::string::npos) {
    const std::size_t colon = detail.find(':', i);
    const std::size_t j = detail.find(']', i);
    if (colon == std::string::npos || j == std::string::npos) break;
    out.panel.emplace_back(parse_pair4(detail, i + 1),
                           std::strtod(detail.c_str() + colon + 1, nullptr));
    i = detail.find('[', j);
  }
  return out;
}
struct DiffEvent { int d = 0, bd = 0; bool mean = false; double tmean = 0.; };
DiffEvent parse_diff(const std::string& detail) {
  DiffEvent e;
  const bool m = detail.find("mean=true") != std::string::npos;
  if (std::sscanf(detail.c_str(), "diff d=%d bd=%d", &e.d, &e.bd) != 2)
    throw std::runtime_error("bad diff event " + detail);
  e.mean = m;
  const std::size_t t = detail.find("tmean=");
  if (t == std::string::npos) throw std::runtime_error("bad diff event " + detail);
  e.tmean = std::strtod(detail.c_str() + t + 6, nullptr);
  return e;
}
struct AiccEvent { double logs = 0., levels = 0., pref = 0.; };
AiccEvent parse_aicc(const std::string& detail) {
  AiccEvent e;
  const std::size_t l = detail.find("logs=");
  const std::size_t v = detail.find("levels=");
  const std::size_t pr = detail.find("preference)=");
  if (l == std::string::npos || v == std::string::npos || pr == std::string::npos)
    throw std::runtime_error("bad aicc event " + detail);
  e.logs = std::strtod(detail.c_str() + l + 5, nullptr);
  e.levels = std::strtod(detail.c_str() + v + 7, nullptr);
  e.pref = std::strtod(detail.c_str() + pr + 12, nullptr);
  return e;
}
// "1 AO (1994-01-01) x13 Irregular" / "0 const - -"
struct OracleVar {
  int index = 0;
  std::string name, ami, regeffect;
  std::vector<std::string> date; // empty for const
};
OracleVar parse_var(const std::string& row) {
  std::istringstream is(row);
  OracleVar v;
  is >> v.index;
  std::vector<std::string> tokens;
  std::string tok;
  while (is >> tok) tokens.push_back(tok);
  if (tokens.size() < 3) throw std::runtime_error("bad var row " + row);
  v.regeffect = tokens.back();
  v.ami = tokens[tokens.size() - 2];
  std::vector<std::string> name_tokens(tokens.begin(), tokens.end() - 2);
  for (std::size_t i = 0; i < name_tokens.size(); ++i) {
    if (i) v.name += " ";
    v.name += name_tokens[i];
  }
  const std::size_t open = v.name.find('(');
  if (open != std::string::npos) {
    std::string digits = v.name.substr(open + 1, v.name.find(')') - open - 1);
    std::replace(digits.begin(), digits.end(), '-', ' ');
    std::istringstream ds(digits);
    std::string t;
    while (ds >> t) v.date.push_back(t);
  }
  return v;
}

// ---- request mirror -------------------------------------------------------

// The decoder-pinned module scalars (pinning notes SECTION 16; the probe
// echoes every case-level scalar in its req fact, mirrored below).
kalos::X11DecompositionMode mode_of(const std::string& s) {
  using M = kalos::X11DecompositionMode;
  if (s == "Undefined") return M::additive;
  if (s == "Multiplicative") return M::multiplicative;
  if (s == "LogAdditive") return M::log_additive;
  if (s == "PseudoAdditive") return M::pseudo_additive;
  throw std::runtime_error("unknown mode " + s);
}
std::string mode_name(kalos::X11DecompositionMode m) {
  using M = kalos::X11DecompositionMode;
  switch (m) {
    case M::additive: return "Additive";
    case M::multiplicative: return "Multiplicative";
    case M::log_additive: return "LogAdditive";
    case M::pseudo_additive: return "PseudoAdditive";
  }
  return "?";
}
kalos::OutlierType type_of(const std::string& s) {
  if (s == "AO") return kalos::OutlierType::Additive;
  if (s == "LS") return kalos::OutlierType::LevelShift;
  if (s == "TC") return kalos::OutlierType::TransitoryChange;
  if (s == "SO") return kalos::OutlierType::SeasonalOutlier;
  throw std::runtime_error("unknown outlier type " + s);
}
const char* type_code(kalos::OutlierType t) {
  switch (t) {
    case kalos::OutlierType::Additive: return "AO";
    case kalos::OutlierType::LevelShift: return "LS";
    case kalos::OutlierType::TransitoryChange: return "TC";
    case kalos::OutlierType::SeasonalOutlier: return "SO";
  }
  return "?";
}

kalos::X13AutomaticRequest mirror_request(const Case& c) {
  kalos::X13AutomaticRequest r;
  const auto& in = c.input;
  // The req fact echoes every scalar the pinned spec held as key/value
  // pairs with single-token values; the inputs.csv lines carry only the
  // case deltas (values, and start/outliers/mode when varied).
  std::map<std::string, std::string> req;
  {
    std::istringstream is(c.fact("req"));
    std::string key, val;
    while (is >> key >> val) req[key] = val;
  }
  auto need = [&](const char* key) -> std::string {
    auto it = req.find(key);
    if (it == req.end())
      throw std::runtime_error(std::string("missing req key ") + key +
                               " for case " + c.id);
    return it->second;
  };
  {
    std::string start = need("start");
    auto it = in.find("start");
    if (it != in.end()) start = it->second;
    const std::size_t colon = start.find(':');
    r.first_year = std::stoi(start.substr(0, colon));
    r.first_period = std::stoi(start.substr(colon + 1));
  }
  r.input = semicolon_reals(in.count("values") ? in.at("values") : "");
  if (static_cast<int>(r.input.size()) != c.n)
    throw std::runtime_error("values length disagrees with cases.csv for " + c.id);
  r.frequency = 12;
  r.aicc_log_correction = real(need("aicdiff"));
  // DifferencingModule: the spec ub scalars arrive as the exact JVM
  // reciprocals (the decoder stores 1/spec into the module).
  r.max_d = 2;
  r.max_bd = 1;
  r.ub1 = 1. / real(need("ub1"));
  r.ub2 = 1. / real(need("ub2"));
  r.cancel = real(need("cancel"));
  r.differencing_precision = 1e-7;
  // ArmaModule (AutoModelSpec DEF_*).
  r.models_count = 5;
  r.max_p = 2;
  r.max_q = 2;
  r.max_bp = 1;
  r.max_bq = 1;
  r.balanced = need("balanced") == "true";
  r.mixed = need("mixed") == "true";
  r.arma_accept_white_noise = false;
  r.arma_eps = 1e-7;
  // OutliersDetectionModule (OutlierSpec; maxIter carries max round).
  r.outliers_used = need("outliers") == "on";
  {
    std::stringstream types(need("otypes"));
    std::string t;
    while (std::getline(types, t, ',')) {
      if (!t.empty()) r.outlier_types.push_back(type_of(t));
    }
    std::sort(r.outlier_types.begin(), r.outlier_types.end(),
              [](kalos::OutlierType a, kalos::OutlierType b) {
                return static_cast<int>(a) < static_cast<int>(b);
              });
  }
  r.outlier_max_round = integer(need("maxiter"));
  r.outlier_max_outliers = 30;
  r.tcrate = real(need("tcrate"));
  // AmiOptions / kernel (DEF_PREDCV and DEF_LJUNGBOX ride as predcv/lblimit).
  r.precision = 1e-7;
  r.mean_precision = 1e-5;
  r.va = 0.;
  r.reduce_va = real(need("predcv"));
  r.ljung_box_limit = real(need("lblimit"));
  r.check_mu = true;
  r.accept_airline = false;
  r.ur_limit = 0.95;
  r.mean_test_initial = 1.96;
  r.mean_test_final = 1.0;
  r.tsig = 1.0;
  // The decoder X-11 block (filters [Msr], Henderson 0, sigma periodic =
  // the qualified accurate policy, calendarSigma None, horizons {0, 0}).
  r.seasonal.assign(12, kalos::X11SeasonalRequest::msr);
  r.trend_length = 0;
  r.sigma = kalos::X11SigmaPolicy::accurate;
  {
    const std::string sigma = need("sigma");
    const std::size_t semi = sigma.find(';');
    r.lower_sigma = std::strtod(sigma.substr(0, semi).c_str(), nullptr);
    r.upper_sigma = std::strtod(sigma.substr(semi + 1).c_str(), nullptr);
  }
  r.mode = mode_of(need("mode"));
  r.backcast = 0;
  r.forecast = 0;
  return r;
}

// ---- driver-side extraction helpers ---------------------------------------

const kalos::AutoDecisionRecord* stage_entry(
    const std::vector<kalos::AutoDecisionRecord>& chain, const std::string& stage,
    std::size_t occurrence) {
  std::size_t seen = 0;
  for (const auto& e : chain) {
    if (e.stage == stage && seen++ == occurrence) return &e;
  }
  return nullptr;
}
std::vector<const kalos::AutoDecisionRecord*> stages(
    const std::vector<kalos::AutoDecisionRecord>& chain,
    const std::string& prefix) {
  std::vector<const kalos::AutoDecisionRecord*> out;
  for (const auto& e : chain) {
    if (e.stage.rfind(prefix, 0) == 0) out.push_back(&e);
  }
  return out;
}
double entry_scalar(const kalos::AutoDecisionRecord& e, const char* key) {
  for (const auto& s : e.scalars)
    if (s.first == key) return s.second;
  throw std::runtime_error("missing scalar " + std::string(key) + " on " + e.stage);
}

// ---- the per-case sweep ----------------------------------------------------

// Bands (contract Envelope; measured worst deviations go to the contract).
constexpr double kTmeanBand = 1e-3;     // 21-D trial-fit-class band
constexpr double kPanelBicBand = 1e-5;  // 21-E kPanelBand
constexpr double kDisplayBand = 1e-5;   // observer %.6g AICC display
constexpr double kBasinBand = 2e-3;     // adapter row-4 landing coefficients
constexpr double kTableBand = 1e-9;     // downstream tables (measured)

void sweep_case(const Case& c) {
  ++g_cases;
  const std::string id = c.id;
  kalos::X13AutomaticRequest request;
  try {
    request = mirror_request(c);
  } catch (const std::exception& e) {
    std::printf("  %s: mirror failed: %s\n", id.c_str(), e.what());
    ++g_cases_failed;
    return;
  }
  const kalos::X13AutomaticFitResult result = kalos::x13_automatic_fit(request);

  // ---- status and final description: bit ----------------------------------
  const std::string where = id;
  bit_s("model-facts", result.status, "available", where + "/status");
  if (result.status != "available") {
    std::printf("  %s: status %s\n", id.c_str(), result.status.c_str());
    ++g_cases_failed;
    return;
  }
  bit_i("model-facts", result.log ? 1 : 0, c.log ? 1 : 0, where + "/log");
  bit_i("model-facts", result.mean ? 1 : 0, c.mean ? 1 : 0, where + "/mean");
  bit_i("model-facts", result.p, c.p, where + "/p");
  bit_i("model-facts", result.d, c.d, where + "/d");
  bit_i("model-facts", result.q, c.q, where + "/q");
  bit_i("model-facts", result.bp, c.bp, where + "/bp");
  bit_i("model-facts", result.bd, c.bd, where + "/bd");
  bit_i("model-facts", result.bq, c.bq, where + "/bq");
  bit_s("model-facts", mode_name(result.fit.mode_effective), c.mode_effective,
        where + "/mode_effective");

  // ---- variables: names/types/positions/regeffect bit, coefficients tier --
  std::vector<OracleVar> oracle_vars;
  for (const auto& row : c.facts_of("var")) oracle_vars.push_back(parse_var(row));
  const std::size_t ami_count = static_cast<std::size_t>(
      std::count_if(oracle_vars.begin(), oracle_vars.end(),
                    [](const OracleVar& v) { return v.ami == "x13"; }));
  bit_i("model-facts", static_cast<long>(result.variables.size()),
        static_cast<long>(ami_count), where + "/nvars");
  {
    // The JVM var list follows its design insertion order; the driver keeps
    // its own. Compare as (position, type) sorted sequences and pair the
    // regcoef slots through the JVM row index (const occupies row 0 when
    // the mean is on).
    std::vector<OracleVar> ami;
    for (const auto& v : oracle_vars) {
      if (v.ami != "x13") {
        bit_s("model-facts", v.ami, "-", where + "/var-nonami");
        continue;
      }
      ami.push_back(v);
    }
    auto var_pos = [&](const OracleVar& v) {
      return v.date.size() == 3
                 ? (std::stoi(v.date[0]) - request.first_year) * 12 +
                       (std::stoi(v.date[1]) - request.first_period)
                 : -1;
    };
    std::vector<std::pair<int, int>> want_key;
    for (const auto& v : ami)
      want_key.emplace_back(var_pos(v), static_cast<int>(type_of(v.name.substr(0, 2))));
    std::sort(want_key.begin(), want_key.end());
    std::stable_sort(ami.begin(), ami.end(),
                     [&](const OracleVar& a, const OracleVar& b) {
                       return var_pos(a) < var_pos(b);
                     });
    std::vector<kalos::AutoVariableRecord> got_sorted = result.variables;
    std::stable_sort(got_sorted.begin(), got_sorted.end(),
                     [](const kalos::AutoVariableRecord& a,
                        const kalos::AutoVariableRecord& b) {
                       return std::make_pair(a.position, static_cast<int>(a.type)) <
                              std::make_pair(b.position, static_cast<int>(b.type));
                     });
    std::vector<std::pair<int, int>> got_key;
    for (const auto& got : got_sorted)
      got_key.emplace_back(static_cast<int>(got.position),
                           static_cast<int>(got.type));
    for (std::size_t i = 0; i < got_key.size() && i < want_key.size(); ++i) {
      bit_i("variables", got_key[i].first, want_key[i].first,
            where + "/var" + std::to_string(i) + "/position");
      bit_i("variables", got_key[i].second, want_key[i].second,
            where + "/var" + std::to_string(i) + "/type");
    }
    for (std::size_t i = 0; i < got_sorted.size() && i < ami.size(); ++i) {
      const std::string tag = where + "/var" + std::to_string(i);
      bit_s("variables", got_sorted[i].name, ami[i].name, tag + "/name");
      bit_s("variables", got_sorted[i].regeffect,
            ami[i].regeffect == "-" ? "" : ami[i].regeffect, tag + "/regeffect");
    }
    // regcoef: the pinned coefficient vector on the final design, in JVM
    // variable-row order (row index carries the const slot). The const
    // coefficient is a landing quantity the driver does not surface.
    const std::vector<double> reg = semicolon_reals(c.fact("regcoef"));
    for (std::size_t i = 0; i < got_sorted.size() && i < ami.size(); ++i) {
      const long slot = ami[i].index;
      if (slot >= 0 && static_cast<std::size_t>(slot) < reg.size())
        close_d("landing-coefficients", got_sorted[i].coefficient, reg[slot],
                kBasinBand, where + "/regcoef" + std::to_string(i));
    }
    // Landing ARMA blocks [AR, MA, SAR, SMA].
    std::vector<double> flat;
    for (const auto& block : result.coefficients)
      flat.insert(flat.end(), block.begin(), block.end());
    const std::vector<double> want = semicolon_reals(c.fact("arma"));
    bit_i("model-facts", static_cast<long>(flat.size()),
          static_cast<long>(want.size()), where + "/narma");
    for (std::size_t i = 0; i < flat.size() && i < want.size(); ++i)
      close_d("landing-coefficients", flat[i], want[i], kBasinBand,
              where + "/arma" + std::to_string(i));
  }

  // ---- landing statistics ---------------------------------------------------
  {
    const std::string stats = c.fact("stats");
    long nobs = 0, neff = 0, npar = 0;
    double loglik = 0., aicc = 0.;
    if (std::sscanf(stats.c_str(), "nobs=%ld neff=%ld npar=%ld loglik=%lf aicc=%lf",
                    &nobs, &neff, &npar, &loglik, &aicc) != 5)
      throw std::runtime_error("bad stats fact for " + id);
    bit_i("landing-stats", static_cast<long>(result.effective_observations), neff,
          where + "/neff");
    close_d("landing-stats", result.log_likelihood, loglik, kBasinBand,
            where + "/loglik");
    close_d("landing-stats", result.aicc, aicc, kBasinBand, where + "/aicc");
    (void)nobs; (void)npar;
  }

  // ---- decision-chain projection -----------------------------------------
  const auto& chain = result.chain;

  // log-level test: exactly one AICC event, the %.6g display pair plus the
  // pinned preference scalar.
  {
    std::vector<const Ev*> aicc_events;
    for (const auto& e : c.events)
      if (e.detail.rfind("obj AICC on logs=", 0) == 0) aicc_events.push_back(&e);
    bit_i("chain-counts", static_cast<long>(aicc_events.size()), 1, where + "/aicc-count");
    const auto* entry = stage_entry(chain, "transformation", 0);
    bit_i("chain-counts", entry ? 1 : 0, 1, where + "/transformation-entry");
    if (!aicc_events.empty() && entry != nullptr && entry->transformation) {
      const AiccEvent want = parse_aicc(aicc_events[0]->detail);
      const kalos::TransformationDecisionResult& t = *entry->transformation;
      close_d("display-aicc", t.aicc_log, want.logs, kDisplayBand, where + "/aicc_log");
      close_d("display-aicc", t.aicc_level, want.levels, kDisplayBand,
              where + "/aicc_level");
      bit_d("model-facts", t.log_chosen ? 1. : 0., c.log ? 1. : 0.,
            where + "/logpref");
      bit_d("chain-scalars", want.pref, request.aicc_log_correction,
            where + "/aicc-pref");
    }
  }

  // differencing events -> refine entries, in order.
  {
    std::vector<const Ev*> diffs, armas;
    for (const auto& e : c.events) {
      if (e.detail.rfind("diff d=", 0) == 0) diffs.push_back(&e);
      if (e.detail.rfind("arma sel=", 0) == 0) armas.push_back(&e);
    }
    std::vector<const kalos::AutoDecisionRecord*> refines;
    for (const auto& e : chain)
      if (e.stage.size() > 7 &&
          e.stage.compare(e.stage.size() - 7, 7, ".refine") == 0)
        refines.push_back(&e);
    bit_i("chain-counts", static_cast<long>(diffs.size()),
          static_cast<long>(refines.size()), where + "/diff-count");
    bit_i("chain-counts", static_cast<long>(armas.size()),
          static_cast<long>(refines.size()), where + "/arma-count");
    for (std::size_t i = 0; i < diffs.size() && i < refines.size(); ++i) {
      const DiffEvent want = parse_diff(diffs[i]->detail);
      const std::string tag = where + "/diff" + std::to_string(i);
      if (!refines[i]->refine || !refines[i]->refine->differencing) {
        bit_i("diff-decisions", -1, 0, tag + "/payload");
        continue;
      }
      const kalos::DifferencingDecision& got = *refines[i]->refine->differencing;
      bit_i("diff-decisions", got.d, want.d, tag + "/d");
      bit_i("diff-decisions", got.seasonal_d, want.bd, tag + "/bd");
      bit_i("diff-decisions", got.mean_correction ? 1 : 0, want.mean ? 1 : 0,
            tag + "/mean");
      close_d("diff-tmean", got.t_mean, want.tmean, kTmeanBand, tag + "/tmean");
    }
    for (std::size_t i = 0; i < armas.size() && i < refines.size(); ++i) {
      const ArmaEvent want = parse_arma(armas[i]->detail);
      const std::string tag = where + "/arma" + std::to_string(i);
      if (!refines[i]->refine) {
        bit_i("arma-decisions", -1, 0, tag + "/payload");
        continue;
      }
      const kalos::ArmaRefinement& got = *refines[i]->refine;
      // The selected orders: the toString pair is (p,q)(bp,bq).
      bit_i("arma-decisions", got.p, want.sel.a, tag + "/sel-p");
      bit_i("arma-decisions", got.q, want.sel.b, tag + "/sel-q");
      bit_i("arma-decisions", got.bp, want.sel.c, tag + "/sel-bp");
      bit_i("arma-decisions", got.bq, want.sel.d, tag + "/sel-bq");
      // The merged preference panel: membership, order and BIC2.
      bit_i("arma-panel", static_cast<long>(got.preferred_models.size()),
            static_cast<long>(want.panel.size()), tag + "/panel-size");
      for (std::size_t j = 0; j < want.panel.size() && j < got.preferred_models.size();
           ++j) {
        const std::string ptag = tag + "/panel" + std::to_string(j);
        bit_i("arma-panel", got.preferred_models[j].p, want.panel[j].first.a,
              ptag + "/p");
        bit_i("arma-panel", got.preferred_models[j].q, want.panel[j].first.b,
              ptag + "/q");
        bit_i("arma-panel", got.preferred_models[j].bp, want.panel[j].first.c,
              ptag + "/bp");
        bit_i("arma-panel", got.preferred_models[j].bq, want.panel[j].first.d,
              ptag + "/bq");
        close_d("arma-panel", got.preferred_models[j].bic, want.panel[j].second,
                kPanelBicBand, ptag + "/bic");
      }
    }
  }

  // ODM events -> outlier entries, in order (round 0 first).
  {
    std::vector<const Ev*> odms;
    for (const auto& e : c.events)
      if (e.detail.rfind("odm ", 0) == 0) odms.push_back(&e);
    std::vector<const kalos::AutoDecisionRecord*> entries;
    for (std::size_t o = 0;; ++o) {
      const auto* e = stage_entry(chain, "outliers0", o);
      if (e == nullptr) break;
      entries.push_back(e);
    }
    for (int round = 1;; ++round) {
      const std::string stage = "round" + std::to_string(round) + ".outliers";
      bool any = false;
      for (std::size_t o = 0;; ++o) {
        const auto* e = stage_entry(chain, stage, o);
        if (e == nullptr) break;
        entries.push_back(e);
        any = true;
      }
      if (!any) break;
    }
    bit_i("chain-counts", static_cast<long>(odms.size()),
          static_cast<long>(entries.size()), where + "/odm-count");
    for (std::size_t i = 0; i < odms.size() && i < entries.size(); ++i) {
      const OdmEvent want = parse_odm(odms[i]->detail);
      const std::string tag = where + "/odm" + std::to_string(i);
      bit_s("odm-decisions", want.types, [&] {
        std::string s;
        for (std::size_t t = 0; t < request.outlier_types.size(); ++t) {
          if (t) s += ",";
          s += type_code(request.outlier_types[t]);
        }
        return s;
      }(), tag + "/types");
      if (!entries[i]->outliers) {
        bit_i("odm-decisions", -1, 0, tag + "/payload");
        continue;
      }
      std::vector<std::pair<int, int>> got;
      for (const auto& o : entries[i]->outliers->outliers) {
        got.emplace_back(static_cast<int>(o.position),
                         static_cast<int>(o.type));
        if (g_dump_odm)
          std::printf("  [driver-odm] %s pos=%d type=%d t=%.17g\n", tag.c_str(),
                      static_cast<int>(o.position), static_cast<int>(o.type),
                      o.t_statistic);
      }
      if (g_dump_odm)
        std::printf("  [driver-odm] %s stop=%s rounds=%zu changed=%d\n",
                    tag.c_str(), entries[i]->outliers->stop_reason.c_str(),
                    entries[i]->outliers->rounds,
                    entries[i]->scalars.empty() ? -1
                        : (int)entry_scalar(*entries[i], "changed"));
      std::sort(got.begin(), got.end());
      std::vector<std::pair<int, int>> want_sorted = want.picks;
      std::sort(want_sorted.begin(), want_sorted.end());
      bit_i("odm-decisions", static_cast<long>(got.size()),
            static_cast<long>(want_sorted.size()), tag + "/count");
      for (std::size_t j = 0; j < got.size() && j < want_sorted.size(); ++j) {
        bit_i("odm-decisions", got[j].first, want_sorted[j].first, tag + "/pos");
        bit_i("odm-decisions", got[j].second, want_sorted[j].second, tag + "/type");
      }
      // The cv in the pinned Info is the module's incoming threshold: the
      // driver's entry scalar. Bit-identical arithmetic on both sides.
      bit_d("odm-decisions", entry_scalar(*entries[i], "cv"), want.va, tag + "/cv");
    }
  }

  // full-estimation pushes -> cascade entries; the reduced-spec remark, if
  // the observer shows one, must equal the final orders.
  {
    long pushes = 0;
    for (const auto& e : c.events)
      if (e.kind == "push" && e.msg == "full estimation") ++pushes;
    long cascades = 0;
    for (const auto& e : chain) {
      if (e.stage.size() > 8 &&
          e.stage.compare(e.stage.size() - 8, 8, ".cascade") == 0)
        ++cascades;
    }
    bit_i("chain-counts", pushes, cascades, where + "/cascade-count");
    std::string remark;
    bool in_full = false;
    for (const auto& e : c.events) {
      if (e.kind == "push" && e.msg == "full estimation") in_full = true;
      if (e.kind == "pop" && in_full && e.depth == 2) in_full = false;
      if (in_full && e.kind == "info" && e.detail.rfind("obj (", 0) == 0)
        remark = e.detail;
    }
    if (!remark.empty()) {
      const std::size_t open = remark.find('(');
      int sp = 0, sd = 0, sq = 0, sbp = 0, sbd = 0, sbq = 0;
      if (std::sscanf(remark.c_str() + open, "(%d,%d,%d)(%d,%d,%d)", &sp, &sd, &sq,
                      &sbp, &sbd, &sbq) != 6)
        throw std::runtime_error("bad cascade remark " + remark);
      bit_i("model-facts", result.p, sp, where + "/cascade-p");
      bit_i("model-facts", result.d, sd, where + "/cascade-d");
      bit_i("model-facts", result.q, sq, where + "/cascade-q");
      bit_i("model-facts", result.bp, sbp, where + "/cascade-bp");
      bit_i("model-facts", result.bd, sbd, where + "/cascade-bd");
      bit_i("model-facts", result.bq, sbq, where + "/cascade-bq");
    }
  }

  // ---- downstream composition tables (fit-sweep families, drift widened) --
  {
    const std::pair<const char*, const std::vector<double>*> tables[] = {
        {"a1", &result.fit.preadjustment.a1}, {"a6", &result.fit.preadjustment.a6},
        {"a7", &result.fit.preadjustment.a7}, {"a8", &result.fit.preadjustment.a8},
        {"a8t", &result.fit.preadjustment.a8t}, {"a8s", &result.fit.preadjustment.a8s},
        {"a8i", &result.fit.preadjustment.a8i}, {"a9", &result.fit.preadjustment.a9},
        {"a9u", &result.fit.preadjustment.a9u},
        {"a9cal", &result.fit.preadjustment.a9cal},
        {"a9sa", &result.fit.preadjustment.a9sa},
        {"a9ser", &result.fit.preadjustment.a9ser},
        {"out", &result.fit.preadjustment.out},
        {"d10", &result.fit.d10}, {"d11", &result.fit.d11},
        {"d12", &result.fit.d12}, {"d13", &result.fit.d13},
        {"c17", &result.fit.c17},
        {"d11final", &result.fit.finals.d11final},
        {"d12final", &result.fit.finals.d12final},
        {"d13final", &result.fit.finals.d13final},
        {"d16", &result.fit.finals.d16}, {"d18", &result.fit.finals.d18},
        {"e1", &result.fit.finals.e1}, {"e2", &result.fit.finals.e2},
        {"e3", &result.fit.finals.e3}, {"e11", &result.fit.finals.e11}};
    for (const auto& t : tables) {
      const std::vector<std::string> rows = c.facts_of(t.first);
      if (rows.empty()) continue;
      const std::vector<double> want = semicolon_reals(rows[0]);
      const std::vector<double>& got = *t.second;
      bit_i("tables", static_cast<long>(got.size()), static_cast<long>(want.size()),
            where + "/" + t.first + "/length");
      long bit_hits = 0;
      for (std::size_t i = 0; i < got.size() && i < want.size(); ++i) {
        if (bit_equal(got[i], want[i])) ++bit_hits;
        close_d("tables", got[i], want[i], kTableBand,
                where + "/" + t.first + "[" + std::to_string(i) + "]");
      }
      if (bit_hits == static_cast<long>(got.size()) && !got.empty())
        std::printf("  %s/%s: bit-identical\n", id.c_str(), t.first);
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (std::getenv("KALOS_DUMP_ODM") != nullptr) g_dump_odm = true;
  std::string dir = "validation/evidence/native/x13-automatic/";
  std::vector<std::string> only;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    std::ifstream probe(a + "cases.csv");
    if (i == 1 && probe.good()) {
      dir = a;
      if (dir.back() != '/') dir += '/';
    } else {
      only.push_back(a);
    }
  }

  std::vector<Case> cases;
  for (const auto& r : read_csv(dir + "cases.csv")) {
    Case c;
    c.id = unquote(r[0]);
    c.note = unquote(r[1]);
    c.n = integer(r[2]);
    c.status = unquote(r[3]);
    c.log = boolean(r[4]);
    c.mean = boolean(r[5]);
    c.p = integer(r[6]); c.d = integer(r[7]); c.q = integer(r[8]);
    c.bp = integer(r[9]); c.bd = integer(r[10]); c.bq = integer(r[11]);
    c.mode_requested = unquote(r[12]);
    c.mode_effective = unquote(r[13]);
    c.nvars = integer(r[14]);
    c.nevents = integer(r[15]);
    cases.push_back(c);
  }
  for (const auto& r : read_csv(dir + "inputs.csv")) {
    for (auto& c : cases) {
      if (c.id != unquote(r[0])) continue;
      const std::string line = unquote(r[1]);
      const std::size_t sp = line.find(' ');
      c.input[line.substr(0, sp)] = line.substr(sp + 1);
    }
  }
  for (const auto& r : read_csv(dir + "oracle-events.csv")) {
    for (auto& c : cases) {
      if (c.id != unquote(r[0])) continue;
      Ev e;
      e.seq = integer(r[1]);
      e.kind = unquote(r[2]);
      e.depth = integer(r[3]);
      e.msg = unquote(r[4]);
      e.detail = unquote(r[5]);
      c.events.push_back(e);
    }
  }
  for (const auto& r : read_csv(dir + "oracle-facts.csv")) {
    for (auto& c : cases) {
      if (c.id != unquote(r[0])) continue;
      c.facts.emplace_back(unquote(r[1]), unquote(r[2]));
    }
  }

  for (auto& c : cases) {
    std::sort(c.events.begin(), c.events.end(),
              [](const Ev& a, const Ev& b) { return a.seq < b.seq; });
  }

  for (const auto& c : cases) {
    if (!only.empty() &&
        std::find(only.begin(), only.end(), c.id) == only.end())
      continue;
    std::printf("== %s (%s)\n", c.id.c_str(), c.note.c_str());
    const long bad_before = [&] {
      long n = 0;
      for (const auto& f : g_families) n += f.second.bad;
      return n;
    }();
    sweep_case(c);
    long bad_after = 0;
    for (const auto& f : g_families) bad_after += f.second.bad;
    std::printf("   -> %s\n", bad_after == bad_before ? "ok" : "FAILURES");
  }

  std::printf("\n== families ==\n%-24s %8s %8s %8s %10s  %s\n", "family", "checks",
              "bad", "exact", "worst-rel", "worst at");
  for (const auto& f : g_families) {
    std::printf("%-24s %8ld %8ld %8ld %10.3g  %s\n", f.first.c_str(),
                f.second.checks, f.second.bad, f.second.exact, f.second.worst,
                f.second.worst_where.c_str());
  }
  long bad = 0;
  for (const auto& f : g_families) bad += f.second.bad;
  std::printf("\ncases %ld (failed %ld), checks %ld, bad %ld : %s\n", g_cases,
              g_cases_failed,
              [&] { long n = 0; for (const auto& f : g_families) n += f.second.checks; return n; }(),
              bad, bad == 0 ? "PASS" : "FAIL");
  return bad == 0 && g_cases_failed == 0 ? 0 : 1;
}
