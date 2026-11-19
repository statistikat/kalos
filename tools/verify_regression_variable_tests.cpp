// Full-sweep verification: the native item 21-G stage-R unit against the
// pinned jdplus 3.9.0 corpus (validation/evidence/native/regression-tests;
// an alternative corpus directory may be passed as the single argument).
//
// Sections: the concentrated-likelihood tstats(0, false) vector (bit-exact,
// the statistics machine behind every stage-R test), the MeanController
// tstat(0, 0, false) of both estimation states (bit-exact on the captured
// "cur" and "add" MEANSTAT rows), the DerivedRegressionTest joint
// t-statistic and value (bit-exact on every OBS row where the JVM ran the
// derived form), the per-pass accept decisions (OBS), the
// RegressionVariablesTest.process outcomes on the four decoder-shaped
// builder configurations (changed/mean/preadjustment/remaining variables),
// the MeanController.test decisions for the pinned cutoffs 1.96 and 1.6,
// and the final-mean-rule comparison on the captured ModelController rtvals.
// All checks are exact or bit-exact: every quantity here is bytecode-
// decided arithmetic over the captured statistics (no estimation lands
// inside the unit), so no tolerance tier applies
// (docs/contracts/regression-tests-contract.md). Cases are corpus-wide:
// 18 double-JVM-captured series over period {4, 12} with TD4/TD2, LoP,
// one/two Easter windows, user columns, mean on/off, log/preadjustment
// states, covering all-fail batch removal, the b/c gates, the derived
// second life, immediate moving-holiday removal, the adjusted preadjustment
// reset and mean add/remove decisions.
#include <kalos/regression_variable_tests.hpp>

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

double parse_real(const std::string& cell) {
  return std::strtod(unquote(cell).c_str(), nullptr);
}

bool bit_eq(double a, double b) {
  // The corpus is written with %.17g, so a parsed value must reproduce the
  // JVM double bit-for-bit; NaNs match as a class (the bit payload of the
  // JVM's NaN is not carried through the text).
  if (std::isnan(a) && std::isnan(b)) return true;
  return std::memcmp(&a, &b, sizeof(double)) == 0;
}

struct Report {
  long checks = 0, bad = 0;
  void note(bool ok, const std::string& id, const char* what) {
    ++checks;
    if (!ok) {
      ++bad;
      std::printf("  FAIL %s: %s\n", id.c_str(), what);
    }
  }
};

struct CaseInfo {
  std::string id;
  bool ok = false;
  bool mean_state = false;
  bool adjusted = false;
  std::string preadj_state; // None | LeapYear | GoodFriday
  bool has_rtval = false;
  double rtval = 0.0;
};

struct CaseData {
  kalos::RegressionTestStatistics stats;
  std::vector<double> captured_tstats;
  std::vector<std::string> names;
  std::vector<kalos::RegressionVariableLayout> vars;
  std::map<std::string, std::vector<std::string>> rvt; // cfg -> row
};

kalos::RegressionVariableKind kind_of(const std::string& kind) {
  if (kind == "td") return kalos::RegressionVariableKind::trading_days;
  if (kind == "lop") return kalos::RegressionVariableKind::length_of_period;
  if (kind == "mh") return kalos::RegressionVariableKind::moving_holiday;
  if (kind == "user") return kalos::RegressionVariableKind::user;
  return kalos::RegressionVariableKind::other;
}

int preadj_code(const std::string& token) {
  if (token == "None") return 0;
  if (token == "LeapYear") return 1;
  return 2; // GoodFriday
}

} // namespace

int main(int argc, char** argv) {
  std::string root = argc > 1
      ? argv[1]
      : "validation/evidence/native/regression-tests/";
  if (!root.empty() && root.back() != '/') root += '/';
  const auto case_rows = read_csv(root + "cases.csv");
  const auto cl_rows = read_csv(root + "cl.csv");
  const auto ucov_rows = read_csv(root + "ucov.csv");
  const auto var_rows = read_csv(root + "vars.csv");
  const auto rvt_rows = read_csv(root + "rvt.csv");
  const auto obs_rows = read_csv(root + "obs.csv");
  const auto mean_rows = read_csv(root + "mean.csv");

  std::map<std::string, CaseInfo> cases;
  std::map<std::string, CaseData> data;
  for (const auto& r : case_rows) {
    CaseInfo c;
    c.id = unquote(r[0]);
    c.ok = unquote(r[2]) == "ok";
    c.mean_state = unquote(r[16]) == "1";
    c.adjusted = unquote(r[19]) == "1";
    c.preadj_state = unquote(r[18]);
    c.has_rtval = !unquote(r[20]).empty();
    if (c.has_rtval) c.rtval = std::strtod(unquote(r[20]).c_str(), nullptr);
    cases[c.id] = c;
  }
  for (const auto& r : cl_rows) {
    CaseData& d = data[unquote(r[0])];
    const int nx = std::atoi(unquote(r[1]).c_str());
    d.stats.dim = std::atoi(unquote(r[2]).c_str());
    d.stats.ssq = parse_real(r[3]);
    d.stats.coefficients = parse_semicolons(r[4]);
    d.captured_tstats = parse_semicolons(r[5]);
    if (static_cast<int>(d.stats.coefficients.size()) != nx) {
      std::printf("  FAIL %s: cl row inconsistent\n", unquote(r[0]).c_str());
      return 2;
    }
    d.stats.unscaled_covariance.assign(
        static_cast<std::size_t>(nx) * static_cast<std::size_t>(nx), 0.0);
  }
  for (const auto& r : ucov_rows) {
    CaseData& d = data[unquote(r[0])];
    const std::size_t nx = d.stats.coefficients.size();
    const std::size_t row =
        static_cast<std::size_t>(std::atoi(unquote(r[1]).c_str()));
    const std::vector<double> values = parse_semicolons(r[2]);
    for (std::size_t j = 0; j < values.size() && j < nx; ++j) {
      d.stats.unscaled_covariance[row * nx + j] = values[j];
    }
  }
  for (const auto& r : var_rows) {
    CaseData& d = data[unquote(r[0])];
    d.names.push_back(unquote(r[2]));
    kalos::RegressionVariableLayout v;
    v.kind = kind_of(unquote(r[3]));
    v.dim = std::atoi(unquote(r[4]).c_str());
    v.position = std::atoi(unquote(r[5]).c_str());
    d.vars.push_back(v);
  }
  Report rep;
  long rvt_cases = 0, mean_checks = 0, obs_rows_used = 0, final_checks = 0;

  // Index the decision rows by case.
  std::map<std::string, std::vector<std::vector<std::string>>> rvt_by_case,
      obs_by_case, mean_by_case;
  for (const auto& r : rvt_rows) {
    if (unquote(r[3]) != "ok") continue;
    rvt_by_case[unquote(r[0])].push_back(r);
  }
  for (const auto& r : obs_rows) obs_by_case[unquote(r[0])].push_back(r);
  for (const auto& r : mean_rows) {
    if (unquote(r[2]) != "ok") continue;
    mean_by_case[unquote(r[0])].push_back(r);
  }

  for (const auto& entry : cases) {
    const CaseInfo& c = entry.second;
    if (!c.ok) continue;
    const auto dit = data.find(c.id);
    if (dit == data.end()) {
      rep.note(false, c.id, "no cl row");
      continue;
    }
    CaseData& d = dit->second;

    // 1. The statistics machine: cl.tstats(0, false), bit-exact.
    const std::vector<double> mine = d.stats.tstats();
    bool tstats_ok = mine.size() == d.captured_tstats.size();
    if (tstats_ok) {
      for (std::size_t j = 0; j < mine.size(); ++j) {
        if (!bit_eq(mine[j], d.captured_tstats[j])) {
          tstats_ok = false;
          break;
        }
      }
    }
    rep.note(tstats_ok, c.id, "tstats(0, false) differs from the capture");

    // 2. RegressionVariablesTest.process on every captured configuration.
    const auto rit = rvt_by_case.find(c.id);
    if (rit != rvt_by_case.end()) {
      ++rvt_cases;
      for (const auto& r : rit->second) {
        const std::string cfg = unquote(r[1]);
        std::stringstream ss(unquote(r[2]));
        std::string item;
        double tmu = 0.0, ttd = 0.0, tmh = 0.0;
        int derived = 0;
        {
          int k = 0;
          while (std::getline(ss, item, ',')) {
            const double v = std::strtod(item.c_str(), nullptr);
            if (k == 0) tmu = v;
            else if (k == 1) ttd = v;
            else if (k == 2) tmh = v;
            else derived = v != 0.0 ? 1 : 0;
            ++k;
          }
        }
        kalos::RegressionVariableTestRequest req;
        req.statistics = d.stats;
        req.variables = d.vars;
        req.mean_test = tmu;
        req.td_test = ttd;
        req.mh_test = tmh;
        req.derived_test = derived == 1;
        req.td_joint = 0.0;
        req.mean = c.mean_state;
        req.adjusted = c.adjusted;
        req.preadjustment = preadj_code(c.preadj_state);
        const std::string cell = c.id + "/rvt" + cfg;
        const kalos::RegressionVariableTestResult res =
            kalos::run_regression_variable_tests(req);
        rep.note(res.changed == (unquote(r[5]) == "1"), cell,
                 "changed differs from the corpus");
        rep.note(res.mean == (unquote(r[6]) == "1"), cell,
                 "mean after the run differs from the corpus");
        rep.note(res.preadjustment == preadj_code(unquote(r[7])), cell,
                 "preadjustment after the run differs from the corpus");
        std::string remaining;
        for (std::size_t i = 0; i < d.vars.size(); ++i) {
          if (res.removed[i]) continue;
          if (!remaining.empty()) remaining += ';';
          remaining += d.names[i];
        }
        if (remaining.empty()) remaining = "-";
        rep.note(remaining == unquote(r[8]), cell,
                 "remaining variables differ from the corpus");

        // OBS rows: the per-pass accepts and the derived statistics of
        // this configuration (the JVM observer called the pinned forms
        // directly, exactly the trigger conditions the unit applies).
        const auto oit = obs_by_case.find(c.id);
        if (oit != obs_by_case.end()) {
          for (const auto& o : oit->second) {
            if (unquote(o[1]) != cfg) continue;
            ++obs_rows_used;
            const std::string kind = unquote(o[3]);
            const std::string accept = unquote(o[4]);
            if (kind == "mean") {
              const bool acc = kalos::t_test_accepts(mine, tmu, tmu, 1, 0, 1);
              rep.note(acc == (accept == "1"), cell + "/meanobs",
                       "meanTest accept differs from the JVM");
              continue;
            }
            const std::size_t w =
                static_cast<std::size_t>(std::atoi(unquote(o[2]).c_str()));
            if (w >= res.tested.size()) {
              rep.note(false, cell + "/obs", "OBS index beyond the variables");
              continue;
            }
            if (kind == "tdlop") {
              const int width = d.vars[w].kind
                      == kalos::RegressionVariableKind::trading_days
                  ? d.vars[w].dim : 1;
              const bool acc = kalos::t_test_accepts(mine, ttd, ttd, 1,
                  d.vars[w].position, width);
              rep.note(acc == (accept == "1"), cell + "/obs",
                       "tdTest accept differs from the JVM");
              const std::string dacc = unquote(o[5]);
              if (dacc != "na") {
                const double dv = parse_real(o[6]);
                const double dt = parse_real(o[7]);
                const double my_t = kalos::derived_test_tstat(d.stats,
                    d.vars[w].position, width);
                rep.note(bit_eq(my_t, dt), cell + "/derived",
                         "derived t-statistic differs from the JVM");
                double sum = 0.0;
                for (int j = 0; j < width; ++j) {
                  sum += d.stats.coefficients[
                      static_cast<std::size_t>(d.vars[w].position) + j];
                }
                rep.note(bit_eq(-sum, dv), cell + "/derived",
                         "derived value differs from the JVM");
                rep.note((std::fabs(my_t) > ttd) == (dacc == "1"),
                    cell + "/derived", "derived accept differs from the JVM");
                // The unit ran the derived form under the same trigger.
                rep.note(res.derived_tested[w]
                             && bit_eq(res.derived_tstats[w], dt),
                         cell + "/derived",
                         "unit derived statistic differs from the OBS row");
              } else if (res.derived_tested[w]) {
                rep.note(false, cell + "/derived",
                         "unit derived where the JVM did not");
              }
            } else { // mh
              const bool acc = kalos::t_test_accepts(mine, tmh, tmh, 1,
                  d.vars[w].position, 1);
              rep.note(acc == (accept == "1"), cell + "/obs",
                       "movingHolidaysTest accept differs from the JVM");
            }
          }
        }
      }
    }

    // 3. MeanController.test for each captured cutoff and estimation state.
    for (const auto& r : mean_by_case[c.id]) {
      const double cval = std::strtod(unquote(r[1]).c_str(), nullptr);
      const std::vector<std::string> stat = [&] {
        std::vector<std::string> out;
        std::stringstream ss(unquote(r[5]));
        std::string item;
        while (std::getline(ss, item, '|')) out.push_back(item);
        return out;
      }();
      if (stat.size() != 6) {
        rep.note(false, c.id + "/mean", "MEANSTAT row malformed");
        continue;
      }
      ++mean_checks;
      kalos::MeanControllerRequest mreq;
      if (stat[0] == "cur") {
        mreq.statistics = d.stats;
      } else { // the mean-added estimation, carried by the row itself
        mreq.statistics.coefficients = {parse_real(stat[1])};
        mreq.statistics.unscaled_covariance = {parse_real(stat[2])};
        mreq.statistics.ssq = parse_real(stat[3]);
        mreq.statistics.dim = std::atoi(stat[4].c_str());
      }
      mreq.mean = c.mean_state;
      mreq.cval = cval;
      const std::string cell = c.id + "/mean" + unquote(r[1]);
      const kalos::MeanControllerResult mres =
          kalos::run_mean_controller(mreq);
      rep.note(mres.changed == (unquote(r[3]) == "1"), cell,
               "MeanController changed differs from the corpus");
      rep.note(mres.mean == (unquote(r[4]) == "1"), cell,
               "mean after MeanController differs from the corpus");
      if (stat[0] == "cur") {
        // The current-state row must agree with the case statistics.
        const bool consistent = mreq.statistics.coefficients.size()
                    == d.stats.coefficients.size()
                && bit_eq(mreq.statistics.coefficients[0],
                    d.stats.coefficients[0])
                && bit_eq(mreq.statistics.unscaled_covariance[0],
                    d.stats.unscaled_covariance[0])
                && bit_eq(mreq.statistics.ssq, d.stats.ssq)
                && mreq.statistics.dim == d.stats.dim;
        rep.note(consistent, cell, "cur row disagrees with the cl row");
      }
      rep.note(bit_eq(mres.tstat, parse_real(stat[5])), cell,
               "tstat(0, 0, false) differs from the capture");
    }

    // 4. The final mean rule on the captured ModelController rtvals.
    if (c.has_rtval) {
      ++final_checks;
      const bool adds = kalos::final_mean_rule_adds_mean(true, false, true,
          c.rtval);
      rep.note(adds == (c.rtval > kalos::kMeanFinalCutoff), c.id + "/rtval",
               "final mean rule differs from the pinned strict >");
    }
  }

  std::printf("regression tests: %ld checks, %ld failures; %ld process "
              "cases, %ld OBS rows, %ld MeanController rows, %ld final-rule "
              "rtvals\n",
              rep.checks, rep.bad, rvt_cases, obs_rows_used, mean_checks,
              final_checks);
  return rep.bad == 0 ? 0 : 1;
}
