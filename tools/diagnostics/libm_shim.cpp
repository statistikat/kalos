// DIAGNOSTIC ONLY — LD_PRELOAD shim for the CI-FDLIBM LEDGER symbols. Never
// linked into kalos; built standalone by tools/ci-shim-invariance.sh.
//
// Interposes the raw host-libm ledger symbols and, per mode, (a) injects
// deterministic +/-1 ulp noise (a pure function of the input bits, so runs
// are reproducible) to emulate a machine whose libm rounds differently, or
// (b) RECORDS every distinct (symbol, arg-bits) pair the process actually
// reached (census; KALOS_LIBM_RECORD). (a) is the machine-independence
// acceptance probe: a bit claim that survives worst-case 1-ulp libm noise at
// the provider boundary is not resting on libm bits. (b) feeds the
// bit-exactness proof of the vendored hostlineage kernels
// (tools/verify_hostlineage.cpp replays the recorded arguments).
//
// Modes via KALOS_LIBM_SHIM (comma-free single token):
//   none | cos | sin | trig | pow | log1p | expm1 | hypot | atan2 | acos
//        | exp | log | explog | tanh | atanh | thatan
//        | ledger-all  (cos,sin,pow,log1p,expm1,hypot,atan2,acos,exp,log)
//        | all         (ledger-all + tanh,atanh; legacy meaning)
//        | ldlog       (80-bit logl/log1pl/expm1l/expl axis)
// Density via KALOS_LIBM_DENSITY (percent of distinct inputs nudged,
// default 30; 100 = every distinct reached argument moves).
//
// Post LEG-A routing (vendored glibc-2.39 hostlineage kernels + JVM fdlibm):
// the ledger symbols resolve inside the vendored kernels, so ledger-all noise
// is INERT for routed sites - a bit-claim suite that still reacts to it has
// an unmigrated raw host-libm dependency. The remaining expected-reactive
// surfaces are listed in the CI-FDLIBM contract ledger.
//
// Record: KALOS_LIBM_RECORD="cos sin pow log1p expm1 hypot atan2 acos exp log
//   logl expm1l log1pl expl acosl"  (whitespace-separated symbol list);
// dumps to KALOS_LIBM_RECORD_FILE (default ./libm_args.txt) at exit, one
// "<sym> <hexbits> [<hexbits2>] <count>" line per distinct call signature.
#define _GNU_SOURCE
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>

namespace {

using dbl_fn = double (*)(double);
using dbl2_fn = double (*)(double, double);
using ld_fn = long double (*)(long double);
using ld2_fn = long double (*)(long double, long double);

template <typename T> T need(const char* name) {
    void* p = dlsym(RTLD_NEXT, name);
    if (!p) { std::fprintf(stderr, "shim3: dlsym(%s) failed\n", name); std::abort(); }
    return reinterpret_cast<T>(p);
}

const dbl_fn  real_cos   = need<dbl_fn>("cos");
const dbl_fn  real_sin   = need<dbl_fn>("sin");
const dbl_fn  real_exp   = need<dbl_fn>("exp");
const dbl_fn  real_log   = need<dbl_fn>("log");
const dbl_fn  real_tanh  = need<dbl_fn>("tanh");
const dbl_fn  real_atanh = need<dbl_fn>("atanh");
const dbl_fn  real_log1p = need<dbl_fn>("log1p");
const dbl_fn  real_expm1 = need<dbl_fn>("expm1");
const dbl_fn  real_acos  = need<dbl_fn>("acos");
const dbl2_fn real_pow   = need<dbl2_fn>("pow");
const dbl2_fn real_hypot = need<dbl2_fn>("hypot");
const dbl2_fn real_atan2 = need<dbl2_fn>("atan2");
const ld_fn   real_logl  = need<ld_fn>("logl");
const ld_fn   real_log1pl= need<ld_fn>("log1pl");
const ld_fn   real_expm1l= need<ld_fn>("expm1l");
const ld_fn   real_expl  = need<ld_fn>("expl");
const ld_fn   real_acosl = need<ld_fn>("acosl");

std::uint64_t bits(double x) { std::uint64_t b; std::memcpy(&b, &x, sizeof b); return b; }
double unbits(std::uint64_t b) { double x; std::memcpy(&x, &b, sizeof x); return x; }

std::uint64_t mix(std::uint64_t z) {  // splitmix64
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// +/-1 ulp on ~30% of distinct inputs (direction by hash parity).
int density() {
    static const int d = [] {
        const char* e = std::getenv("KALOS_LIBM_DENSITY");
        return e ? std::max(0, std::min(100, std::atoi(e))) : 30;
    }();
    return d;
}
// +/-1 ulp on KALOS_LIBM_DENSITY% (default 30) of distinct inputs.
double nudge(double y, std::uint64_t key, std::uint64_t salt) {
    if (!std::isfinite(y) || y == 0.) return y;
    const std::uint64_t h = mix(key ^ salt);
    if ((int)(h % 100) >= density()) return y;
    const int dir = (h >> 1) & 1 ? 1 : -1;
    return unbits(bits(y) + dir);
}
// Amplified variants: density 100% (every distinct input moves).
double nudge100(double y, std::uint64_t key, std::uint64_t salt, bool one_sided) {
    if (!std::isfinite(y) || y == 0.) return y;
    const std::uint64_t h = mix(key ^ salt);
    const int dir = one_sided ? 1 : ((h >> 1) & 1 ? 1 : -1);
    return unbits(bits(y) + dir);
}
long double nudge_ld(long double y, std::uint64_t key, std::uint64_t salt) {
    if (!std::isfinite(y) || y == 0.) return y;
    const std::uint64_t h = mix(key ^ salt);
    if ((int)(h % 100) >= density()) return y;
    const int dir = (h >> 1) & 1 ? 1 : -1;
    return std::nextafterl(y, dir > 0 ? 1.0L / 0.0L : -1.0L / 0.0L);
}

const char* mode() {
    static const char* m = [] {
        const char* e = std::getenv("KALOS_LIBM_SHIM");
        return e ? e : "none";
    }();
    return m;
}

bool on(const char* tag) {
    const char* m = mode();
    if (!std::strcmp(m, "none")) return false;
    if (!std::strcmp(m, "all") || !std::strcmp(m, "ledger-all")) {
        static const char* ledger[] = {"cos","sin","pow","log1p","expm1","hypot","atan2","acos",
                                       "exp","log",nullptr};
        if (!std::strcmp(m, "ledger-all") && !std::strcmp(tag, "tanh")) return false;
        if (!std::strcmp(m, "ledger-all") && !std::strcmp(tag, "atanh")) return false;
        for (int i = 0; ledger[i]; ++i) if (!std::strcmp(tag, ledger[i])) return true;
        return !std::strcmp(tag, "tanh") || !std::strcmp(tag, "atanh");  // "all"
    }
    if (!std::strcmp(m, "trig")) return !std::strcmp(tag, "cos") || !std::strcmp(tag, "sin");
    if (!std::strcmp(m, "explog")) return !std::strcmp(tag, "exp") || !std::strcmp(tag, "log");
    if (!std::strcmp(m, "thatan")) return !std::strcmp(tag, "tanh") || !std::strcmp(tag, "atanh");
    if (!std::strcmp(m, "ldlog"))
        return !std::strcmp(tag, "logl") || !std::strcmp(tag, "log1pl") ||
               !std::strcmp(tag, "expm1l") || !std::strcmp(tag, "expl");
    return !std::strcmp(m, tag);
}

// ------------------------- record mode ---------------------------------
struct RecKey {
    const char* sym;
    std::uint64_t a, b;
    bool operator<(const RecKey& o) const {
        int c = std::strcmp(sym, o.sym);
        return c ? c < 0 : (a != o.a ? a < o.a : b < o.b);
    }
};
std::map<RecKey, unsigned long long> g_records;
std::vector<std::string> g_record_syms;  // empty = recording off

bool recording(const char* sym) {
    if (g_record_syms.empty()) {
        const char* e = std::getenv("KALOS_LIBM_RECORD");
        if (!e || !*e) { static const char none_ = 0; g_record_syms.push_back(""); return false; }
        std::string s(e);
        const char* p = s.c_str();
        while (*p) {
            while (*p == ' ' || *p == ',') ++p;
            const char* q = p;
            while (*q && *q != ' ' && *q != ',') ++q;
            if (q > p) g_record_syms.emplace_back(p, q);
            p = q;
        }
        if (g_record_syms.empty()) g_record_syms.push_back("");
        std::atexit([] {
            if (g_records.empty()) return;
            const char* f = std::getenv("KALOS_LIBM_RECORD_FILE");
            FILE* out = std::fopen(f ? f : "libm_args.txt", "w");
            if (!out) out = stderr;
            for (const auto& kv : g_records) {
                if (kv.first.b)
                    std::fprintf(out, "%s %016llx %016llx %llu\n", kv.first.sym,
                                 (unsigned long long)kv.first.a, (unsigned long long)kv.first.b,
                                 kv.second);
                else
                    std::fprintf(out, "%s %016llx %llu\n", kv.first.sym,
                                 (unsigned long long)kv.first.a, kv.second);
            }
            if (out != stderr) std::fclose(out);
        });
    }
    if (g_record_syms.size() == 1 && g_record_syms[0].empty()) return false;
    for (const auto& s : g_record_syms) if (s == sym) return true;
    return false;
}
}  // namespace

extern "C" {

double cos(double x) {
    const double y = real_cos(x);
    if (recording("cos")) g_records[RecKey{"cos", bits(x), 0}]++;
    return on("cos") ? nudge(y, bits(x), 0xC051ull) : y;
}
double sin(double x) {
    const double y = real_sin(x);
    if (recording("sin")) g_records[RecKey{"sin", bits(x), 0}]++;
    return on("sin") ? nudge(y, bits(x), 0x51DEull) : y;
}
double exp(double x) {
    const double y = real_exp(x);
    if (recording("exp")) g_records[RecKey{"exp", bits(x), 0}]++;
    return on("exp") ? nudge(y, bits(x), 0xE17ull) : y;
}
double log(double x) {
    const double y = real_log(x);
    if (recording("log")) g_records[RecKey{"log", bits(x), 0}]++;
    return on("log") ? nudge(y, bits(x), 0x1694ull) : y;
}
double tanh(double x) {
    const double y = real_tanh(x);
    if (recording("tanh")) g_records[RecKey{"tanh", bits(x), 0}]++;
    return on("tanh") ? nudge(y, bits(x), 0x7A12ull) : y;
}
double atanh(double x) {
    const double y = real_atanh(x);
    if (recording("atanh")) g_records[RecKey{"atanh", bits(x), 0}]++;
    return on("atanh") ? nudge(y, bits(x), 0x0A73ull) : y;
}
double log1p(double x) {
    const double y = real_log1p(x);
    if (recording("log1p")) g_records[RecKey{"log1p", bits(x), 0}]++;
    return on("log1p") ? nudge(y, bits(x), 0x1C0Cull) : y;
}
double expm1(double x) {
    const double y = real_expm1(x);
    if (recording("expm1")) g_records[RecKey{"expm1", bits(x), 0}]++;
    return on("expm1") ? nudge(y, bits(x), 0xE701ull) : y;
}
double acos(double x) {
    const double y = real_acos(x);
    if (recording("acos")) g_records[RecKey{"acos", bits(x), 0}]++;
    return on("acos") ? nudge(y, bits(x), 0xAC05ull) : y;
}
double pow(double x, double e) {
    const double y = real_pow(x, e);
    const std::uint64_t k = bits(x) ^ mix(bits(e));
    if (recording("pow")) g_records[RecKey{"pow", bits(x), bits(e)}]++;
    return on("pow") ? nudge(y, k, 0x9A0Eull) : y;
}
double hypot(double x, double y) {
    const double r = real_hypot(x, y);
    const std::uint64_t k = bits(x) ^ mix(bits(y));
    if (recording("hypot")) g_records[RecKey{"hypot", bits(x), bits(y)}]++;
    if (!std::strcmp(mode(), "hypot100")) return nudge100(r, k, 0x0112ull, false);
    if (!std::strcmp(mode(), "hypotup")) return nudge100(r, k, 0x0112ull, true);
    return on("hypot") ? nudge(r, k, 0x0112ull) : r;
}
double atan2(double y, double x) {
    const double r = real_atan2(y, x);
    const std::uint64_t k = bits(y) ^ mix(bits(x));
    if (recording("atan2")) g_records[RecKey{"atan2", bits(y), bits(x)}]++;
    return on("atan2") ? nudge(r, k, 0x47B1ull) : r;
}
long double logl(long double x) {
    const long double y = real_logl(x);
    if (recording("logl")) g_records[RecKey{"logl", static_cast<std::uint64_t>(x), 0}]++;
    if (!std::strcmp(mode(), "ldlog100")) {
        if (std::isfinite(y) && y != 0.) {
            const std::uint64_t h = mix(bits(static_cast<double>(x)) ^ 0x1695ull);
            return std::nextafterl(y, ((h >> 1) & 1) ? 1.0L / 0.0L : -1.0L / 0.0L);
        }
        return y;
    }
    return on("logl") ? nudge_ld(y, bits(static_cast<double>(x)), 0x1695ull) : y;
}
long double log1pl(long double x) {
    const long double y = real_log1pl(x);
    if (recording("log1pl")) g_records[RecKey{"log1pl", static_cast<std::uint64_t>(x), 0}]++;
    return on("log1pl") ? nudge_ld(y, bits(static_cast<double>(x)), 0x1C0Dull) : y;
}
long double expm1l(long double x) {
    const long double y = real_expm1l(x);
    if (recording("expm1l")) g_records[RecKey{"expm1l", static_cast<std::uint64_t>(x), 0}]++;
    return on("expm1l") ? nudge_ld(y, bits(static_cast<double>(x)), 0xE702ull) : y;
}
long double expl(long double x) {
    const long double y = real_expl(x);
    if (recording("expl")) g_records[RecKey{"expl", static_cast<std::uint64_t>(x), 0}]++;
    return on("expl") ? nudge_ld(y, bits(static_cast<double>(x)), 0xE18ull) : y;
}
long double acosl(long double x) {
    const long double y = real_acosl(x);
    if (recording("acosl")) g_records[RecKey{"acosl", static_cast<std::uint64_t>(x), 0}]++;
    return on("acosl") ? nudge_ld(y, bits(static_cast<double>(x)), 0xAC06ull) : y;
}

}  // extern "C"
