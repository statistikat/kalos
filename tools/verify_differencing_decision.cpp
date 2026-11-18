// Full-sweep verification: the native item 21-D differencing-decision unit
// against the pinned jdplus 3.9.0 DifferencingModule corpus
// (validation/evidence/native/differencing-decision; an alternative corpus
// directory may be passed as the single argument).
// The decision triple (d, bd, mean), the mean t-statistic, the estimation
// couplings (mlused, nextstep count, c_ decrement chain, final orders, final
// model parameters, rmax/rsmax) are checked for every case. Decisions and
// integers are exact; the stMean t-statistic is pinned algebra and checked
// at 1e-12; nstMean t-statistics and the trial-fit-visible internals are
// estimation landings and checked in measured bands (21-A/21-B/21-C tiering).
// Cases where the pinned JVM crashes (negative-size filter, short-window
// Hannan-Rissanen) require a Kalos throw, not a value comparison. See
// docs/contracts/differencing-decision-contract.md.
#include <kalos/differencing_decision.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
std::vector<std::string> split(const std::string& raw, char sep) {
  std::vector<std::string> out;
  std::stringstream parts(raw);
  std::string item;
  while (std::getline(parts, item, sep)) out.push_back(item);
  return out;
}
double real(const std::string& raw) { return std::stod(unquote(raw)); }
int integer(const std::string& raw) { return std::stoi(unquote(raw)); }

double band_error(double value, double pinned, double rel, double abs_floor) {
  return std::abs(value - pinned) - std::max(abs_floor, rel * std::abs(pinned));
}

// Estimation-landing tiers. The machine consumes only phi/theta/bphi/btheta,
// rmax/rsmax and the mean t-statistic; without a trial fit every one of them
// is pure Hannan-Rissanen algebra (qualified at the 21-A.2 HR level), with a
// trial fit or an nstMean fit it is a warm-reentry landing. root_floor bounds
// the rmax/rsmax step0 root-scan diagnostics: those are computed from the
// step0 model landing, which drifts only when a step0 trial fit ran from the
// default fill (the corpus measures 3.2e-5 on rmax and 1.31e-4 on the small
// rsmax in m_i2_144, the only trial-fit case; every other case and every
// final-round landing is bit-identical). The floor is 2e-4 absolute with the
// 5e-5 relative tier; the decisions themselves are compared exactly.
struct Band {
  double coeff, stat, root_floor;
};
Band band_for(const bool nst_mean, const bool trial_fit) {
  if (trial_fit) return {5e-5, 1e-3, 2e-4};
  if (nst_mean) return {1e-9, 1e-3, 1e-9};
  return {1e-9, 1e-9, 1e-9};
}
} // namespace

int main(int argc, char** argv) {
  std::string root = argc > 1 ? argv[1]
                              : "validation/evidence/native/differencing-decision/";
  if (!root.empty() && root.back() != '/') root += '/';
  const auto cases = read_csv(root + "cases.csv");
  const auto inputs = read_csv(root + "inputs.csv");
  if (cases.size() != inputs.size()) {
    std::printf("corpus mismatch: %zu cases vs %zu inputs\n", cases.size(), inputs.size());
    return 1;
  }
  long checks = 0, bad = 0;
  double worst_stat = 0.0, worst_coeff = 0.0;
  std::string worst_stat_id, worst_coeff_id;
  for (std::size_t row = 0; row < cases.size(); ++row) {
    const auto& c = cases[row];
    const auto& i = inputs[row];
    const std::string id = unquote(c[0]);
    if (id != unquote(i[0])) {
      std::printf("  %s: corpus misalignment\n", id.c_str());
      ++bad;
      continue;
    }
    const bool pinned_error = unquote(c[1]) == "error";
    kalos::DifferencingDecisionRequest request;
    for (const auto& t : split(i[4], ';')) request.series.push_back(real(t));
    request.period = integer(i[2]);
    if (pinned_error) {
      // The pinned machine crashes mid-run (documented failure classes); the
      // qualified Kalos behaviour is an explicit throw with no decision.
      bool threw = false;
      try {
        kalos::decide_differencing(request);
      } catch (const std::exception&) {
        threw = true;
      }
      ++checks;
      if (!threw) {
        std::printf("  %s: pinned error [%s] but Kalos produced a decision\n", id.c_str(),
                    unquote(c[21]).c_str());
        ++bad;
      }
      continue;
    }
    request.max_d = integer(c[6]);
    request.max_seasonal_d = integer(c[7]);
    request.ub1 = real(c[8]);
    request.ub2 = real(c[9]);
    request.cancel = real(c[10]);
    request.precision = real(c[11]);
    const std::vector<double> params_raw = [&] {
      std::vector<double> out;
      for (const auto& t : split(unquote(c[20]), ';')) out.push_back(std::stod(t));
      return out;
    }();
    const auto orders = split(unquote(c[19]), ',');

    kalos::DifferencingDecision decision;
    try {
      decision = kalos::decide_differencing(request);
    } catch (const std::exception& e) {
      std::printf("  %s: unexpected Kalos failure: %s\n", id.c_str(), e.what());
      ++bad;
      continue;
    }
    const int pinned_d = integer(c[2]), pinned_bd = integer(c[3]);
    const bool pinned_mu = integer(c[4]) == 1;
    const double pinned_tmean = real(c[5]);
    const bool pinned_mlused = integer(c[14]) == 1;
    const bool pinned_useml = integer(c[15]) == 1;
    const int pinned_iter = integer(c[13]);
    const double pinned_rmax = real(c[16]), pinned_rsmax = real(c[17]);
    const double pinned_c = real(c[18]);

    ++checks;
    if (decision.d != pinned_d || decision.seasonal_d != pinned_bd ||
        decision.mean_correction != pinned_mu) {
      std::printf("  %s: decision (d,bd,mu)=(%d,%d,%d) vs pinned (%d,%d,%d)\n", id.c_str(),
                  decision.d, decision.seasonal_d, decision.mean_correction ? 1 : 0, pinned_d,
                  pinned_bd, pinned_mu ? 1 : 0);
      ++bad;
    }
    if (decision.ml_used != pinned_mlused) {
      std::printf("  %s: mlused %d vs pinned %d\n", id.c_str(), decision.ml_used ? 1 : 0,
                  pinned_mlused ? 1 : 0);
      ++bad;
    }
    if (decision.nextstep_calls != pinned_iter + 1) {
      std::printf("  %s: nextstep calls %d vs pinned iter %d\n", id.c_str(),
                  decision.nextstep_calls, pinned_iter);
      ++bad;
    }
    // stMean is pure pinned algebra (the response enters directly); the
    // nstMean statistic is an ML landing inside the mean-test fit.
    const bool st_mean = pinned_d == 0 && (pinned_bd == 0 || request.period == 1);
    // The estimation-landing band is a property of the RUN: a trial fit in
    // any round (the step0 default-fill ML pass as in m_i2_144, qtr_rw,
    // m_ar9995, m_double_rw, m_trend_seasonal, m_n231_rw) warms every later
    // landing, and it is exactly the class the contract's root_floor (2e-4)
    // describes for the step0-derived rmax/rsmax pair.
    const bool trial_fit = decision.ml_used ||
                           std::any_of(decision.trace.begin(), decision.trace.end(),
                                       [](const kalos::DifferencingIterationRecord& r) {
                                           return r.trial_fit;
                                       });
    const Band band = band_for(!st_mean, trial_fit);
    const double stat_band = st_mean ? 1e-12 : band.stat;
    const double tdiff = band_error(decision.t_mean, pinned_tmean, stat_band, stat_band);
    if (!(tdiff <= 0.0)) {
      std::printf("  %s: tmean %.17g vs pinned %.17g (band %.3g exceeded by %.3g)\n",
                  id.c_str(), decision.t_mean, pinned_tmean, stat_band, tdiff);
      ++bad;
    }
    if (!st_mean) {
      // Decision-margin audit: mu flips are caught by the exact comparison
      // above; here the pinned t-statistic must sit outside the qualified
      // band around the sample-size threshold so the equality is not a
      // coincidence of the band.
      const double th = request.series.size() <= 80 ? 1.96
                      : request.series.size() <= 155 ? 1.98
                      : request.series.size() <= 230 ? 2.1
                      : request.series.size() <= 350 ? 2.3
                                                     : 2.5;
      const double margin = std::abs(std::abs(pinned_tmean) - th);
      if (margin < 10.0 * stat_band) {
        std::printf("  %s: |tmean| %.6g within %.3g of threshold %.6g (margin audit)\n",
                    id.c_str(), std::abs(pinned_tmean), 10.0 * stat_band, th);
        ++bad;
      }
    }
    if (decision.trace.empty()) {
      std::printf("  %s: empty trace\n", id.c_str());
      ++bad;
      continue;
    }
    const auto& last = decision.trace.back();
    if (last.useml != pinned_useml) {
      std::printf("  %s: useml %d vs pinned %d\n", id.c_str(), last.useml ? 1 : 0,
                  pinned_useml ? 1 : 0);
      ++bad;
    }
    if (last.cancel_threshold != pinned_c) {
      std::printf("  %s: c_ %.17g vs pinned %.17g\n", id.c_str(), last.cancel_threshold,
                  pinned_c);
      ++bad;
    }
    if (last.p != integer(orders[0]) || last.q != integer(orders[1]) ||
        last.bp != integer(orders[2]) || last.bq != integer(orders[3])) {
      std::printf("  %s: final orders %d,%d,%d,%d vs pinned %s\n", id.c_str(), last.p,
                  last.q, last.bp, last.bq, unquote(c[19]).c_str());
      ++bad;
    }
    // Pinned final parameters: [phi(1..p), bphi(1..bp), theta(1..q),
    // btheta(1..bq)] in the raw JVM convention; the machine-side records are
    // taken at the same point (after the last initstep).
    if (params_raw.size() == 4 && last.bp == 1 && last.bq == 1) {
      const double cd = band_error(last.phi, params_raw[0], band.coeff, band.coeff);
      const double cbd = band_error(last.bphi, params_raw[1], band.coeff, band.coeff);
      const double td = band_error(last.theta, params_raw[2], band.coeff, band.coeff);
      const double tbd = band_error(last.btheta, params_raw[3], band.coeff, band.coeff);
      const double worst = std::max(std::max(cd, cbd), std::max(td, tbd));
      if (!(worst <= 0.0)) {
        std::printf("  %s: final parameters off band %.3g (phi %.17g/%.17g)\n", id.c_str(),
                    band.coeff, last.phi, params_raw[0]);
        ++bad;
      }
      const double abs_coeff = std::max(
          std::max(std::abs(last.phi - params_raw[0]), std::abs(last.bphi - params_raw[1])),
          std::max(std::abs(last.theta - params_raw[2]), std::abs(last.btheta - params_raw[3])));
      if (abs_coeff > worst_coeff) {
        worst_coeff = abs_coeff;
        worst_coeff_id = id;
      }
    }
    const double rmax_diff = band_error(last.rmax, pinned_rmax, band.coeff, band.root_floor);
    const double rsmax_diff = band_error(last.rsmax, pinned_rsmax, band.coeff, band.root_floor);
    if (!(rmax_diff <= 0.0) || !(rsmax_diff <= 0.0)) {
      std::printf("  %s: rmax/rsmax %.17g/%.17g vs pinned %.17g/%.17g (band %.3g)\n",
                  id.c_str(), last.rmax, last.rsmax, pinned_rmax, pinned_rsmax, band.coeff);
      ++bad;
    }
    const double abs_stat = std::abs(decision.t_mean - pinned_tmean);
    if (abs_stat > worst_stat) {
      worst_stat = abs_stat;
      worst_stat_id = id;
    }
  }
  std::printf("differencing decision: %ld cases, %ld failures; worst |tmean| dev %.3g (%s), "
              "worst |params| dev %.3g (%s)\n",
              static_cast<long>(cases.size()), bad, worst_stat, worst_stat_id.c_str(),
              worst_coeff, worst_coeff_id.c_str());
  return bad == 0 ? 0 : 1;
}
