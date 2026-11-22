// Item 21-H: unit qualification of the SpecialFunctions transcription behind
// the Ljung-Box Chi-square upper-tail p-values used by the automatic model
// controller (docs/contracts/arma-diagnostics-contract.md vocabulary; the
// bytecode pin lives in docs/item21-roadmap.md references and the capture is
// validation/reference/SpecialFunctionsGolden.java). Item 23-C.2 adds the
// incomplete-beta / FProbability half of the same class (the SeasonalFTest
// p-values feeding the TRAMO differencing battery,
// docs/contracts/tramo-differencing-decision-contract.md), replayed from the
// double-captured validation/reference/fcdf_golden.txt. Every golden row is
// a raw-bits comparison against the pinned jdplus 3.9.0 JVM: the machine's
// whole purpose is landing-basin determinism, so there is no tolerance band
// here - a one-ulp difference is a transcription bug.
#include "kalos/detail/special_functions.hpp"

#include "jvm_fcdf.h"
#include "jvm_special_functions.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>

namespace {

using kalos::detail::chi2_upper_p;
using kalos::detail::f_upper_p;
using kalos::detail::special_ig_cont_fract;
using kalos::detail::special_ig_series;
using kalos::detail::special_inc_beta;
using kalos::detail::special_inc_beta_cf;
using kalos::detail::special_inc_gamma;
using kalos::detail::special_log_gamma;

namespace golden_fcdf = kalos::tests::golden_fcdf;

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

// The fcdf table carries the golden result in `out`, not `v` (three grid
// arguments per row); same raw-bits discipline as check_row.
void check_f_row(const golden_fcdf::Row& r, const char* what, bool got_throw, double got) {
    if (r.threw) {
        if (!got_throw) {
            std::printf("FAIL %s (%016llx, %016llx, %016llx): JVM threw, native returned %.17g\n",
                        what, static_cast<unsigned long long>(r.a),
                        static_cast<unsigned long long>(r.b),
                        static_cast<unsigned long long>(r.v), got);
            ++failures;
        }
        return;
    }
    if (got_throw) {
        std::printf("FAIL %s (%016llx, %016llx, %016llx): native threw, JVM returned %016llx\n",
                    what, static_cast<unsigned long long>(r.a),
                    static_cast<unsigned long long>(r.b),
                    static_cast<unsigned long long>(r.v),
                    static_cast<unsigned long long>(r.out));
        ++failures;
        return;
    }
    const double want = golden_fcdf::bits(r.out);
    const bool ok = (std::isnan(want) && std::isnan(got)) || to_bits(got) == r.out;
    if (!ok) {
        std::printf("FAIL %s (%016llx, %016llx, %016llx): got %016llx want %016llx\n",
                    what, static_cast<unsigned long long>(r.a),
                    static_cast<unsigned long long>(r.b),
                    static_cast<unsigned long long>(r.v),
                    static_cast<unsigned long long>(to_bits(got)),
                    static_cast<unsigned long long>(r.out));
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

    // --- item 23-C.2: the incomplete-beta / FProbability kernel --------------
    std::size_t fcount = 0;
    const golden_fcdf::Row* frows = golden_fcdf::table(fcount);
    std::size_t fcounts[4] = {0, 0, 0, 0};
    std::size_t fthrows[4] = {0, 0, 0, 0};
    for (std::size_t i = 0; i < fcount; ++i) {
        const golden_fcdf::Row& r = frows[i];
        const double a = golden_fcdf::bits(r.a);
        const double b = golden_fcdf::bits(r.b);
        const double v = golden_fcdf::bits(r.v);
        ++fcounts[r.tag];
        if (r.threw) {
            ++fthrows[r.tag];
        }
        switch (r.tag) {
        case golden_fcdf::kIncBetaCf: {
            const auto got = special_inc_beta_cf(a, b, v);
            check_f_row(r, "incBetaCf", !got, got.value_or(0.0));
            break;
        }
        case golden_fcdf::kIncBeta: {
            const auto got = special_inc_beta(a, b, v);
            check_f_row(r, "incBeta", !got, got.value_or(0.0));
            break;
        }
        case golden_fcdf::kFProbability:
        case golden_fcdf::kCorpus: {
            // The DStatException surface of FProbability is the NaN p-value
            // (TestsUtility catch): a JVM throw must land on a NaN, a JVM
            // value must match bit for bit.
            const double got = f_upper_p(a, b, v);
            if (r.threw) {
                if (!std::isnan(got)) {
                    std::printf("FAIL fprob idx=%u (%016llx, %016llx, %016llx): JVM threw, "
                                "native returned %.17g\n",
                                r.index, static_cast<unsigned long long>(r.a),
                                static_cast<unsigned long long>(r.b),
                                static_cast<unsigned long long>(r.v), got);
                    ++failures;
                }
                break;
            }
            const double want = golden_fcdf::bits(r.out);
            const bool ok = (std::isnan(want) && std::isnan(got)) || to_bits(got) == r.out;
            if (!ok) {
                std::printf("FAIL fprob idx=%u (%016llx, %016llx, %016llx): got %016llx "
                            "want %016llx\n",
                            r.index, static_cast<unsigned long long>(r.a),
                            static_cast<unsigned long long>(r.b),
                            static_cast<unsigned long long>(r.v),
                            static_cast<unsigned long long>(to_bits(got)),
                            static_cast<unsigned long long>(r.out));
                ++failures;
            }
            break;
        }
        default:
            std::printf("FAIL unknown fcdf golden tag %u\n", r.tag);
            ++failures;
        }
    }
    std::printf("fcdf kernel: %zu rows (cf %zu/%zu throw, beta %zu/%zu throw, fprob %zu/%zu "
                "throw, corpus %zu/%zu throw), %d failures\n",
                fcount, fcounts[0], fthrows[0], fcounts[1], fthrows[1], fcounts[2],
                fthrows[2], fcounts[3], fthrows[3], failures);
    return failures == 0 ? 0 : 1;
}
