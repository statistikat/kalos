// Full-sweep verification: the native item 21-B transformation-decision unit
// against every pinned LogLevelModule fixture of the transformation-decision
// corpus (validation/evidence/native/transformation-decision; an alternative
// corpus directory may be passed as the single argument).
// The decision bit, branch availability, the statistics counts, the log-
// Jacobian data function and the AICC identity are asserted for every case.
// The branch landing (coefficients, likelihood, AICC) is asserted at fit
// tolerance on the clean fixtures, within widened bands on the two fixtures
// with documented flat-ridge stops (growth_log, cal_column) and not asserted
// on the two fixtures where the discrete Multiple alternate-start sign picks
// the other local basin (reflected_level, seasgrowth_log; their decision
// margins stay above 1.0). See docs/contracts/
// transformation-decision-contract.md.
#include <kalos/transformation_decision.hpp>

#include <algorithm>
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

struct Band {
  bool land;
  double coeff, ll, aicc;
};

// Same qualification tiers as the generated ctest case (contract).
Band band(const std::string& id) {
  if (id == "reflected_level" || id == "seasgrowth_log") return {false, 0, 0, 0};
  if (id == "cal_column") return {true, 2e-2, 5e-1, 5e-1};
  if (id == "growth_log") return {true, 2e-3, 1e-2, 3e-2};
  return {true, 2e-3, 5e-4, 2e-3};
}

double abs_diff(double a, double b) { return std::abs(a - b); }
} // namespace

int main(int argc, char** argv) {
  std::string root = argc > 1 ? argv[1]
                              : "validation/evidence/native/transformation-decision/";
  if (!root.empty() && root.back() != '/') root += '/';
  const auto cases = read_csv(root + "cases.csv");
  const auto inputs = read_csv(root + "inputs.csv");
  if (cases.size() != inputs.size()) {
    std::printf("corpus mismatch: %zu cases vs %zu inputs\n", cases.size(), inputs.size());
    return 1;
  }
  long checks = 0, bad = 0;
  double worst_coefficient = 0.0, worst_likelihood = 0.0;
  for (std::size_t row = 0; row < cases.size(); ++row) {
    const auto& c = cases[row];
    const auto& i = inputs[row];
    const std::string id = unquote(c[0]);
    if (id != unquote(i[0])) {
      std::printf("  %s: corpus misalignment\n", id.c_str());
      ++bad;
      continue;
    }
    kalos::TransformationDecisionRequest request;
    request.aicc_log_correction = real(i[8]);
    auto& b = request.branch;
    for (const auto& t : split(i[9], ';')) b.series.values.push_back(real(t));
    b.series.missing.assign(b.series.values.size(), 0);
    const int xcols = integer(i[10]);
    b.design.columns = static_cast<std::size_t>(xcols);
    if (xcols > 0)
      for (const auto& t : split(i[11], ';')) b.design.values.push_back(real(t));
    b.design.coefficients.assign(b.design.values.size() ? xcols : 0, 0.0);
    b.design.fixed.assign(b.design.coefficients.size(), 0);
    const int sizes[4] = {integer(i[2]), integer(i[3]), integer(i[4]), integer(i[5])};
    for (int blk = 0; blk < 4; ++blk) {
      b.blocks[blk].initial.assign(sizes[blk], 0.0);
      b.blocks[blk].fixed.assign(sizes[blk], 0);
    }
    b.period = integer(i[1]);
    b.d = integer(i[6]);
    b.seasonal_d = integer(i[7]);
    b.control.eps = 1e-7;
    b.control.feps = 1e-4;
    b.control.maximum_likelihood = true;
    b.control.unit_root_search = false;
    b.control.corrected_degrees_of_freedom = true;
    b.control.fast_derivatives = true;
    b.control.starting_point = kalos::EstimationStartingPoint::Multiple;

    const auto result = kalos::decide_transformation(request, {});
    ++checks;
    const bool pinned_log = unquote(c[2]) == "log";
    const bool shortcircuit = integer(c[3]) == 1;
    bool ok = true;
    if (shortcircuit) {
      ok = ok && result.outcome == "nonpositive" && !result.log_chosen &&
          result.aicc_level == 0.0 && result.aicc_log == 0.0 &&
          !result.level.available && !result.log.available;
      std::printf("  %s: short-circuit %s\n", id.c_str(), ok ? "ok" : "MISMATCH");
      if (!ok) ++bad;
      continue;
    }
    const Band bands = band(id);
    ok = ok && result.outcome == "decided" && result.log_chosen == pinned_log &&
        result.level.available && result.log.available;
    ok = ok && kalos::log_level_choose_log(
        result.aicc_level, result.aicc_log, request.aicc_log_correction) == result.log_chosen;
    ok = ok && std::abs((result.aicc_log + request.aicc_log_correction) - result.aicc_level) >
        (bands.land ? 1e-3 : 1.0);
    ok = ok && result.level.jacobian == 0.0 &&
        result.level.adjusted_log_likelihood == result.level.log_likelihood;
    const std::size_t warmup = static_cast<std::size_t>(integer(i[6])) +
        static_cast<std::size_t>(integer(i[7])) * static_cast<std::size_t>(integer(i[1]));
    double sum = 0.0;
    for (std::size_t k = warmup; k < b.series.values.size(); ++k)
      sum += std::log(b.series.values[k]);
    ok = ok && abs_diff(result.log.jacobian, -sum) < 1e-9 * std::max(1.0, sum);
    const int base[2] = {6, 13};
    for (int branch = 0; branch < 2; ++branch) {
      const auto& got = branch == 0 ? result.level : result.log;
      const bool estimated = integer(c[base[branch]]) == 1;
      ok = ok && got.available == estimated;
      if (!estimated) continue;
      std::vector<double> want;
      for (const auto& t : split(unquote(c[base[branch] + 1]), ';')) want.push_back(real(t));
      std::vector<double> actual;
      for (const int blk : {0, 2, 1, 3})
        actual.insert(actual.end(), got.estimation.coefficients[blk].begin(),
            got.estimation.coefficients[blk].end());
      ok = ok && actual.size() == want.size();
      double local_worst_coefficient = 0.0, local_worst_likelihood = 0.0;
      for (std::size_t k = 0; ok && k < want.size(); ++k) {
        local_worst_coefficient = std::max(local_worst_coefficient, abs_diff(actual[k], want[k]));
        if (bands.land) ok = ok && abs_diff(actual[k], want[k]) < bands.coeff;
      }
      local_worst_likelihood = abs_diff(got.log_likelihood, real(c[base[branch] + 2]));
      if (bands.land) ok = ok && local_worst_likelihood < bands.ll;
      ok = ok && got.observations == static_cast<std::size_t>(integer(c[base[branch] + 4])) &&
          got.parameters == static_cast<std::size_t>(integer(c[base[branch] + 5]));
      const double n = static_cast<double>(got.observations);
      const double k2 = static_cast<double>(got.parameters);
      ok = ok && abs_diff(got.aicc, -2.0 * (got.adjusted_log_likelihood - (n * k2) / ((n - k2) - 1.0))) <
          1e-9 * std::max(1.0, std::abs(got.aicc));
      if (bands.land) ok = ok && abs_diff(got.aicc, real(c[base[branch] + 6])) < bands.aicc;
      worst_coefficient = std::max(worst_coefficient, local_worst_coefficient);
      worst_likelihood = std::max(worst_likelihood, local_worst_likelihood);
    }
    std::printf("  %s: %s decision=%s own gap %+.4f vs threshold %+.4f\n", id.c_str(),
        ok ? "ok" : "MISMATCH", result.log_chosen ? "log" : "level",
        result.aicc_log - result.aicc_level, -request.aicc_log_correction);
    if (!ok) ++bad;
  }
  std::printf("transformation-decision: %ld cases, %ld bad, worst coeff %.3g, worst ll %.3g\n",
      checks, bad, worst_coefficient, worst_likelihood);
  return bad == 0 ? 0 : 1;
}
