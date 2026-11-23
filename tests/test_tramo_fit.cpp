// Item 23-F F.4 golden replay: the TRAMO automatic-fit assembly
// (kalos::tramo_fit) over the 34-case tramo-fit corpus (ctest #79).
// Per case: build the faithful TramoFitRequest from the double-captured
// Scratch23f stream (jvm_tramo_fit.h) and check, live, the whole
// composition: the 23-A builder mirror and the 23-B transform mirror
// (any desync throws out of the unit), the F.3 round driver, the
// per-seam live/fact tiers against the measured envelope, the
// differenced-response recompute, and the D9 covariance recomputation
// V = unscaledCovariance * (ssq / (dim - nx - k)) bit-for-bit against the
// captured coefficientsCovariance upper triangle. Refusal rows check the
// fail-closed status and the empty chain; the d6 and i10 matrix rows have
// no JVM witness (the decomposition knob never reaches the jar side; every
// non-unused autoMethod resolves with tdAuto = 1) and are pinned with
// synthetic request mutations at the end.
// docs/contracts/tramo-fit-contract.md; pinning SECTION 32.

#include <kalos/tramo_fit.hpp>

#include "jvm_tramo_fit.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

#define CHECK(cond)                                                        \
    if (!(cond)) {                                                         \
        throw std::runtime_error("check failed: " #cond " (line " +        \
                                 std::to_string(__LINE__) + ")");          \
    }

using namespace kalos;

std::uint64_t to_bits(double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, sizeof(double));
    return b;
}

std::string orders_word(const TramoOrders& o) {
    return std::to_string(o.p) + "," + std::to_string(o.d) + "," +
           std::to_string(o.q) + "," + std::to_string(o.bp) + "," +
           std::to_string(o.bd) + "," + std::to_string(o.bq);
}

long long run_case(const FitExpect& e, std::size_t& live_total,
                   std::size_t& fact_total, std::size_t& d9_total) {
    TramoFitRequest rq;
    build_fit(e.id, rq);
    const TramoFitResult r = tramo_fit(rq);

    if (e.refuses[0] != '\0') {
        CHECK(r.status == e.refuses);
        CHECK(r.seam_checks.empty());
        CHECK(r.kernel.iterations == 0);
        CHECK(r.kernel.decisions.empty());
        CHECK(r.kernel.events.empty());
        CHECK(!r.transform_matches_capture);
        CHECK(r.covariance.covariance.empty());
        std::cout << e.id << ": refused " << r.refusal_note << "\n";
        return 0;
    }
    std::size_t regenerated = 0;
    for (auto& input : rq.seam_inputs) {
        if (input.entry_outliers.empty() && !input.ols_residuals.empty()) {
            regenerated += input.ols_residuals.size();
            input.ols_residuals.clear();
            input.nobs = 0;
        }
    }
    if (regenerated) {
        const auto replay = tramo_fit(rq);
        CHECK(replay.status == r.status && replay.seams_live == r.seams_live &&
              replay.seams_fact == r.seams_fact && replay.parameters == r.parameters);
        std::cout << e.id << ": regenerated " << regenerated << " ARMA input observations\n";
    }
    CHECK(r.status == "available");
    CHECK(r.transform_matches_capture);
    if (e.verdict[0] != '-') {
        CHECK(r.transformation.seasonality.verdict == e.verdict);
        CHECK(r.transformation.seasonality.to_int == e.toint);
    }
    CHECK(r.seam_checks.size() ==
          static_cast<std::size_t>(e.live + e.fact));
    CHECK(r.seams_live == static_cast<std::size_t>(e.live));
    CHECK(r.seams_fact == static_cast<std::size_t>(e.fact));
    for (const TramoFitSeamCheck& c : r.seam_checks) {
        CHECK(c.tier == "live" || c.tier == "fact");
        CHECK(!c.note.empty());
        if (c.tier == "live") CHECK(c.note.rfind("match ", 0) == 0);
    }
    // The final surface: echo integrity against the request record (the
    // unit cross-checks the structural links itself and throws).
    CHECK(orders_word(r.orders) == orders_word(rq.final.orders));
    CHECK(r.log == rq.final.log);
    CHECK(r.mean == rq.final.mean);
    CHECK(r.parameters.size() == rq.final.parameters.size());
    for (std::size_t i = 0; i < r.parameters.size(); ++i)
        CHECK(to_bits(r.parameters[i]) == to_bits(rq.final.parameters[i]));
    CHECK(r.variables.size() == rq.final.variables.size());
    for (const auto& var : r.variables) {
        const auto date = var.name.find('(') + 1;
        const int year = std::stoi(var.name.substr(date, 4));
        const int month = std::stoi(var.name.substr(date + 5, 2));
        CHECK(var.position == (year - rq.start_year) * 12 + month - rq.start_month);
    }
    CHECK(r.cl_dim == rq.final.dim);
    CHECK(r.cl_nx == rq.final.nx);
    CHECK(r.cl_nmissing == rq.final.nmissing);
    CHECK(to_bits(r.ssq) == to_bits(rq.final.ssq));
    CHECK(to_bits(r.log_determinant) == to_bits(rq.final.log_determinant));
    CHECK(to_bits(r.ls_ll) == to_bits(rq.final.ls_ll));
    CHECK(to_bits(r.ls_aic) == to_bits(rq.final.ls_aic));
    CHECK(r.full_residuals.size() == rq.final.full_residuals.size());
    for (std::size_t i = 0; i < r.full_residuals.size(); ++i)
        CHECK(to_bits(r.full_residuals[i]) ==
              to_bits(rq.final.full_residuals[i]));
    // The D9 block.
    CHECK(r.covariance.matches_capture);
    CHECK(r.covariance.covariance_upper.size() ==
          static_cast<std::size_t>(e.d9_entries));
    CHECK(r.covariance.captured_upper.size() ==
          r.covariance.covariance_upper.size());
    for (std::size_t i = 0; i < r.covariance.covariance_upper.size(); ++i) {
        CHECK(to_bits(r.covariance.covariance_upper[i]) ==
              to_bits(rq.final.captured_covariance_upper[i]));
    }
    if (rq.final.nx > 0) {
        const double s = rq.final.ssq / static_cast<double>(
            rq.final.dim - rq.final.nx - rq.final.free_arma_parameters);
        CHECK(to_bits(r.covariance_scale) == to_bits(s));
        for (int i = 0; i < rq.final.nx; ++i) {
            for (int j = 0; j < i; ++j) {  // symmetry of the native block
                CHECK(to_bits(r.covariance
                                  .covariance[static_cast<std::size_t>(i) *
                                                  rq.final.nx + j]) ==
                      to_bits(r.covariance
                                  .covariance[static_cast<std::size_t>(j) *
                                                  rq.final.nx + i]));
            }
        }
    } else {
        CHECK(r.covariance.covariance.empty());
        CHECK(rq.final.captured_covariance_upper.empty());
    }
    live_total += r.seams_live;
    fact_total += r.seams_fact;
    d9_total += r.covariance.covariance_upper.size();
    std::cout << e.id << ": available log " << r.log << " mean " << r.mean
              << " orders " << orders_word(r.orders) << " cov "
              << r.covariance.covariance_upper.size() << "/"
              << e.d9_entries << " live " << r.seams_live << " fact "
              << r.seams_fact << "\n";
    return r.covariance.covariance_upper.size();
}

}  // namespace

int main() {
    std::size_t live_total = 0, fact_total = 0, d9_total = 0;
    int failures = 0;
    int refusals = 0;
    for (const FitExpect& e : fit_expectations()) {
        try {
            run_case(e, live_total, fact_total, d9_total);
            if (e.refuses[0] != '\0') ++refusals;
        } catch (const std::exception& ex) {
            std::cout << "FAIL " << e.id << ": " << ex.what() << "\n";
            ++failures;
        }
    }
    // The two matrix rows without a JVM witness, pinned with synthetic
    // request mutations (contract "refusal matrix").
    try {
        TramoFitRequest rq;
        build_fit("fit_tr1_ao", rq);
        rq.spec.seats_decomposition_mode = 0;
        const TramoFitResult r = tramo_fit(rq);
        CHECK(r.status == "refused:d6-decomposition");
        CHECK(r.kernel.iterations == 0);
        TramoFitRequest rq2;
        build_fit("fit_tr1_ao", rq2);
        rq2.spec.auto_method = tramo_auto_method::bic;  // td flags stay off
        const TramoFitResult r2 = tramo_fit(rq2);
        CHECK(r2.status == "refused:i10-automethod");
        TramoFitRequest corrupt;
        build_fit("fit_tr3_air", corrupt);
        for (auto& input : corrupt.seam_inputs) {
            if (input.ols_residuals.empty()) continue;
            input.ols_residuals.front() += 1.0;
            bool refused = false;
            try { (void)tramo_fit(corrupt); }
            catch (const std::invalid_argument& ex) {
                refused = std::string(ex.what()).find("native ARMA residuals") != std::string::npos;
            }
            CHECK(refused);
            break;
        }
        std::cout << "synthetic: d6 and i10 refusals pinned\n";
    } catch (const std::exception& ex) {
        std::cout << "FAIL synthetic: " << ex.what() << "\n";
        ++failures;
    }
    std::cout << "tramo_fit: " << fit_expectations().size() << " cases, "
              << refusals << " refusals, D9 entries " << d9_total
              << ", seam tiers live " << live_total << " fact " << fact_total
              << "\n";
    if (failures != 0) {
        std::cout << failures << " FAILURES\n";
        return 1;
    }
    std::cout << "tramo_fit: OK\n";
    return 0;
}
