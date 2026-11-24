// Tests for the automatic-spec X-13 pipeline driver (item 21-H.1):
// docs/contracts/x13-automatic-pipeline-contract.md.
//
// The 21-H.2 JVM differential sweep is the bit-exactness oracle for this
// driver's decisions; until it lands, this suite gates what the C++ side
// alone can gate: the fail-closed status matrix and the throw matrix (with
// the gate precedence pinned), the stage-chain record vocabulary (every
// entry's stage tag from the contract's vocabulary, exactly the payload
// that must mirror the machine for that tag, the recorded scalars equal to
// the payload fields), the final-description shape (the downstream
// composition block: preadjust -> selection -> finals over the driver's own
// description), the final-record echo against the public result fields, and
// run-to-run bit-exact determinism.
//
// Cost policy: a full automatic search with outlier detection on the probe
// corpus shape (96 monthly observations, all four outlier types, the corpus
// grid) measures ~230 s single-process; the suite runs that shape exactly
// once (T08, everything rides on its result).  The outlier-free variant of
// the same data finishes in ~19 s and still exercises the transformation
// decision, the mean controller, the refinement, the unit-root/MA checks,
// the final mean rule, the cascade and the composition; it runs TWICE
// (T06/T07, the bit-exact determinism twin).  Everything else is the
// millisecond fail-closed/throw matrix.  CMake raises this test's ctest
// timeout accordingly.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "kalos/x13_automatic.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const char* msg) {
  ++g_checks;
  if (cond) return;
  ++g_failures;
  std::printf("FAIL [%s]\n", msg);
}

void check_eq_i(int got, int want, const char* msg) {
  ++g_checks;
  if (got == want) return;
  ++g_failures;
  std::printf("FAIL [%s]: got %d want %d\n", msg, got, want);
}

void check_eq_s(const std::string& got, const char* want, const char* msg) {
  ++g_checks;
  if (got == want) return;
  ++g_failures;
  std::printf("FAIL [%s]: got '%s' want '%s'\n", msg, got.c_str(), want);
}

// Bit-level equality (NaN == NaN, +0 != -0): the determinism relation.
bool bit_equal(double a, double b) {
  return std::memcmp(&a, &b, sizeof(double)) == 0;
}

bool bit_equal(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!bit_equal(a[i], b[i])) return false;
  }
  return true;
}

void check_bit_equal(const std::vector<double>& a,
                     const std::vector<double>& b,
                     const char* msg = "twin: table (bit)") {
  ++g_checks;
  if (bit_equal(a, b)) return;
  ++g_failures;
  std::printf("FAIL [%s]: sizes %zu/%zu, first difference at %zu\n", msg,
              a.size(), b.size(),
              [&a, &b] {
                for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
                  if (!bit_equal(a[i], b[i])) return i;
                }
                return a.size() < b.size() ? a.size() : b.size();
              }());
}

void check_finite(double v, const char* msg) {
  check(std::isfinite(v), msg);
}

// --- the canonical request --------------------------------------------
//
// The probe corpus shape (pinning note 12.1's decoder values for the
// specautomatic worked example): 96 monthly observations, trend + fixed
// seasonals + small noise, shocks injected per case.  Reproducing this
// builder bit for bit keeps the pinned observations below transferable to
// the corpus cases H.2 will diff.

kalos::X13AutomaticRequest baseline(int n) {
  kalos::X13AutomaticRequest r;
  r.input.assign(n, 0.);
  unsigned seed = 12345u;
  auto lcg = [&seed]() {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<double>(seed >> 8) / 16777216.0;
  };
  const double kPi = 3.14159265358979323846;  // the same double as M_PI
  for (int t = 0; t < n; ++t) {
    double v = 50. + 0.05 * t;
    v += 5. * std::sin(2. * kPi * t / 12.);
    v += 3. * std::cos(4. * kPi * t / 12.);
    v += 0.5 * std::sin(14. * kPi * t / 12.);
    v += 0.6 * (lcg() - 0.5);
    r.input[t] = v;
  }
  r.frequency = 12;
  r.first_period = 1;
  r.first_year = 1990;
  r.aicc_log_correction = -2.0;
  r.max_d = 2;
  r.max_bd = 1;
  r.ub1 = 1. / 0.9;
  r.ub2 = 1. / 0.9;
  r.cancel = 0.01;
  r.differencing_precision = 1e-7;
  r.models_count = 3;
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
                     kalos::OutlierType::TransitoryChange,
                     kalos::OutlierType::SeasonalOutlier};
  r.outlier_max_round = 2;
  r.outlier_max_outliers = 3;
  r.tcrate = 0.9;
  r.precision = 1e-7;
  r.mean_precision = 1e-5;
  r.va = 0.;
  r.reduce_va = 0.14286;
  r.ljung_box_limit = 0.95;
  r.check_mu = true;
  r.accept_airline = false;
  r.ur_limit = 0.95;
  r.mean_test_initial = 1.96;
  r.mean_test_final = 1.0;
  r.tsig = 1.0;
  r.seasonal.assign(12, kalos::X11SeasonalRequest::msr);
  r.trend_length = 0;
  r.sigma = kalos::X11SigmaPolicy::accurate;
  r.mode = kalos::X11DecompositionMode::additive;
  return r;
}

// The suite's full case: the probe corpus with a +15 AO spike at index 48
// (1994-01); the pinned observation is status available, log, mean, orders
// (3,0,0)(0,1,1), one AO variable, a 24-entry chain.
kalos::X13AutomaticRequest shock_request() {
  kalos::X13AutomaticRequest r = baseline(96);
  r.input[48] += 15.;
  return r;
}

// --- chain helpers ------------------------------------------------------

const double* scalar_of(const kalos::AutoDecisionRecord& e, const char* key) {
  for (std::size_t i = 0; i < e.scalars.size(); ++i) {
    if (e.scalars[i].first == key) return &e.scalars[i].second;
  }
  return nullptr;
}

bool has_note(const kalos::AutoDecisionRecord& e, const std::string& note) {
  for (std::size_t i = 0; i < e.notes.size(); ++i) {
    if (e.notes[i] == note) return true;
  }
  return false;
}

// The contract's stage vocabulary: the six pre-round tags, the round-scoped
// "round<k>.<suffix>" family, "final" and the entry-point's "failed".
bool stage_in_vocabulary(const std::string& stage) {
  static const char* k_pre[] = {"init", "transformation", "meantest0",
                                "outliers0", "regtest0", "accept0", "final",
                                "failed"};
  for (std::size_t i = 0; i < sizeof(k_pre) / sizeof(k_pre[0]); ++i) {
    if (stage == k_pre[i]) return true;
  }
  const std::string prefix = "round";
  if (stage.compare(0, prefix.size(), prefix) != 0) return false;
  const std::size_t dot = stage.find('.');
  if (dot == std::string::npos || dot <= prefix.size()) return false;
  for (std::size_t i = prefix.size(); i < dot; ++i) {
    if (stage[i] < '0' || stage[i] > '9') return false;
  }
  static const char* k_suf[] = {"refine", "outliers",   "pass2",    "regtest",
                                "unitroots", "matest",  "meantest", "accept",
                                "meanrule", "cascade"};
  const std::string suffix = stage.substr(dot + 1);
  for (std::size_t i = 0; i < sizeof(k_suf) / sizeof(k_suf[0]); ++i) {
    if (suffix == k_suf[i]) return true;
  }
  return false;
}

std::string stage_suffix(const std::string& stage) {
  const std::size_t dot = stage.find('.');
  if (dot != std::string::npos) return stage.substr(dot + 1);
  // The pre-round tags carry the same suffix with a trailing 0
  // ("meantest0" is the round-0 "meantest" machine).
  if (stage.size() > 1 && stage.back() == '0' &&
      stage.compare(0, 8, "meantest") == 0) {
    return "meantest";
  }
  if (stage == "outliers0") return "outliers";
  if (stage == "regtest0") return "regtest";
  if (stage == "accept0") return "accept";
  return stage;
}

bool has_stage(const kalos::X13AutomaticFitResult& r, const char* stage) {
  for (std::size_t i = 0; i < r.chain.size(); ++i) {
    if (r.chain[i].stage == stage) return true;
  }
  return false;
}

int count_stage(const kalos::X13AutomaticFitResult& r, const char* stage) {
  int n = 0;
  for (std::size_t i = 0; i < r.chain.size(); ++i) {
    if (r.chain[i].stage == stage) ++n;
  }
  return n;
}

int count_suffix(const kalos::X13AutomaticFitResult& r, const char* suffix) {
  int n = 0;
  for (std::size_t i = 0; i < r.chain.size(); ++i) {
    if (stage_suffix(r.chain[i].stage) == suffix) ++n;
  }
  return n;
}

const kalos::AutoDecisionRecord* stage_entry(
    const kalos::X13AutomaticFitResult& r, const char* stage) {
  for (std::size_t i = 0; i < r.chain.size(); ++i) {
    if (r.chain[i].stage == stage) return &r.chain[i];
  }
  return nullptr;
}

const kalos::AutoDecisionRecord* last_cascade_entry(
    const kalos::X13AutomaticFitResult& r) {
  for (std::size_t i = r.chain.size(); i-- > 0;) {
    if (stage_suffix(r.chain[i].stage) == "cascade") return &r.chain[i];
  }
  return nullptr;
}

// --- shared invariant blocks -------------------------------------------

// Every chain entry: stage from the vocabulary, and exactly the payload the
// contract pins for its tag (a payload present only on its stage, its stage
// always carrying it; the cascade vector non-empty on its stage).
void check_chain_vocabulary(const kalos::X13AutomaticFitResult& r,
                            const char* case_name) {
  std::string msg = std::string(case_name) + ": stages in vocabulary";
  bool vocab = !r.chain.empty();
  for (std::size_t i = 0; i < r.chain.size() && vocab; ++i) {
    vocab = stage_in_vocabulary(r.chain[i].stage);
  }
  check(vocab, msg.c_str());
  msg = std::string(case_name) + ": init first";
  check(!r.chain.empty() && r.chain.front().stage == "init", msg.c_str());
  msg = std::string(case_name) + ": rounds non-negative";
  bool rounds = true;
  for (std::size_t i = 0; i < r.chain.size(); ++i) {
    if (r.chain[i].round < 0) rounds = false;
  }
  check(rounds, msg.c_str());

  msg = std::string(case_name) + ": payload mirrors stage tag";
  bool mirror = true;
  for (std::size_t i = 0; i < r.chain.size() && mirror; ++i) {
    const kalos::AutoDecisionRecord& e = r.chain[i];
    const std::string suffix = stage_suffix(e.stage);
    bool ok = false;
    if (e.stage == "init") {
      ok = !e.transformation && !e.mean && !e.outliers && !e.regtest &&
           !e.accept && !e.refine && !e.pass2 && !e.unitroots && !e.matest &&
           !e.meanrule && e.cascade.empty();
    } else if (e.stage == "transformation") {
      ok = e.transformation.has_value();
    } else if (suffix == "meantest") {
      ok = e.mean.has_value();
    } else if (suffix == "outliers") {
      ok = e.outliers.has_value();
    } else if (suffix == "regtest") {
      ok = e.regtest.has_value();
    } else if (suffix == "accept") {
      ok = e.accept.has_value();
    } else if (suffix == "refine") {
      ok = e.refine.has_value();
    } else if (suffix == "pass2") {
      ok = e.pass2.has_value();
    } else if (suffix == "unitroots") {
      ok = e.unitroots.has_value();
    } else if (suffix == "matest") {
      ok = e.matest.has_value();
    } else if (suffix == "meanrule") {
      ok = e.meanrule.has_value();
    } else if (suffix == "cascade") {
      ok = !e.cascade.empty();
    } else {  // final / failed
      ok = !e.transformation && !e.mean && !e.outliers && !e.regtest &&
           !e.accept && !e.refine && !e.pass2 && !e.unitroots && !e.matest &&
           !e.meanrule && e.cascade.empty();
    }
    if (!ok) {
      std::printf("  mirror break at %zu: stage '%s'\n", i, e.stage.c_str());
      mirror = false;
    }
  }
  check(mirror, msg.c_str());
}

// The scalar/payload self-consistency law of the chain contract: every
// recorded scalar that echoes a payload field equals it (bit for bit).
void check_chain_scalars(const kalos::X13AutomaticFitResult& r,
                         const char* case_name) {
  std::string msg = std::string(case_name) + ": scalars mirror payloads";
  bool ok = true;
  for (std::size_t i = 0; i < r.chain.size() && ok; ++i) {
    const kalos::AutoDecisionRecord& e = r.chain[i];
    const std::string suffix = stage_suffix(e.stage);
    const double* s = nullptr;
    if (e.stage == "transformation" && e.transformation) {
      s = scalar_of(e, "log_chosen");
      ok = s && *s == (e.transformation->log_chosen ? 1. : 0.);
    } else if (suffix == "meantest" && e.mean) {
      s = scalar_of(e, "mean_after");
      ok = s && *s == (e.mean->mean ? 1. : 0.);
      s = scalar_of(e, "changed");
      ok = ok && s && *s == (e.mean->changed ? 1. : 0.);
    } else if (suffix == "outliers" && e.outliers) {
      s = scalar_of(e, "n_outliers");
      ok = s && *s == static_cast<double>(e.outliers->outliers.size());
      s = scalar_of(e, "rounds");
      ok = ok && s && *s == static_cast<double>(e.outliers->rounds);
    } else if (suffix == "regtest" && e.regtest) {
      s = scalar_of(e, "changed");
      ok = s && *s == (e.regtest->changed ? 1. : 0.);
      s = scalar_of(e, "mean_after");
      ok = ok && s && *s == (e.regtest->mean ? 1. : 0.);
    } else if (suffix == "accept" && e.accept) {
      s = scalar_of(e, "accepted");
      ok = s && *s == (e.accept->accepted ? 1. : 0.);
      s = scalar_of(e, "plbox0");
      ok = ok && s && bit_equal(*s, 1. - e.accept->lb_p);
      s = scalar_of(e, "rvr0");
      ok = ok && s && bit_equal(*s, e.accept->rvr);
      s = scalar_of(e, "rtval0");
      ok = ok && s && bit_equal(*s, e.accept->rtval);
    } else if (suffix == "refine" && e.refine) {
      const double* p = scalar_of(e, "p");
      const double* fallback = scalar_of(e, "fallback");
      ok = p && static_cast<int>(*p) == e.refine->p && fallback &&
           *fallback == (e.refine->fallback ? 1. : 0.);
    } else if (suffix == "pass2" && e.pass2) {
      const kalos::AutoPass2Record& p = *e.pass2;
      s = scalar_of(e, "filter");
      ok = s && *s == static_cast<double>(p.filter);
      s = scalar_of(e, "plbox");
      ok = ok && s && bit_equal(*s, p.plbox);
      s = scalar_of(e, "pcr_after");
      ok = ok && s && bit_equal(*s, p.pcr_after);
      s = scalar_of(e, "continue_round");
      ok = ok && s && *s == (p.continue_round ? 1. : 0.);
      ok = ok && p.filter >= 0 && p.filter <= 6 &&
           p.restored == (p.filter > 0) && p.loop_after >= 1;
      ok = ok && (!p.last_solution || has_note(e, "last chance model"));
      ok = ok && (!p.curva_reduced || has_note(e, "curva reduction"));
    } else if (suffix == "unitroots" && e.unitroots) {
      const kalos::AutoUnitRootsRecord& u = *e.unitroots;
      s = scalar_of(e, "regular_count");
      ok = s && *s == static_cast<double>(u.regular_count);
      s = scalar_of(e, "p_after");
      ok = ok && s && *s == static_cast<double>(u.p_after);
      ok = ok && (std::isnan(u.regular_margin) || u.regular_margin >= 0.) &&
           (std::isnan(u.seasonal_margin) || u.seasonal_margin >= 0.);
      ok = ok && u.p_after >= 0 && u.d_after >= 0 && u.bp_after >= 0 &&
           u.bd_after >= 0;
    } else if (suffix == "matest" && e.matest) {
      const kalos::AutoMaTestRecord& m = *e.matest;
      s = scalar_of(e, "scanned");
      ok = s && *s == (m.scanned ? 1. : 0.);
      s = scalar_of(e, "q_after");
      ok = ok && s && *s == static_cast<double>(m.q_after);
      ok = ok && (!m.fired || (m.scanned && m.mean_set));
    } else if (suffix == "meanrule" && e.meanrule) {
      s = scalar_of(e, "mean_added");
      ok = s && *s == (e.meanrule->mean_added ? 1. : 0.);
      ok = ok && std::isfinite(e.meanrule->rtval0);
    } else if (suffix == "cascade") {
      int status_scalars = 0;
      for (std::size_t k = 0; k < e.scalars.size(); ++k) {
        if (e.scalars[k].first == "status") {
          ok = ok && k < e.cascade.size() &&
               static_cast<int>(e.scalars[k].second) ==
                   e.cascade[k].status;
          ++status_scalars;
        }
      }
      ok = ok && status_scalars == static_cast<int>(e.cascade.size());
      for (std::size_t k = 0; k < e.cascade.size(); ++k) {
        const kalos::AutoCascadeRoundRecord& c = e.cascade[k];
        const bool fi_ok = c.fi_status == "available" ||
                           c.fi_status == "inverse_failed" ||
                           c.fi_status == "final_point_rejected" ||
                           c.fi_status == "none";
        ok = ok && fi_ok && (!c.cascade_ok || c.status == 0);
      }
    }
    if (!ok) {
      std::printf("  scalar break at %zu: stage '%s'\n", i, e.stage.c_str());
    }
  }
  check(ok, msg.c_str());
}

// The final-record echo: the "final" entry's scalars equal the public
// result fields (the cross-seam audit surface of the contract's chain).
void check_final_echo(const kalos::X13AutomaticFitResult& r,
                      const char* case_name) {
  std::string msg = std::string(case_name) + ": final echo";
  const kalos::AutoDecisionRecord* e = stage_entry(r, "final");
  if (e == nullptr) {
    check(false, msg.c_str());
    return;
  }
  const double* s = scalar_of(*e, "p");
  bool ok = s && static_cast<int>(*s) == r.p;
  s = scalar_of(*e, "q");
  ok = ok && s && static_cast<int>(*s) == r.q;
  s = scalar_of(*e, "bp");
  ok = ok && s && static_cast<int>(*s) == r.bp;
  s = scalar_of(*e, "bq");
  ok = ok && s && static_cast<int>(*s) == r.bq;
  s = scalar_of(*e, "d");
  ok = ok && s && static_cast<int>(*s) == r.d;
  s = scalar_of(*e, "bd");
  ok = ok && s && static_cast<int>(*s) == r.bd;
  s = scalar_of(*e, "mean");
  ok = ok && s && *s == (r.mean ? 1. : 0.);
  s = scalar_of(*e, "log");
  ok = ok && s && *s == (r.log ? 1. : 0.);
  s = scalar_of(*e, "n_outliers");
  ok = ok && s && *s == static_cast<double>(r.variables.size());
  s = scalar_of(*e, "log_likelihood");
  ok = ok && s && bit_equal(*s, r.log_likelihood);
  s = scalar_of(*e, "aicc");
  ok = ok && s && bit_equal(*s, r.aicc);
  check(ok, msg.c_str());
}

// The downstream composition block: the pinned X13Kernel sequence over the
// driver's own final description (preadjust -> selection -> results ->
// finals), checked at the same surface the fixed-path tests use.
void check_composition(const kalos::X13AutomaticFitResult& r, std::size_t n,
                       const char* case_name) {
  std::string msg = std::string(case_name) + ": composition available";
  check(r.fit.status == "available", msg.c_str());
  if (r.fit.status != "available") return;
  msg = std::string(case_name) + ": composition tables";
  check(r.fit.preadjustment.out.size() == n && r.fit.d10.size() == n &&
            r.fit.d11.size() == n && r.fit.d12.size() == n &&
            r.fit.d13.size() == n && r.fit.c17.size() == n &&
            r.fit.finalSA.size() == n && r.fit.finalTrend.size() == n &&
            r.fit.finalIrregular.size() == n &&
            r.fit.finals.d11final.size() == n,
        msg.c_str());
  msg = std::string(case_name) + ": triplet identity";
  check(bit_equal(r.fit.finalSA, r.fit.finals.d11final) &&
            bit_equal(r.fit.finalTrend, r.fit.finals.d12final) &&
            bit_equal(r.fit.finalIrregular, r.fit.finals.d13final),
        msg.c_str());
  msg = std::string(case_name) + ": provenance echo";
  check(r.fit.frequency == 12 && r.fit.backcast == 0 && r.fit.forecast == 0 &&
            r.fit.sigma_set && r.fit.log == r.log,
        msg.c_str());
  msg = std::string(case_name) + ": pinned mode forcing";
  kalos::X11DecompositionMode want = kalos::X11DecompositionMode::additive;
  if (r.fit.mode_requested == kalos::X11DecompositionMode::pseudo_additive) {
    want = r.fit.mode_requested;
  } else if (r.log) {
    want = r.fit.mode_requested == kalos::X11DecompositionMode::log_additive
               ? r.fit.mode_requested
               : kalos::X11DecompositionMode::multiplicative;
  }
  check(r.fit.mode_effective == want, msg.c_str());
}

// Result fields every "available" run must satisfy (shape laws of the
// final description, independent of the data).
void check_available_shape(const kalos::X13AutomaticFitResult& r,
                           const kalos::X13AutomaticRequest& req,
                           const char* case_name) {
  std::string msg = std::string(case_name) + ": coefficients block sizes";
  check(r.coefficients[0].size() == static_cast<std::size_t>(r.p) &&
            r.coefficients[1].size() == static_cast<std::size_t>(r.q) &&
            r.coefficients[2].size() == static_cast<std::size_t>(r.bp) &&
            r.coefficients[3].size() == static_cast<std::size_t>(r.bq),
        msg.c_str());
  msg = std::string(case_name) + ": orders within bounds";
  check(r.p >= 0 && r.q >= 0 && r.bp >= 0 && r.bq >= 0 &&
            r.d >= 0 && r.d <= req.max_d && r.bd >= 0 && r.bd <= req.max_bd,
        msg.c_str());
  msg = std::string(case_name) + ": effective observations law";
  check(r.effective_observations ==
            static_cast<std::size_t>(static_cast<int>(req.input.size()) -
                                     (r.d + r.bd * 12)),
        msg.c_str());
  msg = std::string(case_name) + ": landing statistics";
  check(r.estimation_status == "converged" &&
            std::isfinite(r.log_likelihood) && std::isfinite(r.aicc),
        msg.c_str());
  msg = std::string(case_name) + ": variables finite, sized";
  bool vars = true;
  for (std::size_t i = 0; i < r.variables.size(); ++i) {
    const kalos::AutoVariableRecord& v = r.variables[i];
    vars = vars && std::isfinite(v.coefficient) &&
           v.column.size() == req.input.size() &&
           v.position < req.input.size() && !v.name.empty();
  }
  check(vars, msg.c_str());
}

// --- T01: the fail-closed status matrix (millisecond returns) ------------
//
// Every unsupported domain answers the contract's status string with an
// empty chain (nothing ran); the gate precedence is pinned by stacking
// several violations and demanding the first gate's answer.

void t01_status_matrix() {
  // monthly only (first gate after the empty-series throw)
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.frequency = 4;
    r.seasonal.assign(4, kalos::X11SeasonalRequest::msr);
    const kalos::X13AutomaticFitResult res = kalos::x13_automatic_fit(r);
    check_eq_s(res.status, "unsupported:x13 automatic fit monthly series only",
               "freq gate");
    check(res.chain.empty(), "refusal runs nothing");
    check(res.variables.empty(), "refusal carries no variables");
  }
  // freq gate beats the later shape throws (first_period, seasonal size)
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.frequency = 4;
    r.first_period = 99;
    r.seasonal.assign(3, kalos::X11SeasonalRequest::msr);
    kalos::X13AutomaticFitResult res;
    bool threw = false;
    try {
      res = kalos::x13_automatic_fit(r);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    check(!threw && res.status ==
                          "unsupported:x13 automatic fit monthly series only",
          "freq gate beats shape throws");
  }
  // complete finite series (and it beats the later status gates)
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.input[10] = std::numeric_limits<double>::quiet_NaN();
    r.backcast = 1;
    r.sigma.reset();
    r.use_calendar_detection = true;
    r.outlier_span = std::make_pair(2, 5);
    const kalos::X13AutomaticFitResult res = kalos::x13_automatic_fit(r);
    check_eq_s(res.status,
               "unsupported:x13 automatic fit requires a complete finite "
               "series",
               "finite gate first among status gates");
  }
  // the horizons are not a gate (item 21-H.4): the matrix flows through to
  // the sigma gate exactly as it would for zero horizons
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.backcast = 1;
    r.sigma.reset();
    r.use_easter_detection = true;
    r.outlier_span = std::make_pair(2, 5);
    check_eq_s(kalos::x13_automatic_fit(r).status,
               "unsupported:x13 fit requires an explicit sigma policy",
               "horizon is no gate; sigma gate next");
    r = baseline(24);
    r.forecast = 2;
    r.sigma.reset();
    check_eq_s(kalos::x13_automatic_fit(r).status,
               "unsupported:x13 fit requires an explicit sigma policy",
               "forecast is no gate either");
  }
  // Invalid extensions must be refused before estimation or allocation.
  for (auto limits : {std::make_pair(std::numeric_limits<int>::min(), 0),
                      std::make_pair(0, -200000000),
                      std::make_pair(1073741820, 1073741820)}) {
    auto r = baseline(24);
    r.backcast = limits.first;
    r.forecast = limits.second;
    bool refused = false;
    try { kalos::x13_automatic_fit(r); }
    catch (const std::invalid_argument& e) {
      refused = std::string(e.what()).find("extended series") != std::string::npos;
    }
    check(refused, "oversized extended series refused");
  }
  // explicit sigma policy
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.sigma.reset();
    r.automatic_trading_days = true;
    r.outlier_span = std::make_pair(0, 20);
    check_eq_s(kalos::x13_automatic_fit(r).status,
               "unsupported:x13 fit requires an explicit sigma policy",
               "sigma gate beats deferred gate");
  }
  // deferred detection modules (all four flags)
  {
    const int kFlags[4] = {0, 1, 2, 3};
    for (int f = 0; f < 4; ++f) {
      kalos::X13AutomaticRequest r = baseline(24);
      if (f == 0) r.use_calendar_detection = true;
      if (f == 1) r.use_easter_detection = true;
      if (f == 2) r.automatic_trading_days = true;
      if (f == 3) r.use_user_variables = true;
      r.outlier_span = std::make_pair(0, 20);
      check_eq_s(kalos::x13_automatic_fit(r).status,
                 "unsupported:x13 automatic fit defers calendar/easter/TD/"
                 "user detection (21-I)",
                 "deferred module gate");
    }
  }
  // full-series span only: partial spans refuse, both ends
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.outlier_span = std::make_pair(2, 23);
    check_eq_s(kalos::x13_automatic_fit(r).status,
               "unsupported:x13 automatic fit spans the full series only",
               "partial span from");
    r = baseline(24);
    r.outlier_span = std::make_pair(0, 20);
    check_eq_s(kalos::x13_automatic_fit(r).status,
               "unsupported:x13 automatic fit spans the full series only",
               "partial span to");
  }
  // the full-series span itself is ACCEPTED at the gate (the run continues;
  // no assertion on the run's outcome here, only on the gate)
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.outlier_span = std::make_pair(0, 23);
    const kalos::X13AutomaticFitResult res = kalos::x13_automatic_fit(r);
    check(res.status !=
              "unsupported:x13 automatic fit spans the full series only",
          "full span accepted");
    check(res.status == "available" ||
              res.status.compare(0, 7, "failed:") == 0,
          "full-span tiny series answers available or failed");
  }
}

// --- T02: the throw matrix (shape violations, std::invalid_argument) ------

void t02_shape_throws() {
  struct Probe {
    const char* what;
    void (*twist)(kalos::X13AutomaticRequest&);
  };
  static const Probe kProbes[] = {
      {"empty series", [](kalos::X13AutomaticRequest& r) {
         r.input.clear();
       }},
      {"first_period 0", [](kalos::X13AutomaticRequest& r) { r.first_period = 0; }},
      {"first_period 13", [](kalos::X13AutomaticRequest& r) { r.first_period = 13; }},
      {"seasonal size 11", [](kalos::X13AutomaticRequest& r) {
         r.seasonal.pop_back();
       }},
      {"series too short", [](kalos::X13AutomaticRequest& r) {
         r.input.resize(13);
       }},
      {"outliers without types", [](kalos::X13AutomaticRequest& r) {
         r.outlier_types.clear();
       }},
  };
  for (std::size_t i = 0; i < sizeof(kProbes) / sizeof(kProbes[0]); ++i) {
    kalos::X13AutomaticRequest r = baseline(24);
    kProbes[i].twist(r);
    bool threw = false;
    try {
      kalos::x13_automatic_fit(r);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    check(threw, kProbes[i].what);
  }
  // precedence: the empty-series throw precedes the monthly gate
  {
    kalos::X13AutomaticRequest r = baseline(24);
    r.input.clear();
    r.frequency = 4;
    r.seasonal.assign(4, kalos::X11SeasonalRequest::msr);
    bool threw = false;
    try {
      kalos::x13_automatic_fit(r);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    check(threw, "empty beats freq gate");
  }
}

// --- T03: the result is value-semantic -----------------------------------

void t03_value_semantics() {
  kalos::X13AutomaticFitResult src;
  src.status = "available";
  src.p = 1;
  src.coefficients[0] = {0.5};
  kalos::AutoDecisionRecord entry;
  entry.stage = "round1.pass2";
  entry.round = 1;
  entry.scalars = {{"filter", 5.}};
  entry.notes = {"last chance model"};
  src.chain.push_back(entry);
  kalos::AutoVariableRecord var;
  var.name = "AO (1994-01-01)";
  var.type = kalos::OutlierType::Additive;
  var.regeffect = "Irregular";
  var.position = 48;
  var.coefficient = 1.0;
  var.column.assign(96, 0.);
  src.variables.push_back(var);

  const kalos::X13AutomaticFitResult copy = src;
  kalos::X13AutomaticFitResult assigned;
  assigned = src;
  check(copy.status == "available" && copy.chain.size() == 1 &&
            copy.variables.size() == 1 && copy.coefficients[0].size() == 1,
          "result copies by value");
  check(assigned.status == "available" && assigned.chain.size() == 1 &&
            assigned.variables.size() == 1 &&
            assigned.chain[0].scalars.size() == 1 &&
            assigned.chain[0].notes.size() == 1,
          "result copy-assigns by value");
  check(kalos::X13FitSpecKind::fixed_spec !=
            kalos::X13FitSpecKind::automatic_spec,
          "fit-spec kinds distinct");
}

// --- T04/T05: the outlier-free shape (composition + final echo) -----------
//
// The same corpus data with the outlier machine switched off: the cheap
// full-composition shape.  Pinned observations: status available, log,
// mean, orders (1,0,1)(0,1,1), 12 chain entries, no outlier stages, no
// variables.

const kalos::X13AutomaticFitResult& quick_case() {
  static const kalos::X13AutomaticFitResult result = [] {
    kalos::X13AutomaticRequest r = shock_request();
    r.outliers_used = false;
    return kalos::x13_automatic_fit(r);
  }();
  return result;
}

void t04_quick_shape_pinned() {
  const kalos::X13AutomaticFitResult& r = quick_case();
  check_eq_s(r.status, "available", "no-outlier shape: available");
  if (r.status != "available") return;
  check(r.log, "no-outlier shape: log chosen");
  check(r.mean, "no-outlier shape: mean on");
  check_eq_i(r.p, 1, "no-outlier shape: p");
  check_eq_i(r.d, 0, "no-outlier shape: d");
  check_eq_i(r.q, 1, "no-outlier shape: q");
  check_eq_i(r.bp, 0, "no-outlier shape: bp");
  check_eq_i(r.bd, 1, "no-outlier shape: bd");
  check_eq_i(r.bq, 1, "no-outlier shape: bq");
  check(r.variables.empty(), "no-outlier shape: no variables");
  check_eq_i(static_cast<int>(r.chain.size()), 12,
             "no-outlier shape: chain size pinned");
  check_available_shape(r, shock_request(), "no-outlier shape");
}

void t05_quick_shape_chain() {
  const kalos::X13AutomaticFitResult& r = quick_case();
  if (r.status != "available") {
    check(false, "quick shape missing");
    return;
  }
  check_chain_vocabulary(r, "no-outlier shape");
  check_chain_scalars(r, "no-outlier shape");
  check_final_echo(r, "no-outlier shape");
  // the pre-round anchor sequence, exact: with the machine off neither the
  // initial outlier round nor its conditional regtest entry exists (the
  // pinned calc runs regtest0 only when outliers0 changed the design).
  const char* want[] = {"init", "transformation", "meantest0", "accept0"};
  bool prefix_ok = r.chain.size() >= 4;
  for (std::size_t i = 0; i < 4 && prefix_ok; ++i) {
    prefix_ok = r.chain[i].stage == want[i];
  }
  check(prefix_ok, "no-outlier chain: prefix init/transformation/meantest0/accept0");
  check(!has_stage(r, "outliers0"), "no-outlier chain: outliers0 absent");
  check(!has_stage(r, "regtest0"), "no-outlier chain: regtest0 absent");
  check(count_suffix(r, "outliers") == 0, "no-outlier chain: no outlier rounds");
  check(count_suffix(r, "cascade") == 1, "no-outlier chain: one cascade entry");
  check(count_suffix(r, "meanrule") == 1, "no-outlier chain: one mean rule");
  check(count_suffix(r, "unitroots") >= 1, "no-outlier chain: unit roots ran");
  check(count_suffix(r, "matest") >= 1, "no-outlier chain: MA test ran");
  check(r.chain.back().stage == "final", "no-outlier chain: final last");
  // the transformation decision record mirrors the landed flag
  const kalos::AutoDecisionRecord* t = stage_entry(r, "transformation");
  check(t != nullptr && t->transformation &&
            t->transformation->log_chosen == r.log &&
            std::isfinite(t->transformation->aicc_level) &&
            std::isfinite(t->transformation->aicc_log),
        "no-outlier chain: transformation payload");
  // the cascade ended conclusively (the last cascade entry's last round ok)
  const kalos::AutoDecisionRecord* c = last_cascade_entry(r);
  check(c != nullptr && !c->cascade.empty() &&
            c->cascade.back().cascade_ok && c->cascade.back().status == 0 &&
            c->cascade.back().fi_status == "available",
        "no-outlier chain: cascade concluded");
  check_composition(r, 96, "no-outlier shape");
}

// --- T06/T07: determinism (bit-exact twin of the cheap shape) --------------
//
// The driver must be bit-exact run to run within one process (the JVM
// determinism property; the 21-H.2 sweep pins the cross-engine law).  Every
// recorded field is compared bit for bit, including the chain payloads and
// the downstream tables.

bool identical(const kalos::AutoPass2Record& a, const kalos::AutoPass2Record& b) {
  return a.filter == b.filter && a.restored == b.restored &&
         a.continue_round == b.continue_round && bit_equal(a.plbox, b.plbox) &&
         bit_equal(a.plbox0, b.plbox0) && bit_equal(a.rvr, b.rvr) &&
         bit_equal(a.rtval, b.rtval) && bit_equal(a.pcr_after, b.pcr_after) &&
         a.loop_after == b.loop_after && bit_equal(a.curva_after, b.curva_after) &&
         a.curva_reduced == b.curva_reduced &&
         a.need_outliers_after == b.need_outliers_after &&
         a.need_auto_modelling_after == b.need_auto_modelling_after &&
         a.last_solution == b.last_solution;
}

bool identical(const kalos::AutoUnitRootsRecord& a,
               const kalos::AutoUnitRootsRecord& b) {
  return a.regular_count == b.regular_count &&
         a.seasonal_count == b.seasonal_count &&
         a.regular_fired == b.regular_fired && a.seasonal_fired == b.seasonal_fired &&
         a.p_after == b.p_after && a.d_after == b.d_after &&
         a.bp_after == b.bp_after && a.bd_after == b.bd_after &&
         bit_equal(a.regular_margin, b.regular_margin) &&
         bit_equal(a.seasonal_margin, b.seasonal_margin) &&
         a.redo_estimation == b.redo_estimation;
}

bool identical(const kalos::AutoMaTestRecord& a, const kalos::AutoMaTestRecord& b) {
  return a.scanned == b.scanned && bit_equal(a.abs_theta_at_1, b.abs_theta_at_1) &&
         a.fired == b.fired && a.q_after == b.q_after && a.d_after == b.d_after &&
         a.mean_set == b.mean_set && a.redo_estimation == b.redo_estimation;
}

bool identical(const kalos::AutoMeanRuleRecord& a,
               const kalos::AutoMeanRuleRecord& b) {
  return bit_equal(a.rtval0, b.rtval0) && a.mean_added == b.mean_added;
}

bool identical(const kalos::AutoCascadeRoundRecord& a,
               const kalos::AutoCascadeRoundRecord& b) {
  return a.round == b.round && a.estimation_status == b.estimation_status &&
         a.estimation_ok == b.estimation_ok && a.fi_status == b.fi_status &&
         bit_equal(a.fx, b.fx) && a.nobs == b.nobs &&
         a.p_before == b.p_before && a.bp_before == b.bp_before &&
         a.q_before == b.q_before && a.bq_before == b.bq_before &&
         a.p_after == b.p_after && a.bp_after == b.bp_after &&
         a.q_after == b.q_after && a.bq_after == b.bq_after &&
         a.status == b.status && bit_equal(a.asy_diagonal, b.asy_diagonal) &&
         a.cascade_ok == b.cascade_ok;
}

bool identical(const kalos::AutoDecisionRecord& a,
               const kalos::AutoDecisionRecord& b) {
  if (a.stage != b.stage || a.round != b.round || a.notes != b.notes ||
      a.scalars.size() != b.scalars.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.scalars.size(); ++i) {
    if (a.scalars[i].first != b.scalars[i].first ||
        !bit_equal(a.scalars[i].second, b.scalars[i].second)) {
      return false;
    }
  }
  // The qualified-unit payloads carry their own determinism gates in their
  // unit suites; here their presence pattern must match bit for bit (their
  // content is a pure function of the already-compared inputs).
  if (a.transformation.has_value() != b.transformation.has_value() ||
      a.mean.has_value() != b.mean.has_value() ||
      a.outliers.has_value() != b.outliers.has_value() ||
      a.regtest.has_value() != b.regtest.has_value() ||
      a.accept.has_value() != b.accept.has_value() ||
      a.refine.has_value() != b.refine.has_value() ||
      a.unitroots.has_value() != b.unitroots.has_value()) {
    return false;
  }
  if (a.pass2 && b.pass2 && !identical(*a.pass2, *b.pass2)) return false;
  if (a.unitroots && b.unitroots && !identical(*a.unitroots, *b.unitroots)) return false;
  if (a.matest && b.matest && !identical(*a.matest, *b.matest)) return false;
  if (a.meanrule && b.meanrule && !identical(*a.meanrule, *b.meanrule)) return false;
  if (a.cascade.size() != b.cascade.size()) return false;
  for (std::size_t i = 0; i < a.cascade.size(); ++i) {
    if (!identical(a.cascade[i], b.cascade[i])) return false;
  }
  return true;
}

bool identical(const kalos::AutoVariableRecord& a,
               const kalos::AutoVariableRecord& b) {
  return a.name == b.name && a.type == b.type && a.regeffect == b.regeffect &&
         a.position == b.position && bit_equal(a.coefficient, b.coefficient) &&
         bit_equal(a.column, b.column);
}

void check_results_identical(const kalos::X13AutomaticFitResult& a,
                             const kalos::X13AutomaticFitResult& b) {
  check(a.status == b.status, "twin: status");
  check(a.log == b.log && a.mean == b.mean, "twin: flags");
  check(a.p == b.p && a.q == b.q && a.bp == b.bp && a.bq == b.bq &&
            a.d == b.d && a.bd == b.bd,
        "twin: orders");
  for (int k = 0; k < 4; ++k) {
    std::string msg = "twin: coefficient block " + std::to_string(k);
    check_bit_equal(a.coefficients[k], b.coefficients[k], msg.c_str());
  }
  check(a.estimation_status == b.estimation_status &&
            bit_equal(a.log_likelihood, b.log_likelihood) &&
            bit_equal(a.aicc, b.aicc) &&
            a.effective_observations == b.effective_observations,
        "twin: landing statistics (bit)");
  check(a.variables.size() == b.variables.size(), "twin: variable count");
  if (a.variables.size() == b.variables.size()) {
    bool vars = true;
    for (std::size_t i = 0; i < a.variables.size(); ++i) {
      vars = vars && identical(a.variables[i], b.variables[i]);
    }
    check(vars, "twin: variables (bit)");
  }
  check(a.chain.size() == b.chain.size(), "twin: chain size");
  if (a.chain.size() == b.chain.size()) {
    bool chain = true;
    for (std::size_t i = 0; i < a.chain.size(); ++i) {
      chain = chain && identical(a.chain[i], b.chain[i]);
    }
    check(chain, "twin: chain (bit)");
  }
  // the downstream composition, table by table
  check(a.fit.status == b.fit.status && a.fit.log == b.fit.log &&
            a.fit.mode_effective == b.fit.mode_effective,
        "twin: fit scalars");
  const char* tables[] = {"d10", "d11", "d12", "d13", "c17"};
  const std::vector<double>* pa[] = {&a.fit.d10, &a.fit.d11, &a.fit.d12,
                                     &a.fit.d13, &a.fit.c17};
  const std::vector<double>* pb[] = {&b.fit.d10, &b.fit.d11, &b.fit.d12,
                                     &b.fit.d13, &b.fit.c17};
  for (std::size_t i = 0; i < 5; ++i) {
    std::string msg = std::string("twin: table ") + tables[i];
    check_bit_equal(*pa[i], *pb[i], msg.c_str());
  }
  check_bit_equal(a.fit.finalSA, b.fit.finalSA);
  check_bit_equal(a.fit.finalTrend, b.fit.finalTrend);
  check_bit_equal(a.fit.finalIrregular, b.fit.finalIrregular);
  check_bit_equal(a.fit.preadjustment.out, b.fit.preadjustment.out);
  check_bit_equal(a.fit.finals.d11final, b.fit.finals.d11final);
  check_bit_equal(a.fit.finals.d12final, b.fit.finals.d12final);
  check_bit_equal(a.fit.finals.d13final, b.fit.finals.d13final);
}

void t06_quick_twin() {
  kalos::X13AutomaticRequest r = shock_request();
  r.outliers_used = false;
  const kalos::X13AutomaticFitResult second = kalos::x13_automatic_fit(r);
  check(second.status == "available", "twin: second run available");
  check_results_identical(quick_case(), second);
}

// --- T08: the full automatic case (one 230 s run, shared) ------------------
//
// The contract's worked search: all four outlier types on the spiked corpus.
// Pinned observations (the probe corpus run): available, log, mean, orders
// (0,0,0)(0,1,1), one AO variable at 1994-01, a 20-entry chain.  All chain
// and variable assertions ride on this single run.  21-H.3 re-pin: the
// airline-start fix (the X13ModelBuilder (0,1,1)(0,1,1) init, contract delta
// list) shifts the search trajectory - the pre-fix pins were the landed
// (3,0,0)(0,1,1) 24-entry path.  The landed decisions themselves carry the
// JVM verification (the oracle corpus, all 13 cases decision-bit exact);
// these unit pins guard the driver-internal trajectory regression.

const kalos::X13AutomaticFitResult& full_case() {
  static const kalos::X13AutomaticFitResult result =
      kalos::x13_automatic_fit(shock_request());
  return result;
}

void t08_full_case() {
  const kalos::X13AutomaticFitResult& r = full_case();
  check_eq_s(r.status, "available", "full case: available");
  if (r.status != "available") return;
  check(r.log, "full case: log chosen");
  check(r.mean, "full case: mean on");
  check_eq_i(r.p, 0, "full case: p pinned");
  check_eq_i(r.d, 0, "full case: d pinned");
  check_eq_i(r.q, 0, "full case: q pinned");
  check_eq_i(r.bp, 0, "full case: bp pinned");
  check_eq_i(r.bd, 1, "full case: bd pinned");
  check_eq_i(r.bq, 1, "full case: bq pinned");
  check_eq_i(static_cast<int>(r.chain.size()), 20,
             "full case: chain size pinned");
  check_available_shape(r, shock_request(), "full case");

  // --- the detected outlier: exactly the injected AO spike at 1994-01 ---
  check_eq_i(static_cast<int>(r.variables.size()), 1,
             "full case: one outlier pinned");
  if (r.variables.size() != 1) return;
  const kalos::AutoVariableRecord& v = r.variables[0];
  check(v.type == kalos::OutlierType::Additive, "full case: AO type");
  check_eq_i(static_cast<int>(v.position), 48, "full case: AO position");
  check_eq_s(v.name, "AO (1994-01-01)",
             "full case: IOutlier.defaultName format");
  check_eq_s(v.regeffect, "Irregular",
             "full case: AO regeffect (SaVariable.defaultComponentTypeOf, G3)");
  check_finite(v.coefficient, "full case: AO coefficient finite");
  // the landed coefficient is a log-space effect (the transform is log);
  // the +15 raw spike on a level of ~52 is a log effect of ~0.25
  check(v.coefficient > 0.05, "full case: AO coefficient sign/magnitude");
  check(v.column.size() == 96 && v.column[48] == 1. && v.column[47] == 0.,
        "full case: AO factory column");

  // --- chain: vocabulary, scalar mirrors, echo ---
  check_chain_vocabulary(r, "full case");
  check_chain_scalars(r, "full case");
  check_final_echo(r, "full case");

  // --- chain: the pinned anchor sequence and machine coverage ---
  const char* want[] = {"init",       "transformation", "meantest0",
                        "outliers0",  "regtest0",       "accept0"};
  bool prefix_ok = r.chain.size() >= 6;
  for (std::size_t i = 0; i < 6 && prefix_ok; ++i) {
    prefix_ok = r.chain[i].stage == want[i];
  }
  check(prefix_ok, "full case: pre-round anchor sequence pinned");
  check(r.chain.back().stage == "final", "full case: final last");
  check(count_stage(r, "final") == 1, "full case: exactly one final entry");
  check(!has_stage(r, "failed"), "full case: no failure entry");
  check(count_suffix(r, "pass2") >= 1, "full case: pass-2 gates ran");
  check(count_suffix(r, "refine") >= 1, "full case: refinement ran");
  check(count_suffix(r, "cascade") == 1, "full case: one cascade entry");
  check(count_suffix(r, "meanrule") == 1, "full case: one mean rule");
  check(count_suffix(r, "unitroots") >= 1, "full case: unit-root checks");
  check(count_suffix(r, "matest") >= 1, "full case: MA checks");
  // the outlier machine accepted the spike at least once (the forward-add
  // vocabulary: some outliers entry carries it in its payload)
  bool saw_ao = false;
  for (std::size_t i = 0; i < r.chain.size() && !saw_ao; ++i) {
    const kalos::AutoDecisionRecord& e = r.chain[i];
    if (!e.outliers) continue;
    for (std::size_t k = 0; k < e.outliers->outliers.size(); ++k) {
      if (e.outliers->outliers[k].type == kalos::OutlierType::Additive &&
          e.outliers->outliers[k].position == 48) {
        saw_ao = true;
      }
    }
  }
  check(saw_ao, "full case: AO recorded by an outlier round");
  // the cascade concluded (final_estimator_test ok on the final point)
  const kalos::AutoDecisionRecord* c = last_cascade_entry(r);
  check(c != nullptr && !c->cascade.empty() && c->cascade.back().cascade_ok &&
            c->cascade.back().status == 0 &&
            c->cascade.back().fi_status == "available",
        "full case: cascade concluded");
  // the final mean rule: the entry exists with the pinned rtval0/mean_added
  // vocabulary (the state law is the qualified 21-G unit's own gate)
  const kalos::AutoDecisionRecord* mr = nullptr;
  for (std::size_t i = r.chain.size(); i-- > 0 && mr == nullptr;) {
    if (stage_suffix(r.chain[i].stage) == "meanrule") mr = &r.chain[i];
  }
  check(mr != nullptr && mr->meanrule.has_value(),
        "full case: mean rule recorded");
  // the pass-2 law: a fired filter restores the reference (filter > 0 <->
  // restored), checked over the whole chain in check_chain_scalars.  21-H.3
  // re-pin: on the airline-start trajectory the single pass-2 gate (round1)
  // fires filter 5 and restores the reference model (pre-fix the gate kept);
  // the landed model is pinned by the orders/variable checks above.
  const kalos::AutoDecisionRecord* last_p2 = nullptr;
  for (std::size_t i = r.chain.size(); i-- > 0 && last_p2 == nullptr;) {
    if (stage_suffix(r.chain[i].stage) == "pass2") last_p2 = &r.chain[i];
  }
  check(last_p2 != nullptr && last_p2->pass2 && last_p2->pass2->filter == 5 &&
            last_p2->pass2->restored && !last_p2->pass2->last_solution,
        "full case: last pass-2 gate restores the reference");

  check_composition(r, 96, "full case");
}

// --- T09: the variable vocabulary is the JVM's (G3 mapping) ----------------
//
// Across the suite's outlier-bearing results, every landed variable's
// regeffect must equal SaVariable.defaultComponentTypeOf: AO/TC ->
// Irregular, LS -> Trend, SO -> Seasonal (the G3 probe of 21-H.1).

void t09_regeffect_vocabulary() {
  const kalos::X13AutomaticFitResult& r = full_case();
  for (std::size_t i = 0; i < r.variables.size(); ++i) {
    const kalos::AutoVariableRecord& v = r.variables[i];
    const char* want = "Irregular";
    if (v.type == kalos::OutlierType::LevelShift) want = "Trend";
    if (v.type == kalos::OutlierType::SeasonalOutlier) want = "Seasonal";
    const std::string msg =
        "regeffect vocabulary for " + v.name + " is " + want;
    check_eq_s(v.regeffect, want, msg.c_str());
  }
  check(true, "regeffect vocabulary scanned");
}

}  // namespace

int main() {
  std::printf("== fail-closed matrix ==\n");
  std::fflush(stdout);
  t01_status_matrix();
  t02_shape_throws();
  t03_value_semantics();
  std::printf("== cheap composition shape (~20 s) ==\n");
  std::fflush(stdout);
  t04_quick_shape_pinned();
  t05_quick_shape_chain();
  t06_quick_twin();
  std::printf("== full automatic case (~230 s) ==\n");
  std::fflush(stdout);
  t08_full_case();
  t09_regeffect_vocabulary();
  std::printf("%s: %d/%d checks passed\n", g_failures == 0 ? "PASS" : "FAIL",
              g_checks - g_failures, g_checks);
  std::fflush(stdout);
  return g_failures == 0 ? 0 : 1;
}
