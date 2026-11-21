// Item 24-A: SEATS model validation + canonical-decomposition DECISION mirror.
//
// Every routine below transcribes the pinned jdplus 3.9.0 bytecode recorded in
// tmp_opencode/jdprobe (m5/ and m524/ dumps); the mirrored class is named in
// the comments. Operation order, comparison flavor (dcmpg vs dcmpl -> the *_g /
// *_l helpers) and rounding sequence are preserved; the build uses
// -ffp-contract=off so no FMA is introduced.
//
// Nothing here re-implements DefaultModelApproximator (register D6): where the
// live path would call it, this unit reports approximation_required instead.
// Spectral factorization (SymmetricFilter.factorize), the variance allocation
// and the post-allocation component values are item 24-B scope; the unit
// exposes only the pre-allocation (raw) structure and the decision layer.

#include "kalos/tramo_seats_model_validator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace kalos {
namespace {

using vd = std::vector<double>;

constexpr double kPi = 3.141592653589793;
constexpr double kMax = std::numeric_limits<double>::max();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kMinValue = 4.9E-324;  // Double.MIN_VALUE (LU pivot seed)
constexpr double kLuEps = 1e-13;        // LS_Factory.ROBUST_FACTORY (LULinearSystemSolver2 eps)

[[noreturn]] void refuse(const std::string& why) {
  throw std::invalid_argument("tramo_seats_model: " + why);
}

// Java comparison flavors: cmpg pushes +1 on NaN, cmpl pushes -1.
inline bool gt_g(double a, double b) { return !(a <= b); }  // dcmpg + ifgt
inline bool ge_g(double a, double b) { return !(a < b); }   // dcmpg + ifge
inline bool lt_g(double a, double b) { return a < b; }      // dcmpg + iflt
inline bool ge_l(double a, double b) { return a >= b; }     // dcmpl + ifge
inline bool lt_l(double a, double b) { return !(a >= b); }  // dcmpl + iflt
inline bool gt_l(double a, double b) { return a > b; }      // dcmpl + ifgt

bool is_nan(double v) { return v != v; }

// ---------------------------------------------------------------------------
// Complex (api.math.Complex / ComplexType).
// ---------------------------------------------------------------------------
struct cplx {
  double re = 0.0, im = 0.0;
};

cplx c_conj(cplx a) { return {a.re, -a.im}; }
cplx c_neg(cplx a) { return {-a.re, -a.im}; }
cplx c_minus(cplx a, cplx b) { return {a.re - b.re, a.im - b.im}; }
cplx c_times(cplx a, cplx b) {
  return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
}
cplx c_scale(cplx a, double s) {
  if (s == 1.0) return a;
  if (s == 0.0) return {0.0, 0.0};
  return {a.re * s, a.im * s};
}
cplx c_div_d(cplx a, double s) {
  if (s == 1.0) return a;
  return {a.re / s, a.im / s};
}
double c_absq(cplx a) { return a.re * a.re + a.im * a.im; }
double c_abs(double re, double im) {  // ComplexType.abs
  if (re == 0.0 && im == 0.0) return 0.0;
  double ar = std::fabs(re), ai = std::fabs(im);
  double mx = std::max(ar, ai), mn = std::min(ar, ai);
  if (mn == 0.0) return mx;
  double r = mn / mx;
  return mx * std::sqrt(1.0 + r * r);
}
double c_abs(cplx a) { return c_abs(a.re, a.im); }
double c_arg(cplx a) { return std::atan2(a.im, a.re); }

cplx c_inv(cplx a) {  // Complex.inv(): branches on |re| >= |im|
  if (!lt_g(std::fabs(a.re), std::fabs(a.im))) {
    double r = a.im / a.re;
    double d = 1.0 / (a.re + r * a.im);
    return {d, d * (-(a.im / a.re))};
  }
  double r = a.re / a.im;
  double d = 1.0 / (a.im + r * a.re);
  return {d * (a.re / a.im), -d};
}

// ---------------------------------------------------------------------------
// Coefficients.ofInternal / getUsedCoefficients (|c| <= 1e-9 trim from the top).
// ---------------------------------------------------------------------------
int used_coefficients(const vd& a, double eps) {
  int n = static_cast<int>(a.size());
  while (n > 0) {
    if (gt_g(std::fabs(a[static_cast<std::size_t>(n) - 1]), eps)) break;
    --n;
  }
  return n;
}

vd trim_copy(const vd& a) {
  int n = used_coefficients(a, 1e-9);
  if (n == 0) return {0.0};
  if (n == static_cast<int>(a.size())) return a;
  return vd(a.begin(), a.begin() + n);
}

// ---------------------------------------------------------------------------
// Polynomial. The coefficient array is kept EXACTLY as the JVM keeps it: the
// helpers that trim (of/ofInternal/plus/minus-with-flag) do, those that do not
// (times/derivate/divide(double)/valueOf via <init>) do not, because
// degree() is arrayLength - 1 in Java and callers depend on that.
// ---------------------------------------------------------------------------
struct Poly {
  vd c;
  Poly() : c{0.0} {}
  explicit Poly(vd a) : c(std::move(a)) {}
  int degree() const { return static_cast<int>(c.size()) - 1; }
  double get(std::size_t i) const { return c[i]; }
  bool is_zero() const { return c.size() == 1 && c[0] == 0.0; }
  bool is_identity() const { return c.size() == 1 && c[0] == 1.0; }
};

Poly p_of(vd a) { return Poly(trim_copy(a)); }   // Polynomial.of / ofInternal
Poly p_raw(vd a) { return Poly(std::move(a)); }  // new Polynomial(array)

Poly p_one() { return Poly(vd{1.0}); }

Poly p_value_of(double scale, const vd& params) {  // Polynomial.valueOf
  if (params.empty()) {
    if (scale == 0.0) return Poly(vd{0.0});
    if (scale == 1.0) return Poly(vd{1.0});
    return Poly(vd{scale});
  }
  vd a(params.size() + 1);
  a[0] = scale;
  for (std::size_t i = 0; i < params.size(); ++i) a[i + 1] = params[i];
  return p_raw(a);
}

// Polynomial.factor(double, int): 1 - x L^n (x == 0 -> ONE).
Poly p_factor(double x, int n) {
  if (x == 0.0) return p_one();
  vd a(static_cast<std::size_t>(n) + 1, 0.0);
  a[0] = 1.0;
  a[static_cast<std::size_t>(n)] = -x;
  return p_of(std::move(a));
}

vd pl_times(const vd& a, const vd& b) {  // Polynomials.times
  vd r(a.size() + b.size() - 1, 0.0);
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i] == 0.0) continue;
    for (std::size_t j = 0; j < b.size(); ++j) {
      if (b[j] == 0.0) continue;
      r[i + j] += a[i] * b[j];
    }
  }
  return r;
}

Poly p_times(const Poly& a, const Poly& b) {  // times(P) / times(P, boolean)
  if (b.is_zero() || a.is_zero()) return Poly(vd{0.0});
  if (b.is_identity()) return a;
  if (a.is_identity()) return b;
  return p_raw(pl_times(a.c, b.c));
}

Poly p_times_d(const Poly& a, double d) {  // Polynomial.times(double)
  if (d == 1.0) return a;
  if (d == 0.0 || a.is_zero()) return Poly(vd{0.0});
  vd r(a.c.size());
  for (std::size_t i = 0; i < r.size(); ++i) r[i] = d * a.c[i];
  return p_raw(std::move(r));
}

vd pl_plus(const vd& a, const vd& b) {  // Polynomials.plus
  const vd* lg = &a;
  const vd* sh = &b;
  if (a.size() < b.size()) {
    lg = &b;
    sh = &a;
  }
  vd r = *lg;
  for (std::size_t i = 0; i < sh->size(); ++i) r[i] += (*sh)[i];
  return r;
}

vd pl_minus(const vd& a, const vd& b) {  // Polynomials.minus
  if (a.size() >= b.size()) {
    vd r = a;
    for (std::size_t i = 0; i < b.size(); ++i) r[i] -= b[i];
    return r;
  }
  vd r = b;
  for (std::size_t i = 0; i < a.size(); ++i) r[i] = a[i] - b[i];
  for (std::size_t i = a.size(); i < b.size(); ++i) r[i] = -b[i];
  return r;
}

Poly p_plus(const Poly& a, const Poly& b) {  // plus() wraps Coefficients.ofInternal
  if (b.is_zero()) return a;
  return p_raw(trim_copy(pl_plus(a.c, b.c)));
}

Poly p_minus(const Poly& a, const Poly& b, bool trim) {
  vd r = pl_minus(a.c, b.c);
  return p_raw(trim ? trim_copy(r) : r);
}

Poly p_minus_p(const Poly& a, const Poly& b) {  // minus(P): trims iff equal degree
  return p_minus(a, b, a.degree() == b.degree());
}

Poly p_divide_d(const Poly& a, double d) { return p_times_d(a, 1.0 / d); }

double p_value_at(const Poly& p, double x) {  // Polynomial.evaluateAt (Horner)
  int n = p.degree();
  double v = p.c[static_cast<std::size_t>(n)];
  for (int i = n - 1; i >= 0; --i) v = v * x + p.c[static_cast<std::size_t>(i)];
  return v;
}

Poly p_derivate(const Poly& p) {  // Polynomial.derivate
  int n = p.degree();
  if (n == 0) return Poly(vd{0.0});
  vd r(static_cast<std::size_t>(n), 0.0);
  for (int i = 1; i <= n; ++i)
    r[static_cast<std::size_t>(i) - 1] =
        p.c[static_cast<std::size_t>(i)] * static_cast<double>(i);
  return p_raw(std::move(r));
}

// Polynomials.divide(double[],double[]) -> quotient (operands degree-trimmed at
// 1e-9; the quotient is used through Polynomial.ofInternal, i.e. trimmed).
vd pl_divide(const vd& a, const vd& b) {
  int n = static_cast<int>(a.size()) - 1;
  while (n >= 0 && !(ge_g(std::fabs(a[static_cast<std::size_t>(n)]), 1e-9))) --n;
  if (n < 0) return {0.0};  // Coefficients.zero()
  int m = static_cast<int>(b.size()) - 1;
  while (m >= 0 && !(ge_g(std::fabs(b[static_cast<std::size_t>(m)]), 1e-9))) --m;
  if (m < 0) return {kNan};  // Coefficients.nan()/negativeInfinity(): unused here
  if (m > n) return {0.0};
  vd ac = a;
  vd q(static_cast<std::size_t>(n - m + 1), 0.0);
  for (int k = n - m; k >= 0; --k) {
    q[static_cast<std::size_t>(k)] =
        ac[static_cast<std::size_t>(m + k)] / b[static_cast<std::size_t>(m)];
    for (int j = m + k - 1; j >= k; --j)
      ac[static_cast<std::size_t>(j)] -=
          q[static_cast<std::size_t>(k)] * b[static_cast<std::size_t>(j - k)];
  }
  return q;
}

Poly p_quotient(const Poly& a, const Poly& b) { return p_of(pl_divide(a.c, b.c)); }

// Polynomial.divide(Polynomial, Polynomial) -> Division(remainder, quotient).
struct Division {
  Poly remainder, quotient;
  bool exact() const { return remainder.is_zero(); }
};

Division p_divide(const Poly& p, const Poly& q) {
  int n = p.degree(), m = q.degree();
  if (m > n) return Division{p, Poly(vd{0.0})};  // Division(dividend, ZERO)
  std::size_t nn = static_cast<std::size_t>(n) + 1;
  vd ac = p.c, quot(nn, 0.0);
  for (int k = n - m; k >= 0; --k) {
    double v = ac[static_cast<std::size_t>(m + k)] / q.c[static_cast<std::size_t>(m)];
    quot[static_cast<std::size_t>(k)] = v;
    for (int j = m + k - 1; j >= k; --j)
      ac[static_cast<std::size_t>(j)] -= v * q.c[static_cast<std::size_t>(j - k)];
  }
  Poly qq = p_raw(vd(quot.begin(), quot.begin() + static_cast<std::size_t>(n - m + 1)));
  Poly rem = m > 0 ? p_raw(trim_copy(vd(ac.begin(), ac.begin() + static_cast<std::size_t>(m))))
                   : Poly(vd{0.0});
  return Division{rem, std::move(qq)};
}

// ---------------------------------------------------------------------------
// Polynomial.roots() -> FastEigenValuesSolver.factorize. degree >= 3 would
// reach the qr cascade: outside the pinned envelope (register D11).
// ---------------------------------------------------------------------------
std::vector<cplx> p_roots(const Poly& p, const char* what) {
  int n = p.degree();
  if (n >= 3)
    refuse(std::string("degree > 2 root solving (") + what + ", degree " +
           std::to_string(n) + ")");
  std::vector<cplx> out;
  if (n <= 0) return out;
  if (n == 1) {  // monic
    out.push_back({-(p.c[0] / p.c[1]), 0.0});
    return out;
  }
  double a = p.c[2], b = p.c[1], cc = p.c[0];
  double d8 = 2.0 * a;
  double disc = b * b - 4.0 * a * cc;
  if (lt_g(disc, 0.0)) {
    double sq = std::sqrt(-disc);
    cplx r0{-b / d8, sq / d8};
    out.push_back(r0);
    out.push_back(c_conj(r0));
  } else {
    double sq = std::sqrt(disc);
    out.push_back({(-b + sq) / d8, 0.0});
    out.push_back({(-b - sq) / d8, 0.0});
  }
  return out;
}

// Polynomial.fromComplexRoots(Complex[], scale).
Poly p_from_roots(const std::vector<cplx>& roots, double scale = 1.0) {
  if (roots.empty()) return p_raw(vd{scale});
  std::size_t n = roots.size();
  std::vector<cplx> z(n + 1);
  z[0] = {1.0, 0.0};
  for (std::size_t i = 0; i < n; ++i) {
    z[i + 1] = z[i];  // leading slot (the copy IS the B^(i+1) term)
    for (std::size_t j = i; j >= 1; --j)
      z[j] = c_minus(z[j - 1], c_times(z[j], roots[i]));
    z[0] = c_times(z[0], c_neg(roots[i]));
  }
  double c0 = z[0].re;
  if (c0 == 0.0) refuse("fromComplexRoots with a zero root");
  double k = scale / c0;
  vd a(n + 1);
  for (std::size_t i = 0; i <= n; ++i) a[i] = z[i].re * k;
  a[0] = scale;
  return p_raw(std::move(a));
}

// ComplexUtility.lejaOrder.
void leja_order(std::vector<cplx>& z) {
  int n = static_cast<int>(z.size());
  if (n == 0) return;
  double best = 0.0;
  int idx = 0;
  for (int i = 0; i < n; ++i) {
    double s = c_absq(z[static_cast<std::size_t>(i)]);
    if (lt_g(best, s)) {
      best = s;
      idx = i;
    }
  }
  std::swap(z[0], z[static_cast<std::size_t>(idx)]);
  for (int i = 1; i < n; ++i) {
    double sc = 0.0;
    int pick = i;
    for (int j = i; j < n; ++j) {
      double d = 1.0;
      for (int k = 0; k < i; ++k) {
        double re = z[static_cast<std::size_t>(j)].re - z[static_cast<std::size_t>(k)].re;
        double im = z[static_cast<std::size_t>(j)].im - z[static_cast<std::size_t>(k)].im;
        d = d * (re * re + im * im);
      }
      if (lt_g(sc, d)) {
        sc = d;
        pick = j;
      }
    }
    std::swap(z[static_cast<std::size_t>(i)], z[static_cast<std::size_t>(pick)]);
  }
}

// ---------------------------------------------------------------------------
// UnitRoots: D(d) = 1 - L^d, D(d, n) = D(d)^n, S(s, n): 1 at powers k*n, k < s.
// SarimaUtility.differencingFilter(s, d, bd) = D(1, d) * D(s, bd).
// ---------------------------------------------------------------------------
Poly ur_d(int d) {
  vd a(static_cast<std::size_t>(d) + 1, 0.0);
  a[0] = 1.0;
  a[static_cast<std::size_t>(d)] = -1.0;
  return p_of(std::move(a));
}

Poly ur_d_n(int d, int n) {
  if (n == 0) return p_one();
  Poly base = ur_d(d), r = base;
  for (int i = 1; i < n; ++i) r = p_times(r, base);
  return r;
}

Poly ur_s(int s, int n) {
  vd a(static_cast<std::size_t>(n) * (static_cast<std::size_t>(s) - 1) + 1, 0.0);
  for (int i = 0; i < s; ++i) a[static_cast<std::size_t>(i) * n] = 1.0;
  return p_of(std::move(a));
}

Poly differencing_filter(int s, int d, int bd) {
  bool have = false;
  Poly acc;
  if (d > 0) {
    acc = ur_d_n(1, d);
    have = true;
  }
  if (bd > 0) {
    Poly sd = ur_d_n(s, bd);
    acc = have ? p_times(acc, sd) : sd;
    have = true;
  }
  return have ? acc : p_one();
}

// ---------------------------------------------------------------------------
// SymmetricFilter: a trimmed polynomial a[0..n] with response
// a[0] + sum_{k>=1} 2 a[k] cos(k x). BackFilter: one-sided coefficients.
// ---------------------------------------------------------------------------
struct SymF {
  vd a;
  SymF() : a{0.0} {}
  explicit SymF(vd v) : a(std::move(v)) {}
  int length() const { return static_cast<int>(a.size()); }
  int upper_bound() const { return static_cast<int>(a.size()) - 1; }
};

SymF sym_of(vd raw) { return SymF(trim_copy(raw)); }  // SymmetricFilter.ofInternal

// SymmetricFilter.convolutionOf(IFiniteFilter, double): autocorrelation, scaled.
SymF sym_convolution(const vd& w_in, double scale) {
  // BackFilter.weights() indexes polynomial.get(-k) with lower bound -degree,
  // so convolutionOf()'s input is the coefficient vector REVERSED; per-lag
  // products therefore accumulate in descending order (bit-visible on
  // non-palindromic inputs).
  vd w(w_in.rbegin(), w_in.rend());
  std::size_t n = w.size();
  vd r(n, 0.0);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = i; j < n; ++j) r[j - i] += w[i] * w[j];
  if (scale != 1.0)
    for (std::size_t i = 0; i < n; ++i) r[i] *= scale;
  return sym_of(std::move(r));
}

double rfr(const SymF& f, double x) {  // SymmetricFilter.realFrequencyResponse
  std::size_t stop = f.a.size();
  std::size_t k = 0;
  double u = f.a[k++];
  if (k >= stop) return u;
  double t0 = 1.0;
  double cs = std::cos(x);
  double t1 = cs;
  for (;;) {
    u = u + 2.0 * t1 * f.a[k++];
    if (k >= stop) break;
    double tn = 2.0 * cs * t1 - t0;
    t0 = t1;
    t1 = tn;
  }
  return u;
}

// Spectrum$fr.evaluate: f, f', f'' via the angle recurrences.
void fr_eval(const SymF& f, double x, double& fv, double& dfv, double& d2fv) {
  std::size_t n = f.a.size();
  fv = f.a[0];
  dfv = 0.0;
  d2fv = 0.0;
  if (n <= 1) return;
  double cs = std::cos(x), sn = std::sin(x);
  double ck = cs, sk = sn;
  for (std::size_t k = 1; k < n; ++k) {
    double ak = f.a[k];
    double tc = 2.0 * ck * ak;
    double ts = 2.0 * sk * ak;
    double cn = ck * cs - sk * sn;
    double sx = sk * cs + ck * sn;
    ck = cn;
    sk = sx;
    fv += tc;
    dfv -= static_cast<double>(k) * ts;
    d2fv -= static_cast<double>(static_cast<int>(k) * static_cast<int>(k)) * tc;
  }
}

// Spectrum$dfr.evaluate (the k^d-weighted one-sided sum).
double dfr_eval(const SymF& f, int d, double x) {
  std::size_t n = f.a.size();
  double s = 0.0;
  for (std::size_t k = 1; k < n; ++k) {
    double t = static_cast<double>(k);
    for (int i = 1; i < d; ++i) t *= static_cast<double>(k);
    double trig = (d % 2 == 0) ? std::cos(x * static_cast<double>(k))
                               : std::sin(x * static_cast<double>(k));
    t = t * trig * f.a[k];
    s += t;
  }
  return s;
}

struct Spectrum {
  SymF num, den;
};

double spectrum_value(const Spectrum& s, double x) {  // Spectrum.value
  double den = rfr(s.den, x);
  double num = rfr(s.num, x);
  if (gt_l(std::fabs(den), 1e-7)) return num / den;
  if (ge_g(std::fabs(num), 1e-7)) return kNan;
  for (int k = 1; k <= 10; ++k) {
    double dd = dfr_eval(s.den, k, x);
    double nn = dfr_eval(s.num, k, x);
    if (gt_l(std::fabs(dd), 1e-7)) return nn / dd;
    if (!ge_l(std::fabs(nn), 1e-7)) break;
  }
  return kNan;
}

// ---------------------------------------------------------------------------
// SymmetricFrequencyResponse (minimizeByRoots only). m_p =
// Polynomial.of(D2SFR(coefficients)); the transform rows are the Chebyshev T_k
// coefficients (exact integers; ltransform's long table holds the same values).
// ---------------------------------------------------------------------------
struct SFR {
  Poly p;
};

SFR sfr_of(const SymF& f) {
  std::size_t n = f.a.size();
  vd b = f.a;
  for (std::size_t k = 1; k < n; ++k) b[k] *= 2.0;  // D2SFR doubling
  if (n > 2) {
    std::vector<vd> t(n);
    t[0] = vd(n, 0.0);
    t[0][0] = 1.0;
    if (n > 1) {
      t[1] = vd(n, 0.0);
      t[1][1] = 1.0;
      for (std::size_t k = 2; k < n; ++k) {
        t[k] = vd(n, 0.0);
        for (std::size_t j = 1; j <= k; ++j) t[k][j] += 2.0 * t[k - 1][j - 1];
        for (std::size_t j = 0; j + 2 <= k; ++j) t[k][j] -= t[k - 2][j];
      }
    }
    vd r(n, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
      if (b[k] == 0.0) continue;  // UpperTriangularMatrix.Ux skip
      for (std::size_t j = k; j < n; ++j) r[j] += t[k][j] * b[k];
    }
    b = std::move(r);
  }
  return SFR{p_of(std::move(b))};
}

double sfr_integral(const SFR& s) {  // getIntegral
  double v = s.p.c[0];
  double f = 1.0;
  for (int i = 2; i <= s.p.degree(); i += 2) {
    f *= static_cast<double>(i - 1);
    f /= static_cast<double>(i);
    v += s.p.c[static_cast<std::size_t>(i)] * f;
  }
  return v;
}

// ---------------------------------------------------------------------------
// Spectrum$Minimizer (evaluate / minimizeByRoots / minimizeByGrid + the
// bracketed Newton SpectrumFunction).
// ---------------------------------------------------------------------------
double minimizer_evaluate(const Poly& P, const Poly& Q, double x, int depth) {
  if (depth > 64) refuse("minimizer deflation depth exceeded");
  if (lt_g(std::fabs(x), 1e-12)) x = 0.0;
  double p = p_value_at(P, x);
  double q = p_value_at(Q, x);
  if (gt_l(std::fabs(q), 1e-12)) return p / q;
  if (gt_l(std::fabs(p), 1e-12)) return kNan;
  if (x != 0.0) {
    Poly lin = p_of(vd{1.0, -x});  // Polynomial.of({1, -x})
    return minimizer_evaluate(p_quotient(P, lin), p_quotient(Q, lin), x, depth + 1);
  }
  int n = std::min(P.degree(), Q.degree());
  for (int k = 1; k <= n; ++k) {
    double a = P.c[static_cast<std::size_t>(k)];
    double b = Q.c[static_cast<std::size_t>(k)];
    bool ta = gt_l(std::fabs(a), 1e-7);
    bool tb = gt_l(std::fabs(b), 1e-7);
    if (ta && tb) return a / b;
    if (ta) return kNan;
    if (tb) return 0.0;
  }
  return kNan;
}

struct MinResult {
  double min = 0.0;
  double x = 0.0;
};

MinResult minimize_by_roots(const Spectrum& s) {
  SFR sn = sfr_of(s.num), sd = sfr_of(s.den);
  double iden = sfr_integral(sd);
  Poly P = p_divide_d(sn.p, iden);
  Poly Q = p_divide_d(sd.p, iden);
  MinResult r;
  if (P.is_zero()) return r;
  if (P.degree() == 0 && Q.degree() == 0) {
    r.min = P.c[0] / Q.c[0];
    return r;
  }
  r.x = 0.0;
  r.min = kMax;
  double v = minimizer_evaluate(P, Q, 0.0, 0);
  if (!is_nan(v)) {
    r.min = v;
    r.x = kPi / 2.0;
  }
  v = minimizer_evaluate(P, Q, 1.0, 0);
  if (!is_nan(v) && lt_g(v, r.min)) {
    r.min = v;
    r.x = 0.0;
  }
  v = minimizer_evaluate(P, Q, -1.0, 0);
  if (!is_nan(v) && lt_g(v, r.min)) {
    r.min = v;
    r.x = kPi;
  }
  Poly w = (Q.degree() > 0) ? p_minus_p(p_times(p_derivate(P), Q), p_times(P, p_derivate(Q)))
                            : p_derivate(P);
  for (const cplx& z : p_roots(w, "minimizer numerator")) {
    if (ge_g(std::fabs(z.im), 1e-7)) continue;
    double re = z.re;
    if (!(gt_l(re, -1.0)) || !(lt_g(re, 1.0))) continue;
    double e = minimizer_evaluate(P, Q, re, 0);
    if (!is_nan(e) && lt_g(e, r.min)) {
      r.min = e;
      r.x = std::acos(re);
    }
  }
  return r;
}

struct FnInstance {
  double pt = 0.0, f = 0.0, df = 0.0, d2f = 0.0;
};

FnInstance fn_instance(const Spectrum& s, double pt) {  // SpectrumFunctionInstance
  FnInstance out;
  out.pt = pt;
  double nf, ndf, nd2f, df, ddf, dd2f;
  fr_eval(s.num, pt, nf, ndf, nd2f);
  fr_eval(s.den, pt, df, ddf, dd2f);
  out.f = nf / df;
  out.df = (ndf * df - nf * ddf) / (df * df);
  out.d2f = ((nd2f * df - nf * dd2f) * df - 2.0 * (ndf * df - nf * ddf) * ddf) / (df * df * df);
  return out;
}

// SpectrumFunction.min(double): false means the JVM returned null.
bool fn_min(const Spectrum& s, double a, double b, double x0, FnInstance& out) {
  double den = rfr(s.den, x0);
  double x = x0;
  if (lt_g(std::fabs(den), 1e-7)) {
    for (;;) {
      x += 1e-7;
      den = rfr(s.den, x);
      if (gt_g(x, b)) break;
      if (!lt_g(std::fabs(den), 1e-7)) break;
    }
    if (!(x <= b)) return false;
  }
  FnInstance inst = fn_instance(s, x);
  double fmin = inst.f;
  double xn = x, xprev = x;
  int iter = 0;
  for (;;) {
    xprev = xn;
    xn = xn - inst.df / inst.d2f;
    if (is_nan(xn)) break;
    if (lt_g(xn, a))
      xn = a;
    else if (gt_l(xn, b))
      xn = b;
    FnInstance cand = fn_instance(s, xn);
    if (lt_g(cand.f, fmin)) {
      inst = cand;
      fmin = cand.f;
    }
    ++iter;
    if (iter >= 200) break;
    if (!(gt_l(std::fabs(xn - xprev), 1e-12))) break;
  }
  out = inst;
  return true;
}

MinResult minimize_by_grid(const Spectrum& s) {
  MinResult r;
  if (s.num.length() == 1 && s.den.length() == 1) {
    r.min = spectrum_value(s, 0.0);
    return r;
  }
  r.x = 0.0;
  r.min = kMax;
  double v = spectrum_value(s, 0.0);
  if (!is_nan(v)) {
    r.min = v;
    r.x = 0.0;
  }
  v = spectrum_value(s, kPi);
  if (!is_nan(v) && lt_g(v, r.min)) {
    r.min = v;
    r.x = kPi;
  }
  int k = s.num.upper_bound() + s.den.upper_bound() - 1;
  double step = kPi / static_cast<double>(k);
  double h = step / 2.0;
  for (int i = 0; i < k; ++i) {
    double x1 = h + step;
    double xl = h;
    double den = rfr(s.den, xl);
    while (!gt_g(den, 0.0) && lt_g(xl, x1)) {
      xl += step / 7.0;
      den = rfr(s.den, xl);
    }
    bool ok = lt_g(xl, x1);
    double xr = x1;
    if (ok) {
      den = rfr(s.den, x1);
      while (!gt_g(den, 0.0) && gt_g(xr, xl)) {
        xr -= step / 7.0;
        den = rfr(s.den, xr);
      }
      ok = gt_g(xr, xl);
    }
    if (ok) {
      FnInstance inst;
      if (fn_min(s, xl, xr, (xl + xr) / 2.0, inst)) {
        if (lt_g(inst.f, r.min)) {
          r.min = inst.f;
          r.x = inst.pt;
        }
      }
    }
    h += step;
  }
  return r;
}

MinResult minimize_spectrum(const Spectrum& s) {
  if (s.den.length() <= 6) return minimize_by_roots(s);
  return minimize_by_grid(s);
}

// ---------------------------------------------------------------------------
// IRootSelector implementations (toolkit ucarima).
// ---------------------------------------------------------------------------
struct Selector {
  Poly selected, not_selected;
  bool has_selected = true, has_not_selected = true;
  virtual ~Selector() = default;
  virtual bool accept(const cplx& z) const = 0;
  // AbstractRootSelector.select(Polynomial)
  bool select(const Poly& p, const char* what) {
    if (p.degree() == 0) {
      has_selected = false;
      not_selected = p;
      has_not_selected = true;
      return false;
    }
    std::vector<cplx> roots = p_roots(p, what);
    std::vector<cplx> sel, ns;
    for (const cplx& z : roots) (accept(z) ? sel : ns).push_back(z);
    if (sel.empty()) {
      has_selected = false;
      not_selected = p;
      has_not_selected = true;
      return false;
    }
    if (ns.empty()) {
      has_not_selected = false;
      selected = p;
      has_selected = true;
      return true;
    }
    leja_order(sel);
    leja_order(ns);
    Poly s = p_from_roots(sel);
    selected = p_times_d(s, p.get(0) / s.get(0));
    has_selected = true;
    Poly n = p_from_roots(ns);
    not_selected = p_divide_d(n, n.get(0));
    has_not_selected = true;
    return true;
  }
  virtual bool select_unit_roots(const Poly& p) = 0;
};

// TrendCycleSelector: |1/z| bound + low-frequency window on |arg(1/z)|;
// unit roots: greedily extract the (1 - L) factors.
struct TrendCycleSelector : Selector {
  double bound, lfreq;
  explicit TrendCycleSelector(double b) : bound(b), lfreq(kPi / 12.0 - 1e-6) {}
  void set_default_low_freq_threshold(int period) {
    lfreq = kPi / static_cast<double>(period) - 1e-6;
  }
  bool accept(const cplx& z) const override {
    cplx zi = c_inv(z);
    if (z.im == 0.0) return !lt_l(zi.re, bound);
    if (lt_l(c_abs(zi), bound)) return false;
    return !gt_g(std::fabs(c_arg(zi)), lfreq);
  }
  bool select_unit_roots(const Poly& p) override {
    selected = p_one();
    has_selected = true;
    not_selected = p;
    has_not_selected = true;
    Poly d(vd{1.0, -1.0});  // UnitRoots.D1
    for (;;) {
      Division div = p_divide(not_selected, d);
      if (!div.exact()) break;
      selected = p_times(selected, d);
      not_selected = div.quotient;
      if (p.degree() < d.degree()) break;
    }
    return selected.degree() > 0;
  }
};

// SeasonalSelector: roots near 2 pi i / frequency; unit roots: the S(s,1) factors.
struct SeasonalSelector : Selector {
  double k = 0.5, epsphi = 2.0;
  int frequency;
  SeasonalSelector(int freq, double eps) : epsphi(eps), frequency(freq) {}
  void set_k(double v) { k = v; }
  bool accept(const cplx& z) const override {
    if (lt_g(std::fabs(z.im), 1e-6)) return lt_g(1.0 / z.re, -k);
    double w = 6.283185307179586 / static_cast<double>(frequency);
    double a = std::fabs(c_arg(z));
    double tol = epsphi / 180.0 * kPi;
    for (int i = 1; i <= frequency / 2; ++i)
      if (!gt_g(std::fabs(w * static_cast<double>(i) - a), tol)) return true;
    return false;
  }
  bool select_unit_roots(const Poly& p) override {
    selected = p_one();
    has_selected = true;
    not_selected = p;
    has_not_selected = true;
    if (frequency == 1) return false;
    Poly s = ur_s(frequency, 1);
    for (;;) {
      Division div = p_divide(not_selected, s);
      if (!div.exact()) break;
      selected = p_times(selected, s);
      not_selected = div.quotient;
      if (p.degree() < s.degree()) break;
    }
    return selected.degree() > 0;
  }
};

// ---------------------------------------------------------------------------
// ArimaModel (structural part only: stationary AR, unit roots, symmetric MA).
// Two shapes mirror the two constructors: the symmetric form (ar, delta, sma)
// and the one-sided form (ar, delta, ma, var) whose symmetric MA is
// convolutionOf(ma, var). The one-sided MA / variance getters of the symmetric
// form need the 24-B spectral factorization and are not modelled.
// ---------------------------------------------------------------------------
struct Arima {
  Poly ar, delta;
  bool symmetric = true;
  SymF sma;   // symmetric form
  vd ma1;     // one-sided form
  double var = 0.0;

  Poly stationary_ar() const { return ar; }
  Poly non_stationary_ar() const { return delta; }
  Poly full_ar() const { return p_times(ar, delta); }  // ArimaModel.getAr()

  SymF symmetric_ma() const {
    if (symmetric) return sma;
    // AbstractArimaModel.symmetricMa() = convolutionOf(ma, var) at the
    // ArimaModel's OWN innovation variance (0.0 for the checkSpecialCases
    // signal, 1.0 for every model built on the captured path).
    return sym_convolution(ma1, var);
  }
  Spectrum spectrum() const {  // AbstractArimaModel.getSpectrum()
    Spectrum s;
    s.num = symmetric_ma();
    s.den = sym_convolution(full_ar().c, 1.0);  // symmetricAr() = convolutionOf(getAr())
    return s;
  }
};

Arima arima_symmetric(Poly ar, Poly delta, SymF sma) {
  Arima a;
  a.ar = std::move(ar);
  a.delta = std::move(delta);
  a.symmetric = true;
  a.sma = std::move(sma);
  return a;
}

Arima arima_onesided(Poly ar, Poly delta, Poly ma, double var) {
  Arima a;
  a.ar = std::move(ar);
  a.delta = std::move(delta);
  a.symmetric = false;
  a.ma1 = std::move(ma.c);
  a.var = var;
  return a;
}

// ---------------------------------------------------------------------------
// LULinearSystemSolver2 (robustSolver). The flat working array is column-major
// with column stride n (tsar storage order): index(i, j) = i + j * n.
// Returns false for the JVM's MatrixException paths.
// ---------------------------------------------------------------------------
bool lu_solve(int n, int l, vd& a, double eps, std::vector<int>& piv) {
  auto at = [n](std::size_t row, std::size_t col) {
    return row + col * static_cast<std::size_t>(n);
  };
  piv.assign(static_cast<std::size_t>(l), -1);
  std::vector<int>& ipiv = piv;
  for (int k = 0; k < n; ++k) {
    double best = kMinValue;
    int ip = -1;
    for (int i = 0, idx = k; i < n; ++i, idx += n) {
      if (ipiv[static_cast<std::size_t>(i)] != -1) continue;
      double v = a[static_cast<std::size_t>(idx)];
      double av = std::fabs(v);
      if (gt_l(av, best)) {  // dcmpl + ifle: a NaN candidate is not taken
        ip = i;
        best = av;
      }
    }
    if (ip < 0) return false;  // MatrixException()
    ipiv[static_cast<std::size_t>(ip)] = k;
    double piv = a[at(static_cast<std::size_t>(k), static_cast<std::size_t>(ip))];
    if (!ge_g(std::fabs(piv), eps)) return false;  // MatrixException("m_err_sing")
    double d = 1.0 / piv;
    for (int j = 0; j < n; ++j) {
      if (j == k) continue;
      double v = a[at(static_cast<std::size_t>(j), static_cast<std::size_t>(ip))];
      if (v == 0.0) continue;
      double m = d * v;
      for (int c = 0, ic = k, jc = j; c < l; ++c, ic += n, jc += n) {
        if (ipiv[static_cast<std::size_t>(c)] != -1) continue;
        a[static_cast<std::size_t>(jc)] -= m * a[static_cast<std::size_t>(ic)];
      }
    }
    for (int c = 0, ic = k; c < l; ++c, ic += n) {
      if (ipiv[static_cast<std::size_t>(c)] != -1) continue;
      a[static_cast<std::size_t>(ic)] *= d;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// RootDecomposer.calc().
// ---------------------------------------------------------------------------
struct DecomposeFail {
  bool threw = false;
};

// One RootDecomposer pass. `fail` is set when the JVM would throw (singular LU
// / out-of-range assembly), which DefaultModelDecomposer turns into null.
bool root_decompose(const Arima& model, Selector& sel, Arima& signal, Arima& noise,
                    bool& fail) {
  fail = false;
  // splitRoots()
  Poly m_sar = p_one(), m_nar = p_one(), m_sur = p_one(), m_nur = p_one();
  sel.select(model.stationary_ar(), "stationary AR");
  if (sel.has_selected) m_sar = sel.selected;
  if (sel.has_not_selected) m_nar = sel.not_selected;
  sel.select_unit_roots(model.non_stationary_ar());
  if (sel.has_selected) m_sur = sel.selected;
  if (sel.has_not_selected) m_nur = sel.not_selected;

  SymF m_sma = model.symmetric_ma();
  SymF m_sfds = sym_convolution(p_times(m_sur, m_sar).c, 1.0);
  SymF m_sfdn = sym_convolution(p_times(m_nur, m_nar).c, 1.0);

  if (m_sfds.length() == 1) {  // checkSpecialCases(): the whole MA goes to the noise
    signal = arima_onesided(p_one(), m_sur, p_one(), 0.0);
    noise = arima_symmetric(m_nar, m_nur, m_sma);
    return true;
  }

  vd a = m_sma.a;
  vd b = m_sfds.a;
  vd c = m_sfdn.a;  // m_sfdn is never null in this transcription

  int na = static_cast<int>(a.size()) - 1;
  int nb = static_cast<int>(b.size()) + static_cast<int>(c.size()) - 2;
  int ub = static_cast<int>(b.size()) - 1;
  int uc = static_cast<int>(c.size()) - 1;
  int n1 = ub > 0 ? ub - 1 : 0;
  int n2 = uc > 0 ? uc - 1 : 0;
  if (na >= nb) n2 = na - ub;
  int d1 = n1 + 1;
  int d2 = n2 + 1;
  int dim = d1 + d2;
  if (dim <= 0) {
    fail = true;
    return false;
  }
  std::size_t N = static_cast<std::size_t>(dim);
  int l = dim + 1;
  vd mat(N * static_cast<std::size_t>(l), 0.0);
  auto set = [&](int row, int col, double v) {
    if (row < 0 || col < 0 || row >= dim || col >= dim) {
      fail = true;
      return;
    }
    mat[static_cast<std::size_t>(row) + static_cast<std::size_t>(col) * N] = v;
  };
  auto get = [&](int row, int col) -> double {
    if (row < 0 || col < 0 || row >= dim || col >= dim) {
      fail = true;
      return 0.0;
    }
    return mat[static_cast<std::size_t>(row) + static_cast<std::size_t>(col) * N];
  };
  a[0] /= 2.0;
  b[0] /= 2.0;
  c[0] /= 2.0;
  for (int i = 0; i <= n1 && !fail; ++i)
    for (int j = 0; j <= uc && !fail; ++j) set(i + j, i, c[static_cast<std::size_t>(j)]);
  for (int i = 0; i <= n2 && !fail; ++i)
    for (int j = 0; j <= ub && !fail; ++j) set(i + j, d1 + i, b[static_cast<std::size_t>(j)]);
  for (int i = 0; i <= n1 && !fail; ++i)
    for (int j = 0; j <= uc && !fail; ++j) {
      int k = i - j;
      if (k < 0) k = -k;
      set(k, i, get(k, i) + c[static_cast<std::size_t>(j)]);
    }
  for (int i = 0; i <= n2 && !fail; ++i)
    for (int j = 0; j <= ub && !fail; ++j) {
      int k = i - j;
      if (k < 0) k = -k;
      set(k, d1 + i, get(k, d1 + i) + b[static_cast<std::size_t>(j)]);
    }
  vd rhs(N, 0.0);
  for (int i = 0; i <= na; ++i) {
    if (i >= dim || static_cast<std::size_t>(i) >= a.size()) {
      fail = true;
      break;
    }
    rhs[static_cast<std::size_t>(i)] = a[static_cast<std::size_t>(i)];
  }
  if (fail) return false;
  for (std::size_t i = 0; i < N; ++i)
    mat[i + N * N] = rhs[i];  // X copied after the n*n block
  std::vector<int> ipiv;
  if (!lu_solve(dim, l, mat, kLuEps, ipiv)) return false;
  // solve() read-back: X(i, c) = a[ipiv[i] + (n + c) * n].
  vd sol(N, 0.0);
  for (int i = 0; i < dim; ++i)
    sol[static_cast<std::size_t>(i)] =
        mat[static_cast<std::size_t>(ipiv[static_cast<std::size_t>(i)]) + N * N];
  vd x1(static_cast<std::size_t>(d1), 0.0), x2(static_cast<std::size_t>(d2), 0.0);
  for (int i = 0; i < d1; ++i) x1[static_cast<std::size_t>(i)] = sol[static_cast<std::size_t>(i)];
  for (int i = 0; i < d2; ++i) x2[static_cast<std::size_t>(i)] = sol[static_cast<std::size_t>(d1 + i)];
  x1[0] *= 2.0;  // RootDecomposer: get(0) doubled back (a[0] was halved)
  x2[0] *= 2.0;
  signal = arima_symmetric(m_sar, m_sur, sym_of(std::move(x1)));
  noise = arima_symmetric(m_nar, m_nur, sym_of(std::move(x2)));
  return true;
}

// ---------------------------------------------------------------------------
// ModelDecomposer.decompose(IArimaModel): comps[i] = stage i's signal, last
// slot = the remaining noise; an exception anywhere -> null (fail).
// ---------------------------------------------------------------------------
bool model_decompose(const Arima& model, std::vector<std::unique_ptr<Selector>>& sels,
                     std::vector<Arima>& comps, bool& fail) {
  fail = false;
  comps.clear();
  Arima cur = model;
  for (std::size_t i = 0; i < sels.size(); ++i) {
    Arima sig, noi;
    if (!root_decompose(cur, *sels[i], sig, noi, fail)) return false;
    comps.push_back(sig);
    cur = noi;
  }
  comps.push_back(cur);
  return true;
}

// ---------------------------------------------------------------------------
// UcarimaModel.setVarianceMax(-1, noisy): the decomposability decision. Only
// the minima sum and its sign are mirrored; the components' minus()/plus()
// need the 24-B spectral factorization, so the allocated values are not built.
// ---------------------------------------------------------------------------
struct VarianceMax {
  bool ok = false;    // a UcarimaModel came back (vsum >= 0, or noisy)
  double sum = 0.0;   // the minima sum (setVarianceMax's "variance floor")
  int count = 0;      // components count of the resulting UcarimaModel
};

VarianceMax variance_max(const std::vector<Arima>& raw, bool noisy, bool chain_threw) {
  VarianceMax vm;
  if (chain_threw) return vm;  // decompose -> null -> setVarianceMax(null) -> null
  vm.count = static_cast<int>(raw.size()) + 1;  // idx < 0 appends ArimaModel.NULL
  double sum = 0.0;
  for (const Arima& c : raw) {
    MinResult m = minimize_spectrum(c.spectrum());
    if (m.min == 0.0) continue;  // zero-minimum components are skipped
    sum += m.min;
  }
  vm.sum = sum;
  vm.ok = noisy || !lt_g(sum, 0.0);
  return vm;
}

// ---------------------------------------------------------------------------
// SarimaModel + SarimaModel.Builder (parameter vectors define the orders).
// ---------------------------------------------------------------------------
struct Sarima {
  int period = 12, d = 0, bd = 0;
  vd phi, bphi, theta, btheta;

  int p() const { return static_cast<int>(phi.size()); }
  int q() const { return static_cast<int>(theta.size()); }
  int bp() const { return static_cast<int>(bphi.size()); }
  int bq() const { return static_cast<int>(btheta.size()); }

  // SarimaModel.getRegularAR/getRegularMA: valueOf(1, params) (no trim: the
  // constructor stores the raw arrays; ofInternal trims only trailing 1e-9).
  Poly regular_ar() const { return p_value_of(1.0, phi); }
  Poly regular_ma() const { return p_value_of(1.0, theta); }
  // SarimaModel.seasonalAR: sparse at stride `period`, signs negated;
  // seasonalMA: sparse with the btheta signs kept.
  Poly seasonal_ar() const {
    if (bphi.empty()) return p_one();
    if (bphi.size() == 1) return p_factor(-bphi[0], period);
    vd a(static_cast<std::size_t>(bphi.size()) * static_cast<std::size_t>(period) + 1, 0.0);
    a[0] = 1.0;
    for (std::size_t i = 0; i < bphi.size(); ++i)
      a[(i + 1) * static_cast<std::size_t>(period)] = -bphi[i];
    return p_raw(std::move(a));
  }
  Poly seasonal_ma() const {
    if (btheta.empty()) return p_one();
    if (btheta.size() == 1) return p_factor(-btheta[0], period);
    vd a(static_cast<std::size_t>(btheta.size()) * static_cast<std::size_t>(period) + 1, 0.0);
    a[0] = 1.0;
    for (std::size_t i = 0; i < btheta.size(); ++i)
      a[(i + 1) * static_cast<std::size_t>(period)] = btheta[i];
    return p_raw(std::move(a));
  }
  Poly stationary_ar() const { return p_times(regular_ar(), seasonal_ar()); }
  Poly non_stationary_ar() const { return differencing_filter(period, d, bd); }
  Poly ma() const { return p_times(regular_ma(), seasonal_ma()); }

  // ArimaModel.of(SarimaModel): the one-sided shape. The SarimaModel's own
  // innovation variance is the SarimaSpec default (always 1.0 on the captured
  // path): SeatsModelSpec.innovationVariance lives on the SeatsModel and
  // never reaches the decomposition (golden "model var 1.0" under a request
  // var of 2.0, variance-free r sfd facts).
  Arima to_arima() const {
    return arima_onesided(stationary_ar(), non_stationary_ar(), ma(), 1.0);
  }
};

// SarimaModel.Builder.adjust(double[]): drop trailing |v| < 1e-6.
void builder_adjust(vd& v) {
  std::size_t n = v.size();
  while (n > 0 && !(ge_g(std::fabs(v[n - 1]), 1e-6))) --n;
  v.resize(n);
}

// ---------------------------------------------------------------------------
// jdplus.tramoseats.base.core.seats.DefaultModelValidator.
// ---------------------------------------------------------------------------
struct Validator {
  double xl = 0.95, eps = 1e-4;
  Sarima model;
  bool changed = false;

  // canSimplify: non-empty vector with |last| < 1e-6.
  bool can_simplify(const vd& v) const {
    return !v.empty() && lt_g(std::fabs(v[v.size() - 1]), 1e-6);
  }

  // simplifyModel: ONE pass of toBuilder().adjustOrders(true).build(), which
  // trims trailing near-zero parameters from all four vectors at once.
  bool simplify_model() {
    if (can_simplify(model.phi) || can_simplify(model.bphi) || can_simplify(model.theta) ||
        can_simplify(model.btheta)) {
      builder_adjust(model.phi);
      builder_adjust(model.bphi);
      builder_adjust(model.theta);
      builder_adjust(model.btheta);
      return true;
    }
    return false;
  }

  // stabilizeMA(cutOff, theta): the XL projection (unit-degree clamp branch +
  // root inversion re-scaled to |z| = 1/cutOff).
  bool stabilize_ma(double cut, vd& th) {
    if (th.size() == 1) {
      double v = th[0];
      if (lt_g(v, -cut)) {
        th[0] = -cut;
        return true;
      }
      if (gt_l(v, cut)) {
        th[0] = cut;
        return true;
      }
      return false;
    }
    if (th.empty()) return false;
    Poly p = p_value_of(1.0, th);
    std::vector<cplx> roots = p_roots(p, "stabilizeMA");
    bool any = false;
    for (std::size_t i = 0; i < roots.size(); ++i) {
      double m = 1.0 / c_abs(roots[i]);
      if (gt_g(m, cut)) {
        any = true;
        roots[i] = c_scale(roots[i], m / cut);
      }
    }
    if (!any) return false;
    Poly q = p_from_roots(roots);
    q = p_divide_d(q, q.get(0));
    for (std::size_t i = 0; i < th.size(); ++i) th[i] = q.get(i + 1);
    return true;
  }

  // fixMaUnitRoots(): only reached when xl >= 1. The deg-1 quirk (an outside
  // value is sent to -1, never to +1) is a faithful transcription, not a bug.
  bool fix_ma_unit_roots() {
    bool any = false;
    double lim = 1.0 - eps;
    vd& th = model.theta;
    vd& bth = model.btheta;
    if (!bth.empty()) {
      double pw = std::pow(lim, static_cast<double>(model.period));
      double v = bth[0];
      if (lt_g(v, -pw)) {
        bth[0] = -1.0;
        any = true;
      } else if (gt_l(v, pw)) {
        bth[0] = 1.0;
        any = true;
      }
    }
    if (th.size() == 1) {
      double v = th[0];
      if (lt_g(v, -lim)) {
        th[0] = -1.0;
        any = true;
      } else if (gt_l(v, lim)) {
        th[0] = -1.0;
        any = true;
      }
    } else if (th.size() > 1) {
      Poly p = p_value_of(1.0, th);
      std::vector<cplx> roots = p_roots(p, "fixMaUnitRoots");
      bool sub = false;
      for (std::size_t i = 0; i < roots.size(); ++i) {
        double m = c_abs(roots[i]);
        if (lt_g(m, lim)) {
          sub = true;
          roots[i] = c_div_d(roots[i], m);
        }
      }
      if (sub) {
        Poly q = p_from_roots(roots);
        for (std::size_t i = 0; i < th.size(); ++i) th[i] = q.get(i + 1);
        any = true;
      }
    }
    return any;
  }

  bool change_ma() {
    if (!ge_g(xl, 1.0)) {
      bool any = false;
      if (stabilize_ma(xl, model.theta)) any = true;
      if (stabilize_ma(xl, model.btheta)) any = true;
      return any;
    }
    return fix_ma_unit_roots();
  }

  // validate(): true when the model was left untouched. changeAR(false) is a
  // no-op in 3.9.0 (its body is commented out in the released class).
  bool validate(const Sarima& m) {
    model = m;
    bool a = simplify_model();
    bool b = change_ma();
    bool c = false;
    changed = a || b || c;
    return !changed;
  }
};

// ---------------------------------------------------------------------------
// DefaultModelDecomposer.decompose wiring (the two selectors exactly as
// SeatsToolkit.of wires them from the DecompositionSpec knobs).
// ---------------------------------------------------------------------------
struct EngineFacts {
  bool chain_threw = false;  // ModelDecomposer.decompose threw -> um null
  std::vector<Arima> raw;    // the raw (pre-allocation) components
  VarianceMax vm;
};

EngineFacts decompose(const Sarima& m1, double trend_boundary,
                      double seas_tolerance, double seas_boundary, double seas_boundary_pi,
                      bool noisy) {
  EngineFacts ef;
  Arima model = m1.to_arima();
  std::vector<std::unique_ptr<Selector>> sels;
  std::unique_ptr<TrendCycleSelector> tcs(new TrendCycleSelector(trend_boundary));
  tcs->set_default_low_freq_threshold(m1.period);
  std::unique_ptr<SeasonalSelector> ss(new SeasonalSelector(m1.period, seas_tolerance));
  ss->set_k(m1.bd > 0 || m1.bp() > 0 ? seas_boundary : seas_boundary_pi);
  sels.push_back(std::move(tcs));
  sels.push_back(std::move(ss));
  bool fail = false;
  if (!model_decompose(model, sels, ef.raw, fail)) {
    ef.chain_threw = true;
    ef.raw.clear();
    return ef;
  }
  ef.vm = variance_max(ef.raw, noisy, false);
  return ef;
}

void push_event(tramo_seats_model_result& r, int depth, const std::string& kind,
                const std::string& msg, const std::string& detail) {
  tramo_seats_model_result::event e;
  e.seq = static_cast<int>(r.events.size()) + 1;
  e.kind = kind;
  e.depth = depth;
  e.msg = msg;
  e.detail = detail;
  r.events.push_back(e);
}

}  // namespace

tramo_seats_model_result validate_tramo_seats_model(
    const tramo_seats_model_request& request) {
  if (request.period != 12 && request.period != 4)
    refuse("frequency " + std::to_string(request.period) +
           " outside the captured corpus shapes {4, 12}");
  if (request.frequency != 12 && request.frequency != 4)
    refuse("series frequency " + std::to_string(request.frequency) +
           " outside the captured corpus shapes {4, 12}");
  if (!request.bphi.empty())
    refuse("stationary seasonal AR (bphi) needs the degree > 2 roots cascade");
  if (request.phi.size() > 2)
    refuse("stationary AR degree > 2 needs the degree > 2 roots cascade");
  if (request.theta.size() > 2 || request.btheta.size() > 2)
    refuse("MA degree > 2 outside the measured corpus band");
  if (request.d < 0 || request.bd < 0) refuse("negative differencing order");
  for (const vd* v : {&request.phi, &request.bphi, &request.theta, &request.btheta})
    for (double x : *v)
      if (is_nan(x) || std::fabs(x) == std::numeric_limits<double>::infinity())
        refuse("non-finite model parameter");
  if (is_nan(request.xl) || is_nan(request.epsphi) || is_nan(request.rmod) ||
      is_nan(request.smod) || is_nan(request.smod_pi) ||
      is_nan(request.innovation_variance))
    refuse("non-finite setting");

  tramo_seats_model_result out;
  out.xl = request.xl;

  // ---- SeatsKernel.validate seam (DefaultModelValidator) ------------------
  push_event(out, 0, "push", "seats", "-");
  push_event(out, 1, "push", "validation", "-");
  Sarima m0;
  m0.period = request.period;
  m0.d = request.d;
  m0.bd = request.bd;
  m0.phi = request.phi;
  m0.bphi = request.bphi;
  m0.theta = request.theta;
  m0.btheta = request.btheta;
  Validator val;
  val.xl = request.xl;
  val.eps = 1e-4;  // DefaultModelValidator.Builder DEF_EPS (the toolkit sets xl only)
  bool untouched = val.validate(m0);
  out.parameters_cut_off = !untouched;
  if (!untouched) push_event(out, 2, "remark", "arima parameters cut off", "-");
  push_event(out, 1, "pop", "-", "-");
  const Sarima& m1 = val.model;
  out.orders_p = m1.p();
  out.orders_d = m1.d;
  out.orders_q = m1.q();
  out.orders_bp = m1.bp();
  out.orders_bd = m1.bd;
  out.orders_bq = m1.bq();
  out.phi = m1.phi;
  out.bphi = m1.bphi;
  out.theta = m1.theta;
  out.btheta = m1.btheta;
  out.innovation_variance = request.innovation_variance;
  out.period = m1.period;

  // ---- SeatsKernel.decomposeModel seam -------------------------------------
  const bool noisy = request.mode == seats_approx_mode::noisy;
  const bool approximator_present = request.mode != seats_approx_mode::none;
  push_event(out, 1, "push", "decomposition", "-");
  EngineFacts ef;
  try {
    ef = decompose(m1, request.rmod, request.epsphi,
                   request.smod, request.smod_pi, noisy);
  } catch (const std::invalid_argument&) {
    // Envelope violations propagate to the caller (never swallowed): the live
    // path would have used machinery this unit does not model.
    throw;
  }
  out.decomposition_threw = ef.chain_threw;
  out.variance_floor = ef.vm.sum;
  out.variance_max_ok = ef.vm.ok;
  for (const Arima& c : ef.raw) {
    seats_raw_component rc;
    rc.sar = c.stationary_ar().c;
    rc.ur = c.non_stationary_ar().c;
    rc.sfd = c.symmetric_ma().a;
    out.raw_components.push_back(rc);
  }

  // The kernel loop's first iteration decides everything this unit models.
  const bool um_null = !ef.vm.ok;
  if (um_null) push_event(out, 2, "warning", "non decomposable model", "-");
  if (um_null && !approximator_present) {
    push_event(out, 2, "error", "canonical decomposition failed", "-");
    push_event(out, 1, "pop", "-", "-");
    push_event(out, 0, "pop", "-", "-");
    out.seats_exception = true;
    return out;
  }
  if (um_null) {
    out.approximation_required = true;  // D6: the kernel calls the approximator here
    return out;
  }
  if (noisy && lt_g(ef.vm.sum, 0.0)) {
    // setVarianceMax replaced the model with the noisy ArimaModel; the kernel
    // compares um.getModel() against the SarimaModel, sees them differ and
    // marks the model changed.
    out.noisy_model_used = true;
    out.model_changed = true;
    push_event(out, 2, "warning", "noisy model used", "-");
  }
  push_event(out, 1, "pop", "-", "-");
  push_event(out, 0, "pop", "-", "-");
  return out;
}

}  // namespace kalos
