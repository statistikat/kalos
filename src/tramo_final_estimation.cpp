// Item 23-F F.1: transcription of the private decision methods of the
// pinned jdplus.tramoseats.base.core.tramo.FinalEstimator (bytecode
// offsets in tmp_opencode/kalos-m5-pinning.md SECTION 26.2/26.6; probe
// adjudication out/fe1e against the 3.9.0 jars).

#include "kalos/tramo_final_estimation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "kalos/detail/ucarima_engine.hpp"

namespace kalos {
namespace {

using detail::cplx;
using detail::Poly;
using detail::p_roots;
using detail::gt_l; // dcmpl + ifgt: strict, NaN excluded
using detail::gt_g; // dcmpg + ifgt: NaN counts as above
using detail::ge_g; // dcmpg + ifge: NaN counts as at-or-above
using detail::lt_g; // dcmpg + iflt: strict, NaN excluded

[[noreturn]] void refuse(const std::string& why) {
    throw std::invalid_argument("tramo_final_estimation: " + why);
}

// Complex.abs: the scaled modulus m * sqrt(1 + (n / m) ^ 2) with the
// (0, 0) and n == 0 shortcuts, NOT the correctly rounded std::hypot
// (23-C pin of the same JVM primitive).
double jvm_complex_abs(double re, double im) {
    if (re == 0.0 && im == 0.0) {
        return 0.0;
    }
    const double a = std::fabs(re), b = std::fabs(im);
    const double m = std::max(a, b), n = std::min(a, b);
    if (n == 0.0) {
        return m;
    }
    const double t = n / m;
    return m * std::sqrt(1.0 + t * t);
}

// getRegularAR()/getSeasonalAR(): Polynomial.ofInternal([1 | block]) - the
// trimming variant (leading zeros of the block shrink the effective
// degree; mr06 of the corpus pins it) - optionally followed by mirror()
// (plain reversal of the trimmed array, itself untrimmed) and roots().
std::vector<cplx> ar_value_roots(
    const std::vector<double>& block, const char* what, bool mirror) {
    std::vector<double> coefficients;
    coefficients.reserve(block.size() + 1);
    coefficients.push_back(1.0);
    coefficients.insert(coefficients.end(), block.begin(), block.end());
    Poly poly = detail::p_of(std::move(coefficients));
    if (mirror) {
        std::reverse(poly.c.begin(), poly.c.end());
    }
    std::vector<cplx> roots;
    try {
        roots = p_roots(std::move(poly), what);
    } catch (const detail::jvm_decompose_abort& abort) {
        throw std::runtime_error(
            std::string("tramo_final_estimation: ") + what + ": " + abort.reason);
    }
    return roots;
}

std::vector<cplx> ar_mirror_roots(const std::vector<double>& block, const char* what) {
    return ar_value_roots(block, what, true);
}

} // namespace

std::string tramo_final_orders_to_string(const TramoFinalOrders& orders) {
    return "(" + std::to_string(orders.p) + "," + std::to_string(orders.d) + ","
        + std::to_string(orders.q) + ")(" + std::to_string(orders.bp) + ","
        + std::to_string(orders.bd) + "," + std::to_string(orders.bq) + ")";
}

std::vector<TramoFinalComplex> tramo_mirror_roots(
    const std::vector<double>& ascending_coefficients) {
    Poly poly = detail::p_of(std::vector<double>(ascending_coefficients));
    std::reverse(poly.c.begin(), poly.c.end());
    std::vector<cplx> roots;
    try {
        roots = p_roots(std::move(poly), "mirror_roots");
    } catch (const detail::jvm_decompose_abort& abort) {
        throw std::runtime_error(
            "tramo_final_estimation: mirror_roots: " + abort.reason);
    }
    std::vector<TramoFinalComplex> out;
    out.reserve(roots.size());
    for (const cplx& z : roots) {
        out.push_back({z.re, z.im});
    }
    return out;
}

int tramo_searchur_roots(const std::vector<TramoFinalComplex>& roots, double ur) {
    int count = 0;
    for (const TramoFinalComplex& z : roots) {
        const double im = std::fabs(z.im);
        const double modulus = jvm_complex_abs(z.re, z.im);
        if (gt_l(modulus, ur) && !gt_g(im, 0.05) && gt_l(z.re, 0.0)) {
            ++count;
        }
    }
    return count;
}

int tramo_searchur(const std::vector<double>& ar_block, double ur) {
    const std::vector<cplx> roots = ar_mirror_roots(ar_block, "searchur");
    std::vector<TramoFinalComplex> converted;
    converted.reserve(roots.size());
    for (const cplx& z : roots) {
        converted.push_back({z.re, z.im});
    }
    return tramo_searchur_roots(converted, ur);
}

TramoUnitRootsResult tramo_check_unit_roots(const TramoUnitRootsRequest& request) {
    TramoUnitRootsResult result;
    result.orders_after = request.orders;
    if (request.orders.p < 0 || request.orders.d < 0 || request.orders.q < 0
        || request.orders.bp < 0 || request.orders.bd < 0 || request.orders.bq < 0) {
        refuse("negative orders");
    }
    // offsets 16-70: if (p > 0 && d < 2 && searchur(AR mirror roots) != 0)
    // { setP(p - 1); setD(d + 1); ans = false; } The length guards sit
    // inside the JVM read sites: a block is required only when its scan
    // actually runs (a gated-off scan never touches the polynomial).
    if (request.orders.p > 0 && request.orders.d < 2) {
        if (request.phi.size() < static_cast<std::size_t>(request.orders.p)) {
            refuse("phi block shorter than p");
        }
        result.regular_checked = true;
        result.regular_count = tramo_searchur(request.phi, request.ur);
        if (result.regular_count != 0) {
            result.regular_fired = true;
            --result.orders_after.p;
            ++result.orders_after.d;
            result.unchanged = false;
        }
    }
    // offsets 72-126: the seasonal twin with the bd < 1 gate.
    if (request.orders.bp > 0 && request.orders.bd < 1) {
        if (request.bphi.size() < static_cast<std::size_t>(request.orders.bp)) {
            refuse("bphi block shorter than bp");
        }
        result.seasonal_checked = true;
        result.seasonal_count = tramo_searchur(request.bphi, request.ur);
        if (result.seasonal_count != 0) {
            result.seasonal_fired = true;
            --result.orders_after.bp;
            ++result.orders_after.bd;
            result.unchanged = false;
        }
    }
    return result;
}

TramoCommonRootsResult tramo_check_common_roots(const TramoCommonRootsRequest& request) {
    TramoCommonRootsResult result;
    result.orders_after = request.orders;
    if (request.orders.p < 0 || request.orders.d < 0 || request.orders.q < 0
        || request.orders.bp < 0 || request.orders.bd < 0 || request.orders.bq < 0) {
        refuse("negative orders");
    }
    // offsets 16-115: p != 0 && q != 0 computes both root arrays (roots()
    // on getRegularAR()/getRegularMA(), NO mirror) and inverts the entries
    // in place; nothing reads the arrays afterwards - but the roots() calls
    // can still raise, so they execute faithfully and only the results are
    // dropped. Complex.inv on a zero root yields NaN without raising, so
    // the inversion itself is unobservable.
    if (request.orders.p != 0 && request.orders.q != 0) {
        if (request.phi.size() < static_cast<std::size_t>(request.orders.p)
            || request.theta.size() < static_cast<std::size_t>(request.orders.q)) {
            refuse("regular blocks shorter than p/q");
        }
        (void)ar_value_roots(request.phi, "checkCommonRoots regular AR", false);
        (void)ar_value_roots(request.theta, "checkCommonRoots regular MA", false);
    }
    // offsets 116-165: bp == 1 && bq == 1 && |bphi(1) - btheta(1)| < cancel.
    const bool rule_eligible = request.orders.bp == 1 && request.orders.bq == 1;
    if (rule_eligible) {
        if (request.bphi.empty() || request.btheta.empty()) {
            refuse("bphi/btheta block missing for the bp == bq == 1 rule");
        }
        const double delta = std::fabs(request.bphi[0] - request.btheta[0]);
        if (lt_g(delta, request.cancel)) { // dcmpg, ifge: NaN never cancels
            result.cancelled = true;
            result.unchanged = false;
            result.orders_after.bp = 0;
            result.orders_after.bq = 0;
        }
    }
    return result;
}

TramoFinalEstimatorTestResult tramo_final_estimator_test(
    const TramoFinalEstimatorTestRequest& request) {
    const TramoFinalOrders& orders = request.orders;
    if (orders.p < 0 || orders.d < 0 || orders.q < 0 || orders.bp < 0 || orders.bd < 0
        || orders.bq < 0) {
        refuse("negative orders");
    }
    const std::size_t dim = static_cast<std::size_t>(orders.p + orders.bp + orders.q
        + orders.bq);
    if (request.parameters.size() < dim) {
        refuse("parameters shorter than p + bp + q + bq");
    }
    if (dim > 0 && request.asy_covariance_diagonal.size() < dim) {
        refuse("asymptotic covariance diagonal shorter than the parameter count");
    }
    const double tsig = request.tsig;
    const double sig2 = request.sample_length <= 150 ? 0.15 : 0.1; // offset 17-34

    TramoFinalEstimatorTestResult result;
    result.orders_after = orders;
    double ratio = 99999.0; // offset 76; the x13 fork starts at 999
    bool ratio_set = false;
    int index = -1; // offset 81; running SarimaModel parameter index

    // One block: offsets 112-183 (P), 193-275 (Bp), 285-370 (Q), 380-464 (Bq).
    struct Block {
        int order;
        bool* flag;
        bool gated;      // P has no ratio gate
        bool clears_p;
        bool clears_bp;
        bool clears_q;
        bool updates_ratio;
    };
    Block blocks[4] = {{orders.p, &result.fired_p, false, false, false, false, true},
        {orders.bp, &result.fired_bp, true, true, false, false, true},
        {orders.q, &result.fired_q, true, true, true, false, true},
        {orders.bq, &result.fired_bq, true, true, true, true, false}};
    for (const Block& block : blocks) {
        index += block.order;
        if (block.order <= 0) {
            continue;
        }
        const double a = std::fabs(request.parameters[static_cast<std::size_t>(index)]);
        const double v = request.asy_covariance_diagonal[static_cast<std::size_t>(index)];
        if (!gt_l(v, 0.0)) { // dcmpl, ifle: NaN variance skips the block
            continue;
        }
        const double t = a / std::sqrt(v);
        if (ge_g(t, tsig) || ge_g(a, sig2)) { // dcmpg, ifge on both
            continue;
        }
        if (block.gated && !gt_l(ratio, t)) { // dcmpl, ifle: strict, NaN t blocks
            continue;
        }
        *block.flag = true;
        if (block.clears_p) {
            result.fired_p = false;
        }
        if (block.clears_bp) {
            result.fired_bp = false;
        }
        if (block.clears_q) {
            result.fired_q = false;
        }
        if (block.updates_ratio) {
            ratio = t;
            ratio_set = true;
        }
    }
    result.ratio_final = ratio_set ? ratio : 0.0;
    result.total = (result.fired_p ? 1 : 0) + (result.fired_bp ? 1 : 0)
        + (result.fired_q ? 1 : 0) + (result.fired_bq ? 1 : 0);
    if (result.total == 0) { // offsets 477-483: no clone, no event
        return result;
    }
    if (result.fired_p) {
        --result.orders_after.p;
    }
    if (result.fired_bp) {
        --result.orders_after.bp;
    }
    if (result.fired_q) {
        --result.orders_after.q;
    }
    if (result.fired_bq) {
        --result.orders_after.bq;
    }
    result.spec_changed = true;
    result.info_detail = tramo_final_orders_to_string(result.orders_after);
    return result;
}

} // namespace kalos
