// Item 24-C.1 cluster-1 unit test: bit-exact replay of JVM ground truth for
// the linearfilters mirror (include/kalos/detail/seats_linear_filters.hpp).
// Golden hex tokens were captured from the pinned jdplus 3.9.0 jars with the
// /tmp/opencode/ssf/{DecProbe,RFProbe2}.java probes (2026-11-22); every
// comparison is a bit-for-bit IEEE comparison (hex literals parse exactly).
// Coverage: RationalFunction (dd==0 fast path, 32-quantized cache +
// extension, drop, plus/minus/times/divide through Polynomial$SimplifyingTool
// - exact path, false path, and the roots path incl. the real-root
// f1 + stale-f2 double divide and the left==big / LS-quotient tail), the
// filter algebra (Back/Fore/Symmetric/Finite statics, mirror, isSymmetric,
// frequency responses through the Clenshaw recurrence and the host-libm
// trig lineage - see the CI-FDLIBM ledger), RationalBack/ForeFilter.times
// (cross-cancel + leading-coefficient normalization), RationalFilter
// eager/lazy numerator-denominator paths (FiniteFilter vs SymmetricFilter
// denominator), the LULinearSystemSolver2(1.0E-13) decomposition system,
// and the AIOOBE-parity refusal paths (RationalFunction.get(i<0) and
// Polynomial.get over-read). Cluster 2 adds the acf/solver mirror
// (include/kalos/detail/seats_acf_solver.hpp): AutoCovarianceFunction (36-
// quantized cache, ivar scaling incl. NaN, -0.0 extension zeros), the
// default computer (Toeplitz + LU2, ArimaException translation), the
// symmetric computer through the 2-arg ctor (incl. its decompose branch),
// LinearProcess.lazy acf (one- and two-sided denominators) and Symmetric-
// Filter.decompose (length-1 scaling path + Toeplitz path + bare
// MatrixException) - goldens from /tmp/opencode/ssf/AcfProbe.java
// (acfA.txt == acfB.txt, double capture 2026-11-22). Cluster 3 adds the WK
// estimator builder (include/kalos/detail/seats_wk_estimators.hpp):
// WienerKolmogorovEstimators over six hand-built UcarimaModel shapes
// (general/special paths, unit roots, symmetric factorizing components,
// single-component complement NULL fold), replaying
// tests/wk_goldens.inc from /tmp/opencode/ssf/WKProbe.java
// (wkA.txt == wkB.txt, double capture 2026-11-22).
#include "kalos/detail/seats_linear_filters.hpp"
#include "kalos/detail/seats_acf_solver.hpp"
#include "kalos/detail/seats_wk_estimators.hpp"
#include "kalos/detail/seats_exact_forecasts.hpp"
#include "kalos/detail/seats_burman_estimates.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_checks = 0, g_failures = 0;

void report(const std::string& what) {
  ++g_failures;
  if (g_failures <= (std::getenv("LF_ALL_FAILS") ? 100000 : 12))
    std::printf("FAIL: %s\n", what.c_str());
}

bool bits_eq(double a, double b) { return std::memcmp(&a, &b, 8) == 0; }

double hx(const char* s) { return std::strtod(s, nullptr); }  // 0x..p.. exact

// Compare got against a space-separated golden hex token list. "X" marks the
// first over-read where the JVM aborted with AIOOBE (AIOOBE parity: threw
// must be set and the C++ values must have ended exactly at that point).
void chk(const std::string& tag, const kalos::detail::vd& got, bool threw,
         const std::string& gold) {
  ++g_checks;
  std::size_t i = 0, n = gold.size(), k = 0;
  bool x_seen = false;
  while (i < n) {
    while (i < n && gold[i] == ' ') ++i;
    std::size_t j = i;
    while (j < n && gold[j] != ' ') ++j;
    std::string t = gold.substr(i, j - i);
    i = j;
    if (t.empty()) continue;
    if (t == "X") { x_seen = true; break; }
    if (k >= got.size()) {
      report(tag + ": golden has value #" + std::to_string(k) + ", C++ ended");
      return;
    }
    if (!bits_eq(got[k], hx(t.c_str()))) {
      char buf[160];
      std::snprintf(buf, sizeof buf, "%s: value #%zu: got %a want %s",
                    tag.c_str(), k, got[k], t.c_str());
      report(buf);
      return;
    }
    ++k;
  }
  if (x_seen) {
    if (!threw) { report(tag + ": golden X but C++ did not throw"); return; }
    if (k != got.size()) { report(tag + ": golden X before C++ value count"); return; }
  } else {
    if (threw) { report(tag + ": C++ threw but golden complete"); return; }
    if (k != got.size()) { report(tag + ": value count mismatch"); return; }
  }
}

void chk1(const std::string& tag, double got, const char* gold) {
  ++g_checks;
  if (!bits_eq(got, hx(gold))) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "%s: got %a want %s", tag.c_str(), got, gold);
    report(buf);
  }
}

void chk_true(const std::string& tag, bool cond) {
  ++g_checks;
  if (!cond) report(tag);
}

using namespace kalos::detail;
using namespace kalos::detail::seats_lfilters;
namespace sa = kalos::detail::seats_acf;
namespace sm = kalos::detail::seats_model;
namespace swk = kalos::detail::seats_wk;
namespace sx = kalos::detail::seats_xf;

// Poly.get sweep with the AIOOBE-parity stop (probe poly() helper semantics).
void poly_vals(const Poly& p, int n, vd& out, bool& threw) {
  out.clear();
  threw = false;
  for (int i = 0; i < n; ++i) {
    try {
      out.push_back(pg(p, i));
    } catch (const std::out_of_range&) {
      threw = true;
      return;
    }
  }
}

void series(RatFunc& rf, int n, vd& out) { out = rf.coefficients(n); }

// ---- J: WK estimator builder (seats_wk_estimators.hpp) -------------------
// Bit-exact replay of /tmp/opencode/ssf/WKProbe.java (WienerKolmogorov-
// Estimators over six hand-built UcarimaModel shapes: general path, dual
// stationary AR, SPECIAL path, unit roots, symmetric (factorizing) com-
// ponents, single component). Goldens: tests/wk_goldens.inc (double
// capture, wkA.txt == wkB.txt, 2026-11-22). The call ORDER mirrors the
// probe line by line; ".filt"/acf exception lines carry the JVM's throw.
#include "wk_goldens.inc"

const char* wk_find(const std::string& tag) {
  for (const auto& e : WK_G)
    if (tag == e.tag) return e.gold;
  return nullptr;
}
void wk_arr(const std::string& tag, const vd& got) {
  const char* g = wk_find(tag);
  if (!g) {
    ++g_checks;
    report("wk golden missing: " + tag);
    return;
  }
  chk(tag, got, false, std::string(g));
}
void wk_d(const std::string& tag, double got) {
  const char* g = wk_find(tag);
  if (!g) {
    ++g_checks;
    report("wk golden missing: " + tag);
    return;
  }
  chk1(tag, got, g);
}
void wk_null(const std::string& tag) {
  ++g_checks;
  const char* g = wk_find(tag);
  if (!g || std::strcmp(g, "NULL") != 0) report(tag + ": expected NULL golden");
}
void wk_ex(const std::string& tag) {
  ++g_checks;
  const char* g = wk_find(tag);
  if (!g || std::strcmp(g, "EX") != 0) report(tag + ": expected EX golden");
}

sm::AModel wk_am4(const vd& ar, const vd& del, const vd& ma, double var) {
  sm::AModel a;
  a.ar = p_of(vd(ar));
  a.delta = p_of(vd(del));
  a.symmetric = false;
  a.has_ma = true;
  a.ma1 = p_of(vd(ma)).c;  // BackFilter.ofInternal (trim)
  a.var = var;
  return a;
}
sm::AModel wk_am3(const vd& ar, const vd& del, const vd& sma) {
  sm::AModel a;
  a.ar = p_of(vd(ar));
  a.delta = p_of(vd(del));
  a.symmetric = true;
  a.sma = sm::SymF(trim_copy(vd(sma)));  // SymmetricFilter.ofInternal
  a.has_ma = false;
  a.var = 0.0;
  return a;
}
vd wk_wts(RationalFilter& rf, int lo, int hi) {
  vd w;
  for (int t = lo; t <= hi; ++t) w.push_back(rf.weight(t));
  return w;
}
vd wk_acf_get(sa::Acf& ac, int n) {
  vd v;
  for (int i = 0; i < n; ++i) v.push_back(ac.get(i));
  return v;
}

void wk_dump_est(const std::string& tag, swk::WkEstimators& E, int comp, bool side) {
  std::string k = tag + ".c" + std::to_string(comp) + (side ? "T" : "F");
  std::optional<swk::WkEstimator> e;
  try {
    e = E.final_estimator(comp, side);
  } catch (const std::exception&) {
    wk_ex(k);
    return;
  }
  if (!e.has_value()) {
    wk_null(k);
    return;
  }
  try {
    wk_arr(k + ".wkf", wk_wts(e->wkf, -6, 10));
    wk_d(k + ".iv", e->est.get_innovation_variance());
    wk_arr(k + ".lpf", wk_wts(e->est.get_filter(), -2, 12));
  } catch (const std::exception&) {
    wk_ex(k + ".filt");
  }
  try {
    wk_arr(k + ".acf", wk_acf_get(e->est.get_auto_covariance_function(), 9));
  } catch (const std::exception&) {
    wk_ex(k + ".acf");
  }
}

void wk_case(const std::string& tag, const sm::UcarimaModel& um, const sm::AModel& outer,
             int n) {
  swk::WkEstimators E(&um, outer);
  for (int c = 0; c < n; ++c) {
    wk_dump_est(tag, E, c, false);
    wk_dump_est(tag, E, c, true);
  }
  for (int c = 0; c < n; ++c) {
    std::string k = tag + ".e" + std::to_string(c);
    std::optional<sm::AModel> em;
    try {
      em = E.final_error_model(c);
    } catch (const std::exception&) {
      wk_ex(k);
      continue;
    }
    if (!em.has_value()) {
      wk_null(k);
      continue;
    }
    try {
      wk_arr(k + ".ar", swk::am_stationary_ar(*em).p.c);
      wk_arr(k + ".delta", swk::am_non_stationary_ar(*em).p.c);
      wk_arr(k + ".sma", em->symmetric_ma().a);
      wk_arr(k + ".ma", em->get_ma().p.c);
      wk_d(k + ".iv", em->get_iv());
      sa::Acf ac = sa::am_acf(*em);
      wk_arr(k + ".acf", wk_acf_get(ac, 7));
    } catch (const std::exception&) {
      wk_ex(k);
    }
  }
  for (int c = 0; c < n; ++c)
    for (int side = 0; side < 2; ++side) {
      std::string k = tag + ".s" + std::to_string(c) + std::to_string(side);
      std::optional<swk::StationaryTransformation> st;
      try {
        st = E.final_stationary_estimator(c, side != 0);
      } catch (const std::exception&) {
        wk_ex(k);
        continue;
      }
      if (!st.has_value()) {
        wk_null(k);
        continue;
      }
      try {
        wk_arr(k + ".lpf", wk_wts(st->proc.get_filter(), -2, 12));
        wk_d(k + ".iv", st->proc.get_innovation_variance());
        wk_arr(k + ".delta", st->delta.p.c);
      } catch (const std::exception&) {
        wk_ex(k);
      }
    }
  for (int c = 0; c < n; ++c) {
    std::string k = tag + ".r" + std::to_string(c);
    for (int hh : {-1, 0, 1, 2, 3}) {
      std::string kk = k + ".h" + std::to_string(hh);
      try {
        std::optional<sa::LinearProc> rm = E.revision_model(c, hh);
        if (!rm.has_value()) {
          wk_null(kk);
          continue;
        }
        wk_arr(kk + ".f", wk_wts(rm->get_filter(), -2, 14));
        wk_d(kk + ".iv", rm->get_innovation_variance());
      } catch (const std::exception&) {
        wk_ex(kk);
      }
    }
  }
  {
    const int pns[6][2] = {{0, 8}, {1, 8}, {3, 8}, {-1, 4}, {-2, 4}, {0, 1}};
    for (int c = 0; c < n; ++c) {
      std::string k = tag + ".rv" + std::to_string(c);
      for (int s = 0; s < 2; ++s)
        for (const auto& pn : pns) {
          std::string kk = k + (s ? "T" : "F") + std::to_string(pn[0]) + "_" +
                           std::to_string(pn[1]);
          try {
            wk_arr(kk, *E.revision_variance(c, s != 0, pn[0], pn[1]));
          } catch (const std::exception&) {
            wk_ex(kk);
          }
        }
    }
  }
  {
    const int pns[3][2] = {{0, 8}, {2, 8}, {-1, 4}};
    for (int c = 0; c < n; ++c) {
      std::string k = tag + ".rr" + std::to_string(c);
      for (const auto& pn : pns) {
        std::string kk = k + std::to_string(pn[0]) + "_" + std::to_string(pn[1]);
        try {
          wk_arr(kk, *E.relative_revision_variance(c, true, pn[0], pn[1]));
        } catch (const std::exception&) {
          wk_ex(kk);
        }
      }
    }
  }
  {
    const int pns[3][2] = {{0, 8}, {2, 8}, {-1, 4}};
    for (int c = 0; c < n; ++c) {
      std::string k = tag + ".tv" + std::to_string(c);
      for (const auto& pn : pns) {
        std::string kk = k + std::to_string(pn[0]) + "_" + std::to_string(pn[1]);
        try {
          wk_arr(kk, *E.total_error_variance(c, true, pn[0], pn[1]));
        } catch (const std::exception&) {
          wk_ex(kk);
        }
      }
    }
  }
  for (int c = 0; c < n; ++c) {
    std::string k = tag + ".ta" + std::to_string(c);
    for (int hh : {0, 1}) {
      std::string kk = k + ".h" + std::to_string(hh);
      try {
        sa::Acf f = E.total_error_acf(c, hh);
        wk_arr(kk, wk_acf_get(f, 8));
      } catch (const std::exception&) {
        wk_ex(kk);
      }
    }
    try {
      sa::Acf f = E.revision_acf(c, 0);
      wk_arr(k + ".r0", wk_acf_get(f, 8));
    } catch (const std::exception&) {
      wk_ex(k + ".r0");
    }
  }
  for (int c = 0; c < n; ++c) {
    std::string k = tag + ".vp" + std::to_string(c);
    for (int tot = 0; tot < 2; ++tot)
      for (int hk : {0, 1, 4}) {
        std::string kk = k + (tot ? "T" : "F") + std::to_string(hk);
        try {
          wk_d(kk, E.variation_precision(c, 0, hk, tot != 0));
        } catch (const std::exception&) {
          wk_ex(kk);
        }
      }
    try {
      wk_d(k + ".vrv", E.variation_revision_variance(c, 1, 3, 5));
    } catch (const std::exception&) {
      wk_ex(k + ".vrv");
    }
    try {
      wk_d(k + ".vrv2", E.variation_revision_variance(c, 0, 1, 2));
    } catch (const std::exception&) {
      wk_ex(k + ".vrv2");
    }
  }
}

void wk_replay() {
  {  // A: general path, MA + white-noise components
    sm::UcarimaModel um;
    um.comps = {wk_am4({1.0, -0.2}, {1.0}, {1.0, 0.1}, 0.6),
                wk_am4({1.0}, {1.0}, {1.0}, 0.4)};
    sm::AModel outer = wk_am4({1.0, -0.4}, {1.0}, {1.0, 0.3}, 1.0);
    wk_case("A", um, outer, 2);
  }
  {  // B: general path, both components stationary AR
    sm::UcarimaModel um;
    um.comps = {wk_am4({1.0, -0.3}, {1.0}, {1.0, 0.2}, 0.5),
                wk_am4({1.0, 0.15}, {1.0}, {1.0, -0.3}, 0.3)};
    sm::AModel outer = wk_am4({1.0, -0.5, 0.2}, {1.0}, {1.0, 0.3, 0.1}, 1.1);
    wk_case("B", um, outer, 2);
  }
  {  // C: SPECIAL path (outer order 1 < component orders 1 + 1)
    sm::UcarimaModel um;
    um.comps = {wk_am4({1.0, -0.2}, {1.0}, {1.0, 0.1}, 0.5),
                wk_am4({1.0, 0.3}, {1.0}, {1.0, -0.25}, 0.3)};
    sm::AModel outer = wk_am4({1.0, -0.4}, {1.0}, {1.0, 0.35}, 0.9);
    wk_case("C", um, outer, 2);
  }
  {  // D: unit roots (delta (1,-1)) + white-noise complement
    sm::UcarimaModel um;
    um.comps = {wk_am4({1.0}, {1.0, -1.0}, {1.0, -0.4}, 0.5),
                wk_am4({1.0}, {1.0}, {1.0}, 0.3)};
    sm::AModel outer = wk_am4({1.0, -0.3}, {1.0, -1.0}, {1.0, 0.25}, 1.0);
    wk_case("D", um, outer, 2);
  }
  {  // E: symmetric (3-arg) components - factorized getMa paths
    sm::UcarimaModel um;
    um.comps = {wk_am3({1.0, -0.2}, {1.0}, {0.6, 0.25, 0.05}),
                wk_am3({1.0, -0.5}, {1.0}, {0.35, 0.1, 0.0})};
    sm::AModel outer = wk_am4({1.0, -0.3}, {1.0}, {1.0, 0.2}, 1.0);
    wk_case("E", um, outer, 2);
  }
  {  // F: single component (getComplement -> NULL singleton path)
    sm::UcarimaModel um;
    um.comps = {wk_am4({1.0, -0.3}, {1.0}, {1.0, 0.4}, 1.0)};
    sm::AModel outer = wk_am4({1.0, -0.3}, {1.0}, {1.0, 0.4}, 1.0);
    wk_case("F", um, outer, 1);
  }
}

// ---- K: ExactArimaForecasts (seats_exact_forecasts.hpp) -------------------
// Bit-exact replay of /tmp/opencode/ssf/XFProbe.java over the 36-case 24-C
// corpus: the live SeatsKernel's outer UcarimaModel model (ar/sar/ur/ma/iv
// captured as inputs so the replay is self-contained), driven through the
// Burman-order surface prepare/forecasts(nfx)/backcasts(nfx)/getMean plus
// the {0,1,3,period} horizon grid with the getMean re-read (mean-overwrite
// quirk). Goldens: tests/xf_goldens.inc (xf.out.A == xf.out.B, double
// capture 2026-11-22); .skip rows mark kernel refusals (covered by the WK
// corpus goldens, no forecaster surface).
#include "xf_goldens.inc"

const char* xf_find(const std::string& tag) {
  for (const auto& e : XF_G)
    if (tag == e.tag) return e.gold;
  return nullptr;
}
// Parse a golden hex-token list into a value vector (input rows).
vd xf_input(const std::string& tag) {
  const char* g = xf_find(tag);
  vd out;
  if (!g) {
    ++g_checks;
    report("xf golden missing (input): " + tag);
    return out;
  }
  std::size_t i = 0, n = std::strlen(g);
  while (i < n) {
    while (i < n && g[i] == ' ') ++i;
    std::size_t j = i;
    while (j < n && g[j] != ' ') ++j;
    if (j > i) out.push_back(hx(std::string(g + i, j - i).c_str()));
    i = j;
  }
  return out;
}
int xf_int(const std::string& tag) {
  const char* g = xf_find(tag);
  if (!g) {
    ++g_checks;
    report("xf golden missing (int): " + tag);
    return 0;
  }
  return std::atoi(g);
}
void xf_arr(const std::string& tag, const vd& got) {
  const char* g = xf_find(tag);
  if (!g) {
    ++g_checks;
    report("xf golden missing: " + tag);
    return;
  }
  chk(tag, got, false, std::string(g));
}
void xf_d(const std::string& tag, double got) {
  const char* g = xf_find(tag);
  if (!g) {
    ++g_checks;
    report("xf golden missing: " + tag);
    return;
  }
  chk1(tag, got, g);
}

void xf_emit(const std::string& id, bool bmean, const sx::XfModel& model,
             const vd& data, int nfx, int period) {
  const std::string tag = id + (bmean ? ".m" : ".z");
  sx::ExactFc fc;  // fresh instance per emitXf (mean field starts at 0)
  bool ok = fc.prepare(model, bmean);
  xf_d(tag + ".ok", ok ? 1.0 : 0.0);
  xf_arr(tag + ".fc", fc.forecasts(data, nfx));
  xf_arr(tag + ".bc", fc.backcasts(data, nfx));
  xf_d(tag + ".mean", fc.get_mean());
  const int grid[4] = {0, 1, 3, period};
  for (int k : grid) {
    const std::string ks = std::to_string(k);
    xf_arr(tag + ".fc" + ks, fc.forecasts(data, k));
    xf_arr(tag + ".bc" + ks, fc.backcasts(data, k));
  }
  xf_d(tag + ".meang", fc.get_mean());
}

void xf_replay() {
  // Case ids in golden order (deduplicated by first appearance).
  std::vector<std::string> ids;
  for (const auto& e : XF_G) {
    std::string t = e.tag;
    std::size_t dot = t.find('.');
    std::string id = t.substr(0, dot);
    if (ids.empty() || ids.back() != id) ids.push_back(id);
  }
  for (const std::string& id : ids) {
    if (xf_find(id + ".skip")) continue;  // kernel refusal: no XF surface
    sx::XfModel m;
    m.stat = p_of(xf_input(id + ".sar"));
    m.nonstat = p_of(xf_input(id + ".ur"));
    m.ma = p_of(xf_input(id + ".ma"));
    m.var = xf_input(id + ".iv").at(0);
    const vd data = xf_input(id + ".data");
    const int period = xf_int(id + ".period");
    // Inputs cross-check: getAr() = stat x nonstat must reproduce .ar.
    xf_arr(id + ".ar", p_times(m.stat, m.nonstat).c);
    const char* g = xf_find(id + ".orders");
    int p_ = 0, q_ = 0, nf_ = 0;
    std::sscanf(g, "%d %d %d", &p_, &q_, &nf_);
    chk_true(id + ".orders.p",
             p_ == static_cast<int>(p_times(m.stat, m.nonstat).degree()));
    chk_true(id + ".orders.q", q_ == static_cast<int>(m.ma.degree()));
    const int nfx = q_ > p_ ? 2 * q_ : p_ + q_;
    chk_true(id + ".orders.nf", nf_ == nfx);
    // State dim = SsfArima.stateComponent(xm).dim() (pre-prepare model).
    sx::ExactFc fcz;
    fcz.prepare(m, false);
    chk_true(id + ".sfdim", fcz.state_dim() == xf_int(id + ".sfdim"));
    try {
      xf_emit(id, false, m, data, nfx, period);
      xf_emit(id, true, m, data, nfx, period);
    } catch (const std::exception& ex) {
      ++g_checks;
      report(id + ": C++ threw: " + ex.what());
    } catch (...) {
      ++g_checks;
      report(id + ": C++ threw (non-std)");
    }
  }
}

// ---- L: BurmanEstimates (seats_burman_estimates.hpp) ----------------------
// Bit-exact replay of /tmp/opencode/ssf/BurmanProbe.java over the same
// 36-case 24-C corpus: the live SeatsKernel's FULL UcarimaModel (every
// component's stat/ur/ma|sma/iv plus the outer and the working data, all
// captured as id-tagged input rows) driven through BurmanEstimates on a
// (bmean, ser, nf) grid: g0 (f, 1, nfx) g1 (t, 1, nfx) g2 (f, 1, period)
// g3 (t, .5, period), mcmp = 0 (Builder default). Every public getter plus
// the mean/meanc/nf/nbc/nfc privates are hex-compared against the JVM
// double capture (bmA.txt == bmB.txt). NULL = captured null; EX:cls = the
// null-slot fastOp NPE (JvmNpe parity).
#include "bm_goldens.inc"

namespace sb = kalos::detail::seats_burman;

const char* bm_find(const std::string& tag) {
  for (const auto& e : BM_G)
    if (tag == e.tag) return e.gold;
  return nullptr;
}
vd bm_input(const std::string& tag) {
  const char* g = bm_find(tag);
  vd out;
  if (!g) {
    ++g_checks;
    report("bm golden missing (input): " + tag);
    return out;
  }
  std::size_t i = 0, n = std::strlen(g);
  while (i < n) {
    while (i < n && g[i] == ' ') ++i;
    std::size_t j = i;
    while (j < n && g[j] != ' ') ++j;
    if (j > i) out.push_back(hx(std::string(g + i, j - i).c_str()));
    i = j;
  }
  return out;
}
int bm_int(const std::string& tag) { return std::atoi(bm_find(tag)); }
double bm_one(const std::string& tag) { return hx(bm_find(tag)); }

// optional<vd> surface (getter already called): NULL/EX/seq parity.
void bm_opt(const std::string& tag, const std::optional<vd>& got, bool threw) {
  const char* g = bm_find(tag);
  if (!g) {
    ++g_checks;
    report("bm golden missing: " + tag);
    return;
  }
  std::string gold(g);
  if (gold.rfind("EX:", 0) == 0) {
    ++g_checks;
    if (!threw) report(tag + ": golden NPE, C++ returned");
    return;
  }
  if (threw) {
    ++g_checks;
    report(tag + ": C++ threw, golden has data");
    return;
  }
  if (gold == "NULL") {
    ++g_checks;
    if (got.has_value()) report(tag + ": golden NULL, C++ has values");
    return;
  }
  if (!got.has_value()) {
    ++g_checks;
    report(tag + ": C++ null, golden has values");
    return;
  }
  chk(tag, *got, false, gold);
}
void bm_seq(const std::string& tag, const vd& got) {
  const char* g = bm_find(tag);
  if (!g) {
    ++g_checks;
    report("bm golden missing: " + tag);
    return;
  }
  chk(tag, got, false, std::string(g));
}
// call a Burman getter, mapping JvmNpe onto the EX: parity channel
#define BM_OPT(expr, tag)                                       \
  {                                                             \
    std::optional<vd> r;                                        \
    bool threw = false;                                         \
    try {                                                       \
      r = (expr);                                               \
    } catch (const sm::JvmNpe&) {                               \
      threw = true;                                             \
    }                                                           \
    bm_opt(tag, r, threw);                                      \
  }

void bm_case(const std::string& id) {
  vd data = bm_input(id + ".xf.data");
  vd sar = bm_input(id + ".xf.sar"), ur = bm_input(id + ".xf.ur");
  vd ma = bm_input(id + ".xf.ma"), ar = bm_input(id + ".xf.ar");
  double iv = bm_one(id + ".xf.iv");
  int period = static_cast<int>(bm_one(id + ".xf.period"));
  sm::AModel outer = wk_am4(sar, ur, ma, iv);
  const int nc = bm_int(id + ".bm.ncomp");
  sm::UcarimaModel um;
  for (int c = 0; c < nc; ++c) {
    const std::string pc = id + ".bm.c" + std::to_string(c) + ".";
    vd cstat = bm_input(pc + "stat"), cur = bm_input(pc + "ur");
    if (bm_find(pc + "ma")) {
      // JVM ArimaModel carries a constructor-set sma FIELD (smaset=1 for
      // every decomposition item; BurmanProbe bc rows) - symmetricMa()
      // returns it without deriving. Feed the captured field, not the
      // convolution of (ma, var), which differs at ULP level.
      sm::AModel a =
          wk_am4(cstat, cur, bm_input(pc + "ma"), bm_one(pc + "iv"));
      a.symmetric = true;
      a.sma = sm::SymF(trim_copy(bm_input(pc + "sma")));
      um.comps.push_back(a);
    } else {  // symmetric-factorization component (getMa throws in the JVM)
      bm_find(pc + "maex");  // presence checked; value unused
      um.comps.push_back(wk_am3(cstat, cur, bm_input(pc + "sma")));
    }
    chk_true(pc + "null", um.comps.back().is_null() == (bm_int(pc + "null") != 0));
  }
  const int q2 = static_cast<int>(ma.size()) - 1;
  const int p2 = static_cast<int>(ar.size()) - 1;
  const int nfx = q2 > p2 ? 2 * q2 : p2 + q2;
  swk::WkEstimators E(&um, outer);
  for (int gg = 0; gg < 4; ++gg) {
    const bool bmean = (gg & 1) != 0;
    const double ser = gg < 2 ? 1.0 : 0.5;
    const int nf = gg < 2 ? nfx : period;
    const std::string tag = id + ".bm.g" + std::to_string(gg);
    std::unique_ptr<sb::BurmanEstimates> be;
    try {
      be.reset(new sb::BurmanEstimates(data, E, bmean, /*mcmp=*/0, ser, nf, nf));
    } catch (const std::exception& ex) {
      ++g_checks;
      report(tag + ": C++ ctor threw: " + ex.what());
      continue;
    }
    {  // .priv mean <hex> meanc <hex> nf <i> nbc <i> nfc <i>
      std::string gold = bm_find(tag + ".priv");
      std::size_t p = gold.find_first_not_of(' ');  // "mean" label
      p = gold.find_first_not_of(' ', gold.find(' ', p));  // value start
      std::size_t e1 = gold.find(' ', p);
      chk1(tag + ".priv.mean", be->mean, gold.substr(p, e1 - p).c_str());
      p = gold.find_first_not_of(' ', e1 + 1);  // "meanc" label
      p = gold.find_first_not_of(' ', gold.find(' ', p));  // value start
      e1 = gold.find(' ', p);
      chk1(tag + ".priv.meanc", be->meanc, gold.substr(p, e1 - p).c_str());
      ++g_checks;
      if (std::atoi(gold.substr(gold.find("nf ") + 3).c_str()) != be->nf ||
          std::atoi(gold.substr(gold.find("nbc ") + 4).c_str()) != be->nbc ||
          std::atoi(gold.substr(gold.find("nfc ") + 4).c_str()) != be->nfc)
        report(tag + ".priv orders");
    }
    chk1(tag + ".mc", be->is_mean_correction() ? 1.0 : 0.0,
         bm_find(tag + ".mc"));
    bm_seq(tag + ".sfc", be->series_forecasts());
    bm_seq(tag + ".sbc", be->series_backcasts());
    for (int c = 0; c < nc; ++c) {
      const std::string cs = std::to_string(c);
      BM_OPT(be->estimates(c, true), tag + ".est" + cs + "T");
      BM_OPT(be->estimates(c, false), tag + ".est" + cs + "F");
      BM_OPT(be->forecasts(c, true), tag + ".fc" + cs + "T");
      BM_OPT(be->forecasts(c, false), tag + ".fc" + cs + "F");
      BM_OPT(be->backcasts(c, true), tag + ".bc" + cs + "T");
      BM_OPT(be->backcasts(c, false), tag + ".bc" + cs + "F");
      bm_seq(tag + ".se" + cs, be->stdev_estimates(c));
      BM_OPT(be->stdev_forecasts(c, true), tag + ".sf" + cs + "T");
      BM_OPT(be->stdev_forecasts(c, false), tag + ".sf" + cs + "F");
      BM_OPT(be->stdev_backcasts(c, true), tag + ".sb" + cs + "T");
      BM_OPT(be->stdev_backcasts(c, false), tag + ".sb" + cs + "F");
    }
  }
}

void bm_replay() {
  std::vector<std::string> ids;
  for (const auto& e : BM_G) {
    std::string t = e.tag;
    std::size_t dot = t.find('.');
    std::string id = t.substr(0, dot);
    if (ids.empty() || ids.back() != id) ids.push_back(id);
  }
  for (const std::string& id : ids)
    if (bm_find(id + ".bm.ncomp")) bm_case(id);
}

}  // namespace

int main() {
  vd tmp;
  bool thr = false;

  // ---- A: decomposition system (DecProbe ground truth) ---------------------
  {
    BackFilter bk = BackFilter::of_internal(vd{1.0, -0.5});
    ForeFilter fo = ForeFilter::of_internal(vd{1.0, 0.5});
    FiniteFilter w(vd{0.25, 0.5, 0.25}, -1);
    Decomposition dec = Decomposition::of(w, bk, fo);
    chk_true("A.bounds.back",
             dec.back.get_l_bound() == INT_MIN && dec.back.get_u_bound() == 0);
    chk_true("A.bounds.fore",
             dec.fore.get_l_bound() == 0 && dec.fore.get_u_bound() == INT_MAX);
    RatBackFilter rb = dec.back;
    RatForeFilter rf = dec.fore;
    series(rb.rf, 8, tmp);
    chk("A.backw", tmp, false,
        "0x1.999999999999ap-2 0x1.ccccccccccccdp-2 0x1.ccccccccccccdp-3 "
        "0x1.ccccccccccccdp-4 0x1.ccccccccccccdp-5 0x1.ccccccccccccdp-6 "
        "0x1.ccccccccccccdp-7 0x1.ccccccccccccdp-8");
    series(rf.rf, 8, tmp);
    chk("A.forew", tmp, false,
        "0x0.0p0 0x1.999999999999ap-5 -0x1.999999999999ap-6 0x1.999999999999ap-7 "
        "-0x1.999999999999ap-8 0x1.999999999999ap-9 -0x1.999999999999ap-10 "
        "0x1.999999999999ap-11");
    {
      static const char* bw[7] = {"0x1.ccccccccccccdp-7", "0x1.ccccccccccccdp-6",
                                  "0x1.ccccccccccccdp-5", "0x1.ccccccccccccdp-4",
                                  "0x1.ccccccccccccdp-3", "0x1.ccccccccccccdp-2",
                                  "0x1.999999999999ap-2"};
      for (int t = -6, k = 0; t <= 0; ++t, ++k) chk1("A.bw", rb.weight(t), bw[k]);
      static const char* fw[7] = {"0x0.0p0", "0x1.999999999999ap-5",
                                  "-0x1.999999999999ap-6", "0x1.999999999999ap-7",
                                  "-0x1.999999999999ap-8", "0x1.999999999999ap-9",
                                  "-0x1.999999999999ap-10"};
      for (int t = 0, k = 0; t <= 6; ++t, ++k) chk1("A.fw", rf.weight(t), fw[k]);
    }
    cplx frb = rb.frequency_response(0.5);
    chk1("A.frb.re", frb.re, "0x1.b6653cc6f7c34p-1");
    chk1("A.frb.im", frb.im, "-0x1.289a08ceaccebp-1");
    cplx frf = rf.frequency_response(0.5);
    chk1("A.frf.re", frf.re, "0x1.0935f543e5d0fp-5");
    chk1("A.frf.im", frf.im, "0x1.7131aca2cf33p-7");

    RationalFilter rfilter = RationalFilter::of_back(rb);
    std::shared_ptr<FinFilter> dn = rfilter.get_denominator();
    chk_true("A.den.bounds", dn->get_lower_bound() == -1 && dn->get_upper_bound() == 0);
    chk("A.dnw", dn->weights_to_array(), false, "-0x1p-1 0x1p+0");
    chk("A.nmw", rfilter.get_numerator()->weights_to_array(), false,
        "0x1p-2 0x1.999999999999ap-2");

    RationalFilter rf2 = RationalFilter::of_bbff(bk, bk, fo, fo);
    std::shared_ptr<FinFilter> dn2 = rf2.get_denominator();
    chk_true("A.den2.bounds", dn2->get_lower_bound() == -1 && dn2->get_upper_bound() == 1);
    chk("A.dn2w", dn2->weights_to_array(), false, "-0x1p-1 0x1.8p-1 0x1p-1");
    cplx fr2 = rf2.frequency_response(0.3);
    chk1("A.fr2.re", fr2.re, "0x1p+0");
    chk1("A.fr2.im", fr2.im, "0x0p+0");
  }

  // ---- B1/B2/B3: RationalFunction caching + drop ---------------------------
  RatFunc a1 = RatFunc::of(Poly(vd{3.0}), Poly(vd{2.0}));
  series(a1, 3, tmp);
  chk("B.a1", tmp, false, "0x1.8p0 0x0.0p0 0x0.0p0");
  chk_true("B.a1fin", a1.is_finite());

  RatFunc a2 = RatFunc::of(Poly(vd{1.0, 2.0}), Poly(vd{1.0, -0.5}));
  chk1("B.a2g3", a2.get(3), "0x1.4p-1");
  chk1("B.a2g40", a2.get(40), "0x1.4p-38");
  chk1("B.a2g33", a2.get(33), "0x1.4p-31");
  series(a2, 70, tmp);
  {
    std::string g = "0x1.0p0 0x1.4p1 0x1.4p0";
    for (int i = 1; i <= 67; ++i) g += " 0x1.4p-" + std::to_string(i);
    chk("B.a2c70", tmp, false, g);
  }

  RatFunc a3 = RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, -0.5}));
  RatFunc a3d = a3.drop(2);
  series(a3d, 5, tmp);
  chk("B.a3d", tmp, false, "0x1.0p-2 0x1.0p-3 0x1.0p-4 0x1.0p-5 0x1.0p-6");

  // ---- B4: SimplifyingTool via plus/minus/times/divide ---------------------
  RatFunc f1 = RatFunc::of(Poly(vd{1.0, 0.5}), Poly(vd{1.0, -0.5}));
  RatFunc f2 = RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, 0.5}));
  RatFunc t4 = f1.times(f2);  // exact cross-cancel on the swap side
  poly_vals(t4.num, 3, tmp, thr);
  chk("B.t4n", tmp, thr, "0x1.0p0 X");
  poly_vals(t4.den, 3, tmp, thr);
  chk("B.t4d", tmp, thr, "0x1.0p0 -0x1.0p-1 X");
  series(t4, 5, tmp);
  chk("B.t4", tmp, false, "0x1.0p0 0x1.0p-1 0x1.0p-2 0x1.0p-3 0x1.0p-4");
  RatFunc d4 = f1.divide(f2);
  poly_vals(d4.num, 3, tmp, thr);
  chk("B.d4n", tmp, thr, "0x1.0p0 0x1.0p0 0x1.0p-2");
  poly_vals(d4.den, 3, tmp, thr);
  chk("B.d4d", tmp, thr, "0x1.0p0 -0x1.0p-1 X");
  series(d4, 5, tmp);
  chk("B.d4", tmp, false, "0x1.0p0 0x1.8p0 0x1.0p0 0x1.0p-1 0x1.0p-2");
  RatFunc s4 = f1.plus(f2);
  poly_vals(s4.num, 4, tmp, thr);
  chk("B.s4n", tmp, thr, "0x1.0p1 0x1.0p-1 0x1.0p-2 X");
  poly_vals(s4.den, 4, tmp, thr);
  chk("B.s4d", tmp, thr, "0x1.0p0 0x0.0p0 -0x1.0p-2 X");
  series(s4, 4, tmp);
  chk("B.s4", tmp, false, "0x1.0p1 0x1.0p-1 0x1.8p-1 0x1.0p-3");
  RatFunc m4 = f1.minus(f2);
  poly_vals(m4.num, 4, tmp, thr);
  chk("B.m4n", tmp, thr, "0x0.0p0 0x1.8p0 0x1.0p-2 X");  // minus: raw (unequal deg)
  poly_vals(m4.den, 4, tmp, thr);
  chk("B.m4d", tmp, thr, "0x1.0p0 0x0.0p0 -0x1.0p-2 X");
  series(m4, 4, tmp);
  chk("B.m4", tmp, false, "0x0.0p0 0x1.8p0 0x1.0p-2 0x1.8p-2");

  // ---- B5: evaluateAt (+ pole refusals) -------------------------------------
  chk1("B.a5v", RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, -0.5})).evaluate_at(0.25),
       "0x1.2492492492492p0");
  {
    bool caught = false;
    try {
      RatFunc::of(Poly(vd{1.0}), Poly(vd{0.0, 1.0})).evaluate_at(0.0);
    } catch (const polynomial_exception& e) {
      caught = std::strcmp(e.what(), "rf_err_pole") == 0;
    }
    chk_true("B.a5pole", caught);
    caught = false;
    try {
      RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, -1.0})).evaluate_at(cplx{1.0, 0.0});
    } catch (const polynomial_exception& e) {
      caught = std::strcmp(e.what(), "rf_err_pole") == 0;
    }
    chk_true("B.a5cpole", caught);
  }

  // ---- B6/B7: simplify paths (exact / roots / false) ------------------------
  RatFunc t6 = RatFunc::of(Poly(vd{2.0, 0.4, 0.7 + 1e-13, 0.2, -0.15}), Poly(vd{1.0}))
                   .times(RatFunc::of(Poly(vd{1.0}), Poly(vd{2.0, 0.0, 1.0})));
  poly_vals(t6.num, 5, tmp, thr);
  chk("B.t6n", tmp, thr, "0x1.0000000000015p0 0x1.999999999999ap-3 -0x1.3333333332dd8p-3 X X");
  poly_vals(t6.den, 5, tmp, thr);
  chk("B.t6d", tmp, thr, "0x1.0p0 X");
  series(t6, 4, tmp);
  chk("B.t6", tmp, false,
      "0x1.0000000000015p0 0x1.999999999999ap-3 -0x1.3333333332dd8p-3 0x0.0p0");

  RatFunc t7 = RatFunc::of(Poly(vd{1.0, 0.8 + 1e-13, -0.15}), Poly(vd{1.0}))
                   .times(RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, 0.3})));
  poly_vals(t7.num, 3, tmp, thr);
  chk("B.t7n", tmp, thr, "0x1.0p0 0x1.9999999999d1fp-1 -0x1.3333333333333p-3");
  poly_vals(t7.den, 3, tmp, thr);
  chk("B.t7d", tmp, thr, "0x1.0p0 0x1.3333333333333p-2 X");
  series(t7, 4, tmp);
  chk("B.t7", tmp, false,
      "0x1.0p0 0x1.0000000000386p-1 -0x1.333333333355p-2 0x1.70a3d70a3d993p-4");

  // Roots path (real): small = (1+2z)(1+3z) shares ONLY the -1/3 root with
  // big = (1+3z)(1+100z) -> inexact LS; roots of SMALL; the im==0 branch runs
  // the f1 divide AND the f2 stale-tail divide; numerator stays `big'.
  RatFunc t7b = RatFunc::of(Poly(vd{1.0, 103.0, 300.0}), Poly(vd{1.0}))
                    .times(RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, 5.0, 6.0})));
  poly_vals(t7b.num, 4, tmp, thr);
  chk("B.t7bn", tmp, thr, "0x1.0p0 0x1.9cp6 0x1.2cp8 X");
  poly_vals(t7b.den, 4, tmp, thr);
  chk("B.t7bd", tmp, thr, "0x1.cc70fdc8d63eap-15 0x1.179ec9cbd821ep-14 X");
  series(t7b, 4, tmp);
  chk("B.t7b", tmp, false,
      "0x1.1caa7e720212ap14 0x1.c4bb655fa6703p20 0x1.883f58b8706e1p21 "
      "-0x1.dc69f59a92eb4p21");

  RatFunc t7c = RatFunc::of(Poly(vd{2.0, 200.0, 1.0, 100.0}), Poly(vd{1.0}))
                    .times(RatFunc::of(Poly(vd{1.0}), Poly(vd{2.0, 0.0, 1.0})));
  poly_vals(t7c.num, 5, tmp, thr);
  chk("B.t7cn", tmp, thr, "0x1.0p0 0x1.9p6 X");
  poly_vals(t7c.den, 5, tmp, thr);
  chk("B.t7cd", tmp, thr, "0x1.0p0 X");
  series(t7c, 4, tmp);
  chk("B.t7c", tmp, false, "0x1.0p0 0x1.9p6 0x0.0p0 0x0.0p0");

  RatFunc t7d = RatFunc::of(Poly(vd{1.0, 103.0, 300.0}), Poly(vd{1.0}))
                    .times(RatFunc::of(Poly(vd{1.0}), Poly(vd{2.0, 0.0, 1.0})));
  poly_vals(t7d.num, 4, tmp, thr);
  chk("B.t7dn", tmp, thr, "0x1.0p0 0x1.9cp6 0x1.2cp8 X");
  poly_vals(t7d.den, 4, tmp, thr);
  chk("B.t7dd", tmp, thr, "0x1.0p1 0x0.0p0 0x1.0p0 X");
  series(t7d, 4, tmp);
  chk("B.t7d", tmp, false, "0x1.0p-1 0x1.9cp5 0x1.2b8p7 -0x1.9cp4");

  // ---- B8: RationalBackFilter.times ------------------------------------------
  RatBackFilter rbA(RatFunc::of(Poly(vd{1.0, 0.5}), Poly(vd{1.0, -0.5})), 0);
  RatBackFilter rbB(RatFunc::of(Poly(vd{1.0, -0.3}), Poly(vd{1.0, 0.5})), 1);
  RatBackFilter rbC = rbA.times(rbB);
  {
    static const char* rw[8] = {"0x1.999999999999ap-9", "0x1.999999999999ap-8",
                                "0x1.999999999999ap-7", "0x1.999999999999ap-6",
                                "0x1.999999999999ap-5", "0x1.999999999999ap-4",
                                "0x1.999999999999ap-3", "0x1.0p0"};
    for (int t = -8, k = 0; t <= -1; ++t, ++k) chk1("B.rbC", rbC.weight(t), rw[k]);
    for (int t = 0; t <= 1; ++t) {
      bool thrown = false;
      try {
        rbC.weight(t);  // rf.get(-bshift - t) = get(-1), get(-2) -> AIOOBE parity
      } catch (const std::out_of_range&) {
        thrown = true;
      }
      chk_true("B.rbC.threw", thrown);
    }
    chk_true("B.rbCb", rbC.get_l_bound() == INT_MIN && rbC.get_u_bound() == -1 &&
                              !rbC.has_lower_bound());
  }
  cplx rbfr = rbC.frequency_response(0.5);
  chk1("B.rbCfr.re", rbfr.re, "0x1.dd3bd8655b997p-1");
  chk1("B.rbCfr.im", rbfr.im, "-0x1.9aecec247edfap-1");

  // ---- B9: RationalForeFilter.times (k-normalization, fshift sum) -----------
  RatForeFilter ffA(RatFunc::of(Poly(vd{1.0, 0.5}), Poly(vd{2.0, 1.0})), 0);
  RatForeFilter ffB(RatFunc::of(Poly(vd{1.0, -0.5}), Poly(vd{1.0, 0.25})), 2);
  RatForeFilter ffC = ffA.times(ffB);
  poly_vals(ffC.get_numerator().p, 4, tmp, thr);
  chk("B.ffCn", tmp, thr, "0x1.0p-1 0x0.0p0 -0x1.0p-3 X");
  poly_vals(ffC.get_denominator().p, 4, tmp, thr);
  chk("B.ffCd", tmp, thr, "0x1.0p0 0x1.8p-1 0x1.0p-3 X");
  {
    static const char* fw[7] = {"0x1.0p-1", "-0x1.8p-2", "0x1.8p-4", "-0x1.8p-6",
                                "0x1.8p-8", "-0x1.8p-10", "0x1.8p-12"};
    for (int t = 0; t <= 6; ++t) chk1("B.ffC", ffC.weight(t), fw[t]);
  }

  // ---- B10: lazy RationalFilter (SymmetricFilter denominator) ---------------
  {
    RatBackFilter rbZ(RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, -0.5})), 0);
    RatForeFilter ffZ(RatFunc::of(Poly(vd{1.0}), Poly(vd{1.0, -0.5})), 0);
    RationalFilter lat(rbZ, ffZ, nullptr, nullptr);
    std::shared_ptr<FinFilter> dnl = lat.get_denominator();
    chk_true("B.dnl.sym", dynamic_cast<SymmetricFilter*>(dnl.get()) != nullptr);
    chk_true("B.dnl.bounds", dnl->get_lower_bound() == -1 && dnl->get_upper_bound() == 1);
    chk("B.dnlw", dnl->weights_to_array(), false, "-0x1.0p-1 0x1.4p0 -0x1.0p-1");
    chk("B.nmlw", lat.get_numerator()->weights_to_array(), false,
        "-0x1.0p-1 0x1.0p1 -0x1.0p-1");
    cplx fl = lat.frequency_response(0.4);
    chk1("B.fl.re", fl.re, "0x1.a3d8f0784354ep1");
    chk1("B.fl.im", fl.im, "0x0.0p0");
    chk1("B.wt0", lat.weight(0), "0x1.0p1");
    chk1("B.wtm1", lat.weight(-1), "0x1.0p-1");
    chk1("B.wt1", lat.weight(1), "0x1.0p-1");
    chk_true("B.latb", lat.get_l_bound() == 0 && lat.get_u_bound() == INT_MAX);
  }

  // ---- B11: frequency responses of the concrete filters ----------------------
  {
    FiniteFilter w11(vd{0.25, 0.5, 0.25}, -1);
    cplx f = w11.frequency_response(0.3);
    chk1("B.fw11.re", f.re, "0x1.f490eea1784dcp-1");
    chk1("B.fw11.im", f.im, "0x0.0p0");
    BackFilter bd = BackFilter::of_internal(vd{1.0, -0.5});
    ForeFilter fd = ForeFilter::of_internal(vd{1.0, -0.5});
    f = bd.frequency_response(0.3);
    chk1("B.bw11.re", f.re, "0x1.0b6f115e87b23p-1");
    chk1("B.bw11.im", f.im, "0x1.2e9cd95baba33p-3");
    f = fd.frequency_response(0.3);
    chk1("B.ffw11.re", f.re, "0x1.0b6f115e87b23p-1");
    chk1("B.ffw11.im", f.im, "-0x1.2e9cd95baba33p-3");
    SymmetricFilter sf = SymmetricFilter::of_internal(vd{1.25, -0.5});
    f = sf.frequency_response(0.3);
    chk1("B.sfw.re", f.re, "0x1.2dbc457a1ec8cp-2");
    chk1("B.sfw.im", f.im, "0x0.0p0");
    chk("B.sfwA", sf.weights_to_array(), false, "-0x1.0p-1 0x1.4p0 -0x1.0p-1");
  }

  // ---- B12: FiniteFilter statics ---------------------------------------------
  {
    FiniteFilter w11(vd{0.25, 0.5, 0.25}, -1);
    FiniteFilter g12(vd{1.0, 2.0}, 0);
    FiniteFilter ac = ff_add_d(w11, 2.5);
    chk("B.addc", ac.weights_to_array(), false, "0x1.0p-2 0x1.8p1 0x1.0p-2");
    chk_true("B.addc.b", ac.get_lower_bound() == -1 && ac.get_upper_bound() == 1);
    FiniteFilter af = ff_add(w11, g12);
    chk("B.addf", af.weights_to_array(), false, "0x1.0p-2 0x1.8p0 0x1.2p1");
    chk_true("B.addf.b", af.get_lower_bound() == -1 && af.get_upper_bound() == 1);
    FiniteFilter mf = ff_multiply(w11, g12);
    chk("B.mulf", mf.weights_to_array(), false, "0x1.0p-2 0x1.0p0 0x1.4p0 0x1.0p-1");
    chk_true("B.mulf.b", mf.get_lower_bound() == -1 && mf.get_upper_bound() == 2);
    chk("B.mulc", ff_multiply_d(w11, 3.0).weights_to_array(), false,
        "0x1.8p-1 0x1.8p0 0x1.8p-1");
    chk("B.subf", ff_subtract(w11, g12).weights_to_array(), false,
        "0x1.0p-2 -0x1.0p-1 -0x1.cp0");
    chk("B.neg", ff_negate(w11).weights_to_array(), false,
        "-0x1.0p-2 -0x1.0p-1 -0x1.0p-2");
    FiniteFilter mi = w11.mirror();
    chk("B.mir", mi.weights_to_array(), false, "0x1.0p-2 0x1.0p-1 0x1.0p-2");
    chk_true("B.mir.b", mi.get_lower_bound() == -1 && mi.get_upper_bound() == 1);
    chk_true("B.sym", !FiniteFilter(vd{1.0, 2.0, 1.0}, -1).is_symmetric());
    chk_true("B.sym2", !FiniteFilter(vd{1.0, 2.0}, -1).is_symmetric());
    chk_true("B.sym3", FiniteFilter(vd{1.0, 2.0, 1.0}, -2).is_symmetric());
  }

  // ---- B13: eager of(BF, BF, FF, FF) denominator ------------------------------
  {
    BackFilter bd = BackFilter::of_internal(vd{1.0, -0.5});
    ForeFilter fd = ForeFilter::of_internal(vd{1.0, -0.5});
    RationalFilter rf13 = RationalFilter::of_bbff(bd, bd, fd, fd);
    std::shared_ptr<FinFilter> d13 = rf13.get_denominator();
    chk_true("B.d13.ff", dynamic_cast<FiniteFilter*>(d13.get()) != nullptr &&
                            dynamic_cast<SymmetricFilter*>(d13.get()) == nullptr);
    chk("B.d13w", d13->weights_to_array(), false, "-0x1.0p-1 0x1.4p0 -0x1.0p-1");
  }

  // ---- B14: zero RationalBackFilter --------------------------------------------
  {
    RatBackFilter zrb;
    chk1("B.z0", zrb.weight(0), "0x0.0p0");
    chk1("B.zm5", zrb.weight(-5), "0x0.0p0");
    chk_true("B.z", zrb.has_lower_bound() && zrb.has_upper_bound() &&
                        zrb.get_u_bound() == 0);
  }

  // ---- E: AutoCovarianceFunction (cluster 2, AcfProbe ground truth) ---------
  {
    sa::Acf a1 = sa::Acf::of_polys(Poly(vd{1.0, -0.6}), Poly(vd{1.0}), 1.0);
    chk("E.a1v5", a1.values(5), false, "0x1.5c28f5c28f5c2p0 -0x1.3333333333333p-1 0x0.0p0 0x0.0p0 0x0.0p0");
    chk("E.a1v40", a1.values(40), false, "0x1.5c28f5c28f5c2p0 -0x1.3333333333333p-1 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 -0x0.0p0 " 
                  "-0x0.0p0 -0x0.0p0 -0x0.0p0");
    chk_true("E.a1v0", a1.values(0).empty());
    {
      vd g;
      for (int i = 0; i < 4; ++i) g.push_back(a1.get(i));
      chk("E.a1g", g, false, "0x1.5c28f5c28f5c2p0 -0x1.3333333333333p-1 0x0.0p0 0x0.0p0");
    }
    chk1("E.a1g100", a1.get(100), "-0x0.0p0");
    chk_true("E.a1b", a1.has_bound() && a1.get_bound() == 2);
    sa::Acf a2 = sa::Acf::of_polys(Poly(vd{1.0, -0.6}), Poly(vd{1.0}), 2.0);
    chk("E.a2v5", a2.values(5), false, "0x1.5c28f5c28f5c2p1 -0x1.3333333333333p0 0x0.0p0 0x0.0p0 0x0.0p0");
    sa::Acf a3 = sa::Acf::of_polys(Poly(vd{1.0, -0.6}), Poly(vd{1.0}), 0.0);
    chk("E.a3v5", a3.values(5), false, "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0");
    sa::Acf a4 = sa::Acf::of_polys(Poly(vd{1.0, -0.6}), Poly(vd{1.0}),
                                   std::numeric_limits<double>::quiet_NaN());
    chk("E.a4v5", a4.values(5), false, "NaN NaN 0x0.0p0 0x0.0p0 0x0.0p0");
  }

  // ---- F: default computer (Toeplitz + LU2), cache quantization, failures ---
  {
    sa::Acf b1 = sa::Acf::of_polys(Poly(vd{1.0}), Poly(vd{1.0, -0.5}), 1.0);
    chk("F.b1v5", b1.values(5), false, "0x1.5555555555555p0 0x1.5555555555555p-1 0x1.5555555555555p-2 " 
                  "0x1.5555555555555p-3 0x1.5555555555555p-4");
    chk("F.b1v36", b1.values(36), false, "0x1.5555555555555p0 0x1.5555555555555p-1 0x1.5555555555555p-2 " 
                  "0x1.5555555555555p-3 0x1.5555555555555p-4 0x1.5555555555555p-5 " 
                  "0x1.5555555555555p-6 0x1.5555555555555p-7 0x1.5555555555555p-8 " 
                  "0x1.5555555555555p-9 0x1.5555555555555p-10 0x1.5555555555555p-11 " 
                  "0x1.5555555555555p-12 0x1.5555555555555p-13 0x1.5555555555555p-14 " 
                  "0x1.5555555555555p-15 0x1.5555555555555p-16 0x1.5555555555555p-17 " 
                  "0x1.5555555555555p-18 0x1.5555555555555p-19 0x1.5555555555555p-20 " 
                  "0x1.5555555555555p-21 0x1.5555555555555p-22 0x1.5555555555555p-23 " 
                  "0x1.5555555555555p-24 0x1.5555555555555p-25 0x1.5555555555555p-26 " 
                  "0x1.5555555555555p-27 0x1.5555555555555p-28 0x1.5555555555555p-29 " 
                  "0x1.5555555555555p-30 0x1.5555555555555p-31 0x1.5555555555555p-32 " 
                  "0x1.5555555555555p-33 0x1.5555555555555p-34 0x1.5555555555555p-35");
    chk("F.b1v37", b1.values(37), false, "0x1.5555555555555p0 0x1.5555555555555p-1 0x1.5555555555555p-2 " 
                  "0x1.5555555555555p-3 0x1.5555555555555p-4 0x1.5555555555555p-5 " 
                  "0x1.5555555555555p-6 0x1.5555555555555p-7 0x1.5555555555555p-8 " 
                  "0x1.5555555555555p-9 0x1.5555555555555p-10 0x1.5555555555555p-11 " 
                  "0x1.5555555555555p-12 0x1.5555555555555p-13 0x1.5555555555555p-14 " 
                  "0x1.5555555555555p-15 0x1.5555555555555p-16 0x1.5555555555555p-17 " 
                  "0x1.5555555555555p-18 0x1.5555555555555p-19 0x1.5555555555555p-20 " 
                  "0x1.5555555555555p-21 0x1.5555555555555p-22 0x1.5555555555555p-23 " 
                  "0x1.5555555555555p-24 0x1.5555555555555p-25 0x1.5555555555555p-26 " 
                  "0x1.5555555555555p-27 0x1.5555555555555p-28 0x1.5555555555555p-29 " 
                  "0x1.5555555555555p-30 0x1.5555555555555p-31 0x1.5555555555555p-32 " 
                  "0x1.5555555555555p-33 0x1.5555555555555p-34 0x1.5555555555555p-35 " 
                  "0x1.5555555555555p-36");
    chk("F.b1v80", b1.values(80), false, "0x1.5555555555555p0 0x1.5555555555555p-1 0x1.5555555555555p-2 " 
                  "0x1.5555555555555p-3 0x1.5555555555555p-4 0x1.5555555555555p-5 " 
                  "0x1.5555555555555p-6 0x1.5555555555555p-7 0x1.5555555555555p-8 " 
                  "0x1.5555555555555p-9 0x1.5555555555555p-10 0x1.5555555555555p-11 " 
                  "0x1.5555555555555p-12 0x1.5555555555555p-13 0x1.5555555555555p-14 " 
                  "0x1.5555555555555p-15 0x1.5555555555555p-16 0x1.5555555555555p-17 " 
                  "0x1.5555555555555p-18 0x1.5555555555555p-19 0x1.5555555555555p-20 " 
                  "0x1.5555555555555p-21 0x1.5555555555555p-22 0x1.5555555555555p-23 " 
                  "0x1.5555555555555p-24 0x1.5555555555555p-25 0x1.5555555555555p-26 " 
                  "0x1.5555555555555p-27 0x1.5555555555555p-28 0x1.5555555555555p-29 " 
                  "0x1.5555555555555p-30 0x1.5555555555555p-31 0x1.5555555555555p-32 " 
                  "0x1.5555555555555p-33 0x1.5555555555555p-34 0x1.5555555555555p-35 " 
                  "0x1.5555555555555p-36 0x1.5555555555555p-37 0x1.5555555555555p-38 " 
                  "0x1.5555555555555p-39 0x1.5555555555555p-40 0x1.5555555555555p-41 " 
                  "0x1.5555555555555p-42 0x1.5555555555555p-43 0x1.5555555555555p-44 " 
                  "0x1.5555555555555p-45 0x1.5555555555555p-46 0x1.5555555555555p-47 " 
                  "0x1.5555555555555p-48 0x1.5555555555555p-49 0x1.5555555555555p-50 " 
                  "0x1.5555555555555p-51 0x1.5555555555555p-52 0x1.5555555555555p-53 " 
                  "0x1.5555555555555p-54 0x1.5555555555555p-55 0x1.5555555555555p-56 " 
                  "0x1.5555555555555p-57 0x1.5555555555555p-58 0x1.5555555555555p-59 " 
                  "0x1.5555555555555p-60 0x1.5555555555555p-61 0x1.5555555555555p-62 " 
                  "0x1.5555555555555p-63 0x1.5555555555555p-64 0x1.5555555555555p-65 " 
                  "0x1.5555555555555p-66 0x1.5555555555555p-67 0x1.5555555555555p-68 " 
                  "0x1.5555555555555p-69 0x1.5555555555555p-70 0x1.5555555555555p-71 " 
                  "0x1.5555555555555p-72 0x1.5555555555555p-73 0x1.5555555555555p-74 " 
                  "0x1.5555555555555p-75 0x1.5555555555555p-76 0x1.5555555555555p-77 " 
                  "0x1.5555555555555p-78 0x1.5555555555555p-79");
    chk_true("F.b1b", !b1.has_bound() && b1.get_bound() == -1);
    sa::Acf b2 = sa::Acf::of_polys(Poly(vd{1.0, 0.4}), Poly(vd{1.0, -0.3}), 1.5);
    {
      vd g;
      for (int i = 35; i < 37; ++i) g.push_back(b2.get(i));
      chk("F.b2q1", g, false, "0x1.3e0d57f5629c6p-59 0x1.7da99cc00feedp-61");
      g.clear();
      for (int i = 71; i < 74; ++i) g.push_back(b2.get(i));
      chk("F.b2q2", g, false, "0x1.b84df87fdf496p-122 0x1.082ec84cb92cp-123 0x1.3d04f05c1168p-125");
      g.clear();
      for (int i = 0; i < 3; ++i) g.push_back(b2.get(i));
      chk("F.b2q3", g, false, "0x1.2762762762762p1 0x1.4ad4ad4ad4ad4p0 0x1.8cff3659cc031p-2");
    }
    sa::Acf c1 = sa::Acf::of_polys(Poly(vd{1.0, 0.2, 0.3}), Poly(vd{1.0, -0.5}), 1.0);
    chk("F.c1v3", c1.values(3), false, "0x1.06d3a06d3a06dp1 0x1.6fc962fc962fdp0 0x1.04b17e4b17e4bp0");
    chk("F.c1v50", c1.values(50), false, "0x1.06d3a06d3a06dp1 0x1.6fc962fc962fdp0 0x1.04b17e4b17e4bp0 " 
                  "0x1.04b17e4b17e4bp-1 0x1.04b17e4b17e4bp-2 0x1.04b17e4b17e4bp-3 " 
                  "0x1.04b17e4b17e4bp-4 0x1.04b17e4b17e4bp-5 0x1.04b17e4b17e4bp-6 " 
                  "0x1.04b17e4b17e4bp-7 0x1.04b17e4b17e4bp-8 0x1.04b17e4b17e4bp-9 " 
                  "0x1.04b17e4b17e4bp-10 0x1.04b17e4b17e4bp-11 0x1.04b17e4b17e4bp-12 " 
                  "0x1.04b17e4b17e4bp-13 0x1.04b17e4b17e4bp-14 0x1.04b17e4b17e4bp-15 " 
                  "0x1.04b17e4b17e4bp-16 0x1.04b17e4b17e4bp-17 0x1.04b17e4b17e4bp-18 " 
                  "0x1.04b17e4b17e4bp-19 0x1.04b17e4b17e4bp-20 0x1.04b17e4b17e4bp-21 " 
                  "0x1.04b17e4b17e4bp-22 0x1.04b17e4b17e4bp-23 0x1.04b17e4b17e4bp-24 " 
                  "0x1.04b17e4b17e4bp-25 0x1.04b17e4b17e4bp-26 0x1.04b17e4b17e4bp-27 " 
                  "0x1.04b17e4b17e4bp-28 0x1.04b17e4b17e4bp-29 0x1.04b17e4b17e4bp-30 " 
                  "0x1.04b17e4b17e4bp-31 0x1.04b17e4b17e4bp-32 0x1.04b17e4b17e4bp-33 " 
                  "0x1.04b17e4b17e4bp-34 0x1.04b17e4b17e4bp-35 0x1.04b17e4b17e4bp-36 " 
                  "0x1.04b17e4b17e4bp-37 0x1.04b17e4b17e4bp-38 0x1.04b17e4b17e4bp-39 " 
                  "0x1.04b17e4b17e4bp-40 0x1.04b17e4b17e4bp-41 0x1.04b17e4b17e4bp-42 " 
                  "0x1.04b17e4b17e4bp-43 0x1.04b17e4b17e4bp-44 0x1.04b17e4b17e4bp-45 " 
                  "0x1.04b17e4b17e4bp-46 0x1.04b17e4b17e4bp-47");
    {
      bool threw = false;
      std::string msg;
      try {
        sa::Acf d1 = sa::Acf::of_polys(Poly(vd{1.0}), Poly(vd{1.0, -1.0}), 1.0);
        (void)d1.values(5);
      } catch (const sa::arima_exception& e) {
        threw = true;
        msg = e.what();
      }
      chk_true("F.d1", threw && msg == "acgf of non stationary model");
      threw = false;
      msg.clear();
      try {
        sa::Acf d2 = sa::Acf::of_polys(Poly(vd{1.0}), Poly(vd{1.0, -2.0, 1.0}), 1.0);
        (void)d2.values(9);
      } catch (const sa::arima_exception& e) {
        threw = true;
        msg = e.what();
      }
      chk_true("F.d2", threw && msg == "acgf of non stationary model");
    }
    {  // get(i < 0) AIOOBE parity (after prepare ran)
      bool threw = false;
      try {
        (void)b1.get(-1);
      } catch (const std::out_of_range&) {
        threw = true;
      }
      chk_true("F.h1", threw);
    }
  }

  // ---- G: LinearProcess lazy acf ---------------------------------------------
  {
    FiniteFilter wE(Poly(vd{1.0, -0.6}), 0);
    RationalFilter rfE =
        RationalFilter::of_wbf(wE, BackFilter::of_internal(vd{1.0, -0.5}), ForeFilter::one());
    sa::LinearProc lpE(rfE, 1.7);
    chk_true("G.lpE", !lpE.is_null() && lpE.is_invertible() && lpE.is_stationary());
    chk("G.lpEv5", lpE.get_auto_covariance_function().values(5), false, "0x1.b900aec33e1f7p0 -0x1.44f3078263ab1p-3 -0x1.44f3078263ab1p-4 " 
                  "-0x1.44f3078263ab1p-5 -0x1.44f3078263ab1p-6");
    chk("G.lpEv50", lpE.get_auto_covariance_function().values(50), false, "0x1.b900aec33e1f7p0 -0x1.44f3078263ab1p-3 -0x1.44f3078263ab1p-4 " 
                  "-0x1.44f3078263ab1p-5 -0x1.44f3078263ab1p-6 -0x1.44f3078263ab1p-7 " 
                  "-0x1.44f3078263ab1p-8 -0x1.44f3078263ab1p-9 -0x1.44f3078263ab1p-10 " 
                  "-0x1.44f3078263ab1p-11 -0x1.44f3078263ab1p-12 -0x1.44f3078263ab1p-13 " 
                  "-0x1.44f3078263ab1p-14 -0x1.44f3078263ab1p-15 -0x1.44f3078263ab1p-16 " 
                  "-0x1.44f3078263ab1p-17 -0x1.44f3078263ab1p-18 -0x1.44f3078263ab1p-19 " 
                  "-0x1.44f3078263ab1p-20 -0x1.44f3078263ab1p-21 -0x1.44f3078263ab1p-22 " 
                  "-0x1.44f3078263ab1p-23 -0x1.44f3078263ab1p-24 -0x1.44f3078263ab1p-25 " 
                  "-0x1.44f3078263ab1p-26 -0x1.44f3078263ab1p-27 -0x1.44f3078263ab1p-28 " 
                  "-0x1.44f3078263ab1p-29 -0x1.44f3078263ab1p-30 -0x1.44f3078263ab1p-31 " 
                  "-0x1.44f3078263ab1p-32 -0x1.44f3078263ab1p-33 -0x1.44f3078263ab1p-34 " 
                  "-0x1.44f3078263ab1p-35 -0x1.44f3078263ab1p-36 -0x1.44f3078263ab1p-37 " 
                  "-0x1.44f3078263ab1p-38 -0x1.44f3078263ab1p-39 -0x1.44f3078263ab1p-40 " 
                  "-0x1.44f3078263ab1p-41 -0x1.44f3078263ab1p-42 -0x1.44f3078263ab1p-43 " 
                  "-0x1.44f3078263ab1p-44 -0x1.44f3078263ab1p-45 -0x1.44f3078263ab1p-46 " 
                  "-0x1.44f3078263ab1p-47 -0x1.44f3078263ab1p-48 -0x1.44f3078263ab1p-49 " 
                  "-0x1.44f3078263ab1p-50 -0x1.44f3078263ab1p-51");
    RationalFilter rfF = RationalFilter::of_wbf(
        wE, BackFilter::of_internal(vd{1.0, -0.5}), ForeFilter::of_internal(vd{1.0, -0.3}));
    sa::LinearProc lpF(rfF, 1.0);
    chk("G.lpFv50", lpF.get_auto_covariance_function().values(50), false, "0x1.0a89210a89212p0 0x1.9ecea19ecea22p-3 0x1.80180180180cfp-8 " 
                  "-0x1.a4f3280b598cbp-6 -0x1.5f29d34aaef11p-6 -0x1.b39392da7d0acp-7 " 
                  "-0x1.e6396c30c580ap-8 -0x1.024e774bdee3cp-8 -0x1.0b6c519df62c4p-9 " 
                  "-0x1.10e4a168d0f16p-10 -0x1.142cd148edce1p-11 -0x1.1624ee0298b8ep-12 " 
                  "-0x1.175365a53279p-13 -0x1.1808e0a05b85ep-14 -0x1.1875c403da8dap-15 " 
                  "-0x1.18b7193f8d2bep-16 -0x1.18de4c635ebe1p-17 -0x1.18f5d178dc7c4p-18 " 
                  "-0x1.1903ee1f5b21ap-19 -0x1.190c65b673eb5p-20 -0x1.19117a444f979p-21 " 
                  "-0x1.191486996cffp-22 -0x1.19165acc7ea38p-23 -0x1.191773b7ef9fep-24 " 
                  "-0x1.19181c45336a8p-25 -0x1.19188166c27dcp-26 -0x1.1918be147ebc9p-27 " 
                  "-0x1.1918e27cbc7bep-28 -0x1.1918f854e1884p-29 -0x1.191905702af62p-30 " 
                  "-0x1.19190d4d5704ep-31 -0x1.191912053e41p-32 -0x1.191914d9fc31ep-33 " 
                  "-0x1.1919168cd48f4p-34 -0x1.19191791bcc76p-35 -0x1.1919182e481c5p-36 " 
                  "-0x1.1919188c354f5p-37 -0x1.191918c4906ep-38 -0x1.191918e660b3ap-39 " 
                  "-0x1.191918faaa77p-40 -0x1.19191906d6b91p-41 -0x1.1919190e247a6p-42 " 
                  "-0x1.191919128654dp-43 -0x1.1919191527718p-44 -0x1.19191916bb4f9p-45 " 
                  "-0x1.19191917ada1ap-46 -0x1.191919183f062p-47 -0x1.1919191896428p-48 " 
                  "-0x1.19191918ca9ap-49 -0x1.19191918ea01cp-50");
    chk_true("G.lpFnull", sa::LinearProc(rfF, 0.0).is_null());
  }

  // ---- H: SymmetricFilter.decompose (SymmetricFilterAlgorithms lambda) -------
  {
    SymmetricFilter sF = SymmetricFilter::conv_of(BackFilter::of_internal(vd{1.0, 0.5}));
    chk("H.f1", sa::sf_decompose(sF, BackFilter::of_internal(vd{2.0})).p.c, false, "0x1.4p-2 0x1.0p-2");
    chk("H.f2", sa::sf_decompose(sF, BackFilter::of_internal(vd{1.0})).p.c, false, "0x1.4p-1 0x1.0p-1");
    chk("H.f3", sa::sf_decompose(sF, BackFilter::of_internal(vd{-1.0})).p.c, false, "0x1.4p-1 0x1.0p-1");
    chk("H.f4", sa::sf_decompose(sF, BackFilter::of_internal(vd{0.0})).p.c, false, "Infinity Infinity");
    chk("H.f5", sa::sf_decompose(sF, BackFilter::of_internal(vd{-0.5})).p.c, false, "0x1.4p0 0x1.0p0");
    SymmetricFilter sG = SymmetricFilter::conv_of(BackFilter::of_internal(vd{1.0, -0.5}));
    chk("H.f6", sa::sf_decompose(sG, BackFilter::of_internal(vd{1.0, -0.5})).p.c, false, "0x1.0p-1 -0x1.0p-2");
    {
      bool threw = false;
      try {
        (void)sa::sf_decompose(SymmetricFilter::one(),
                               BackFilter::of_internal(vd{1.0, -1.0}));
      } catch (const matrix_exception&) {
        threw = true;
      }
      chk_true("H.f7", threw);
    }
    SymmetricFilter sH =
        SymmetricFilter::conv_of(BackFilter::of_internal(vd{1.0, -0.3, -0.1}));
    chk("H.f8", sa::sf_decompose(sH, BackFilter::of_internal(vd{1.0, -0.5, 0.09})).p.c,
        false, "0x1.0d7ab7bdd27aep-1 -0x1.2e9529abad26p-4 -0x1.2dd004b4f9da5p-3");
  }

  // ---- I: symmetric computer through the 2-arg Acf ctor -----------------------
  {
    SymmetricFilter sG = SymmetricFilter::conv_of(BackFilter::of_internal(vd{1.0, -0.5}));
    sa::Acf g1 = sa::Acf::of_sym(sG, Poly(vd{1.0}));
    chk("I.g1v5", g1.values(5), false, "0x1.4p0 -0x1.0p-1 0x0.0p0 0x0.0p0 0x0.0p0");
    chk("I.g1v40", g1.values(40), false, "0x1.4p0 -0x1.0p-1 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 " 
                  "0x0.0p0 0x0.0p0 0x0.0p0 0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0");
    sa::Acf g2 = sa::Acf::of_sym(sG, Poly(vd{1.0, -0.5}));
    chk("I.g2v5", g2.values(5), false, "0x1.0p0 0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0");
    chk("I.g2v40", g2.values(40), false, "0x1.0p0 0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 " 
                  "-0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 " 
                  "-0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 " 
                  "-0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 " 
                  "-0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0 " 
                  "-0x0.0p0 -0x0.0p0 -0x0.0p0 -0x0.0p0");
  }

  wk_replay();
  xf_replay();
  bm_replay();

  std::printf("checks=%d failures=%d\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
