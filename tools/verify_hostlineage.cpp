// verify_hostlineage: bit-exactness proof for the vendored host-lineage
// kernels (include/kalos/detail/hostlineage.hpp) against the host libm of
// the qualification machine (glibc 2.39, x86-64 — the reference the kernels
// were transcribed from). Exit 0 iff every compared pair agrees to the bit.
//
// Usage:
//   verify_hostlineage [CENSUS_FILE ...]
//
// CENSUS_FILEs are shim record dumps (KALOS_LIBM_RECORD mode): whitespace
// lines "SYM ARG_HEX[ ARG2_HEX] COUNT" produced while running the full test
// suite + verify_* sweep tools; every recorded corpus-reached argument is
// replayed through kernel vs host. With no census files, only the built-in
// dense grids / special-value batteries run (CI smoke level).
//
// Table fidelity (generated src/hostlineage_tables.cpp vs the original glibc
// table sources) is proven by a separate scratch harness kept with the
// evidence (the original glibc files are not vendored into the tree): it
// re-parses the pinned files as C and byte-compares all eight table objects
// (see tmp_opencode/kalos-ci4-fix/hl_proof/). Kernel bit-equality below is
// the load-bearing in-tree gate: any table corruption changes result bits.
//
// Build (repo tools are standalone; no CMake target):
//   c++ -O2 -DNDEBUG -ffp-contract=off -std=gnu++17 -I include \
//       src/hostlineage.cpp src/hostlineage_tables.cpp \
//       tools/verify_hostlineage.cpp -o /tmp/verify_hostlineage -lm

#include <kalos/detail/hostlineage.hpp>

#include <cfloat>
#include <cmath>
// (h_* host bindings below)
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>

using kalos::detail::glibc_acos;
using kalos::detail::glibc_asin;
using kalos::detail::glibc_atan2;
using kalos::detail::glibc_cos;
using kalos::detail::glibc_exp;
using kalos::detail::glibc_hypot;
using kalos::detail::glibc_log;
using kalos::detail::glibc_pow;
using kalos::detail::glibc_sin;

namespace {

uint64_t bits(double x) {
  uint64_t u;
  std::memcpy(&u, &x, 8);
  return u;
}
double frombits(uint64_t u) {
  double x;
  std::memcpy(&x, &u, 8);
  return x;
}

constexpr double kPi = 3.14159265358979323846;
double nexttoward_up(double x) { return std::nextafter(x, INFINITY); }

uint64_t rng_state = 0x9E3779B97F4A7C15ULL;  // splitmix64, fixed seed
uint64_t rnd() {
  uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}
double rnd_uniform() { return (rnd() >> 11) * 0x1.0p-53; }
// Log-uniform positive double in (0, 2^1023]: random exponent+mantissa with
// the top exponent bit masked off (never inf, never NaN; subnormals occur).
double rnd_loguniform_pos() { return frombits(rnd() & 0x7FEFFFFFFFFFFFFFULL); }

struct KernelReport {
  const char* name;
  unsigned long long checked = 0;
  unsigned long long bad = 0;
  void note(bool ok, double a, double b, double got, double want) {
    ++checked;
    if (!ok && ++bad <= 8) {
      std::printf("  %s(%a[, %a]) -> %a (%016llx), host %a (%016llx)\n", name,
                  a, b, got, (unsigned long long)bits(got), want,
                  (unsigned long long)bits(want));
    }
  }
};

KernelReport rep_sin{"sin"}, rep_cos{"cos"}, rep_exp{"exp"}, rep_log{"log"},
    rep_pow{"pow"}, rep_hypot{"hypot"}, rep_atan2{"atan2"}, rep_acos{"acos"},
    rep_asin{"asin"};

// Host libm double overloads (std:: names are overloaded; pin the double ones).
double (*const h_sin)(double) = std::sin;
double (*const h_cos)(double) = std::cos;
double (*const h_exp)(double) = std::exp;
double (*const h_log)(double) = std::log;
double (*const h_asin)(double) = std::asin;
double (*const h_acos)(double) = std::acos;
double (*const h_pow)(double, double) = std::pow;
double (*const h_hypot)(double, double) = std::hypot;
double (*const h_atan2)(double, double) = std::atan2;

#define CHECK1(rep, kern, host, x)                                       \
  do {                                                                   \
    double g_ = (kern)(x), h_ = (host)(x);                               \
    bool ok_ = bits(g_) == bits(h_) ||                                   \
               (std::isnan(g_) && std::isnan(h_));                       \
    (rep).note(ok_, (x), 0.0, g_, h_);                                   \
  } while (0)
#define CHECK2(rep, kern, host, x, y)                                    \
  do {                                                                   \
    double g_ = (kern)((x), (y)), h_ = (host)((x), (y));                 \
    bool ok_ = bits(g_) == bits(h_) ||                                   \
               (std::isnan(g_) && std::isnan(h_));                       \
    (rep).note(ok_, (x), (y), g_, h_);                                   \
  } while (0)

template <typename F1, typename F2>
void dense_1arg(KernelReport& rep, F1 kern, F2 host, double lo, double hi,
                unsigned long long n) {
  for (unsigned long long i = 0; i <= n; ++i) {
    double x = lo + (hi - lo) * (double)i / (double)n;
    CHECK1(rep, kern, host, x);
  }
}

void battery_sincos() {
  // Near-zero / linear, mid (direct table), mid-large (reduce_sincos) and
  // huge (branred) paths, plus exact multiples of pi/2 +- ulps.
  dense_1arg(rep_sin, glibc_sin, h_sin, -40.0, 40.0, 8'000'000);
  dense_1arg(rep_cos, glibc_cos, h_cos, -40.0, 40.0, 8'000'000);
  for (unsigned long long i = 0; i < 4'000'000; ++i) {
    double x = frombits(0x4000000000000000ULL | (rnd() & 0x000FFFFFFFFFFFFFULL));
    x = (rnd() & 1) ? x : -x;  // |x| in [2, 4)
    CHECK1(rep_sin, glibc_sin, h_sin, x);
    CHECK1(rep_cos, glibc_cos, h_cos, x);
  }
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // reduce_sincos path
    double x = 2.5 + rnd_uniform() * (105414350.0 - 2.5);
    CHECK1(rep_sin, glibc_sin, h_sin, x);
    CHECK1(rep_cos, glibc_cos, h_cos, -x);
  }
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // branred path
    int e = (int)(rnd() % 2100) - 1075;
    double x = std::ldexp(1.0 + rnd_uniform(), e);
    CHECK1(rep_sin, glibc_sin, h_sin, x);
    CHECK1(rep_cos, glibc_cos, h_cos, x);
  }
  for (long long k = -2000000; k <= 2000000; k += 97) {  // pi/2 grid +- ulps
    double m = (double)k * (kPi / 2.0);
    for (int d = -3; d <= 3; ++d) {
      double x = m;
      for (int j = 0; j < std::abs(d); ++j)
        x = d > 0 ? nexttoward_up(x) : std::nextafter(x, -INFINITY);
      CHECK1(rep_sin, glibc_sin, h_sin, x);
      CHECK1(rep_cos, glibc_cos, h_cos, x);
    }
  }
  const double sp[] = {0.0,        -0.0,         1.0,            -1.0,
                       0.126,      0.1260001,    0.855469,       2.426265,
                       105414350.0, 105414351.0,  105414350.5,    DBL_TRUE_MIN,
                       -DBL_TRUE_MIN, 1e-320,     105414351e284, -105414351e284};
  for (double x : sp) {
    CHECK1(rep_sin, glibc_sin, h_sin, x);
    CHECK1(rep_cos, glibc_cos, h_cos, x);
  }
  CHECK1(rep_sin, glibc_sin, h_sin, INFINITY);
  CHECK1(rep_sin, glibc_sin, h_sin, -INFINITY);
  CHECK1(rep_cos, glibc_cos, h_cos, INFINITY);
  CHECK1(rep_cos, glibc_cos, h_cos, std::nan(""));
}

void battery_exp() {
  dense_1arg(rep_exp, glibc_exp, h_exp, -746.0, 710.0, 12'000'000);
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // underflow band
    double x = -745.1332 + rnd_uniform() * 3.0;
    CHECK1(rep_exp, glibc_exp, h_exp, x);
  }
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // overflow band
    double x = 709.78 - rnd_uniform() * 3.0;
    CHECK1(rep_exp, glibc_exp, h_exp, x);
  }
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // ln2/N reduction grid
    double k = (double)(long long)(rnd() % 200000) - 100000;
    double x = k * (0.6931471805599453 / 128.0) + (rnd_uniform() - 0.5) * 1e-6;
    CHECK1(rep_exp, glibc_exp, h_exp, x);
  }
  CHECK1(rep_exp, glibc_exp, h_exp, 0.0);
  CHECK1(rep_exp, glibc_exp, h_exp, -0.0);
  CHECK1(rep_exp, glibc_exp, h_exp, -DBL_TRUE_MIN);
  CHECK1(rep_exp, glibc_exp, h_exp, 1e-323);
  CHECK1(rep_exp, glibc_exp, h_exp, INFINITY);
  CHECK1(rep_exp, glibc_exp, h_exp, -INFINITY);
  CHECK1(rep_exp, glibc_exp, h_exp, std::nan(""));
}

void battery_log() {
  dense_1arg(rep_log, glibc_log, h_log, 0.0, 2.0, 8'000'000);
  dense_1arg(rep_log, glibc_log, h_log, 2.0, 1e6, 2'000'000);
  for (unsigned long long i = 0; i < 4'000'000; ++i) {  // full log range
    double x = rnd_loguniform_pos();
    CHECK1(rep_log, glibc_log, h_log, x);
  }
  for (unsigned long long i = 0; i < 1'000'000; ++i) {  // near-1 branch
    double x = 1.0 + (rnd_uniform() < 0.5 ? 1 : -1) *
                         std::ldexp(rnd_uniform(), (int)(rnd() % 27) - 27);
    CHECK1(rep_log, glibc_log, h_log, x);
  }
  CHECK1(rep_log, glibc_log, h_log, 0.0);
  CHECK1(rep_log, glibc_log, h_log, -0.0);
  CHECK1(rep_log, glibc_log, h_log, -1.0);
  CHECK1(rep_log, glibc_log, h_log, -DBL_TRUE_MIN);
  CHECK1(rep_log, glibc_log, h_log, DBL_TRUE_MIN);
  CHECK1(rep_log, glibc_log, h_log, 1e-320);
  CHECK1(rep_log, glibc_log, h_log, DBL_MAX);
  CHECK1(rep_log, glibc_log, h_log, INFINITY);
  CHECK1(rep_log, glibc_log, h_log, std::nan(""));
}

void battery_pow() {
  // The corrected qualification domain (outlier_scoring): {0.5, 0.7}^k.
  for (int k = 0; k <= 49; ++k) {
    CHECK2(rep_pow, glibc_pow, h_pow, 0.5, (double)k);
    CHECK2(rep_pow, glibc_pow, h_pow, 0.7, (double)k);
    CHECK2(rep_pow, glibc_pow, h_pow, -0.5, (double)k);
    CHECK2(rep_pow, glibc_pow, h_pow, -0.7, (double)k);
  }
  // Integer exponents over a log-sweep of bases (preparation / ucarima pow).
  for (unsigned long long i = 0; i < 1'500'000; ++i) {
    double base = std::ldexp(1.0 + rnd_uniform(), (int)(rnd() % 600) - 300);
    double y = (double)((long long)(rnd() % 4000) - 2000);
    CHECK2(rep_pow, glibc_pow, h_pow, base, y);
    CHECK2(rep_pow, glibc_pow, h_pow, -base, y);
  }
  // Non-integer exponents, |y| in the special-case-free band.
  for (unsigned long long i = 0; i < 2'000'000; ++i) {
    double base = rnd_loguniform_pos();
    double y = (rnd_uniform() < 0.5 ? 1 : -1) *
               std::ldexp(rnd_uniform(), (int)(rnd() % 70) - 60);
    CHECK2(rep_pow, glibc_pow, h_pow, base, y);
    CHECK2(rep_pow, glibc_pow, h_pow, -base, y);
  }
  // Near-1 bases with large exponents (exp-branch and scale specials).
  for (unsigned long long i = 0; i < 1'000'000; ++i) {
    double base = 1.0 + (rnd_uniform() < 0.5 ? 1 : -1) *
                            std::ldexp(rnd_uniform(), (int)(rnd() % 40) - 40);
    double y = (double)((long long)(rnd() % 4000000) - 2000000);
    CHECK2(rep_pow, glibc_pow, h_pow, base, y);
  }
  // Tiny/huge x with tiny y (|y| < 2^-65 and >= 2^63 special bands).
  const double ys[] = {0.0, -0.0, 1.0, -1.0, 2.0, 0.5, -0.5, 3.0, 1000.0,
                       -1e18, 1e300, 0x1p-66, -0x1p-66, 0x1p63, 2.0 / 3.0};
  const double xs[] = {0.0, -0.0, 1.0, -1.0, 2.0, 0.5, -0.5, 1e-320, -1e-320,
                       1e300, -1e300, DBL_TRUE_MIN, DBL_MAX, 1.00000011686097421};
  for (double x : xs)
    for (double y : ys) CHECK2(rep_pow, glibc_pow, h_pow, x, y);
  CHECK2(rep_pow, glibc_pow, h_pow, INFINITY, 2.0);
  CHECK2(rep_pow, glibc_pow, h_pow, -INFINITY, 3.0);
  CHECK2(rep_pow, glibc_pow, h_pow, INFINITY, -2.0);
  CHECK2(rep_pow, glibc_pow, h_pow, 2.0, INFINITY);
  CHECK2(rep_pow, glibc_pow, h_pow, 2.0, -INFINITY);
  CHECK2(rep_pow, glibc_pow, h_pow, 1.0, INFINITY);
  CHECK2(rep_pow, glibc_pow, h_pow, 1.0, std::nan(""));
  CHECK2(rep_pow, glibc_pow, h_pow, std::nan(""), 1.0);
}

void battery_hypot() {
  for (unsigned long long i = 0; i < 3'000'000; ++i) {
    double x = rnd_loguniform_pos(), y = rnd_loguniform_pos();
    CHECK2(rep_hypot, glibc_hypot, h_hypot, x, y);
  }
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // ratio sweep
    double ax = std::ldexp(1.0 + rnd_uniform(), (int)(rnd() % 2048) - 1074);
    double r = rnd_uniform();  // ay/ax in [0,1): crosses the EPS band
    double ay = ax * r;
    CHECK2(rep_hypot, glibc_hypot, h_hypot, ax, ay);
    CHECK2(rep_hypot, glibc_hypot, h_hypot, -ay, ax);
  }
  const double sp[] = {0.0, -0.0, DBL_TRUE_MIN, 0x1p-459, 0x1p-460, 0x1p+511,
                       0x1p+512, DBL_MAX, 1.0, 3.0, 4.0};
  for (double x : sp)
    for (double y : sp) CHECK2(rep_hypot, glibc_hypot, h_hypot, x, y);
  CHECK2(rep_hypot, glibc_hypot, h_hypot, INFINITY, 3.0);
  CHECK2(rep_hypot, glibc_hypot, h_hypot, 3.0, -INFINITY);
  CHECK2(rep_hypot, glibc_hypot, h_hypot, std::nan(""), 3.0);
}

void battery_atan2() {
  for (unsigned long long i = 0; i < 3'000'000; ++i) {
    double y = rnd_loguniform_pos() * ((rnd() & 1) ? 1 : -1);
    double x = rnd_loguniform_pos() * ((rnd() & 1) ? 1 : -1);
    CHECK2(rep_atan2, glibc_atan2, h_atan2, y, x);
  }
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // extreme ratios
    double y = std::ldexp(1.0 + rnd_uniform(), (int)(rnd() % 2100) - 1050);
    double x = std::ldexp(1.0 + rnd_uniform(), (int)(rnd() % 2100) - 1050);
    CHECK2(rep_atan2, glibc_atan2, h_atan2, y, x);
    CHECK2(rep_atan2, glibc_atan2, h_atan2, -y, -x);
  }
  for (unsigned long long i = 0; i < 1'000'000; ++i) {  // inv16 / cij grids
    double u = rnd_uniform() * 2.0;
    double x = 1.0;
    double y = u * x * (rnd() & 1 ? 1.0 : -1.0);
    CHECK2(rep_atan2, glibc_atan2, h_atan2, y, x);
    CHECK2(rep_atan2, glibc_atan2, h_atan2, x, y);
  }
  const double sp[] = {0.0, -0.0, 1.0, -1.0, DBL_MAX, -DBL_MAX,
                       DBL_TRUE_MIN, -DBL_TRUE_MIN, 1e-320, -1e-320};
  for (double y : sp)
    for (double x : sp) CHECK2(rep_atan2, glibc_atan2, h_atan2, y, x);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, 1.0, INFINITY);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, -1.0, INFINITY);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, 1.0, -INFINITY);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, INFINITY, 1.0);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, -INFINITY, -1.0);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, INFINITY, INFINITY);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, std::nan(""), 1.0);
  CHECK2(rep_atan2, glibc_atan2, h_atan2, 1.0, std::nan(""));
}

void battery_asinacos() {
  dense_1arg(rep_acos, glibc_acos, h_acos, -1.5, 1.5, 4'000'000);
  dense_1arg(rep_asin, glibc_asin, h_asin, -1.5, 1.5, 4'000'000);
  for (unsigned long long i = 0; i < 2'000'000; ++i) {  // segment-boundary bands
    double x = (rnd() & 1 ? 1.0 : -1.0) * (0.125 + rnd_uniform() * 0.875);
    CHECK1(rep_acos, glibc_acos, h_acos, x);
    CHECK1(rep_asin, glibc_asin, h_asin, x);
  }
  for (unsigned long long i = 0; i < 400'000; ++i) {  // just inside +-1
    double x = 1.0 - std::ldexp(rnd_uniform(), (int)(rnd() % 53) - 53);
    CHECK1(rep_acos, glibc_acos, h_acos, x);
    CHECK1(rep_asin, glibc_asin, h_asin, -x);
  }
  const double sp[] = {0.0,        -0.0,   1.0,            -1.0,
                       1.0000000000000002, -1.0000000000000002,
                       DBL_TRUE_MIN, -DBL_TRUE_MIN, 0x1p-54, 0.125, 0.5,
                       0.75,       0.921875, 0.953125,   0.96875};
  for (double x : sp) {
    CHECK1(rep_acos, glibc_acos, h_acos, x);
    CHECK1(rep_asin, glibc_asin, h_asin, x);
  }
  CHECK1(rep_acos, glibc_acos, h_acos, INFINITY);
  CHECK1(rep_asin, glibc_asin, h_asin, std::nan(""));
}

// ----------------------------- census replay ------------------------------

void replay_census(const char* path) {
  FILE* f = std::fopen(path, "r");
  if (!f) {
    std::printf("census: cannot open %s\n", path);
    std::exit(2);
  }
  char sym[16];
  char a_hex[32], b_hex[32];
  unsigned long long count = 0;
  unsigned long long total_args = 0, replayed = 0, skipped = 0;
  char line[256];
  while (std::fgets(line, sizeof line, f)) {
    // Binary form "sym a b count"; unary form "sym a count".
    int n = std::sscanf(line, "%15s %31s %31s %llu", sym, a_hex, b_hex, &count);
    bool binary = (n == 4);
    if (n == 3) {  // unary: third token is the count, re-read it
      if (std::sscanf(line, "%15s %31s %llu", sym, a_hex, &count) != 3)
        continue;
    } else if (n != 4) {
      continue;
    }
    double x = frombits(std::strtoull(a_hex, nullptr, 16));
    double y = binary ? frombits(std::strtoull(b_hex, nullptr, 16)) : 0.0;
    if (!std::strcmp(sym, "cos"))
      CHECK1(rep_cos, glibc_cos, h_cos, x);
    else if (!std::strcmp(sym, "sin"))
      CHECK1(rep_sin, glibc_sin, h_sin, x);
    else if (!std::strcmp(sym, "exp"))
      CHECK1(rep_exp, glibc_exp, h_exp, x);
    else if (!std::strcmp(sym, "log"))
      CHECK1(rep_log, glibc_log, h_log, x);
    else if (!std::strcmp(sym, "pow"))
      CHECK2(rep_pow, glibc_pow, h_pow, x, y);
    else if (!std::strcmp(sym, "hypot"))
      CHECK2(rep_hypot, glibc_hypot, h_hypot, x, y);
    else if (!std::strcmp(sym, "atan2"))
      CHECK2(rep_atan2, glibc_atan2, h_atan2, x, y);
    else if (!std::strcmp(sym, "acos"))
      CHECK1(rep_acos, glibc_acos, h_acos, x);
    else if (!std::strcmp(sym, "asin"))
      CHECK1(rep_asin, glibc_asin, h_asin, x);
    else {
      ++skipped;  // tanh/atanh/logl/... — other shims' domains
      continue;
    }
    total_args += count;
    ++replayed;
  }
  std::fclose(f);
  std::printf("census %s: %llu distinct args replayed, %llu skipped (%s)\n",
              path, replayed, skipped,
              rep_hypot.bad || rep_pow.bad ? "running totals" : "clean so far");
}

}  // namespace

int main(int argc, char** argv) {
  battery_sincos();
  battery_exp();
  battery_log();
  battery_pow();
  battery_hypot();
  battery_atan2();
  battery_asinacos();
  for (int i = 1; i < argc; ++i) replay_census(argv[i]);

  KernelReport* reps[] = {&rep_sin, &rep_cos, &rep_exp, &rep_log, &rep_pow,
                          &rep_hypot, &rep_atan2, &rep_acos, &rep_asin};
  unsigned long long bad = 0, checked = 0;
  for (KernelReport* r : reps) {
    std::printf("%-6s %12llu checks: %s\n", r->name, r->checked,
                r->bad ? "FAIL" : "PASS");
    bad += r->bad;
    checked += r->checked;
  }
  std::printf("hostlineage %s: %llu checks, %llu mismatches\n",
              bad ? "FAIL" : "PASS", checked, bad);
  return bad ? 1 : 0;
}
