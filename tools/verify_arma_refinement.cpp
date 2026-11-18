// Full-sweep verification: the native item 21-F ARMA-refinement unit against
// the pinned jdplus 3.9.0 x13 AutoModellingModule.process corpus
// (validation/evidence/native/arma-refinement; an alternative corpus
// directory may be passed as the single argument).
//
// Sections: the linearized data (RegArimaUtility.linearizedData), the re-
// differencing filter sequence the module hands to ArmaModule.select (the
// pinned BackFilter (1 - B^d)(1 - B^period)^bd over the linearized data),
// the differencing decision quadruple (d, bd, mean, tmean) through the 21-D
// unit, the select outcome orders, the merged refinement panel (membership,
// BIC2 and landing coefficients per slot) and the pinned final comparison
// flags (specification/mean changed) including the two airline-fallback
// cases. Tiering (docs/contracts/arma-refinement-contract.md): the pure
// arithmetic of the linearized data and the filter application, all
// decisions and flags, and the panel membership are exact; tmean is banded
// per the 21-D estimation tiers with a decision-margin audit; panel BIC2 and
// landing coefficients are estimation landings, checked exact, in the 21-E
// tight bands or - bounded and counted - in the documented basin/drift
// classes. Cases are corpus-wide: 23 (21 normal, 2 airline fallback),
// white noise and structured series at n in [16, 144], period {4, 12},
// d up to 2 with the pinned one-tap D polynomials, mean on/off, zero/two
// user regressors.
#include <kalos/arma_refinement.hpp>

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

// The four JVM parameter blocks phi, bphi, theta, btheta of a params cell
// (blocks joined with '/', values with ';'), converted to the Kalos block
// order [ar, ma, sar, sma] with the AR blocks negated (Kalos stores
// 1 - phi(B); include/kalos/numerics.hpp).
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

struct PanelRow {
  int slot = 0, p = 0, q = 0, bp = 0, bq = 0;
  double bic = 0.0;
  std::array<std::vector<double>, 4> params;
};

struct Case {
  std::string id, note, kind;
  int period = 12;
  bool mean = true;
  int cur_p = 0, cur_q = 0, cur_bp = 0, cur_bq = 0, cur_d = 0, cur_bd = 0;
  std::vector<double> y;
  std::vector<std::vector<double>> users;
  std::vector<double> coef;    // estimation.csv: current cl coefficients
  std::vector<double> lin;     // lin.csv: pinned linearizedData
  std::string z_kind;          // identity | ok | none
  std::vector<double> z;       // z.csv: pinned filtered sequence
  // cases.csv decision record (empty rows on a stage failure).
  bool dif_error = false, sel_error = false;
  bool has_dif = false;
  int d = 0, bd = 0;
  bool mu = false;
  double tmean = 0.0;
  int sel_p = 0, sel_q = 0, sel_bp = 0, sel_bq = 0;
  int fin_p = 0, fin_q = 0, fin_bp = 0, fin_bq = 0, fin_d = 0, fin_bd = 0;
  bool fin_mean = false, spec_changed = false, mean_changed = false;
  bool changed = false, fallback = false;
  std::vector<PanelRow> panel;
};

// The decoder-pinned module scalars (X13SpecDecoder.readAutoModel; verified
// identical across cases through the probe's MODULE line, see metadata.R).
kalos::ArmaRefinementRequest make_request(const Case& c) {
  kalos::ArmaRefinementRequest r;
  r.series = c.y;
  r.regressors = c.users;
  r.mean = c.mean;
  r.regression_coefficients = c.coef;
  r.p = c.cur_p;
  r.q = c.cur_q;
  r.bp = c.cur_bp;
  r.bq = c.cur_bq;
  r.d = c.cur_d;
  r.seasonal_d = c.cur_bd;
  r.period = c.period;
  r.max_d = 2;
  r.max_seasonal_d = 1;
  r.ub1 = 0.96999999999999990;
  r.ub2 = 0.87999999999999990;
  r.cancel = 0.10000000000000000;
  r.precision = 1.0000000000000000e-07;
  r.models_count = 5;
  r.max_p = 2;
  r.max_q = 2;
  r.max_bp = 1;
  r.max_bq = 1;
  r.balanced = false;
  r.mixed = true;
  r.accept_white_noise = false;
  r.eps = 1.0000000000000000e-07;
  return r;
}

struct UnitRun {
  kalos::ArmaRefinement result;
  bool threw = false;
  std::string message;
};

class Runner {
 public:
  explicit Runner(std::map<std::string, Case> cases)
      : cases_(std::move(cases)) {}

  const UnitRun& run(const std::string& cid) {
    auto it = cache_.find(cid);
    if (it != cache_.end()) return it->second;
    UnitRun r;
    try {
      r.result = kalos::refine_arma_model(make_request(cases_.at(cid)));
    } catch (const std::exception& e) {
      r.threw = true;
      r.message = e.what();
    }
    return cache_.emplace(cid, std::move(r)).first->second;
  }

 private:
  std::map<std::string, Case> cases_;
  std::map<std::string, UnitRun> cache_;
};

struct Report {
  long checks = 0, bad = 0, basin = 0, exact = 0;
  double worst_lin = 0.0, worst_z = 0.0, worst_tmean = 0.0, worst_bic = 0.0,
         worst_par = 0.0;
  std::string worst_lin_id, worst_z_id, worst_tmean_id, worst_bic_id,
      worst_par_id;

  void note(bool ok, const std::string& id, const char* what) {
    ++checks;
    if (!ok) {
      ++bad;
      std::printf("  FAIL %s: %s\n", id.c_str(), what);
    }
  }
  void track(double dev, const std::string& id, double& worst,
             std::string& where) {
    if (dev > worst) { worst = dev; where = id; }
  }
};

double band_error(double value, double pinned, double rel, double abs_floor) {
  return std::abs(value - pinned) - std::max(abs_floor, rel * std::abs(pinned));
}

// Tiers (contract "Tiers" section). The linearized data and the filter
// application are pure double arithmetic over the captured inputs in the
// pinned operation order: bit identity (measured 0 corpus-wide). Decisions
// and flags are exact. tmean is an estimation landing, banded per the 21-D
// tiers (algebraic stMean path at 1e-12, nstMean fit at 1e-3 relative) with
// the decision-margin audit. Panel cells are 21-E select landings: exact,
// inside the 21-E tight bands, or in the documented basin/drift classes with
// the measured ceilings of docs/contracts/
// initial-arma-identification-contract.md; nothing here relaxes those.
constexpr double kPanelBand = 1e-5;    // tight-band merged panel BIC2
constexpr double kParBand = 2e-3;      // tight-band landing coefficients
constexpr double kBasinBound = 2e-1;   // basin-class BIC2 ceiling (measured)
constexpr double kParBasinBound = 2.0; // basin-class coefficient ceiling
constexpr double kDriftBic = 1e-3;     // iteration-limit drift BIC2 ceiling
constexpr double kDriftPar = 5e-2;     // iteration-limit drift coeff ceiling
constexpr double kTMeanStBand = 1e-12; // algebraic stMean path
constexpr double kTMeanBand = 1e-3;    // nstMean fit (21-D tier)

double nstmean_threshold(std::size_t n) {
  return n <= 80 ? 1.96 : n <= 155 ? 1.98 : n <= 230 ? 2.1 : n <= 350 ? 2.3
                                                                      : 2.5;
}

const kalos::ArmaCandidateEvaluation* find_eval(
    const kalos::ArmaRefinement& r, int p, int q, int bp, int bq) {
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

// Cascade check of one panel landing cell: exact -> tight band -> documented
// basin class (converged both sides) -> drift class -> failure.
void check_panel_cell(const std::string& id, double dbic, double dpar,
                      bool converged, Report& rep) {
  rep.track(dbic, id, rep.worst_bic, rep.worst_bic_id);
  rep.track(dpar, id, rep.worst_par, rep.worst_par_id);
  if (dbic == 0.0 && dpar == 0.0) {
    ++rep.checks;
    ++rep.exact;
    return;
  }
  if (dbic <= kPanelBand && dpar <= kParBand) {
    ++rep.checks;
    return;
  }
  if (converged && dbic <= kBasinBound && dpar <= kParBasinBound) {
    ++rep.checks;
    ++rep.basin;
    return;
  }
  if (!converged && dbic <= kDriftBic && dpar <= kDriftPar) {
    ++rep.checks;
    ++rep.basin;
    return;
  }
  std::ostringstream msg;
  msg.precision(17);
  msg << "panel cell outside every tier: dbic " << dbic << " dpar " << dpar
      << " converged " << (converged ? "yes" : "no");
  rep.note(false, id, msg.str().c_str());
}

double seq_dev(const std::vector<double>& got, const std::vector<double>& want) {
  double dev = 0.0;
  for (std::size_t i = 0; i < want.size(); ++i) {
    dev = std::max(dev, std::fabs(got[i] - want[i]));
  }
  return dev;
}

}  // namespace

int main(int argc, char** argv) {
  std::string root = argc > 1
      ? argv[1]
      : "validation/evidence/native/arma-refinement/";
  if (!root.empty() && root.back() != '/') root += '/';
  const auto in_rows = read_csv(root + "inputs.csv");
  const auto est_rows = read_csv(root + "estimation.csv");
  const auto lin_rows = read_csv(root + "lin.csv");
  const auto z_rows = read_csv(root + "z.csv");
  const auto case_rows = read_csv(root + "cases.csv");
  const auto panel_rows = read_csv(root + "panel.csv");

  std::map<std::string, Case> cases;
  for (const auto& r : in_rows) {
    Case c;
    c.id = unquote(r[0]);
    c.note = unquote(r[1]);
    c.period = std::stoi(r[2]);
    c.mean = unquote(r[3]) == "TRUE";
    c.kind = unquote(r[4]);
    c.cur_p = std::stoi(r[5]);
    c.cur_q = std::stoi(r[6]);
    c.cur_bp = std::stoi(r[7]);
    c.cur_bq = std::stoi(r[8]);
    c.cur_d = std::stoi(r[9]);
    c.cur_bd = std::stoi(r[10]);
    const std::size_t n = static_cast<std::size_t>(std::stoi(r[11]));
    c.y = parse_semicolons(r[12]);
    if (c.y.size() != n) {
      std::printf("series length mismatch in %s\n", c.id.c_str());
      return 1;
    }
    if (c.kind != "none") {
      std::vector<std::string> cols;
      {
        std::stringstream ss(c.kind);
        std::string item;
        while (std::getline(ss, item, '+')) {
          if (!item.empty()) cols.push_back(item);
        }
      }
      if (static_cast<std::size_t>(cols.size()) * n
          != parse_semicolons(r[13]).size()) {
        std::printf("user block size mismatch in %s\n", c.id.c_str());
        return 1;
      }
      const std::vector<double> flat = parse_semicolons(r[13]);
      c.users.assign(cols.size(), {});
      for (std::size_t u = 0; u < cols.size(); ++u) {
        c.users[u].assign(flat.begin() + static_cast<std::ptrdiff_t>(u * n),
                          flat.begin() + static_cast<std::ptrdiff_t>((u + 1) * n));
      }
    }
    cases.emplace(c.id, std::move(c));
  }
  for (const auto& r : est_rows) {
    Case& c = cases.at(unquote(r[0]));
    if (unquote(r[1]) != "ok") {
      std::printf("corpus estimation-error row in %s (no sweep case is built "
                  "on these)\n", c.id.c_str());
      return 1;
    }
    c.coef = parse_semicolons(r[2]);
  }
  for (const auto& r : lin_rows) {
    Case& c = cases.at(unquote(r[0]));
    c.lin = parse_semicolons(r[2]);
    if (c.lin.size() != c.y.size()) {
      std::printf("lin length mismatch in %s\n", c.id.c_str());
      return 1;
    }
  }
  for (const auto& r : z_rows) {
    Case& c = cases.at(unquote(r[0]));
    c.z_kind = unquote(r[1]);
    c.z = parse_semicolons(r[2]);
  }
  for (const auto& r : case_rows) {
    Case& c = cases.at(unquote(r[0]));
    c.dif_error = unquote(r[1]) == "error";
    c.sel_error = unquote(r[6]) == "error";
    if (!c.dif_error) {
      c.has_dif = true;
      c.d = std::stoi(r[2]);
      c.bd = std::stoi(r[3]);
      c.mu = unquote(r[4]) == "1";
      c.tmean = std::strtod(unquote(r[5]).c_str(), nullptr);
    }
    if (!c.sel_error) {
      c.sel_p = std::stoi(r[7]);
      c.sel_q = std::stoi(r[8]);
      c.sel_bp = std::stoi(r[9]);
      c.sel_bq = std::stoi(r[10]);
    }
    c.fin_p = std::stoi(r[11]);
    c.fin_q = std::stoi(r[12]);
    c.fin_bp = std::stoi(r[13]);
    c.fin_bq = std::stoi(r[14]);
    c.fin_d = std::stoi(r[15]);
    c.fin_bd = std::stoi(r[16]);
    c.fin_mean = unquote(r[17]) == "1";
    c.spec_changed = unquote(r[18]) == "1";
    c.mean_changed = unquote(r[19]) == "1";
    c.changed = unquote(r[20]) == "1";
    c.fallback = unquote(r[21]) == "1";
  }
  for (const auto& r : panel_rows) {
    Case& c = cases.at(unquote(r[0]));
    PanelRow pr;
    pr.slot = std::stoi(r[1]);
    pr.p = std::stoi(r[2]);
    pr.q = std::stoi(r[3]);
    pr.bp = std::stoi(r[4]);
    pr.bq = std::stoi(r[5]);
    pr.bic = std::strtod(unquote(r[6]).c_str(), nullptr);
    pr.params = parse_blocks(r[7]);
    c.panel.push_back(std::move(pr));
  }

  Runner runner(cases);
  Report rep;

  for (const auto& [id, c] : cases) {
    const UnitRun& u = runner.run(id);
    if (u.threw) {
      rep.note(false, id, ("unit threw: " + u.message).c_str());
      continue;
    }
    const kalos::ArmaRefinement& r = u.result;

    // 1. Linearized data: pure pinned arithmetic, bit identity.
    if (r.linearized.size() != c.lin.size()) {
      rep.note(false, id, "linearized length differs");
    } else {
      const double dev = seq_dev(r.linearized, c.lin);
      rep.track(dev, id, rep.worst_lin, rep.worst_lin_id);
      std::ostringstream msg;
      msg.precision(17);
      msg << "linearized dev " << dev;
      rep.note(dev == 0.0, id, msg.str().c_str());
    }

    // 2. Re-differencing filter sequence handed to select.
    if (c.fallback) {
      rep.note(r.re_differenced.empty(), id,
               "fallback case must stop before the filter");
      rep.note(!r.differencing.has_value(), id,
               "fallback case must carry no differencing record");
    } else if (c.z_kind == "identity") {
      rep.note(r.re_differenced.size() == r.linearized.size()
                   && seq_dev(r.re_differenced, r.linearized) == 0.0,
               id, "identity path must hand the linearized data itself");
    } else if (c.z_kind == "ok") {
      if (r.re_differenced.size() != c.z.size()) {
        rep.note(false, id, "filtered length differs");
      } else {
        const double dev = seq_dev(r.re_differenced, c.z);
        rep.track(dev, id, rep.worst_z, rep.worst_z_id);
        std::ostringstream msg;
        msg.precision(17);
        msg << "filtered dev " << dev;
        rep.note(dev == 0.0, id, msg.str().c_str());
      }
    } else {
      rep.note(false, id, "untranscribed z kind");
    }

    // 3. Differencing decision: quadruple exact, tmean banded.
    if (c.has_dif) {
      if (!r.differencing) {
        rep.note(false, id, "missing differencing record");
      } else {
        const auto& dn = *r.differencing;
        rep.note(dn.d == c.d && dn.seasonal_d == c.bd
                     && dn.mean_correction == c.mu,
                 id, "differencing quadruple differs");
        const bool st_mean = c.d + c.period * c.bd == 0;
        const double band = st_mean ? kTMeanStBand : kTMeanBand;
        const double tdiff = band_error(dn.t_mean, c.tmean, band, band);
        rep.track(std::abs(dn.t_mean - c.tmean), id, rep.worst_tmean,
                  rep.worst_tmean_id);
        std::ostringstream msg;
        msg.precision(17);
        msg << "tmean " << dn.t_mean << " vs pinned " << c.tmean
            << " (band " << band << " exceeded by " << tdiff << ")";
        rep.note(tdiff <= 0.0, id, msg.str().c_str());
        if (!st_mean) {
          // Decision-margin audit (21-D convention): the pinned t-statistic
          // must sit outside the qualified band around the threshold taken
          // from the full-length response the machine received.
          const double th = nstmean_threshold(c.lin.size());
          const double margin = std::abs(std::abs(c.tmean) - th);
          if (margin < 10.0 * band) {
            std::ostringstream audit;
            audit.precision(6);
            audit << "|tmean| " << std::abs(c.tmean) << " within "
                  << 10.0 * band << " of threshold " << th
                  << " (margin audit)";
            rep.note(false, id, audit.str().c_str());
          }
          ++rep.checks;
        }
      }
    }

    // 4. Select outcome and the pinned comparison flags.
    rep.note(r.p == c.fin_p && r.q == c.fin_q && r.bp == c.fin_bp
                 && r.bq == c.fin_bq && r.d == c.fin_d
                 && r.seasonal_d == c.fin_bd && r.mean == c.fin_mean,
             id, "resulting specification differs");
    if (!c.sel_error) {
      rep.note(r.p == c.sel_p && r.q == c.sel_q && r.bp == c.sel_bp
                   && r.bq == c.sel_bq,
               id, "select orders differ from the corpus select row");
    }
    rep.note(r.specification_changed == c.spec_changed
                 && r.mean_changed == c.mean_changed && r.changed == c.changed,
             id, "changed flags differ");
    rep.note(r.fallback == c.fallback, id, "fallback flag differs");
    if (c.fallback) {
      const char* want = c.dif_error ? "differencing" : "select";
      rep.note(r.fallback_stage == want, id,
               (std::string("fallback stage ") + r.fallback_stage
                + ", corpus says " + want).c_str());
    } else {
      rep.note(r.fallback_stage.empty(), id, "fallback stage set without a "
                                             "fallback");
    }

    // 5. Merged refinement panel: slot order, membership, BIC2 and the
    // landing coefficients of every slot.
    if (c.sel_error) {
      rep.note(r.preferred_models.empty() && r.evaluations.empty()
                   && r.refinement.empty(),
               id, "records must stop before the failed stage");
      continue;
    }
    if (r.preferred_models.size() != c.panel.size()) {
      rep.note(false, id, "panel length differs");
      continue;
    }
    for (std::size_t k = 0; k < c.panel.size(); ++k) {
      const PanelRow& want = c.panel[k];
      const auto& got = r.preferred_models[k];
      const std::string cell = id + "/" + std::to_string(want.slot);
      rep.note(got.p == want.p && got.q == want.q && got.bp == want.bp
                   && got.bq == want.bq,
               cell, "panel slot orders differ");
      bool size_ok = true;
      const double dpar = blocks_dev(got.coefficients, want.params, size_ok);
      if (!size_ok) {
        rep.note(false, cell, "panel landing block size differs");
        continue;
      }
      const auto* e = find_eval(r, want.p, want.q, want.bp, want.bq);
      const bool converged = !e || e->fit_status == "converged";
      check_panel_cell(cell, std::fabs(got.bic - want.bic), dpar, converged,
                       rep);
    }
  }

  std::printf("arma refinement: %ld checks, %ld failures, %ld exact, %ld "
              "basin-class; worst lin dev %.3g (%s), z dev %.3g (%s), tmean "
              "dev %.3g (%s), panel bic dev %.3g (%s), par dev %.3g (%s)\n",
              rep.checks, rep.bad, rep.exact, rep.basin,
              rep.worst_lin, rep.worst_lin_id.c_str(),
              rep.worst_z, rep.worst_z_id.c_str(),
              rep.worst_tmean, rep.worst_tmean_id.c_str(),
              rep.worst_bic, rep.worst_bic_id.c_str(),
              rep.worst_par, rep.worst_par_id.c_str());
  return rep.bad == 0 ? 0 : 1;
}
