// Full-sweep verification: the native item 21-A estimation-control layer
// against every pinned RegSarimaComputer fixture of the estimation-control
// corpus (validation/evidence/native/estimation-control; an alternative
// corpus directory may be passed as the first argument). Agreement is
// asserted at fit tolerance because the pinned Levenberg-Marquardt and the
// Kalos BFGS solvers are independent implementations; the measured pinned
// run-to-run spread (Zero against Default start on identical targets) bounds
// the coefficient tolerance at 2e-3 and the likelihood tolerance at 5e-4,
// while the likelihood-only guard cases compare at 1e-9. See
// docs/contracts/estimation-control-contract.md.
#include <kalos/estimation_control.hpp>

#include <cmath>
#include <cstdio>
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

double worst_coeff(const std::vector<double>& got, const std::vector<double>& want) {
  if (got.size() != want.size()) return 1e300;
  double worst = 0.0;
  for (std::size_t i = 0; i < got.size(); ++i)
    worst = std::max(worst, std::abs(got[i] - want[i]));
  return worst;
}
} // namespace

int main(int argc, char** argv) {
  const std::string root =
      argc > 1 ? argv[1] : "validation/evidence/native/estimation-control/";
  const auto cases = read_csv(root + "cases.csv");
  const auto inputs = read_csv(root + "inputs.csv");
  if (cases.size() != inputs.size()) {
    std::printf("corpus mismatch: %zu cases vs %zu inputs\n", cases.size(), inputs.size());
    return 1;
  }
  long checks = 0, bad = 0;
  double worst_coefficient = 0.0, worst_likelihood = 0.0;
  long gate_firings = 0, acceptances = 0;
  for (std::size_t row = 0; row < cases.size(); ++row) {
    const auto& c = cases[row];
    const auto& i = inputs[row];
    const std::string id = unquote(c[0]);
    if (id != unquote(i[0])) {
      std::printf("  %s: corpus misalignment\n", id.c_str());
      ++bad;
      continue;
    }
    kalos::SarimaEstimationRequest r;
    r.series.values = [&] {
      std::vector<double> v;
      for (const auto& t : split(i[8], ';')) v.push_back(real(t));
      return v;
    }();
    r.series.missing.assign(r.series.values.size(), 0);
    for (const auto& m : split(i[9], ';'))
      if (!m.empty()) r.series.missing[integer(m) - 1] = 1;
    r.design.columns = 0;
    if (!i[10].empty()) {
      for (const auto& column : split(unquote(i[10]), '|')) {
        for (const auto& t : split(column, ';')) r.design.values.push_back(real(t));
        ++r.design.columns;
      }
    }
    r.design.coefficients.assign(r.design.columns, 0.0);
    r.design.fixed.assign(r.design.columns, 0);
    const int sizes[4] = {integer(i[1]), integer(i[2]), integer(i[3]), integer(i[4])};
    for (int b = 0; b < 4; ++b) {
      r.blocks[b].initial.assign(sizes[b], 0.0);
      r.blocks[b].fixed.assign(sizes[b], 0);
    }
    r.period = 12;
    r.d = integer(i[5]);
    r.seasonal_d = integer(i[6]);
    r.control = kalos::EstimationControl{1e-7, 1e-4, true, false, true, true,
      unquote(i[7]) == "Zero" ? kalos::EstimationStartingPoint::Zero
                              : kalos::EstimationStartingPoint::Default};
    std::vector<double> want;
    if (!c[4].empty())
      for (const auto& t : split(unquote(c[4]), ';')) want.push_back(real(t));
    const double want_ll = real(c[5]);
    if (r.design.values.size() != r.design.columns * r.series.values.size()) {
      std::printf("  %s: design parse mismatch values=%zu columns=%zu n=%zu\n", id.c_str(),
          r.design.values.size(), r.design.columns, r.series.values.size());
      ++bad;
      continue;
    }
    const auto result = kalos::estimate_sarima_fixed(r, {});
    bool ok = true;
    double coeff_error = 0.0, ll_error = 0.0;
    if (id.rfind("guard_", 0) == 0) {
      ok = result.status == "likelihood_only" && want.empty() && result.passes.empty() &&
           std::abs(result.log_likelihood - want_ll) < 1e-9;
      ll_error = std::abs(result.log_likelihood - want_ll);
      ++checks;
      if (!ok) {
        std::printf("  case %s: guard mismatch status=%s ll=%.17g want %.17g\n", id.c_str(),
            result.status.c_str(), result.log_likelihood, want_ll);
        ++bad;
      }
      continue;
    }
    std::vector<double> got;
    // Stored order [ar, sar, ma, sma]; blocks are [ar, ma, sar, sma].
    got.insert(got.end(), result.coefficients[0].begin(), result.coefficients[0].end());
    got.insert(got.end(), result.coefficients[2].begin(), result.coefficients[2].end());
    got.insert(got.end(), result.coefficients[1].begin(), result.coefficients[1].end());
    got.insert(got.end(), result.coefficients[3].begin(), result.coefficients[3].end());
    coeff_error = worst_coeff(got, want);
    ll_error = std::abs(result.log_likelihood - want_ll);
    ok = coeff_error <= 2e-3 && ll_error <= 5e-4 && result.passes.size() == 3 &&
        result.status != "initial_unavailable";
    // Decision records: pinned acceptance is strict improvement, and a pass
    // that fires must record a coherent decision.
    for (std::size_t k = 1; k < 3 && ok; ++k) {
      const auto& pass = result.passes[k];
      if (pass.accepted) ++acceptances;
      if (pass.gates_passed) ++gate_firings;
      ok = !pass.accepted || (pass.attempted &&
          pass.log_likelihood > result.passes[0].log_likelihood);
    }
    ++checks;
    if (!ok) {
      std::printf("  case %s: status=%s coeff_error=%.3g ll_error=%.3g\n", id.c_str(),
          result.status.c_str(), coeff_error, ll_error);
      ++bad;
    }
    worst_coefficient = std::max(worst_coefficient, coeff_error);
    worst_likelihood = std::max(worst_likelihood, ll_error);
  }
  std::printf("%ld cases, %ld failures; worst coefficient %.3g, worst likelihood %.3g, "
              "gate firings %ld, acceptances %ld\n",
      static_cast<long>(cases.size()), bad, worst_coefficient, worst_likelihood,
      gate_firings, acceptances);
  return bad != 0;
}
