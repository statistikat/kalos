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
// Polynomial.get over-read).
#include "kalos/detail/seats_linear_filters.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_checks = 0, g_failures = 0;

void report(const std::string& what) {
  ++g_failures;
  if (g_failures <= 12) std::printf("FAIL: %s\n", what.c_str());
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

  std::printf("checks=%d failures=%d\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
