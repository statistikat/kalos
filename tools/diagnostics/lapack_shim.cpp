// DIAGNOSTIC ONLY — LAPACK-boundary noise interposers for the exact symbols
// the kalos pipeline imports (LAPACKE_*_work + cblas_dgemm; OpenBLAS
// INTERFACE64=0 => lapack_int=int). Never linked into kalos; built
// standalone by tools/ci-shim-invariance.sh. Mode KALOS_LIBM_SHIM=lapack1
// applies deterministic +/-1 ulp noise (30% density) to the floating-point
// OUTPUTS of TOP-LEVEL calls only (direct caller = main executable, where
// libkalos.a is statically linked). Emulates "a differently compiled /
// kernel-dispatched provider" at the provider boundary.
#define _GNU_SOURCE
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <algorithm>
#include <atomic>
#include <string>
#include <dlfcn.h>
#include <link.h>

namespace {
using li = int;  // lapack_int (INTERFACE64=0)

std::uint64_t bits(double x) { std::uint64_t b; std::memcpy(&b, &x, sizeof b); return b; }
double unbits(std::uint64_t b) { double x; std::memcpy(&x, &b, sizeof x); return x; }
std::uint64_t mix(std::uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
bool mode_on() {
    static const char* m = [] {
        const char* e = std::getenv("KALOS_LIBM_SHIM");
        return e ? e : "none";
    }();
    return !std::strcmp(m, "lapack1");
}
struct ExeBase { void* base = nullptr; };
int exe_cb(struct dl_phdr_info* info, size_t, void* data) {
    auto* e = static_cast<ExeBase*>(data);
    if (!info->dlpi_name || !info->dlpi_name[0]) { e->base = (void*)info->dlpi_addr; return 1; }
    return 0;
}
bool caller_is_exe_uncached(void* ra) {
    static void* base = [] {
        ExeBase e;
        dl_iterate_phdr(exe_cb, &e);
        return e.base;
    }();
    Dl_info info;
    if (!dladdr(ra, &info) || !info.dli_fbase) return false;
    return info.dli_fbase == base;
}
bool caller_is_exe(void* ra) {  // memoized; benign races, idempotent entries
    static std::atomic<uintptr_t> keys[4096];
    static std::atomic<bool> vals[4096];
    const uintptr_t k = (uintptr_t)ra;
    const std::size_t slot = (((k >> 4) * 2654435761ull) >> 44) & 4095;
    if (keys[slot].load(std::memory_order_relaxed) == k)
        return vals[slot].load(std::memory_order_relaxed);
    const bool v = caller_is_exe_uncached(ra);
    vals[slot].store(v, std::memory_order_relaxed);
    keys[slot].store(k, std::memory_order_relaxed);
    return v;
}
double nudge(double y, std::uint64_t key) {
    if (!std::isfinite(y) || y == 0.) return y;
    const std::uint64_t h = mix(key ^ bits(y));
    static const int dens = [] { const char* e = std::getenv("KALOS_LIBM_DENSITY"); return e ? std::atoi(e) : 30; }();
    if ((int)(h % 100) >= dens) return y;
    return unbits(bits(y) + (((h >> 1) & 1) ? 1 : -1));
}
void noise(bool top, double* v, std::size_t n, std::uint64_t salt) {
    if (!top || !v) return;
    for (std::size_t i = 0; i < n; ++i) v[i] = nudge(v[i], salt + 1000003ull * i);
}
// noise on the lower triangle (incl. diagonal) of a packed n x n matrix
void noise_lower(bool top, double* a, li n, li lda, std::uint64_t salt) {
    if (!top) return;
    for (li j = 0; j < n; ++j)
        for (li i = j; i < n; ++i)
            a[i + (std::size_t)j * lda] = nudge(a[i + (std::size_t)j * lda], salt + i * 31 + j);
    for (li j = 0; j < n; ++j)
        for (li i = 0; i <= j; ++i)
            a[i + (std::size_t)j * lda] = nudge(a[i + (std::size_t)j * lda], salt + i * 17 + j);
}
template <typename T> T need(const char* name) {
    void* p = dlsym(RTLD_NEXT, name);
    if (!p) { std::fprintf(stderr, "lapackshim: dlsym(%s) failed\n", name); std::abort(); }
    return reinterpret_cast<T>(p);
}
}  // namespace

extern "C" {

li LAPACKE_dgeqrf_work(int layout, li m, li n, double* a, li lda, double* tau,
                       double* work, li lwork) {
    static auto real = need<li(*)(int, li, li, double*, li, double*, double*, li)>("LAPACKE_dgeqrf_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, m, n, a, lda, tau, work, lwork);
    noise(top, tau, std::min(m, n), 0x1000);
    if (top)
        for (li j = 0; j < n; ++j)
            for (li i = j; i < m; ++i)
                a[i + (std::size_t)j * lda] = nudge(a[i + (std::size_t)j * lda], 0x2000 + i * 31 + j);
    return info;
}

li LAPACKE_dpotrf_work(int layout, char uplo, li n, double* a, li lda) {
    static auto real = need<li(*)(int, char, li, double*, li)>("LAPACKE_dpotrf_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, uplo, n, a, lda);
    if (top) {
        for (li j = 0; j < n; ++j)
            for (li i = (uplo == 'L' || uplo == 'l' ? j : 0); i <= j; ++i)
                a[i + (std::size_t)j * lda] = nudge(a[i + (std::size_t)j * lda], 0x3000 + i * 17 + j);
        if (uplo == 'U' || uplo == 'u')
            for (li j = 0; j < n; ++j)
                for (li i = 0; i <= j; ++i)
                    a[i + (std::size_t)j * lda] = nudge(a[i + (std::size_t)j * lda], 0x3400 + i * 17 + j);
    }
    return info;
}

li LAPACKE_dpotrs_work(int layout, char uplo, li n, li nrhs, const double* a, li lda,
                       double* b, li ldb) {
    static auto real = need<li(*)(int, char, li, li, const double*, li, double*, li)>("LAPACKE_dpotrs_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, uplo, n, nrhs, a, lda, b, ldb);
    noise(top, b, (std::size_t)ldb * nrhs, 0x4000);
    return info;
}

li LAPACKE_dpocon_work(int layout, char uplo, li n, const double* a, li lda, double anorm,
                       double* rcond, double* work, li* iwork) {
    static auto real = need<li(*)(int, char, li, const double*, li, double, double*, double*,
                                 li*)>("LAPACKE_dpocon_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, uplo, n, a, lda, anorm, rcond, work, iwork);
    noise(top, rcond, 1, 0x5000);
    return info;
}

li LAPACKE_dsyev_work(int layout, char jobz, char uplo, li n, double* a, li lda, double* w,
                      double* work, li lwork) {
    static auto real = need<li(*)(int, char, char, li, double*, li, double*, double*, li)>("LAPACKE_dsyev_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, jobz, uplo, n, a, lda, w, work, lwork);
    noise(top, w, n, 0x6000);
    if (top && jobz == 'V') noise(top, a, (std::size_t)n * lda, 0x6100);
    return info;
}

li LAPACKE_dgesvd_work(int layout, char jobu, char jobvt, li m, li n, double* a, li lda,
                       double* s, double* u, li ldu, double* vt, li ldvt,
                       double* work, li lwork) {
    static auto real = need<li(*)(int, char, char, li, li, double*, li, double*, double*, li,
                                 double*, li, double*, li)>("LAPACKE_dgesvd_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, jobu, jobvt, m, n, a, lda, s, u, ldu, vt, ldvt, work, lwork);
    noise(top, s, std::min(m, n), 0x7000);
    return info;
}

li LAPACKE_dgelsy_work(int layout, li m, li n, li nrhs, double* a, li lda, double* b, li ldb,
                       li* jpvt, double rcond, li* rank, double* work, li lwork) {
    static auto real = need<li(*)(int, li, li, li, double*, li, double*, li, li*, double,
                                 li*, double*, li)>("LAPACKE_dgelsy_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, m, n, nrhs, a, lda, b, ldb, jpvt, rcond, rank, work, lwork);
    noise(top, b, (std::size_t)ldb * nrhs, 0x8000);
    return info;
}

li LAPACKE_dgeev_work(int layout, char jobvl, char jobvr, li n, double* a, li lda, double* wr,
                      double* wi, double* vl, li ldvl, double* vr, li ldvr,
                      double* work, li lwork) {
    static auto real = need<li(*)(int, char, char, li, double*, li, double*, double*, double*,
                                 li, double*, li, double*, li)>("LAPACKE_dgeev_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(layout, jobvl, jobvr, n, a, lda, wr, wi, vl, ldvl, vr, ldvr, work, lwork);
    noise(top, wr, n, 0x9000);
    noise(top, wi, n, 0x9100);
    return info;
}

li LAPACKE_dlarfg_work(li n, double* alpha, double* x, li incx, double* tau) {
    static auto real = need<li(*)(li, double*, double*, li, double*)>("LAPACKE_dlarfg_work");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    const li info = real(n, alpha, x, incx, tau);
    noise(top, tau, 1, 0xA100);
    if (top && x && incx > 0) for (li i = 0; i + 1 < n; ++i) x[i * incx] = nudge(x[i * incx], 0xA200 + i);
    return info;
}

void cblas_dgemm(int order, int ta, int tb, li m, li n, li k, double alpha, const double* a,
                 li lda, const double* b, li ldb, double beta, double* c, li ldc) {
    static auto real = need<void(*)(int, int, int, li, li, li, double, const double*, li,
                                    const double*, li, double, double*, li)>("cblas_dgemm");
    const bool top = mode_on() && caller_is_exe(__builtin_return_address(0));
    real(order, ta, tb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
    noise(top, c, (std::size_t)ldc * n, 0xB000);
}

}  // extern "C"
