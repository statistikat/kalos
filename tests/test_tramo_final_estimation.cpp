// Item 23-F replay test: drives the kalos::tramo_final_estimation decision
// layer (FinalEstimator.searchur/checkUnitRoots/checkCommonRoots/test of the
// pinned tramo side of the 21-H fork) and the F.2 estimate() round-loop
// driver over the generated golden JVM corpus
// (tests/jvm_tramo_final_estimation.h) and pins the fork-critical hand-built
// edges. All double comparisons are raw IEEE-754 bit equality, the fe_est
// rows included.
#include "kalos/tramo_final_estimation.hpp"

#include "jvm_tramo_final_estimation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

#define CHECK(cond)                                                                            \
    if (!(cond)) {                                                                             \
        throw std::runtime_error("check failed: " #cond " (line " + std::to_string(__LINE__)   \
            + ")");                                                                            \
    }

using namespace kalos;

double bits_to_double(std::uint64_t b) {
    double v;
    std::memcpy(&v, &b, sizeof(double));
    return v;
}

std::vector<double> to_values(const std::uint64_t* bits, int count) {
    std::vector<double> out(count);
    for (int i = 0; i < count; ++i) {
        out[i] = bits_to_double(bits[i]);
    }
    return out;
}

std::uint64_t double_to_bits(double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, sizeof(double));
    return b;
}

std::string hex16(std::uint64_t b) {
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(b));
    return buf;
}

TramoFinalOrders orders_at(const std::uint64_t* slots) {
    return TramoFinalOrders{static_cast<int>(slots[0]), static_cast<int>(slots[1]),
        static_cast<int>(slots[2]), static_cast<int>(slots[3]),
        static_cast<int>(slots[4]), static_cast<int>(slots[5])};
}

bool orders_equal(const TramoFinalOrders& a, const std::uint64_t* slots) {
    return a.p == static_cast<int>(slots[0]) && a.d == static_cast<int>(slots[1])
        && a.q == static_cast<int>(slots[2]) && a.bp == static_cast<int>(slots[3])
        && a.bd == static_cast<int>(slots[4]) && a.bq == static_cast<int>(slots[5]);
}

void replay_sr(const golden_tfe::Row& r) {
    const double ur = bits_to_double(r.in[0]);
    const int n = (r.in_n - 1) / 2;
    CHECK(n == r.exp_n);
    for (int i = 0; i < n; ++i) {
        const std::vector<TramoFinalComplex> roots{
            TramoFinalComplex{bits_to_double(r.in[1 + 2 * i]),
                bits_to_double(r.in[2 + 2 * i])}};
        CHECK(tramo_searchur_roots(roots, ur) == static_cast<int>(r.exp[i]));
    }
}

void replay_mr(const golden_tfe::Row& r) {
    // The probe builds Polynomial.valueOf(1.0, block) ([1 | coefficients])
    // before mirror().roots(); the golden carries the block.
    std::vector<double> coefficients{1.0};
    const std::vector<double> block = to_values(r.in, r.in_n);
    coefficients.insert(coefficients.end(), block.begin(), block.end());
    const std::vector<TramoFinalComplex> roots = tramo_mirror_roots(coefficients);
    const int k = static_cast<int>(r.exp[0]);
    if (static_cast<int>(roots.size()) != k) {
        throw std::runtime_error(std::string("root count diverges for ") + r.id
            + ": native " + std::to_string(roots.size()) + " jvm " + std::to_string(k));
    }
    for (int i = 0; i < k; ++i) {
        std::uint64_t re, im;
        std::memcpy(&re, &roots[i].re, 8);
        std::memcpy(&im, &roots[i].im, 8);
        if (re != r.exp[1 + 2 * i] || im != r.exp[2 + 2 * i]) {
            throw std::runtime_error(
                std::string("root bits diverge for ") + r.id + " root " + std::to_string(i));
        }
    }
}

void replay_ru(const golden_tfe::Row& r) {
    TramoUnitRootsRequest request;
    int k = 0;
    request.ur = bits_to_double(r.in[k++]);
    request.orders = orders_at(r.in + k);
    k += 6;
    const int np = static_cast<int>(r.in[k++]);
    request.phi = to_values(r.in + k, np);
    k += np;
    const int nbp = static_cast<int>(r.in[k++]);
    request.bphi = to_values(r.in + k, nbp);
    k += nbp;
    CHECK(k == r.in_n);
    const TramoUnitRootsResult result = tramo_check_unit_roots(request);
    CHECK((result.unchanged ? 1 : 0) == static_cast<int>(r.exp[0]));
    CHECK(orders_equal(result.orders_after, r.exp + 1));
    // exp[7] carries the JVM's post-promotion parameter count; the decision
    // layer never fabricates the specification-default landing (the F.2
    // driver owns setSpecification), so the count is pinned as an audit row.
    CHECK(static_cast<int>(r.exp[7])
        == result.orders_after.p + result.orders_after.bp + result.orders_after.q
            + result.orders_after.bq);
}

void replay_cur(const golden_tfe::Row& r) {
    TramoCommonRootsRequest request;
    int k = 0;
    request.cancel = bits_to_double(r.in[k++]);
    request.orders = orders_at(r.in + k);
    k += 6;
    for (int b = 0; b < 4; ++b) {
        const int n = static_cast<int>(r.in[k++]);
        std::vector<double> block = to_values(r.in + k, n);
        k += n;
        switch (b) {
        case 0:
            request.phi = std::move(block);
            break;
        case 1:
            request.bphi = std::move(block);
            break;
        case 2:
            request.theta = std::move(block);
            break;
        default:
            request.btheta = std::move(block);
            break;
        }
    }
    CHECK(k == r.in_n);
    const TramoCommonRootsResult result = tramo_check_common_roots(request);
    CHECK((result.unchanged ? 1 : 0) == static_cast<int>(r.exp[0]));
    CHECK(orders_equal(result.orders_after, r.exp + 1));
    CHECK(!result.cancelled == (result.unchanged));
}

void replay_tc(const golden_tfe::Row& r) {
    TramoFinalEstimatorTestRequest request;
    int k = 0;
    request.tsig = bits_to_double(r.in[k++]);
    request.sample_length = static_cast<int>(r.in[k++]);
    request.orders = orders_at(r.in + k);
    k += 6;
    const int npar = static_cast<int>(r.in[k++]);
    request.parameters = to_values(r.in + k, npar);
    k += npar;
    const int nvd = static_cast<int>(r.in[k++]);
    request.asy_covariance_diagonal = to_values(r.in + k, nvd);
    k += nvd;
    CHECK(k == r.in_n);
    const TramoFinalEstimatorTestResult result = tramo_final_estimator_test(request);
    CHECK(result.total == static_cast<int>(r.exp[0]));
    CHECK(orders_equal(result.orders_after, r.exp + 1));
    CHECK(result.spec_changed == (result.total > 0));
    CHECK(result.info_detail == r.text);
}

// The live estimate() driver (F.2): replay the round loop and diff every
// observed fact against the capture, raw bit equality per exp slot.
void replay_est(const golden_tfe::Row& r) {
    int k = 0;
    TramoFinalEstimatorRunRequest req;
    req.n = static_cast<int>(r.in[k++]);
    req.mean = r.in[k++] != 0;
    req.eps = bits_to_double(r.in[k++]);
    req.ur = bits_to_double(r.in[k++]);
    req.pass = static_cast<int>(r.in[k++]);
    req.ami = r.in[k++] != 0;
    req.outliers = r.in[k++] != 0;
    req.airline = static_cast<int>(r.in[k]) < 0;
    if (!req.airline) {
        req.orders = orders_at(r.in + k);
    }
    k += 6;
    req.family = static_cast<TramoEstFamily>(static_cast<int>(r.in[k++]));
    CHECK(k == r.in_n);

    const TramoFinalEstimatorRunResult res = tramo_final_estimator_run(req);

    std::vector<std::pair<std::string, std::uint64_t>> got;
    auto push = [&got](const std::string& what, std::uint64_t bits) {
        got.emplace_back(what, bits);
    };
    push("return", res.returned ? 1 : 0);
    push("nnsig", static_cast<std::uint64_t>(res.nnsig));
    push("mean_out", res.mean_out ? 1 : 0);
    const int o6[6] = {res.orders_after.p, res.orders_after.d, res.orders_after.q,
        res.orders_after.bp, res.orders_after.bd, res.orders_after.bq};
    for (int i = 0; i < 6; ++i) {
        push("order[" + std::to_string(i) + "]", static_cast<std::uint64_t>(o6[i]));
    }
    push("np", res.parameters_after.size());
    for (std::size_t i = 0; i < res.parameters_after.size(); ++i) {
        push("par[" + std::to_string(i) + "]", double_to_bits(res.parameters_after[i]));
    }
    push("state", static_cast<std::uint64_t>(res.cl_state));
    if (res.cl_state == 0) {
        push("cl_dim", 0);
        push("ssq", 0);
        push("nc", 0);
    } else {
        push("cl_dim", static_cast<std::uint64_t>(res.cl_dim));
        push("ssq", double_to_bits(res.cl_ssq));
        push("nc", res.cl_coefs.size());
        for (std::size_t i = 0; i < res.cl_coefs.size(); ++i) {
            push("coef[" + std::to_string(i) + "]", double_to_bits(res.cl_coefs[i]));
        }
    }
    const int nv = res.cl_state == 1 ? static_cast<int>(res.covariance_diagonal.size()) : 0;
    push("nv", static_cast<std::uint64_t>(nv));
    for (int i = 0; i < nv; ++i) {
        push("vd[" + std::to_string(i) + "]", double_to_bits(res.covariance_diagonal[i]));
    }
    push("nev", res.events.size());
    for (std::size_t i = 0; i < res.events.size(); ++i) {
        push("ekind[" + std::to_string(i) + "]", static_cast<std::uint64_t>(res.events[i].first));
        push("edepth[" + std::to_string(i) + "]",
            static_cast<std::uint64_t>(res.events[i].second));
    }

    CHECK(got.size() == static_cast<std::size_t>(r.exp_n));
    const std::size_t n = std::min(got.size(), static_cast<std::size_t>(r.exp_n));
    for (std::size_t i = 0; i < n; ++i) {
        if (got[i].second != r.exp[i]) {
            throw std::runtime_error(std::string("fe_est ") + r.id + " slot "
                + std::to_string(i) + " " + got[i].first + " native "
                + hex16(got[i].second) + " jvm " + hex16(r.exp[i]) + " (line "
                + std::to_string(__LINE__) + ")");
        }
    }
}

void replay_all() {
    int count = 0;
    const golden_tfe::Row* rows = golden_tfe::table(count);
    CHECK(count == 75);
    int est = 0;
    for (int i = 0; i < count; ++i) {
        const golden_tfe::Row& r = rows[i];
        switch (r.kind) {
        case golden_tfe::TFE_SR:
            replay_sr(r);
            break;
        case golden_tfe::TFE_MR:
            replay_mr(r);
            break;
        case golden_tfe::TFE_RU:
            replay_ru(r);
            break;
        case golden_tfe::TFE_CUR:
            replay_cur(r);
            break;
        case golden_tfe::TFE_TC:
            replay_tc(r);
            break;
        case golden_tfe::TFE_EST:
            replay_est(r);
            ++est;
            break;
        }
    }
    CHECK(est == 10);
}

// Hand-built fork edges beyond the corpus: each one differs from the 21-H
// x13 side of the fork or pins a boundary the corpus reaches only through
// estimation.
void fork_edges() {
    // toString byte shape (the "arima spec changed" detail).
    CHECK(tramo_final_orders_to_string(TramoFinalOrders{0, 1, 1, 1, 1, 1})
        == "(0,1,1)(1,1,1)");

    // Ratio start 99999.0 and the takeover gate (x13 starts at 999): a fired
    // P with t = 100 BLOCKS a costlier Bp (t = 500 requires ratio > t);
    // total = 1, p decremented.
    TramoFinalEstimatorTestRequest r;
    r.orders = TramoFinalOrders{1, 1, 0, 1, 1, 0};
    r.parameters = {0.1, 0.05};               // both below sig2 = 0.15
    r.asy_covariance_diagonal = {1e-6, 1e-8}; // t_P = 100, t_Bp = 500
    r.sample_length = 96;
    r.tsig = 1e9;
    TramoFinalEstimatorTestResult fired = tramo_final_estimator_test(r);
    CHECK(fired.total == 1 && fired.fired_p && !fired.fired_bp);
    CHECK(fired.orders_after.p == 0 && fired.orders_after.bp == 1);
    CHECK(fired.ratio_final == 100.0);
    CHECK(fired.info_detail == "(0,1,0)(1,1,0)");

    // A cheaper Bp (t below the incumbent ratio) takes P over: cleared P,
    // bp decremented, ratio moved.
    r.asy_covariance_diagonal = {1e-6, 1e-4}; // t_P = 100, t_Bp = 5
    fired = tramo_final_estimator_test(r);
    CHECK(fired.total == 1 && fired.fired_bp && !fired.fired_p);
    CHECK(fired.orders_after.p == 1 && fired.orders_after.bp == 0);
    CHECK(fired.ratio_final == 5.0);
    CHECK(fired.info_detail == "(1,1,0)(0,1,0)");

    // Bq fires over Q (ratio 100 > 50) but does NOT update the ratio: the
    // audit ratio stays at the Q value (bytecode offsets 452-462, x13 fork
    // point; a ratio-updating Bq is unobservable here, so the audit field
    // pins it directly).
    r.orders = TramoFinalOrders{0, 1, 1, 0, 1, 1};
    r.parameters = {0.1, 0.05};               // theta, btheta
    r.asy_covariance_diagonal = {1e-6, 2.5e-5}; // t_Q = 100, t_Bq = 10
    fired = tramo_final_estimator_test(r);
    CHECK(fired.total == 1 && fired.fired_bq && !fired.fired_q);
    CHECK(fired.orders_after.q == 1 && fired.orders_after.bq == 0);
    CHECK(fired.ratio_final == 100.0);

    // NaN and strict-boundary polarities.
    r.orders = TramoFinalOrders{1, 1, 0, 0, 1, 0};
    r.parameters = {0.1, 0.05};
    r.asy_covariance_diagonal = {std::numeric_limits<double>::quiet_NaN(), 1e-4};
    fired = tramo_final_estimator_test(r);
    CHECK(fired.total == 0); // NaN variance skips P; no Bp block (bp = 0)

    TramoUnitRootsRequest ur;
    ur.orders = TramoFinalOrders{1, 1, 0, 0, 1, 0};
    ur.phi = {std::numeric_limits<double>::quiet_NaN()};
    ur.ur = 0.9;
    CHECK(tramo_check_unit_roots(ur).unchanged); // NaN root never counts
    ur.ur = std::numeric_limits<double>::quiet_NaN();
    ur.phi = {-0.999};
    CHECK(tramo_check_unit_roots(ur).unchanged); // NaN threshold excludes

    // Cancel boundary is strict: |delta| == cancel does not cancel, one ulp
    // below does.
    const double cancel = 0.044;
    const double just_under = std::nextafter(cancel, 0.0);
    TramoCommonRootsRequest cr;
    cr.orders = TramoFinalOrders{0, 1, 0, 1, 1, 1};
    cr.bphi = {0.0};
    cr.cancel = cancel;
    cr.btheta = {cancel};
    CHECK(tramo_check_common_roots(cr).unchanged);
    cr.btheta = {just_under};
    CHECK(!tramo_check_common_roots(cr).unchanged);
    cr.btheta[0] = std::numeric_limits<double>::quiet_NaN();
    CHECK(tramo_check_common_roots(cr).unchanged); // NaN never cancels

    // Static-input validation fails explicitly.
    bool threw = false;
    try {
        TramoFinalEstimatorTestRequest bad;
        bad.orders = TramoFinalOrders{1, 1, 0, 0, 1, 0};
        bad.parameters = {};
        bad.asy_covariance_diagonal = {1.0};
        tramo_final_estimator_test(bad);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    threw = false;
    try {
        TramoUnitRootsRequest bad;
        bad.orders = TramoFinalOrders{-1, 1, 0, 0, 1, 0};
        tramo_check_unit_roots(bad);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);

    // Read-site validation: the phi block is required only when the scan
    // gate (p > 0 && d < 2) opens.
    {
        TramoUnitRootsRequest gated_off;
        gated_off.orders = TramoFinalOrders{1, 2, 0, 0, 1, 0};
        CHECK(tramo_check_unit_roots(gated_off).unchanged); // no throw, no scan
        bool threw_open = false;
        try {
            TramoUnitRootsRequest open_gate;
            open_gate.orders = TramoFinalOrders{1, 1, 0, 0, 1, 0};
            tramo_check_unit_roots(open_gate);
        } catch (const std::invalid_argument&) {
            threw_open = true;
        }
        CHECK(threw_open);
    }
}

} // namespace

int main() {
    try {
        replay_all();
        fork_edges();
        std::cout << "test_tramo_final_estimation: OK (75 golden rows + fork edges)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_tramo_final_estimation: FAILED: " << error.what() << "\n";
        return 1;
    }
}
