// Item 23-B: the round-0 TRAMO outlier scan of the pinned jdplus 3.9.0
// stack (transcription map: docs/contracts/tramo-outlier-scan-contract.md).
//
// Machine order of a legal request: the OutliersDetectionModule.make size
// gate (comatip with TramoUtility.autlar), the ModelDescription pre-stage
// (AverageInterpolator.cleanMissings with the retained missing positions,
// the log transformation), the differenced RegArmaModel (response through
// the pinned differencing backfilter, design = interpolated point columns,
// the constant column when the mean is estimated, the pre-registered
// indicator columns), FastOutliersDetector.estimateModel at round 0 (Ols
//Computer residuals, HannanRissanen start with stabilization and the round-0
// installation, the warm maximum-likelihood seam when the tail demands it),
// the FastOutlierDetector initmodel (DEFAULT_COMPUTER generalized least
// squares on the whitened design, the stationary-model Kalman innovations,
// Mad2(50)), the processOutlier T/coefficient tables over the pi * filter
// weight columns, the searchMax sweep and the |maxT| >= cv gate.
#include "kalos/tramo_outlier_scan.hpp"

#include "kalos/detail/fdlibm.hpp"
#include "kalos/detail/ssq_deviances.hpp"
#include "kalos/estimation_control.hpp"
#include "kalos/hannan_rissanen.hpp"
#include "kalos/outlier_scoring.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kalos {
namespace {

constexpr int kPeriod = 12;

double qnan() { return std::numeric_limits<double>::quiet_NaN(); }

// AverageInterpolator.cleanMissings: backward scan; a finite value closing a
// gap (left != -1) fills left + 1..right - 1 with (data[right] +
// data[left]) / 2 (data[left] alone when right >= n), where right is the
// first finite index right of the gap; every non-finite index is recorded
// (descending). The trailing branch fills a leading gap 0..right - 1 with
// data[right] and reports false only when no finite value exists at all.
bool clean_missings(std::vector<double>& data, std::vector<int>& missing_at) {
    const int n = static_cast<int>(data.size());
    int left = -1;
    int right = -1;
    for (int i = n - 1; i >= 0; --i) {
        const double v = data[static_cast<std::size_t>(i)];
        if (!std::isfinite(v)) {
            if (right == -1) {
                right = i + 1;
            }
            left = i - 1;
            missing_at.push_back(i);
        } else if (left != -1) {
            const double avg = right >= n
                ? data[static_cast<std::size_t>(left)]
                : (data[static_cast<std::size_t>(right)]
                      + data[static_cast<std::size_t>(left)]) / 2.0;
            for (int j = left + 1; j < right; ++j) {
                data[static_cast<std::size_t>(j)] = avg;
            }
            right = -1;
            left = -1;
        }
    }
    if (right > 0) {
        if (right >= n) {
            return false;
        }
        for (int j = 0; j < right; ++j) {
            data[static_cast<std::size_t>(j)] = data[static_cast<std::size_t>(right)];
        }
    }
    return true;
}

struct ScanOrders {
    int p, d, q, bp, bd, bq;
};

// TramoUtility.autlar(n, orders): the pinned default AR order heuristics
// (note the JVM integer log argument n2 * n2 and the integer divisions).
int tramo_autlar(int n, const ScanOrders& o) {
    const int d_total = o.d + kPeriod * o.bd;
    const int q_total = o.q + kPeriod * o.bq;
    const int p_total = o.p + kPeriod * o.bp;
    const int n2 = n - d_total;
    int k = static_cast<int>(std::log(static_cast<double>(n2 * n2)));
    const int kmax = std::max(2 * q_total, k);
    if (kmax > k) {
        k = kmax;
    }
    if (!(k < n2)) {
        k = n2 - n2 / 4;
    }
    if (k > 50) {
        k = 50;
    }
    const int tmp = o.p + o.bp * (1 + o.p) + o.q + o.bq * (1 + o.q);
    return (n2 - k) - std::max(p_total, q_total) - tmp;
}

// OutliersDetectionModule.comatip over the finite count of the raw series:
// -1 rejects the series (make hands back the null driver, module Failed);
// 0 keeps the estimation as requested; 1 forces the maximum-likelihood
// flag of the FastOutliersDetector builder.
int tramo_comatip(int n_finite, const ScanOrders& o, bool mean, bool ml) {
    int need = std::max(o.d + o.p + kPeriod * (o.bd + o.bp), o.q + kPeriod * o.bq);
    need += mean ? 1 : 0;
    need += (15 * n_finite) / 100;
    need += kPeriod;
    if (!(n_finite - need > 0)) {
        return -1;
    }
    if (ml) {
        return 1;
    }
    const int v5 = tramo_autlar(n_finite, o);
    int v6 = 0;
    if (o.p + o.bp > 0 && o.q + o.bq > 0) {
        ScanOrders reduced = o;
        reduced.p = 0;
        reduced.bp = 0;
        v6 = tramo_autlar(n_finite - (o.p + kPeriod * o.bp), reduced);
    }
    return v5 < 0 || v6 < 0 ? 1 : 0;
}

// RobustStandardDeviationComputer.Mad2(50).compute: sort (JVM Double.compare
// order: NaN last), the plain median, the dcmpg-signed absolute deviations
// (a NaN deviation keeps the "x - med" branch), re-sort and the (n + 1) -
// based centile at 50 divided by Phi^-1(0.75) on the pinned quantile path.
double mad2_default(const std::vector<double>& x) {
    const std::size_t n = x.size();
    std::vector<double> a = x;
    std::sort(a.begin(), a.end());
    const std::size_t half = n / 2;
    const double med = 2 * half == n ? (a[half - 1] + a[half]) / 2.0 : a[half];
    for (double& v : a) {
        v = std::isless(v, med) ? med - v : v - med;
    }
    std::sort(a.begin(), a.end());
    const double q = static_cast<double>(n + 1) * 50.0 / 100.0;
    const int idx = static_cast<int>(q);
    const double f = q - static_cast<double>(idx);
    const double v = f < 1e-9 ? a[static_cast<std::size_t>(idx) - 1]
                              : a[static_cast<std::size_t>(idx) - 1] * (1.0 - f)
                                    + a[static_cast<std::size_t>(idx)] * f;
    return v / standard_normal_lower_quantile(0.5 + 0.005 * 50.0);
}

// LinearModel.calcResiduals at lm.mean = false (the scan machine always
// carries an explicit constant column when it carries a constant): a copy of
// the response, then per design column in ascending order the whole column
// with -b(k) (DataBlock.addAY over the shared result block).
std::vector<double> calc_residuals(const std::vector<double>& y,
    const std::vector<std::vector<double>>& xcols, const std::vector<double>& b) {
    std::vector<double> res = y;
    for (std::size_t k = 0; k < xcols.size(); ++k) {
        const double coef = -b[k];
        for (std::size_t r = 0; r < res.size(); ++r) {
            res[r] += coef * xcols[k][r];
        }
    }
    return res;
}

// SarimaModel.getRegularAR / getSeasonalAR raw polynomials at JVM-sign
// coordinates (trailing 1e-9 trimming cannot change a power-series quotient,
// so the untrimmed convolution is used directly).
std::vector<double> raw_regular(const std::vector<double>& phi) {
    std::vector<double> p(phi.size() + 1, 0.0);
    p[0] = 1.0;
    for (std::size_t i = 0; i < phi.size(); ++i) {
        p[i + 1] = phi[i];
    }
    return p;
}

std::vector<double> raw_seasonal(const std::vector<double>& bphi) {
    std::vector<double> p = {1.0};
    for (const double c : bphi) {
        std::vector<double> factor(static_cast<std::size_t>(kPeriod) + 1, 0.0);
        factor[0] = 1.0;
        factor[static_cast<std::size_t>(kPeriod)] = c;
        p = detail::pinned_polynomial_times(p, factor);
    }
    return p;
}

// HannanRissanenStart blocks [ar, sar, ma, sma] (Kalos convention) to the
// JVM coordinates [phi, bphi, theta, btheta] of the pinned machines.
std::vector<double> hr_to_jvm(const std::array<std::vector<double>, 4>& c) {
    std::vector<double> jvm;
    for (const double v : c[0]) jvm.push_back(-v);
    for (const double v : c[1]) jvm.push_back(-v);
    for (const double v : c[2]) jvm.push_back(v);
    for (const double v : c[3]) jvm.push_back(v);
    return jvm;
}

// Raw (pre-differencing) column of a pre-registered variable: AO is the
// unit spike, LS the pinned zero-ended step and TC the repeated-multiply
// decay c^(t - pos) (the probe-pinned floating-point pattern of the
// convolved column requires the repeated multiplication, not pow).
std::vector<double> prespec_raw(int kind, double tc_rate, int pos, std::size_t n) {
    std::vector<double> col(n, 0.0);
    if (kind == 0) { // AO
        col[static_cast<std::size_t>(pos)] = 1.0;
    } else if (kind == 1) { // LS
        for (std::size_t t = static_cast<std::size_t>(pos); t < n; ++t) {
            col[t] = 1.0;
        }
    } else { // TC
        double v = 1.0;
        for (std::size_t t = static_cast<std::size_t>(pos); t < n; ++t) {
            col[t] = v;
            v *= tc_rate;
        }
    }
    return col;
}

struct FactoryRep {
    int kind;
    int zone_start;
    int zone_end;
    std::vector<double> filter_den; // numerator is 1 for every rep
    double correction;              // only live at differencing degree 0
};

} // namespace

TramoOutlierScanDecision scan_tramo_outliers(const TramoOutlierScanRequest& request) {
    if (request.frequency != kPeriod) {
        throw std::invalid_argument(
            "scan_tramo_outliers: monthly data only (register D11)");
    }
    const int p = request.p, d = request.d, q = request.q;
    const int bp = request.bp, bd = request.bd, bq = request.bq;
    if (p < 0 || d < 0 || q < 0 || bp < 0 || bd < 0 || bq < 0) {
        throw std::invalid_argument("scan_tramo_outliers: negative orders are rejected");
    }
    if (d + bd == 0) {
        throw std::invalid_argument(
            "scan_tramo_outliers: a positive differencing order is required");
    }
    if (request.y.empty()) {
        throw std::invalid_argument("scan_tramo_outliers: an empty series is rejected");
    }
    if (request.tc_rate <= 0.0 || !(request.tc_rate < 1.0) || !std::isfinite(request.tc_rate)) {
        throw std::invalid_argument("scan_tramo_outliers: the tc rate must lie in (0, 1)");
    }
    if (request.lower_bound > request.upper_bound) {
        throw std::invalid_argument(
            "scan_tramo_outliers: resolved span bounds must be ordered");
    }
    for (const auto& pre : request.prespec) {
        if (pre.second == TramoOutlierKind::So) {
            throw std::invalid_argument("scan_tramo_outliers: a pre-registered SO is "
                                        "outside the transcribed pinned surface");
        }
        const int kind = static_cast<int>(pre.second);
        if (kind < 0 || kind > 3 || pre.first < 0) {
            throw std::invalid_argument("scan_tramo_outliers: unknown prespec variable");
        }
    }
    if (!request.interpolate) {
        for (const double v : request.y) {
            if (!std::isfinite(v)) {
                throw std::invalid_argument("scan_tramo_outliers: missing observations "
                                            "require interpolation");
            }
        }
    }

    TramoOutlierScanDecision decision;
    decision.status = TramoOutlierScanDecision::Status::Unchanged;
    decision.position = -1;
    decision.type = -1;
    decision.max_t = qnan();
    decision.coefficient = qnan();
    decision.factories = 0;
    decision.lower_bound = request.lower_bound;
    decision.upper_bound = request.upper_bound;
    decision.critical_value = request.cv;

    // Registered factories in the pinned order (echoed even for a rejected
    // series: the module would build the same mix).
    std::vector<FactoryRep> factories;
    if (request.ao) {
        factories.push_back({0, 0, 0, {1.0}, 0.0});
    }
    if (request.ls) {
        factories.push_back({1, 1, 1, {1.0, -1.0}, -1.0});
    }
    if (request.tc) {
        factories.push_back({2, 0, 1, {1.0, -request.tc_rate}, 0.0});
    }
    if (request.so) {
        std::vector<double> den(static_cast<std::size_t>(kPeriod) + 1, 0.0);
        den[0] = 1.0;
        den[static_cast<std::size_t>(kPeriod)] = -1.0;
        factories.push_back({3, 2 * kPeriod, 2 * kPeriod, std::move(den), 0.0});
    }
    decision.factories = static_cast<int>(factories.size());

    const std::size_t n = request.y.size();

    // make(): the comatip gate over the finite count of the raw series; a
    // positive verdict also forces the maximum-likelihood flag.
    int n_finite = 0;
    for (const double v : request.y) {
        if (std::isfinite(v)) {
            ++n_finite;
        }
    }
    const ScanOrders orders{p, d, q, bp, bd, bq};
    const int matip = tramo_comatip(n_finite, orders, request.mean,
        request.maximum_likelihood);
    if (matip < 0) {
        decision.status = TramoOutlierScanDecision::Status::Failed;
        return decision;
    }
    const bool mvx = matip > 0;

    // calc() switches the scan off on an empty mix or collapsed bounds (the
    // module then reports the empty selection: Unchanged).
    if (factories.empty() || request.upper_bound <= request.lower_bound) {
        return decision; // Unchanged
    }

    // The pre-stage: AverageInterpolator.cleanMissings (the positions are
    // retained as the model missings) and the log transformation.
    std::vector<double> z = request.y;
    std::vector<int> missing_at;
    {
        bool any_missing = false;
        for (const double v : z) {
            if (!std::isfinite(v)) {
                any_missing = true;
                break;
            }
        }
        if (any_missing) {
            if (!clean_missings(z, missing_at)) {
                decision.status = TramoOutlierScanDecision::Status::Failed;
                return decision;
            }
            std::sort(missing_at.begin(), missing_at.end()); // JVM IntList: descending
        }
    }
    if (request.log) {
        for (const double v : z) {
            if (!(v > 0.0)) {
                throw std::invalid_argument(
                    "scan_tramo_outliers: the log requires strictly positive observations");
            }
        }
        for (double& v : z) {
            v = detail::fdlibm_log(v);
        }
    }

    // The differenced model: response and design through the pinned
    // differencing; design = missing-point columns, the constant when the
    // mean is estimated, the pre-registered variable columns (in-domain
    // only; an out-of-domain variable is tagged excluded and never enters).
    const std::vector<double> kernel =
        detail::pinned_differencing_polynomial(kPeriod, d, bd);
    const std::size_t deg = kernel.size() - 1;
    const std::vector<double> dly = detail::pinned_backfilter_apply(kernel, z);
    if (dly.empty()) {
        decision.status = TramoOutlierScanDecision::Status::Failed;
        return decision;
    }
    std::vector<std::vector<double>> xcols;
    for (const int m : missing_at) {
        std::vector<double> col(z.size(), 0.0);
        col[static_cast<std::size_t>(m)] = 1.0;
        xcols.push_back(detail::pinned_backfilter_apply(kernel, col));
    }
    if (request.mean) {
        xcols.push_back(std::vector<double>(dly.size(), 1.0));
    }
    std::vector<std::pair<std::size_t, int>> prespec_cells; // (row, factory column)
    for (const auto& pre : request.prespec) {
        const int kind = static_cast<int>(pre.second);
        if (static_cast<std::size_t>(pre.first) >= n) {
            continue; // excluded by ModelDescription; the module exclude no-ops
        }
        xcols.push_back(detail::pinned_backfilter_apply(
            kernel, prespec_raw(kind, request.tc_rate, pre.first, n)));
        const int row = pre.first;
        int col = -1;
        for (std::size_t o = 0; o < factories.size(); ++o) {
            if (factories[o].kind == kind) {
                col = static_cast<int>(o);
                break;
            }
        }
        if (col >= 0) { // outlierType < 0 (factory missing) leaves exclude no-op
            prespec_cells.emplace_back(static_cast<std::size_t>(row), col);
        }
    }

    // estimateModel at round 0: the OlsComputer residuals (a null result is
    // "Ols failed"), then the HannanRissanen start with stabilization; a
    // fitted or zero-variance start is installed (round 0 always installs)
    // and a stabilizer rewrite (or the maximum-likelihood flag) sends the
    // tail through the warm re-estimation seam.
    std::vector<double> res = dly;
    if (!xcols.empty()) {
        const auto ols = detail::pinned_ols_coefficients(dly, xcols);
        if (!ols) {
            decision.status = TramoOutlierScanDecision::Status::Failed;
            return decision;
        }
        res = calc_residuals(dly, xcols, *ols);
    }

    // The SarimaSpec builder fill at JVM coordinates (phi -0.1, theta and
    // btheta -0.2), the model estimateModel keeps when HannanRissanen fails.
    std::vector<double> jvm;
    for (int i = 0; i < p + bp; ++i) jvm.push_back(-0.1);
    for (int i = 0; i < q + bq; ++i) jvm.push_back(-0.2);

    bool need_optimize = mvx;
    if (p + bp + q + bq > 0) {
        const HannanRissanenStart hr = hannan_rissanen_start(
            res, RegressionDesign{0, {}, {}, {}},
            HannanRissanenOrders{p, bp, q, bq, kPeriod});
        if (hr.source != HannanRissanenStart::Source::Defaulted) {
            // hr.coefficients is the stabilized landing (the JVM installs
            // SarimaMapping.stabilize(hr.getModel())); the landing itself may
            // sit outside the stability region, in which case the stabilize
            // was not the identity (flag6 = 0) and the detector tail runs the
            // warm Levenberg-Marquardt optimizeModel from the installed model.
            jvm = hr_to_jvm(hr.coefficients);
            if (hr.stabilized) {
                need_optimize = true;
            }
        }
    }
    if (need_optimize) {
        // RegArimaUtility.processor(true, eps).optimize: the qualified warm
        // seam covers a design-free or constant-column design only (all
        // corpus cases on this seam).
        const bool qualified = xcols.empty()
            || (xcols.size() == 1 && missing_at.empty() && request.mean
                && request.prespec.empty());
        if (!qualified) {
            throw std::runtime_error("scan_tramo_outliers: warm re-estimation with "
                                     "non-constant designs is outside the qualified seam");
        }
        SarimaEstimationRequest warm;
        warm.series.values = z;
        warm.series.missing.assign(z.size(), 0);
        if (request.mean) {
            warm.design.columns = 1;
            warm.design.values.assign(z.size(), 1.0);
            warm.design.coefficients = {0.0};
            warm.design.fixed = {0};
        }
        warm.blocks[0].initial.assign(static_cast<std::size_t>(p), 0.0);
        warm.blocks[0].fixed.assign(static_cast<std::size_t>(p), 0);
        warm.blocks[1].initial.assign(static_cast<std::size_t>(q), 0.0);
        warm.blocks[1].fixed.assign(static_cast<std::size_t>(q), 0);
        warm.blocks[2].initial.assign(static_cast<std::size_t>(bp), 0.0);
        warm.blocks[2].fixed.assign(static_cast<std::size_t>(bp), 0);
        warm.blocks[3].initial.assign(static_cast<std::size_t>(bq), 0.0);
        warm.blocks[3].fixed.assign(static_cast<std::size_t>(bq), 0);
        warm.period = kPeriod;
        warm.d = d;
        warm.seasonal_d = bd;
        warm.control = EstimationControl{request.precision, 1e-4, true, false, true, true,
            EstimationStartingPoint::HannanRissanen,
            EstimationSolver::PinnedLevenbergMarquardt};
        std::array<std::vector<double>, 4> start;
        {
            const auto at = [&](std::size_t o) {
                return jvm[static_cast<std::size_t>(o)];
            };
            for (int i = 0; i < p; ++i) start[0].push_back(-at(i));
            for (int i = 0; i < bp; ++i) start[2].push_back(-at(p + i));
            for (int i = 0; i < q; ++i) start[1].push_back(at(p + bp + i));
            for (int i = 0; i < bq; ++i) start[3].push_back(at(p + bp + q + i));
        }
        const SarimaEstimationResult est = optimize_sarima_from(warm, start);
        const auto& c = est.coefficients; // [phi, theta, bphi, btheta], Kalos
        for (const auto& block : c) {
            for (const double value : block) {
                if (!std::isfinite(value)) {
                    decision.status = TramoOutlierScanDecision::Status::Failed;
                    return decision;
                }
            }
        }
        jvm.clear();
        for (const double v : c[0]) jvm.push_back(-v);
        for (const double v : c[2]) jvm.push_back(-v);
        for (const double v : c[1]) jvm.push_back(v);
        for (const double v : c[3]) jvm.push_back(v);
    }

    // FastOutlierDetector.initmodel: the DEFAULT_COMPUTER regression over
    // the whitened design (a rank failure is "Gls failed"), the regression
    // residuals filtered through the stationary model (the design-free
    // Kalman machine when allCoefficients is empty) and the Mad2 scale.
    const std::size_t p_sz = static_cast<std::size_t>(p);
    const std::size_t bp_sz = static_cast<std::size_t>(bp);
    const std::size_t q_sz = static_cast<std::size_t>(q);
    const std::size_t bq_sz = static_cast<std::size_t>(bq);
    std::vector<double> el;
    if (!xcols.empty()) {
        const auto gls = detail::pinned_gls_coefficients(
            dly, p_sz, bp_sz, q_sz, bq_sz, kPeriod, xcols, jvm);
        if (!gls) {
            decision.status = TramoOutlierScanDecision::Status::Failed;
            return decision;
        }
        const std::vector<double> wr = calc_residuals(dly, xcols, *gls);
        const auto innovations = detail::pinned_fast_kalman_residuals(
            wr, p_sz, bp_sz, q_sz, bq_sz, kPeriod, jvm);
        if (!innovations) {
            decision.status = TramoOutlierScanDecision::Status::Failed;
            return decision;
        }
        el = *innovations;
    } else {
        const auto innovations = detail::pinned_kalman_innovations(
            dly, p_sz, bp_sz, q_sz, bq_sz, kPeriod, jvm);
        if (!innovations) {
            decision.status = TramoOutlierScanDecision::Status::Failed;
            return decision;
        }
        el = *innovations;
    }
    const double mad = mad2_default(el);

    // The scan tables: SingleOutlierDetector.prepare rows (the estimation
    // domain length, matching el + differencing degree), zones per factory
    // and the exclusions (missing cells for every factory column, the
    // in-domain pre-registered cells at their own column).
    const std::size_t m = el.size() + deg;
    if (m != n) {
        decision.status = TramoOutlierScanDecision::Status::Failed;
        return decision; // defensive: the pinned prepare uses the domain length
    }
    const std::size_t k = factories.size();
    std::vector<char> allowed(m * k, 0);
    for (std::size_t o = 0; o < k; ++o) {
        const long long zs = factories[o].zone_start;
        const long long ze = factories[o].zone_end;
        for (std::size_t row = 0; row < m; ++row) {
            allowed[row + o * m] = static_cast<long long>(row) >= zs
                && static_cast<long long>(row) < static_cast<long long>(m) - ze;
        }
    }
    for (const int mp : missing_at) {
        for (std::size_t o = 0; o < k; ++o) {
            allowed[static_cast<std::size_t>(mp) + o * m] = 0; // exclude guard holds
        }
    }
    for (const auto& cell : prespec_cells) {
        allowed[cell.first + static_cast<std::size_t>(cell.second) * m] = 0;
    }

    // pi = ar / ma of the full model (the differencing belongs into the
    // numerator, SarimaModel.getAr); processOutlier composes it with the
    // factory filter and reads m weights.
    std::vector<double> phi_jvm(jvm.begin(), jvm.begin() + static_cast<std::ptrdiff_t>(p));
    std::vector<double> bphi_jvm(jvm.begin() + static_cast<std::ptrdiff_t>(p),
        jvm.begin() + static_cast<std::ptrdiff_t>(p + bp));
    std::vector<double> th_jvm(jvm.begin() + static_cast<std::ptrdiff_t>(p + bp),
        jvm.begin() + static_cast<std::ptrdiff_t>(p + bp + q));
    std::vector<double> bth_jvm(jvm.begin() + static_cast<std::ptrdiff_t>(p + bp + q),
        jvm.end());
    std::vector<double> ar_raw = detail::pinned_polynomial_times(
        raw_regular(phi_jvm), raw_seasonal(bphi_jvm));
    ar_raw = detail::pinned_polynomial_times(ar_raw, kernel);
    const std::vector<double> ma_raw = detail::pinned_polynomial_times(
        raw_regular(th_jvm), raw_seasonal(bth_jvm));

    std::vector<double> t_table(m * k, 0.0);
    std::vector<double> coef_table(m * k, 0.0);
    const std::size_t n_el = el.size();
    for (std::size_t o = 0; o < k; ++o) {
        // RationalBackFilter.times(pi, rep.filter): the SimplifyingTool
        // cross-cancels rep.den against the pi numerator through
        // LeastSquaresDivision (simplifyExact) before multiplying. The LS and
        // SO denominators (1 - B, 1 - B^12) divide the unit-root-laden pi
        // numerator exactly, so those columns run on the quotient over the
        // plain MA; the AO constant denominator skips the simplifier and the
        // TC geometric denominator misses its root window (the roots pass
        // tests |pi(1/delta)| < 1e-12, which fails by orders of magnitude),
        // leaving the unsimplified product.
        const std::vector<double>& fden = factories[o].filter_den;
        std::optional<std::vector<double>> quotient;
        if (fden.size() > 1) {
            // The public simplify() runs its private pass on the LONGER
            // polynomial as dividend (swapping the results back afterwards),
            // so a pi numerator shorter than the filter denominator divides
            // INTO it instead, leaving numerator 1 and the quotient in the
            // denominator.
            const bool swapped = fden.size() > ar_raw.size();
            quotient = detail::pinned_ls_divide(swapped ? fden : ar_raw,
                swapped ? ar_raw : fden);
            if (!quotient && !swapped && fden.size() == 2) {
                // Roots pass on a linear denominator: the single root must
                // miss the 1e-12 evaluation window (the TC geometric root
                // 1/c of the pi numerator, off by orders of magnitude).
                const double root = -fden[0] / fden[1];
                double val = 0.0; // real-root Horner (the Complex walk at im = 0)
                for (std::size_t i = ar_raw.size(); i-- > 0;) {
                    val = val * root + ar_raw[i];
                }
                if (std::abs(val) < 1e-12) {
                    throw std::invalid_argument(
                        "scan_tramo_outliers: the simplifier's roots pass "
                        "engaged, which is outside the pinned machine");
                }
            }
            if (!quotient && (swapped || fden.size() > 2)) {
                throw std::invalid_argument(
                    "scan_tramo_outliers: a non-exact non-linear filter "
                    "denominator leaves the pinned simplifier's roots pass");
            }
        }
        std::vector<double> num, den;
        if (!quotient) {
            num = detail::pinned_polynomial_times(ar_raw, {1.0});
            den = detail::pinned_polynomial_times(ma_raw, fden);
        } else if (fden.size() <= ar_raw.size()) {
            num = detail::pinned_polynomial_times(*quotient, {1.0});
            den = ma_raw; // the MA times ONE
        } else { // swapped exact pass: numerator ONE, quotient in the tail
            num = detail::pinned_polynomial_times(std::vector<double>{1.0}, {1.0});
            den = detail::pinned_polynomial_times(ma_raw, *quotient);
        }
        const std::vector<double> w =
            detail::pinned_rational_weights(num, den, m);
        // The rep.correction branch is gated on ur.getDegree() == 0 and the
        // request gate keeps the differencing positive, so the constant
        // offset stays at zero here (dormant by pin, not by omission).
        const double c0 = 0.0;
        double ssq = 0.0; // c0 != 0 would open at c0 * c0 * n_el
        for (std::size_t i = 0; i < m; ++i) {
            ssq += w[i] * w[i];
            if (c0 != 0.0) {
                ssq -= c0 * c0;
            }
            std::size_t j = i + 1;
            if (j > n_el) {
                j = n_el;
                ssq -= w[i - n_el] * w[i - n_el];
                if (c0 != 0.0) {
                    ssq += c0 * c0;
                }
            }
            double num = 0.0;
            for (std::size_t t = 0; t < j; ++t) {
                num += el[n_el - 1 - t] * w[i - t];
            }
            if (c0 != 0.0) {
                double s = 0.0;
                for (std::size_t t = 0; t < n_el - j; ++t) {
                    s += el[t];
                }
                num += s * c0;
            }
            const std::size_t row = m - 1 - i;
            const bool live = allowed[row + o * m]
                && static_cast<long long>(row) >= request.lower_bound
                && static_cast<long long>(row) < request.upper_bound;
            if (live) {
                const double cf = num / ssq;
                coef_table[row + o * m] = cf;
                t_table[row + o * m] = cf * std::sqrt(ssq) / mad;
            } else {
                coef_table[row + o * m] = qnan();
                t_table[row + o * m] = qnan();
            }
        }
    }

    // searchMax: column-major sweep over the raw storage with unit weights,
    // a NaN candidate loses (dcmpl against the running maximum) and a column
    // that found nothing at all aborts the search (the JVM then trips on the
    // (-1, -1) table read: Failed).
    int idx = -1;
    double dmax = 0.0;
    std::size_t storage = 0;
    for (std::size_t o = 0; o < k; ++o) {
        for (std::size_t row = 0; row < m; ++row, ++storage) {
            const double val = std::abs(t_table[storage]) * 1.0;
            if (val > dmax) { // dcmpl ifle: a NaN value never updates
                idx = static_cast<int>(storage);
                dmax = val;
            }
        }
        if (idx == -1) {
            decision.status = TramoOutlierScanDecision::Status::Failed;
            return decision;
        }
    }

    const int pos_max = idx % static_cast<int>(m);
    const int o_max = idx / static_cast<int>(m);
    const double max_t = t_table[static_cast<std::size_t>(idx)];
    // FastOutliersDetector.process gate: Math.abs(maxT) < cv breaks (the
    // dcmpg comparison lets a NaN pass through).
    if (std::isless(std::abs(max_t), request.cv)) {
        return decision; // Unchanged
    }
    decision.status = TramoOutlierScanDecision::Status::Selected;
    decision.position = pos_max;
    decision.type = o_max;
    decision.max_t = max_t;
    decision.coefficient = coef_table[static_cast<std::size_t>(idx)];
    return decision;
}

} // namespace kalos
