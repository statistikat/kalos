// Full-sweep verification: the native item 21-A estimation-control layer
// against every pinned RegSarimaComputer fixture of the estimation-control
// corpora (validation/evidence/native/estimation-control and .../estimation-
// control-hr; alternative corpus directories may be passed as arguments).
// Agreement is asserted at fit tolerance because the pinned Levenberg-
// Marquardt and the Kalos BFGS solvers are independent implementations; the
// measured pinned run-to-run spread (Zero against Default start on identical
// targets) bounds the coefficient tolerance at 2e-3 and the likelihood
// tolerance at 5e-4, while the likelihood-only guard cases compare at 1e-9.
// The pass-record shape and selection bits are asserted structurally per
// starting point. See docs/contracts/estimation-control-contract.md.
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

kalos::EstimationStartingPoint parse_start(const std::string& raw) {
  if (raw == "Zero") return kalos::EstimationStartingPoint::Zero;
  if (raw == "Default") return kalos::EstimationStartingPoint::Default;
  if (raw == "HannanRissanen") return kalos::EstimationStartingPoint::HannanRissanen;
  return kalos::EstimationStartingPoint::Multiple;
}

// Stage-shape expectations per starting point (21-A.2): the HR-family starts
// prepend "initialize"; the Multiple policy adds the two internal-precision
// chains, each an estimate plus its two unit-root restarts.
bool stages_shaped(const kalos::SarimaEstimationResult& result,
                   kalos::EstimationStartingPoint start, std::string& why) {
  const auto& p = result.passes;
  const auto at = [&](std::size_t k) { return k < p.size() ? p[k].stage : std::string(); };
  const std::size_t n = p.size();
  const std::size_t trailing = n >= 3 ? n - 3 : 0;
  const bool final_chain = n >= 3 && at(trailing) == "estimate" &&
      at(trailing + 1) == "ur_regular" && at(trailing + 2) == "ur_seasonal";
  if (!final_chain) { why = "final chain shape"; return false; }
  if (start == kalos::EstimationStartingPoint::Zero ||
      start == kalos::EstimationStartingPoint::Default) {
    if (n != 3) { why = "expected three passes"; return false; }
    return true;
  }
  if (p.empty() || at(0) != "initialize" || p[0].detail.empty()) {
    why = "initialize record"; return false;
  }
  if (start == kalos::EstimationStartingPoint::HannanRissanen) {
    if (n != 4) { why = "expected four passes"; return false; }
    if (!p[0].accepted) { why = "HR start not retained"; return false; }
    return true;
  }
  // Multiple: initialize, multiple_first.* chain, multiple_second.* chain,
  // final chain (10 records) — or, only on the documented unavailable-first
  // deviation, initialize, multiple_first.* chain, final chain (7 records).
  const std::size_t expected = p.size() == 7 ? 0 : 3;
  if (n != 7 + expected + 0) { why = "expected seven or ten passes"; return false; }
  const char* chains[2] = {"multiple_first.", "multiple_second."};
  for (std::size_t chain = 0; chain < 2; ++chain) {
    const std::size_t base = 1 + expected * chain;
    if (chain == 1 && expected == 0) break;
    const std::string prefix = chains[chain];
    if (at(base) != prefix + "estimate" || at(base + 1) != prefix + "ur_regular" ||
        at(base + 2) != prefix + "ur_seasonal") {
      why = "chain " + prefix + " shape"; return false;
    }
  }
  int retained = (p[0].accepted ? 1 : 0) + (p[1].accepted ? 1 : 0) +
      (n > 7 ? (p[4].accepted ? 1 : 0) : 0);
  if (retained != 1) { why = "multiple selection retention"; return false; }
  return true;
}

int sweep(const std::string& root, long& checks, long& bad, double& worst_coefficient,
          double& worst_likelihood, long& gate_firings, long& acceptances) {
  const auto cases = read_csv(root + "cases.csv");
  const auto inputs = read_csv(root + "inputs.csv");
  if (cases.size() != inputs.size()) {
    std::printf("corpus mismatch: %zu cases vs %zu inputs\n", cases.size(), inputs.size());
    return 1;
  }
  const long bad_before = bad, gate_before = gate_firings, accept_before = acceptances;
  double local_worst_coefficient = 0.0, local_worst_likelihood = 0.0;
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
    // The HR corpus carries the pinned precisions as cases.csv columns 6/7;
    // the 21-A.1 corpus (six columns) runs the builder defaults 1e-7/1e-4.
    double eps = 1e-7, feps = 1e-4;
    if (c.size() >= 8) { eps = real(c[6]); feps = real(c[7]); }
    const kalos::EstimationStartingPoint start = parse_start(unquote(i[7]));
    r.control = kalos::EstimationControl{eps, feps, true, false, true, true, start};
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
    for (const auto& pass : result.passes)
      if (pass.stage == "initialize")
        std::printf("  %s: initialize detail=%s accepted=%d\n", id.c_str(),
            pass.detail.c_str(), static_cast<int>(pass.accepted));
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
    std::string why;
    ok = coeff_error <= 2e-3 && ll_error <= 5e-4 && stages_shaped(result, start, why) &&
        result.status != "initial_unavailable";
    if (!ok && why.empty() && coeff_error <= 2e-3 && ll_error <= 5e-4)
      why = "status " + result.status;
    // Decision records: one estimate plus two unit-root restarts per chain;
    // a pass that fires must record a coherent decision, and a retained
    // restart must strictly improve its own chain's estimate record.
    std::vector<std::size_t> chain_bases;
    if (ok) {
      const std::size_t n = result.passes.size();
      chain_bases.push_back(n - 3); // Final eps chain.
      if (result.passes[0].stage == "initialize") {
        if (n == 10) { chain_bases.push_back(1); chain_bases.push_back(4); }
        else if (n == 7) chain_bases.push_back(1);
      }
    }
    for (const std::size_t base : chain_bases) {
      for (std::size_t k = base + 1; k <= base + 2 && ok; ++k) {
        const auto& pass = result.passes[k];
        if (pass.accepted) ++acceptances;
        if (pass.gates_passed) ++gate_firings;
        ok = !pass.accepted || (pass.attempted && !std::isnan(pass.log_likelihood) &&
            pass.log_likelihood > result.passes[base].log_likelihood);
      }
      if (ok && base == result.passes.size() - 3 && result.passes[base].accepted) {
        // The final result carries the final chain's retained likelihood.
        // Intermediate Multiple chains only seed the final pass, whose
        // re-optimization at eps may land slightly elsewhere in either
        // direction.
        ok = std::isfinite(result.log_likelihood) &&
            result.log_likelihood >= result.passes[base].log_likelihood;
        if (!ok) why = "result likelihood below retained estimate";
      }
    }
    ++checks;
    if (!ok) {
      std::printf("  case %s: status=%s coeff_error=%.3g ll_error=%.3g %s\n", id.c_str(),
          result.status.c_str(), coeff_error, ll_error, why.c_str());
      ++bad;
    }
    local_worst_coefficient = std::max(local_worst_coefficient, coeff_error);
    local_worst_likelihood = std::max(local_worst_likelihood, ll_error);
  }
  worst_coefficient = std::max(worst_coefficient, local_worst_coefficient);
  worst_likelihood = std::max(worst_likelihood, local_worst_likelihood);
  std::printf("%s: %zu cases, %ld failures; worst coefficient %.3g, worst likelihood %.3g, "
              "gate firings %ld, acceptances %ld\n",
      root.c_str(), cases.size(), bad - bad_before, local_worst_coefficient,
      local_worst_likelihood, gate_firings - gate_before, acceptances - accept_before);
  return bad != bad_before;
}

int main(int argc, char** argv) {
  std::vector<std::string> roots;
  for (int i = 1; i < argc; ++i) roots.emplace_back(argv[i]);
  if (roots.empty())
    roots = {"validation/evidence/native/estimation-control/",
             "validation/evidence/native/estimation-control-hr/"};
  long checks = 0, bad = 0, gate_firings = 0, acceptances = 0;
  double worst_coefficient = 0.0, worst_likelihood = 0.0;
  for (const auto& root : roots)
    sweep(root, checks, bad, worst_coefficient, worst_likelihood, gate_firings, acceptances);
  std::printf("%ld checks, %ld failures; worst coefficient %.3g, worst likelihood %.3g, "
              "gate firings %ld, acceptances %ld\n",
      checks, bad, worst_coefficient, worst_likelihood, gate_firings, acceptances);
  return bad != 0;
}
