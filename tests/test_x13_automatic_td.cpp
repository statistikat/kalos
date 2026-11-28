// Item 21-I stage-1 golden replay: the CEDM ATD step, its LoP preadjust-vs-
// column fork, and the per-round re-fires, replayed against the frozen JVM
// TD-only battery (validation/evidence/native/calendar-detection, captures/
// run1 + replay/). The oracle table is generated; do not edit by hand.
//
// Pin policy (corpus README): every ATD arm selection, the transformation
// bit, the preadjust fork bit and the final orders are pinned as decision
// bits; the final ll/aicc are pinned as native bit patterns with a 64-ulp
// cross-lane tolerance. Cases flagged agree pin against the JVM capture
// (qualified <= 10 ulp); the remaining cases pin the current native state
// (baseline lock for the documented ODM-boundary divergences and the
// pre-existing clean-corpus gap, established by the m17-td1 replay
// qualification). All pinned decisions carry raw AICC margins >= 0.1.
//
// Cost: one run of all nine fits, each on the JVM battery corpora (96-156
// monthly observations). CMake raises this test's ctest timeout.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "kalos/x13_automatic.hpp"

#include "jvm_x13_automatic_td.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const std::string& msg) {
  ++g_checks;
  if (cond) return;
  ++g_failures;
  std::printf("FAIL [%s]\n", msg.c_str());
}

double from_bits(std::uint64_t u) {
  double d;
  std::memcpy(&d, &u, sizeof(double));
  return d;
}

std::int64_t order_of(double x) {
  std::int64_t i;
  std::memcpy(&i, &x, sizeof(double));
  return i < 0 ? -(i & 0x7FFFFFFFFFFFFFFFLL) - 1 : i;
}

bool close_within_ulp(double got, double want, std::int64_t tol) {
  return std::abs(order_of(got) - order_of(want)) <= tol;
}

// The decoder-shaped automatic request the Td1Probe battery consumed,
// verbatim (drivers/td1_replay.cpp shape()).
kalos::X13AutomaticRequest battery_request(const golden_x13auto_td::Case& c) {
  kalos::X13AutomaticRequest r{};
  r.input.assign(c.series, c.series + c.n);
  r.frequency = 12;
  r.first_period = 1;
  r.first_year = c.start_year;
  r.aicc_log_correction = -2.;
  r.max_d = 2;
  r.max_bd = 1;
  r.ub1 = 1. / 1.0309278350515465;
  r.ub2 = 1. / 1.1363636363636365;
  r.cancel = 0.1;
  r.differencing_precision = 1e-7;
  r.models_count = 5;
  r.max_p = 2;
  r.max_q = 2;
  r.max_bp = 1;
  r.max_bq = 1;
  r.balanced = false;
  r.mixed = true;
  r.arma_accept_white_noise = false;
  r.arma_eps = 1e-7;
  r.outliers_used = true;
  r.outlier_types = {kalos::OutlierType::Additive, kalos::OutlierType::LevelShift,
                     kalos::OutlierType::TransitoryChange};
  r.outlier_max_round = 30;
  r.outlier_max_outliers = 30;
  r.tcrate = 0.7;
  r.precision = 1e-7;
  r.mean_precision = 1e-5;
  r.va = 0.;
  r.reduce_va = 0.14286;
  r.ljung_box_limit = 0.95;
  r.check_mu = true;
  r.accept_airline = false;
  r.ur_limit = 0.95;
  r.mean_test_initial = 1.96;
  r.mean_test_final = 1.;
  r.tsig = 1.;
  r.seasonal.assign(12, kalos::X11SeasonalRequest::msr);
  r.trend_length = 0;
  r.sigma = kalos::X11SigmaPolicy::legacy;
  r.lower_sigma = 1.5;
  r.upper_sigma = 2.5;
  r.msr_policy = kalos::X11MsrIterationPolicy::jdplus;
  r.mode = kalos::X11DecompositionMode::additive;
  r.backcast = 0;
  r.forecast = -1;
  r.automatic_trading_days = true;
  r.td_clustering = c.td_clustering;
  r.td_auto_adjust = c.td_auto_adjust;
  r.td_leap_year = true;
  r.td_aicc_diff = 0.;
  r.td_estimation_eps = 1e-5;
  return r;
}

std::string orders_of(const kalos::X13AutomaticFitResult& r) {
  return std::to_string(r.p) + " " + std::to_string(r.d) + " "
       + std::to_string(r.q) + " " + std::to_string(r.bp) + " "
       + std::to_string(r.bd) + " " + std::to_string(r.bq);
}

void run_case(const golden_x13auto_td::Case& c) {
  const std::string tag = std::string(c.tpl) + "/" + c.corpus;
  const kalos::X13AutomaticFitResult r =
      kalos::x13_automatic_fit(battery_request(c));
  check(r.status == "available", tag + ": available");
  if (r.status != "available") return;

  // Margin discipline: every pinned decision is far from its threshold.
  check(c.log_margin >= 0.1 && c.td_margin >= 0.1, tag + ": margins >= 0.1");

  // Decision bits, JVM-agreeing on all nine runs.
  check(r.log == (c.jvm_log != 0), tag + ": log bit == jvm");
  check(r.log == (c.native_log != 0), tag + ": log bit == pinned");
  check(r.mean == (c.native_mean != 0), tag + ": mean bit pinned");

  // ATD firings: count and the arm selection of the first firing.
  int firings = 0;
  const kalos::AutoAtdRecord* first = nullptr;
  for (const auto& e : r.chain) {
    if (!e.atd) continue;
    if (firings < 4 && c.native_bests[firings] >= 0) {
      check(e.atd->best == c.native_bests[firings],
            tag + ": firing " + std::to_string(firings) + " best pinned");
    }
    ++firings;
    if (!first) first = &*e.atd;
  }
  check(firings == c.native_firings, tag + ": native firing count pinned");
  check(c.jvm_firings >= 1, tag + ": jvm fired");
  check(first != nullptr, tag + ": first firing recorded");
  if (!first) return;
  check(first->best == c.atd_best_first, tag + ": atd0 best bit");
  // Every paired firing ordinal selects the same arm as the JVM battery
  // (tables/atd-firings.csv best_match column, all "yes").
  for (int k = 0; k < 4 && c.jvm_bests[k] >= 0 && c.native_bests[k] >= 0; ++k) {
    check(c.jvm_bests[k] == c.native_bests[k],
          tag + ": firing " + std::to_string(k) + " best == jvm");
  }
  check(first->names.size() == 2, tag + ": two arms recorded");
  for (int k = 0; k < 2; ++k) {
    const double a = first->arms[k].aicc;
    check(std::isfinite(a) && first->arms[k].ssq >= 0.,
          tag + ": arm " + std::to_string(k) + " finite");
  }
  // The winner is the AICC-minimal arm (internal consistency).
  check((first->best == 0) == (first->arms[0].aicc <= first->arms[1].aicc),
        tag + ": best == argmin aicc");

  // The LoP preadjust-vs-column fork, against the JVM lopTransform bit.
  const bool preadjusted = r.auto_calendar && r.auto_calendar->preadjusted;
  check(preadjusted == (std::strcmp(c.jvm_lop, "LeapYear") == 0),
        tag + ": preadjust fork == jvm lopTransform");
  const int pre_bit = r.auto_calendar ? (r.auto_calendar->preadjusted ? 1 : 0)
                                      : -1;
  check(pre_bit == c.native_preadjust, tag + ": preadjust bit pinned");

  // Final shape + likelihood bits.
  check(orders_of(r) == c.native_orders, tag + ": orders pinned");
  if (c.agree) check(orders_of(r) == c.jvm_orders, tag + ": orders == jvm");
  check(static_cast<int>(r.variables.size()) == c.native_n_outliers,
        tag + ": outlier count pinned");
  check(close_within_ulp(r.log_likelihood, from_bits(c.native_ll), 64),
        tag + ": log-likelihood bits (<= 64 ulp)");
  check(close_within_ulp(r.aicc, from_bits(c.native_aicc), 64),
        tag + ": aicc bits (<= 64 ulp)");
}

}  // namespace

int main() {
  std::printf("== 21-I stage-1 TD replay (9 battery cases) ==\n");
  for (std::size_t i = 0; i < golden_x13auto_td::kCaseCount; ++i) {
    run_case(golden_x13auto_td::kCases[i]);
  }
  if (g_failures != 0) {
    std::printf("FAIL: %d/%d checks failed\n", g_failures, g_checks);
    return 1;
  }
  std::printf("PASS: %d checks\n", g_checks);
  return 0;
}
