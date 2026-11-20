// Item 21-H.4: JVM acceptance for the automatic pipeline with non-zero X-11
// horizons (the initialStep forecast route over the preadjusted series).
//
// docs/contracts/arima-forecasts-contract.md and
// docs/contracts/x13-automatic-pipeline-contract.md.  Each case replays a
// base record from tests/jvm_x13_automatic.h with the X-11 spec horizons of
// the capture set (tests/jvm_x13_automatic_horizon.h); the corpus and the
// double-capture protocol live in validation/evidence/native/x13-horizon
// (capture-x13-automatic-horizon.R).
//
// Tier policy (measured, recorded in the capture header):
//   - decisions are BIT-exact: availability, the final description and the
//     transformation/mean flags, plus the table null/non-null pattern;
//   - a1, d18 and the d18a/d18b slices are bit-exact claims (the extended
//     route consumes the pinned banded landing coefficients, the regression
//     is bit-exact, so the adjusted series itself is unshifted);
//   - every other table carries the measured band: the extension inherits
//     the oracle test's landing-coefficients basin drift (worst 1.2e-4 at
//     zero horizon) amplified through the route tail, giving ~1e-6 on
//     a1a/out, ~1e-7 on the split finals, ~2e-4 on the d13 class, ~1e-3 on
//     c17 and ~2e-2 on the e2 outlier-position entries.
//
// Cost policy: the two outlier-detection-on cases run the full automatic
// search twice each (mirror + fit are the same pass); same envelope as the
// zero-horizon oracle.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "kalos/x13_automatic.hpp"

#include "jvm_x13_automatic.h"
#include "jvm_x13_automatic_horizon.h"

namespace {

int g_failures = 0;
long long g_checks = 0;

struct Family {
  long long checks = 0, bad = 0, exact = 0;
  double worst = 0.;
  std::string worst_where;
};
std::map<std::string, Family> g_families;

void tally(const std::string& fam, bool ok, bool exact, double rel,
           const std::string& where) {
  Family& f = g_families[fam];
  ++f.checks;
  if (!ok) {
    ++f.bad;
    ++g_failures;
    std::printf("FAIL %s: %s\n", where.c_str(), fam.c_str());
  }
  if (exact) ++f.exact;
  if (rel > f.worst) {
    f.worst = rel;
    f.worst_where = where;
  }
  ++g_checks;
}

double rdbits(std::uint64_t bits) {
  double v;
  std::memcpy(&v, &bits, sizeof(v));
  return v;
}

bool bit_equal(double a, double b) {
  return std::memcmp(&a, &b, sizeof(double)) == 0;
}

// Bit-exact when band == 0, otherwise bit-equal-or-relative within band.
void check_table(const std::string& case_id, const char* table,
                 const std::vector<double>& got, const std::uint64_t* want,
                 int length, double band) {
  const std::string fam = std::string("hz/") + table;
  const std::string where = case_id + "/" + table;
  if (static_cast<int>(got.size()) != length) {
    tally(fam, false, false, 1., where + " (length)");
    return;
  }
  if (want == nullptr) {  // The JVM table is null; length pins emptiness.
    tally(fam, true, true, 0., where);
    return;
  }
  for (int i = 0; i < length; ++i) {
    const double w = rdbits(want[i]);
    const double g = got[static_cast<std::size_t>(i)];
    if (band == 0.) {
      tally(fam, bit_equal(g, w), true, bit_equal(g, w) ? 0. : 1.,
            where + "[" + std::to_string(i) + "]");
      continue;
    }
    if (bit_equal(g, w)) {
      tally(fam, true, true, 0., where + "[" + std::to_string(i) + "]");
      continue;
    }
    const double rel =
        std::abs(w) > 0. ? std::abs(g - w) / std::abs(w) : std::abs(g);
    tally(fam, std::isfinite(g) && rel <= band, false, rel,
          where + "[" + std::to_string(i) + "]");
  }
}

void check_str(const std::string& fam, const std::string& got,
               const std::string& want, const std::string& where) {
  tally(fam, got == want, got == want, got == want ? 0. : 1., where);
}

void check_int(const std::string& fam, long long got, long long want,
               const std::string& where) {
  tally(fam, got == want, got == want, got == want ? 0. : 1., where);
}

// ---- request mirror (identical to test_x13_automatic_oracle.cpp) ---------

kalos::X13AutomaticRequest mirror_request(const golden_x13auto::Case& c) {
  kalos::X13AutomaticRequest r;
  r.input.reserve(static_cast<std::size_t>(c.n));
  for (int i = 0; i < c.n; ++i) r.input.push_back(rdbits(c.values[i]));
  r.frequency = 12;
  r.first_year = c.first_year;
  r.first_period = c.first_period;
  r.mode = static_cast<kalos::X11DecompositionMode>(c.mode);
  r.aicc_log_correction = rdbits(c.aicc_log_correction);
  r.max_d = 2;
  r.max_bd = 1;
  r.ub1 = rdbits(c.ub1);   // module-space reciprocals, raw bits
  r.ub2 = rdbits(c.ub2);
  r.cancel = rdbits(c.cancel);
  r.differencing_precision = 1e-7;
  r.models_count = 5;
  r.max_p = 2;
  r.max_q = 2;
  r.max_bp = 1;
  r.max_bq = 1;
  r.balanced = c.balanced != 0;
  r.mixed = c.mixed != 0;
  r.arma_accept_white_noise = false;
  r.arma_eps = 1e-7;
  r.outliers_used = c.outliers_used != 0;
  for (int i = 0; i < c.n_types; ++i)
    r.outlier_types.push_back(static_cast<kalos::OutlierType>(c.otypes[i]));
  r.outlier_max_round = static_cast<std::size_t>(c.maxiter);
  r.outlier_max_outliers = 30;
  r.tcrate = rdbits(c.tcrate);
  r.precision = 1e-7;
  r.mean_precision = 1e-5;
  r.va = 0.;
  r.reduce_va = rdbits(c.reduce_va);
  r.ljung_box_limit = rdbits(c.ljung_box_limit);
  r.check_mu = true;
  r.accept_airline = false;
  r.ur_limit = 0.95;
  r.mean_test_initial = 1.96;
  r.mean_test_final = 1.0;
  r.tsig = 1.0;
  r.seasonal.assign(12, kalos::X11SeasonalRequest::msr);
  r.trend_length = 0;
  r.sigma = kalos::X11SigmaPolicy::accurate;
  r.lower_sigma = rdbits(c.lower_sigma);
  r.upper_sigma = rdbits(c.upper_sigma);
  r.backcast = 0;
  r.forecast = 0;
  return r;
}

const golden_x13auto::Case* find_base(const char* id) {
  std::size_t count = 0;
  const golden_x13auto::Case* all = golden_x13auto::table(count);
  for (std::size_t i = 0; i < count; ++i)
    if (std::strcmp(all[i].id, id) == 0) return &all[i];
  return nullptr;
}

void run_case(const golden_x13auto_hz::HzCase& c) {
  std::printf("== %s\n", c.id);
  const golden_x13auto::Case* base = find_base(c.base);
  if (base == nullptr) {
    std::printf("FAIL %s: base case '%s' missing\n", c.id, c.base);
    ++g_failures;
    return;
  }
  kalos::X13AutomaticRequest request = mirror_request(*base);
  request.backcast = c.bc;
  request.forecast = c.fc;
  const kalos::X13AutomaticFitResult result =
      kalos::x13_automatic_fit(request);

  check_str("hz/status", result.status, "available", std::string(c.id) + "/status");
  if (result.status != "available") return;
  const kalos::X13FitResult& fit = result.fit;
  check_int("hz/orders", result.p, c.p, std::string(c.id) + "/p");
  check_int("hz/orders", result.d, c.d, std::string(c.id) + "/d");
  check_int("hz/orders", result.q, c.q, std::string(c.id) + "/q");
  check_int("hz/orders", result.bp, c.bp, std::string(c.id) + "/bp");
  check_int("hz/orders", result.bd, c.bd, std::string(c.id) + "/bd");
  check_int("hz/orders", result.bq, c.bq, std::string(c.id) + "/bq");
  check_int("hz/orders", result.log ? 1 : 0, c.log, std::string(c.id) + "/log");
  check_int("hz/orders", result.mean ? 1 : 0, c.mean, std::string(c.id) + "/mean");

  using golden_x13auto_hz::kTableCount;
  const std::vector<double>* tables[kTableCount] = {
      &fit.preadjustment.a1,   &fit.preadjustment.a1a, &fit.preadjustment.a1b,
      &fit.preadjustment.out,  &fit.d10,               &fit.d11,
      &fit.d12,                &fit.d13,               &fit.c17,
      &fit.finals.d11final,    &fit.finals.d12final,   &fit.finals.d13final,
      &fit.finals.d16,         &fit.finals.d18,        &fit.finals.e1,
      &fit.finals.e2,          &fit.finals.e3,         &fit.finals.e11,
      &fit.finals.d11a,        &fit.finals.d12a,       &fit.finals.d16a,
      &fit.finals.d18a,        &fit.finals.d11b,       &fit.finals.d12b,
      &fit.finals.d16b,        &fit.finals.d18b,
  };
  static const char* const names[kTableCount] = {
      "a1",  "a1a",          "a1b",  "out",  "d10",     "d11",
      "d12", "d13",          "c17",  "d11final", "d12final", "d13final",
      "d16", "d18",          "e1",   "e2",
      "e3",  "e11",          "d11a", "d12a",   "d16a",      "d18a",
      "d11b", "d12b",        "d16b", "d18b",
  };
  for (int t = 0; t < kTableCount; ++t)
    check_table(c.id, names[t], *tables[t], c.bits[t], c.length[t], c.band[t]);
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> only(argv + 1, argv + argc);
  std::size_t count = 0;
  const golden_x13auto_hz::HzCase* cases = golden_x13auto_hz::table(count);
  int ran = 0;
  for (std::size_t i = 0; i < count; ++i) {
    if (!only.empty() &&
        std::find(only.begin(), only.end(), std::string(cases[i].id)) ==
            only.end())
      continue;
    ++ran;
    run_case(cases[i]);
  }
  if (ran == 0) {
    std::printf("no horizon case matched the filter\n");
    return 1;
  }
  std::printf("\n== families ==\n%-16s %8s %8s %8s %10s  %s\n", "family",
              "checks", "bad", "exact", "worst-rel", "worst at");
  long long bad = 0;
  for (const auto& f : g_families) {
    std::printf("%-16s %8lld %8lld %8lld %10.3g  %s\n", f.first.c_str(),
                f.second.checks, f.second.bad, f.second.exact, f.second.worst,
                f.second.worst_where.c_str());
    bad += f.second.bad;
  }
  std::printf("\ncases %d, checks %lld, failures %d : %s\n", ran, g_checks,
              g_failures,
              bad == 0 && g_failures == 0 ? "PASS" : "FAIL");
  return bad == 0 && g_failures == 0 ? 0 : 1;
}
