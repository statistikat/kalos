// Full-sweep verification: the native item 21-E initial-ARMA-identification
// unit against the pinned jdplus 3.9.0 ArmaModule initial-identification
// corpus (validation/evidence/native/initial-arma-identification; an
// alternative corpus directory may be passed as the single argument).
//
// Sections: the OLS-residual cursor of every captured case, the per-candidate
// (logLikelihood, BIC2, landing coefficients) landings of the three-stage
// grids on both the mixed and the non-mixed route, and the full select panels
// with the pinned refinement outcome. Tiering (docs/contracts/
// initial-arma-identification-contract.md): decisions (selected orders,
// refinement), panel membership as a set and the one-column OLS cursor are
// exact; multi-column cursors carry the documented DGELSY deviation;
// candidate and panel cells are estimation landings, checked exact, in a
// tight band, or - above the band, bounded and counted - in the documented
// basin/drift classes around the 21-A.2 HannanRissanen 1-ulp start band.
// One annihilated-design case (ar120 trend+sine at d = bd = 1) is a
// documented skip. Cases are corpus-wide: white noise and structured series
// at n in [36, 144], period {4, 12}, d/bd in {0, 1}^2, mean on/off,
// zero/one/two user regressors.
#include <kalos/initial_arma_identification.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
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

std::vector<double> parse_semicolons(const std::string& cell) {
  std::vector<double> out;
  std::stringstream ss(unquote(cell));
  std::string item;
  while (std::getline(ss, item, ';')) {
    if (!item.empty()) out.push_back(std::strtod(item.c_str(), nullptr));
  }
  return out;
}

// The four JVM parameter blocks phi, bphi, theta, btheta of a candidates.csv
// params cell (blocks joined with '/', values with ';'), converted to the
// Kalos block order [ar, ma, sar, sma] with the AR blocks negated (Kalos
// stores 1 - phi(B); include/kalos/numerics.hpp).
std::array<std::vector<double>, 4> parse_blocks(const std::string& cell) {
  std::array<std::vector<double>, 4> jvm;
  std::string s = unquote(cell);
  std::size_t pos = 0;
  for (int b = 0; b < 4; ++b) {
    const std::size_t next = s.find('/', pos);
    const std::string blk = s.substr(pos, next == std::string::npos
                                               ? std::string::npos
                                               : next - pos);
    std::stringstream ss(blk);
    std::string item;
    while (std::getline(ss, item, ';')) {
      if (!item.empty()) jvm[b].push_back(std::strtod(item.c_str(), nullptr));
    }
    pos = next == std::string::npos ? s.size() : next + 1;
  }
  std::array<std::vector<double>, 4> out;
  out[0] = jvm[0];  // ar = -phi
  out[2] = jvm[1];  // sar = -bphi
  for (double& v : out[0]) v = -v;
  for (double& v : out[2]) v = -v;
  out[1] = jvm[2];  // ma = theta
  out[3] = jvm[3];  // sma = btheta
  return out;
}

struct OlsCase {
  std::string id;
  int period = 12, d = 0, bd = 0, k = 0;
  bool mean = true;
  std::vector<double> y;
  std::vector<std::vector<double>> users;
  std::vector<double> cursor;
};

struct UnitRun {
  kalos::InitialArmaIdentification result;
  bool threw = false;
  std::string message;
};

class Runner {
 public:
  explicit Runner(std::map<std::string, OlsCase> cases)
      : cases_(std::move(cases)) {}

  // The unit over one captured case with one module configuration; cached.
  const UnitRun& run(const std::string& cid, bool mixed, int models_count,
                     bool balanced) {
    const std::string key = cid + '|' + (mixed ? 'm' : 'x') + '|'
        + std::to_string(models_count) + '|' + (balanced ? 'b' : 'u');
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    const OlsCase& c = cases_.at(cid);
    kalos::InitialArmaIdentificationRequest req;
    req.series = c.y;
    req.regressors = c.users;
    req.mean_correction = c.mean;
    req.period = c.period;
    req.d = c.d;
    req.seasonal_d = c.bd;
    req.models_count = models_count;
    req.max_p = req.max_q = 2;
    req.max_bp = req.max_bq = 1;
    req.balanced = balanced;
    req.mixed = mixed;
    req.accept_white_noise = false;
    req.eps = 1e-7;
    UnitRun r;
    try {
      r.result = kalos::identify_initial_arma(req);
    } catch (const std::exception& e) {
      r.threw = true;
      r.message = e.what();
    }
    return cache_.emplace(key, std::move(r)).first->second;
  }

 private:
  std::map<std::string, OlsCase> cases_;
  std::map<std::string, UnitRun> cache_;
};

struct Report {
  long checks = 0, bad = 0, basin = 0, exact = 0;
  double worst_cursor = 0.0, worst_bic = 0.0, worst_ll = 0.0, worst_par = 0.0,
         worst_panel = 0.0;
  std::string worst_cursor_id, worst_bic_id, worst_ll_id, worst_par_id,
      worst_panel_id;

  void note(bool ok, const std::string& id, const char* what) {
    ++checks;
    if (!ok) {
      ++bad;
      std::printf("  FAIL %s: %s\n", id.c_str(), what);
    }
  }
  void track(double dev, const std::string& id, double& worst,
             std::string& where, const char* what) {
    (void)what;
    if (dev > worst) { worst = dev; where = id; }
  }
};

// Tiering (contract section "Tiers"): decisions, panel multiset membership
// and the one-column (pinned OLS) cursor are exact. Multi-column cursors
// carry the documented DGELSY-versus-pinned-Householder deviation at 1e-9.
// Candidate cells and panel BICs are estimation landings: exact where the
// transcribed chain is bit-identical, otherwise inside the measured
// tight bands below. Above the tight bands sits the documented basin class:
// the 21-A.2 HannanRissanen start is qualified with tolerance (not bit
// identity), and near its 1-ulp deltas the small-n likelihood surfaces of
// the corpus carry distinct local maxima separated by ridges, so the pinned
// LM chain and the Kalos chain can converge - both to certified converged
// landings - in different basins; when the pinned chain instead stops at the
// iteration limit near a flat ridge, the landing drifts by a small bounded
// amount. Measured corpus-wide (1908 candidate cells, 288 select cases): 1
// converged basin flip (worst dBIC2 9.7e-02, dlogLik 1.7, dcoef 1.7), 8
// iteration-limit drift cells (worst dBIC2 4.5e-04), and 20 panel cells
// affected through their candidates, including 2 value-order permutations of
// an otherwise identical panel set; every decision (selected orders,
// refinement, REF) is unaffected in all 288 select cases. The ceilings below
// are those measurements with margin. Cells exceeding their class ceiling, or
// exceeding the tight band with a converged-yes status above the basin
// ceiling, are FAILURES, never bands. See
// docs/contracts/initial-arma-identification-contract.md.
constexpr double kExact = 0.0;
constexpr double kScalarBand = 1e-5;   // tight-band BIC2 cells
constexpr double kLlBandFactor = 5e-4; // tight-band loglik: factor * n_eff
constexpr double kParBand = 2e-3;      // tight-band landing coefficients
constexpr double kPanelBand = 1e-5;    // tight-band merged panel BIC2
constexpr double kBasinBound = 2e-1;   // basin-class BIC2 ceiling (measured)
constexpr double kLlBasinBound = 3.0;  // basin-class loglik ceiling
constexpr double kParBasinBound = 2.0; // basin-class coefficient ceiling
// Iteration-limit drift: cells whose pinned chain hits the pinned iteration
// limit (status not "converged" on the Kalos side, e.g. (2,2,0,0) landings
// crawling along a near-flat ridge) end at a path-sensitive point near the
// ridge; the two chains stop within a small drift of each other. Measured
// worst drift dBIC2 4.5e-04, dlogLik 1.9e-02, dcoef 1.9e-02. Drift outside
// these ceilings is a failure.
constexpr double kDriftBic = 1e-3;
constexpr double kDriftLl = 5e-2;
constexpr double kDriftPar = 5e-2;

// Documented deviation: the ar120 trend+sine cursor at d = bd = 1 feeds the
// OLS a design whose columns are annihilated by the differencing polynomial
// (the trend dies under (1 - B)(1 - B^12), the period-12 sine dies under
// (1 - B^12)); the least-squares projection is then conditioned on
// 1e-15-magnitude pivots, so the cursor (and everything downstream) is only
// path-stable inside one implementation, not across implementations. The
// pinned JVM is bit-stable across its own processes (the double capture);
// the Kalos DGELSY transcription lands elsewhere in the same ill-conditioned
// set. Documented in docs/contracts/initial-arma-identification-contract.md;
// the case is skipped below with the skip counted in the summary.
bool is_annihilated_design(const OlsCase& c) {
  return c.id.rfind("ar120/u2.o11.n", 0) == 0;
}

// The first evaluation of an order inside the unit's enumeration (a stage-3
// repeat evaluates the same order again; the first pass is the one the merge
// list of that stage carries).
const kalos::ArmaCandidateEvaluation* find_eval(
    const kalos::InitialArmaIdentification& r, int p, int q, int bp, int bq) {
  const kalos::ArmaCandidateEvaluation* first = nullptr;
  for (const auto& e : r.evaluations) {
    if (e.p == p && e.q == q && e.bp == bp && e.bq == bq) {
      if (!first) first = &e;
    }
  }
  return first;
}

double blocks_dev(const std::array<std::vector<double>, 4>& got,
                  const std::array<std::vector<double>, 4>& want,
                  bool& size_ok) {
  size_ok = true;
  double dev = 0.0;
  for (int b = 0; b < 4; ++b) {
    if (got[b].size() != want[b].size()) {
      size_ok = false;
      return std::numeric_limits<double>::infinity();
    }
    for (std::size_t i = 0; i < want[b].size(); ++i) {
      dev = std::max(dev, std::fabs(got[b][i] - want[b][i]));
    }
  }
  return dev;
}

// Cascade check of one landing-class cell: exact -> tight band -> documented
// basin class (converged both sides, inside the measured basin bound) ->
// failure. Tracks the worst deviations either way.
void check_cell(const std::string& id, double dbic, double dll, double dpar,
                double ll_tight, bool converged, bool panel, Report& rep) {
  rep.track(dbic, id, panel ? rep.worst_panel : rep.worst_bic,
            panel ? rep.worst_panel_id : rep.worst_bic_id, "bic");
  rep.track(dll, id, rep.worst_ll, rep.worst_ll_id, "ll");
  rep.track(dpar, id, rep.worst_par, rep.worst_par_id, "par");
  if (dbic == 0.0 && dll == 0.0 && dpar == 0.0) {
    ++rep.checks;
    ++rep.exact;
    return;
  }
  if (dbic <= (panel ? kPanelBand : kScalarBand) && dll <= ll_tight
      && dpar <= kParBand) {
    ++rep.checks;
    return;
  }
  if (converged && dbic <= kBasinBound && dll <= kLlBasinBound
      && dpar <= kParBasinBound) {
    ++rep.checks;
    ++rep.basin;
    return;
  }
  if (!converged && dbic <= kDriftBic && dll <= kDriftLl && dpar <= kDriftPar) {
    ++rep.checks;
    ++rep.basin;
    return;
  }
  std::ostringstream msg;
  msg.precision(17);
  msg << (panel ? "panel cell" : "cell") << " outside every tier: dbic "
      << dbic << " dll " << dll << " dpar " << dpar << " converged "
      << (converged ? "yes" : "no");
  rep.note(false, id, msg.str().c_str());
}


}  // namespace

int main(int argc, char** argv) {
  std::string root = argc > 1
      ? argv[1]
      : "validation/evidence/native/initial-arma-identification/";
  if (!root.empty() && root.back() != '/') root += '/';
  const auto ols_in = read_csv(root + "ols-inputs.csv");
  const auto ols_out = read_csv(root + "ols.csv");
  const auto cand_in = read_csv(root + "candidates-inputs.csv");
  const auto cand_out = read_csv(root + "candidates.csv");
  const auto sel_in = read_csv(root + "select-inputs.csv");
  const auto sel_out = read_csv(root + "select.csv");
  if (ols_in.size() != ols_out.size() || cand_in.size() != cand_out.size()
      || sel_in.size() != sel_out.size()) {
    std::printf("corpus section mismatch\n");
    return 1;
  }

  std::map<std::string, OlsCase> cases;
  for (std::size_t i = 0; i < ols_in.size(); ++i) {
    OlsCase c;
    c.id = unquote(ols_in[i][0]);
    c.period = std::stoi(ols_in[i][2]);
    c.d = std::stoi(ols_in[i][3]);
    c.bd = std::stoi(ols_in[i][4]);
    c.mean = unquote(ols_in[i][5]) == "TRUE";
    const std::string kind = unquote(ols_in[i][6]);
    c.k = kind == "none" ? 0 : (kind == "trend" ? 1 : 2);
    c.y = parse_semicolons(ols_in[i][8]);
    if (c.k > 0) {
      const std::vector<double> flat = parse_semicolons(ols_in[i][9]);
      if (flat.size() != c.y.size() * static_cast<std::size_t>(c.k)) {
        std::printf("corpus user block size mismatch in %s\n", c.id.c_str());
        return 1;
      }
      c.users.assign(static_cast<std::size_t>(c.k), {});
      for (int u = 0; u < c.k; ++u) {
        c.users[static_cast<std::size_t>(u)].assign(
            flat.begin() + static_cast<std::ptrdiff_t>(
                static_cast<std::size_t>(u) * c.y.size()),
            flat.begin() + static_cast<std::ptrdiff_t>(
                (static_cast<std::size_t>(u) + 1) * c.y.size()));
      }
    }
    if (unquote(ols_out[i][1]) != "ok") {
      std::printf("corpus error row in ols for %s\n", c.id.c_str());
      return 1;
    }
    c.cursor = parse_semicolons(ols_out[i][3]);
    cases.emplace(c.id, std::move(c));
  }

  Runner runner(cases);
  Report rep;

  // 1. Cursors: the unit's internal cursor equals the pinned olsResiduals.
  long skips = 0;
  for (const auto& [id, c] : cases) {
    if (is_annihilated_design(c)) {
      ++skips;
      continue;
    }
    const UnitRun& r = runner.run(id, true, 5, false);
    if (r.threw) {
      rep.note(false, id, ("cursor: " + r.message).c_str());
      continue;
    }
    if (r.result.residuals.size() != c.cursor.size()) {
      rep.note(false, id, "cursor length differs");
      continue;
    }
    double dev = 0.0;
    for (std::size_t i = 0; i < c.cursor.size(); ++i) {
      dev = std::max(dev, std::fabs(r.result.residuals[i] - c.cursor[i]));
    }
    const int cols = (c.mean ? 1 : 0) + c.k;
    const double tol = cols >= 2 ? 1e-9 : kExact;  // documented DGELSY class
    const bool ok = dev <= tol;
    if (dev > rep.worst_cursor) {
      rep.worst_cursor = dev;
      rep.worst_cursor_id = id;
    }
    std::ostringstream msg;
    msg << "cursor dev " << dev << " (cols=" << cols << ")";
    rep.note(ok, id, msg.str().c_str());
  }

  // 2. Candidates: every grid row of both routes.
  for (std::size_t i = 0; i < cand_in.size(); ++i) {
    const std::string id = unquote(cand_in[i][0]);
    const std::string cid = id.substr(0, id.rfind('/'));
    if (is_annihilated_design(cases.at(cid))) continue;
    const std::string tail = id.substr(id.rfind('/') + 1);
    const bool mixed = tail.front() == 'm';
    std::vector<std::string> o = fields(tail.substr(1));
    const int p = std::stoi(o[0]), q = std::stoi(o[1]), bp = std::stoi(o[2]),
              bq = std::stoi(o[3]);
    if (unquote(cand_out[i][1]) != "ok") {
      rep.note(false, id, "corpus error row (pinned throw expected)");
      continue;
    }
    const UnitRun& r = runner.run(cid, mixed, 5, false);
    if (r.threw) {
      rep.note(false, id, ("candidate: " + r.message).c_str());
      continue;
    }
    const auto* e = find_eval(r.result, p, q, bp, bq);
    if (!e) {
      rep.note(false, id, "orders outside the Kalos enumeration (stage winner "
                          "diverged)");
      continue;
    }
    const double want_bic = std::strtod(unquote(cand_out[i][6]).c_str(), nullptr);
    const double want_ll = std::strtod(unquote(cand_out[i][7]).c_str(), nullptr);
    const std::size_t neff = static_cast<std::size_t>(
        std::strtod(unquote(cand_out[i][9]).c_str(), nullptr));
    const auto want = parse_blocks(cand_out[i][10]);
    if (e->bic != want_bic && e->log_likelihood == want_ll) {
      rep.note(false, id, "identical logLikelihood but different BIC2");
    }
    bool size_ok = true;
    const double dpar = blocks_dev(e->coefficients, want, size_ok);
    if (!size_ok) {
      rep.note(false, id, "landing block size differs");
      continue;
    }
    const double dbic = std::fabs(e->bic - want_bic);
    const double dll = std::fabs(e->log_likelihood - want_ll);
    check_cell(id, dbic, dll, dpar,
               kLlBandFactor * static_cast<double>(neff),
               e->fit_status == "converged", false, rep);
  }

  // 3. Select: full module runs, panel membership, BICs and outcome.
  for (std::size_t i = 0; i < sel_in.size(); ++i) {
    const std::string id = unquote(sel_in[i][0]);
    const std::string cid = unquote(sel_in[i][1]);
    const int d = std::stoi(sel_in[i][2]), bd = std::stoi(sel_in[i][3]);
    const std::string variant = unquote(sel_in[i][4]);
    const int nmod = std::stoi(sel_in[i][5]);
    const int maxp = std::stoi(sel_in[i][6]), maxq = std::stoi(sel_in[i][7]);
    const int maxbp = std::stoi(sel_in[i][8]), maxbq = std::stoi(sel_in[i][9]);
    const bool balanced = unquote(sel_in[i][10]) == "TRUE";
    const bool mixed = unquote(sel_in[i][11]) == "TRUE";
    if (maxp != 2 || maxq != 2 || maxbp != 1 || maxbq != 1) {
      std::printf("corpus variant with untranscribed grid bounds: %s\n",
                  id.c_str());
      return 1;
    }
    (void)d;
    (void)bd;  // the captured case already encodes d/bd in its cursor row
    if (is_annihilated_design(cases.at(cid))) continue;
    const UnitRun& r = runner.run(cid, mixed, nmod, balanced);
    if (r.threw) {
      rep.note(false, id, ("select: " + r.message).c_str());
      continue;
    }
    if (unquote(sel_out[i][1]) != "ok") {
      rep.note(false, id, "corpus error row (pinned throw expected)");
      continue;
    }
    const int wp = std::stoi(sel_out[i][2]), wq = std::stoi(sel_out[i][3]),
              wbp = std::stoi(sel_out[i][4]), wbq = std::stoi(sel_out[i][5]);
    const int nmerged = std::stoi(sel_out[i][6]);
    {
      std::ostringstream msg;
      msg << "selected orders differ got " << r.result.p << ',' << r.result.q
          << ',' << r.result.bp << ',' << r.result.bq << " want " << wp << ','
          << wq << ',' << wbp << ',' << wbq;
      rep.note(r.result.p == wp && r.result.q == wq && r.result.bp == wbp
                   && r.result.bq == wbq,
               id, msg.str().c_str());
    }
    std::vector<std::string> merged;
    {
      std::stringstream ss(unquote(sel_out[i][7]));
      std::string item;
      while (std::getline(ss, item, ';')) {
        if (!item.empty()) merged.push_back(item);
      }
    }
    rep.note(static_cast<long>(r.result.preferred_models.size())
                 == static_cast<long>(merged.size())
                 && static_cast<long>(merged.size()) == nmerged,
             id, "panel length differs");
    const long n = std::min<long>(
        static_cast<long>(merged.size()),
        static_cast<long>(r.result.preferred_models.size()));
    // Indexed by (p, q, bp, bq): the pinned BIC2 of that order in the corpus
    // panel, so reordering (a value effect at basin-flipped entries) can be
    // checked as a set while still asserting every cell's value.
    std::map<std::array<int, 4>, double> want_panel;
    bool want_unique = true;
    for (long k = 0; k < n; ++k) {
      std::vector<std::string> e = fields(merged[static_cast<std::size_t>(k)]);
      const std::array<int, 4> key{std::stoi(e[0]), std::stoi(e[1]),
          std::stoi(e[2]), std::stoi(e[3])};
      if (want_panel.count(key)) want_unique = false;
      want_panel[key] = std::strtod(e[4].c_str(), nullptr);
    }
    rep.note(want_unique, id, "duplicate order in the corpus panel");
    long matched = 0;
    for (long k = 0; k < n; ++k) {
      const auto& got = r.result.preferred_models[static_cast<std::size_t>(k)];
      const std::array<int, 4> key{got.p, got.q, got.bp, got.bq};
      auto it = want_panel.find(key);
      if (it == want_panel.end()) continue;  // counted as a mismatch below
      ++matched;
      const double dbic = std::fabs(got.bic - it->second);
      check_cell(id, dbic, 0.0, 0.0, 0.0, true, true, rep);
    }
    if (matched != n) {
      // Panel membership (the set) is exact: a genuine membership or
      // length change is a failure, never a band.
      std::ostringstream msg;
      msg << "panel membership differs (" << matched << '/' << n
          << " orders shared)";
      rep.note(false, id, msg.str().c_str());
    }
    // Value order of the panel: ascending BIC; an exact-sequence check on
    // top, basin reorders accepted only when the set already matched.
    bool sequence_ok = matched == n;
    for (long k = 0; k < n && sequence_ok; ++k) {
      std::vector<std::string> e = fields(merged[static_cast<std::size_t>(k)]);
      const auto& got = r.result.preferred_models[static_cast<std::size_t>(k)];
      sequence_ok = got.p == std::stoi(e[0]) && got.q == std::stoi(e[1])
          && got.bp == std::stoi(e[2]) && got.bq == std::stoi(e[3]);
    }
    if (sequence_ok && matched == n) {
      ++rep.checks;
      ++rep.exact;
    } else if (matched == n) {
      ++rep.checks;
      ++rep.basin;  // set-equal panel, BIC-order permuted at basin cells
    }
  }

  std::printf("initial arma identification: %ld checks, %ld failures, %ld "
              "exact, %ld basin-class, %ld documented skips; "
              "worst cursor dev %.3g (%s), bic dev %.3g (%s), ll dev %.3g "
              "(%s), par dev %.3g (%s), panel bic dev %.3g (%s)\n",
              rep.checks, rep.bad, rep.exact, rep.basin, skips,
              rep.worst_cursor, rep.worst_cursor_id.c_str(),
              rep.worst_bic, rep.worst_bic_id.c_str(), rep.worst_ll,
              rep.worst_ll_id.c_str(), rep.worst_par, rep.worst_par_id.c_str(),
              rep.worst_panel, rep.worst_panel_id.c_str());
  return rep.bad == 0 ? 0 : 1;
}
