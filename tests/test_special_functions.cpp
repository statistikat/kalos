// Item 21-H: unit qualification of the SpecialFunctions transcription behind
// the Ljung-Box Chi-square upper-tail p-values used by the automatic model
// controller (docs/contracts/arma-diagnostics-contract.md vocabulary; the
// bytecode pin lives in docs/item21-roadmap.md references and the capture is
// validation/reference/SpecialFunctionsGolden.java). Every golden row is a
// raw-bits comparison against the pinned jdplus 3.9.0 JVM: the machine's
// whole purpose is landing-basin determinism, so there is no tolerance band
// here - a one-ulp difference is a transcription bug.
#include "kalos/detail/special_functions.hpp"

#include "jvm_special_functions.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>

namespace {

using kalos::detail::chi2_upper_p;
using kalos::detail::special_ig_cont_fract;
using kalos::detail::special_ig_series;
using kalos::detail::special_inc_gamma;
using kalos::detail::special_log_gamma;

std::uint64_t to_bits(double d) {
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof(u));
    return u;
}

int failures = 0;

void check_row(const golden::Row& r, const char* what, bool got_throw, double got) {
    if (r.threw) {
        if (!got_throw) {
            std::printf("FAIL %s a=%016llx x=%016llx: JVM threw, native returned %.17g\n",
                        what, static_cast<unsigned long long>(r.a),
                        static_cast<unsigned long long>(r.b), got);
            ++failures;
        }
        return;
    }
    if (got_throw) {
        std::printf("FAIL %s a=%016llx x=%016llx: native threw, JVM returned %016llx\n",
                    what, static_cast<unsigned long long>(r.a),
                    static_cast<unsigned long long>(r.b),
                    static_cast<unsigned long long>(r.v));
        ++failures;
        return;
    }
    const double want = golden::bits(r.v);
    // NaN equality is by class (the grid captured none, but the guard
    // surfaces could yield one).
    const bool ok = (std::isnan(want) && std::isnan(got)) || to_bits(got) == r.v;
    if (!ok) {
        std::printf("FAIL %s a=%016llx x=%016llx: got %016llx want %016llx\n",
                    what, static_cast<unsigned long long>(r.a),
                    static_cast<unsigned long long>(r.b),
                    static_cast<unsigned long long>(to_bits(got)),
                    static_cast<unsigned long long>(r.v));
        ++failures;
    }
}

} // namespace

int main() {
    std::size_t count = 0;
    const golden::Row* rows = golden::table(count);
    std::size_t counts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::size_t throws[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (std::size_t i = 0; i < count; ++i) {
        const golden::Row& r = rows[i];
        const double a = golden::bits(r.a);
        const double b = golden::bits(r.b);
        ++counts[r.tag];
        if (r.threw) {
            ++throws[r.tag];
        }
        switch (r.tag) {
        case golden::kLogGamma: {
            const auto got = special_log_gamma(a);
            check_row(r, "logGamma", !got, got.value_or(0.0));
            break;
        }
        case golden::kIgSeries: {
            const auto got = special_ig_series(a, b);
            check_row(r, "IGSeries", !got, got.value_or(0.0));
            break;
        }
        case golden::kIgContFract: {
            const auto got = special_ig_cont_fract(a, b);
            check_row(r, "IGContFract", !got, got.value_or(0.0));
            break;
        }
        case golden::kIncGamma: {
            const auto got = special_inc_gamma(a, b);
            check_row(r, "incGamma", !got, got.value_or(0.0));
            break;
        }
        case golden::kChi2Upper: {
            const double got = chi2_upper_p(a, b);
            check_row(r, "chi2Upper", false, got);
            break;
        }
        default:
            std::printf("FAIL unknown golden tag %u\n", r.tag);
            ++failures;
        }
    }
    // Surface probes that the grid cannot express: JVM guard semantics.
    // logGamma throws for a <= 0 and NaN.
    if (special_log_gamma(0.0) || special_log_gamma(-1.0) || special_log_gamma(std::nan(""))) {
        std::printf("FAIL logGamma guard surface (0, -1, NaN must map to nullopt)\n");
        ++failures;
    }
    // chiSquare's x < 0 guard yields the NaN p-value (TestsUtility catch);
    // a NaN statistic slips the guard and ends in the non-convergence throw,
    // which is also NaN.
    if (!std::isnan(chi2_upper_p(10.0, -0.5)) || !std::isnan(chi2_upper_p(10.0, std::nan("")))) {
        std::printf("FAIL chi2_upper_p NaN surface (negative / NaN statistic)\n");
        ++failures;
    }
    // x = 0 is the pinned degenerate row: lower probability 0, upper 1.
    if (to_bits(chi2_upper_p(10.0, 0.0)) != to_bits(1.0)) {
        std::printf("FAIL chi2_upper_p(df, 0) != 1\n");
        ++failures;
    }
    std::printf("special_functions: %zu rows (loggamma %zu, igseries %zu/%zu throw, igcf %zu/%zu "
                "throw, incgamma %zu, chi2upper %zu/%zu throw), %d failures\n",
                count, counts[0], counts[1], throws[1], counts[2], throws[2], counts[3],
                counts[4], throws[4], failures);
    return failures == 0 ? 0 : 1;
}
