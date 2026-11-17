#!/usr/bin/env python3
"""Generate tests/test_estimation_control.cpp from the pinned corpora.

The 21-A.1 corpus (Zero/Default starts) lives in
validation/evidence/native/estimation-control and the 21-A.2 corpus
(HannanRissanen/Multiple starts, per-case eps/feps) lives in
validation/evidence/native/estimation-control-hr. Both are embedded verbatim.
"""
import csv

import os
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
CORPUS = ROOT + "/validation/evidence/native/estimation-control"
CORPUS_HR = ROOT + "/validation/evidence/native/estimation-control-hr"

def split(v):
    return [] if v == "" else v.split(";")

def cvals(v):
    if not v:
        return "{}"
    return "{" + ", ".join("%.17g" % float(x) for x in v) + "}"

def iis(v):
    if not v:
        return "{}"
    return "{" + ", ".join(x for x in v) + "}"

def read(directory):
    with open(directory + "/cases.csv") as f:
        cases = {r["id"]: r for r in csv.DictReader(f)}
    with open(directory + "/inputs.csv") as f:
        inputs = {r["id"]: r for r in csv.DictReader(f)}
    return cases, inputs

def emit(rows, directory):
    cases, inputs = read(directory)
    for cid in cases:
        c, i = cases[cid], inputs[cid]
        params = split(c["params"])
        cols = [] if i["columns"] == "" else [r.split(";") for r in i["columns"].split("|")]
        missing = [int(x) - 1 for x in split(i["missing"]) if x != ""]
        eps = c.get("eps") or "1e-07"
        feps = c.get("feps") or "1e-04"
        rows.append(
            '  Case{"%s", "%s", %s, %s, %s, %s, %s, %s, %d, %d,\n'
            '       %s,\n       %s,\n       {%s},\n       {%s}, %s},\n'
            % (cid, i["start"], "%.17g" % float(eps), "%.17g" % float(feps),
               i["p_ar"], i["p_ma"], i["p_sar"], i["p_sma"],
               int(i["d"]), int(i["D"]),
               cvals(split(i["y"])), iis([str(x) for x in missing]),
               ", ".join("{" + ", ".join("%.17g" % float(x) for x in col) + "}" for col in cols),
               ", ".join("%.17g" % float(x) for x in params), c["loglik"]))
    return list(cases)

rows = []
base_order = emit(rows, CORPUS)
hr_order = emit(rows, CORPUS_HR) if os.path.isdir(CORPUS_HR) else None
if hr_order is None:
    sys.exit("missing HR corpus " + CORPUS_HR)
ORDER = base_order + hr_order
body = "\n".join(rows)

source = """// Item 21-A estimation control: the fixed-order estimation-CONTROL layer over
// the qualified objective and optimizer. Expected values are the pinned jdplus
// RegSarimaComputer outcomes captured by
// validation/reference/capture-estimation-control.R (Zero/Default starts) and
// validation/reference/capture-estimation-control-hr.R (HannanRissanen and
// Multiple starts, per-case eps/feps), both rjd3 3.9.0, installed under
// validation/evidence/native/estimation-control{,-hr} and embedded verbatim at
// 17 significant digits; the sweep over the on-disk corpora is
// tools/verify_estimation_control.cpp. Because the pinned Levenberg-Marquardt
// and the Kalos BFGS solvers are independent, agreement is asserted at fit
// tolerance; the measured run-to-run spread of the pinned side (Zero against
// Default start on identical targets) bounds the coefficient tolerance at
// 2e-3 and the likelihood tolerance at 5e-4. The pass-record shape and the
// selection bits are asserted structurally per starting point. The pinned
// rms < 1e-9 variance guard is qualified in test_hannan_rissanen.cpp instead:
// it fires only where the likelihood is unbounded, and the pinned landing
// point there is process-dependent (see the capture script note). See
// docs/contracts/estimation-control-contract.md.
#include "kalos/estimation_control.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Case {
    const char* id;
    const char* start;
    double eps, feps;             // Pinned precisions (builder defaults except
                                  // the pinned PROCESSOR case at 1e-9).
    int p_ar, p_ma, p_sar, p_sma;
    int d, D;
    std::vector<double> y;
    std::vector<int> missing;
    std::vector<std::vector<double>> columns;
    std::vector<double> params; // Pinned, Kalos convention [ar, sar, ma, sma].
    double log_likelihood;
};

const Case cases[] = {
%s
};

const Case& find_case(const char* id) {
    for (const auto& c : cases)
        if (std::string(c.id) == id) return c;
    assert(false);
}

void threads(const char* value) {
#ifdef _WIN32
    assert(_putenv_s("PERSEPHONE_NUM_THREADS", value) == 0);
#else
    assert(setenv("PERSEPHONE_NUM_THREADS", value, 1) == 0);
#endif
}

kalos::EstimationControl control(const Case& c) {
    kalos::EstimationControl k;
    k.eps = c.eps;                        // Pinned DEF_EPS (or PROCESSOR).
    k.feps = c.feps;                      // Pinned DEF_INTERNAL_EPS.
    k.maximum_likelihood = true;          // Pinned ml.
    k.unit_root_search = false;           // Pinned mt.
    k.corrected_degrees_of_freedom = true;  // Pinned cdf.
    k.fast_derivatives = true;            // Pinned fast.
    const std::string start = c.start;
    k.starting_point = start == "Zero"      ? kalos::EstimationStartingPoint::Zero
                       : start == "Default" ? kalos::EstimationStartingPoint::Default
                       : start == "HannanRissanen"
                           ? kalos::EstimationStartingPoint::HannanRissanen
                           : kalos::EstimationStartingPoint::Multiple;
    return k;
}

kalos::SarimaEstimationRequest request(const Case& c) {
    kalos::SarimaEstimationRequest r;
    r.series.values = c.y;
    r.series.missing.assign(c.y.size(), 0);
    for (const int m : c.missing) r.series.missing[m] = 1;
    r.design.columns = c.columns.size();
    for (const auto& column : c.columns) {
        assert(column.size() == c.y.size());
        r.design.values.insert(r.design.values.end(), column.begin(), column.end());
    }
    r.design.coefficients.assign(c.columns.size(), 0.0);
    r.design.fixed.assign(c.columns.size(), 0);
    const int sizes[4] = {c.p_ar, c.p_ma, c.p_sar, c.p_sma};
    for (int b = 0; b < 4; ++b) {
        r.blocks[b].initial.assign(sizes[b], 0.0);   // The policy fills the start.
        r.blocks[b].fixed.assign(sizes[b], 0);
    }
    r.period = 12;
    r.d = c.d;
    r.seasonal_d = c.D;
    r.control = control(c);
    return r;
}

double abs_diff(double a, double b) { return std::abs(a - b); }

// Chronological record vocabulary (21-A.2): the final eps chain is always
// estimate, ur_regular, ur_seasonal; the HannanRissanen family prepends
// "initialize" and the Multiple policy inserts its two internal-precision
// chains, each estimate, ur_regular, ur_seasonal.
void check_shape(const kalos::SarimaEstimationResult& result, const Case& c) {
    const std::string start = c.start;
    const auto& p = result.passes;
    const std::size_t n = p.size();
    const auto chain = [&](std::size_t base, const char* prefix) {
        assert(p[base].stage == std::string(prefix) + "estimate");
        assert(p[base + 1].stage == std::string(prefix) + "ur_regular");
        assert(p[base + 2].stage == std::string(prefix) + "ur_seasonal");
    };
    assert(n >= 3);
    chain(n - 3, "");
    if (start == "Zero" || start == "Default") {
        assert(n == 3);
        return;
    }
    assert(p[0].stage == "initialize");
    assert(!p[0].detail.empty());
    assert(p[0].detail ==
        std::string(std::string(c.id) == "hrs_near_unit" ? "fitted,stabilized" : "fitted"));
    if (start == "HannanRissanen") {
        assert(n == 4);
        assert(p[0].accepted); // The initialized start is the final start.
        chain(1, "");
        return;
    }
    // Multiple: the pinned selection retains exactly one of the initialized
    // start and the two internal-precision estimates as the final seed.
    assert(n == 10);
    chain(1, "multiple_first.");
    chain(4, "multiple_second.");
    const int retained = (p[0].accepted ? 1 : 0) + (p[1].accepted ? 1 : 0) +
        (p[4].accepted ? 1 : 0);
    assert(retained == 1);
}

void check_case(const Case& c) {
    const auto result = kalos::estimate_sarima_fixed(request(c), {});
    const int p_ar = c.p_ar, p_ma = c.p_ma, p_sar = c.p_sar, p_sma = c.p_sma;
    assert(result.coefficients[0].size() == static_cast<std::size_t>(p_ar));
    assert(result.coefficients[1].size() == static_cast<std::size_t>(p_ma));
    assert(result.coefficients[2].size() == static_cast<std::size_t>(p_sar));
    assert(result.coefficients[3].size() == static_cast<std::size_t>(p_sma));
    const std::string id(c.id);
    if (id.rfind("guard_", 0) == 0) {
        assert(result.status == "likelihood_only");
        assert(!result.converged);
        assert(result.passes.empty());
        assert(c.params.empty());
        assert(abs_diff(result.log_likelihood, c.log_likelihood) < 1e-9);
        std::printf("  %%s: likelihood_only ok (ll %%.6g)\\n", c.id, result.log_likelihood);
        return;
    }
    // Solver status is the qualified solver's own first-order numerical
    // verdict; on the flat near-unit-root directions the pinned Levenberg-
    // Marquardt function-precision stop and the BFGS gradient criterion can
    // disagree while the outcome coincides. The contract qualifies the
    // outcome (gates, acceptance, coefficients, likelihood).
    if (result.status != "converged") {
        std::printf("  %%s: status %%s (outcome still qualified)\\n", c.id, result.status.c_str());
    }
    check_shape(result, c);
    // Every chain: a restart is kept exactly when its candidate likelihood is
    // strictly better than the current incumbent (transitively, better than
    // the chain's own estimate record).
    std::vector<std::size_t> bases{result.passes.size() - 3};
    if (result.passes[0].stage == "initialize" && result.passes.size() == 10) {
        bases.push_back(1);
        bases.push_back(4);
    }
    for (const std::size_t base : bases) {
        for (std::size_t k = base + 1; k <= base + 2; ++k) {
            const auto& pass = result.passes[k];
            assert(!pass.accepted || pass.attempted);
            if (pass.attempted)
                assert(pass.accepted ==
                    (pass.log_likelihood > result.passes[base].log_likelihood));
        }
    }
    // Far-from-cancellation fixtures miss the gates on both sides by a wide
    // margin; the near-unit fixtures sit close to the 0.1 boundary, so only
    // the record consistency above is asserted there.
    const bool near_unit_fixture = id.rfind("urp_", 0) == 0 || id.rfind("urm_", 0) == 0 ||
        id == "hrs_near_unit";
    const std::size_t n = result.passes.size();
    if (!near_unit_fixture) {
        assert(!result.passes[n - 2].gates_passed && !result.passes[n - 1].gates_passed);
    }
    const std::vector<double> pinned = c.params;
    std::vector<double> actual;
    actual.insert(actual.end(), result.coefficients[0].begin(), result.coefficients[0].end());
    actual.insert(actual.end(), result.coefficients[2].begin(), result.coefficients[2].end());
    actual.insert(actual.end(), result.coefficients[1].begin(), result.coefficients[1].end());
    actual.insert(actual.end(), result.coefficients[3].begin(), result.coefficients[3].end());
    assert(actual.size() == pinned.size());
    double worst = 0.0;
    for (std::size_t i = 0; i < pinned.size(); ++i) {
        worst = std::max(worst, abs_diff(actual[i], pinned[i]));
        assert(abs_diff(actual[i], pinned[i]) < 2e-3);
    }
    assert(abs_diff(result.log_likelihood, c.log_likelihood) < 5e-4);
    std::printf("  %%s: ok (worst coeff %%.3g, ll delta %%.3g, urp gates %%d accepted %%d)\\n",
        c.id, worst, abs_diff(result.log_likelihood, c.log_likelihood),
        static_cast<int>(result.passes[n - 2].gates_passed),
        static_cast<int>(result.passes[n - 2].accepted));
}

} // namespace

int main() {
    threads("2");
    for (const auto& c : cases) check_case(c);

    // Structural controls: invalid fields fail before any work.
    auto good = control(find_case("zero_r11"));
    try { auto bad = good; bad.eps = 0.0; bad.validate(); assert(false); }
    catch (const std::invalid_argument&) {}
    try { auto bad = good; bad.feps = std::nan(""); bad.validate(); assert(false); }
    catch (const std::invalid_argument&) {}
    try { auto bad = good; bad.maximum_likelihood = false; bad.validate(); assert(false); }
    catch (const std::invalid_argument&) {}
    // The HannanRissanen initializer runs on the differenced linear model, so
    // its starts reject masked series and fixed regression coefficients
    // explicitly instead of approximating the pinned behaviour (contract).
    {
        auto r = request(find_case("hrs_r11"));
        r.series.missing[0] = 1;
        try {
            kalos::estimate_sarima_fixed(r, {});
            assert(false);
        } catch (const std::invalid_argument& e) {
            assert(std::string(e.what()).find("complete series") != std::string::npos);
        }
    }
    {
        auto r = request(find_case("multiple_r11"));
        r.design.fixed[0] = 1;
        try {
            kalos::estimate_sarima_fixed(r, {});
            assert(false);
        } catch (const std::invalid_argument& e) {
            assert(std::string(e.what()).find("fully free regression designs") !=
                   std::string::npos);
        }
    }

    // Fixed coefficients survive exactly (the pinned SarimaFixedMapping
    // seam): the free MA factor is estimated around the frozen value.
    {
        auto r = request(find_case("zero_r11"));
        r.blocks[1].fixed[0] = 1;
        r.blocks[1].initial[0] = 0.25;
        const auto result = kalos::estimate_sarima_fixed(r, {});
        assert(result.coefficients[1][0] == 0.25);
        assert(result.coefficients[1][0] != 0.0);
        assert(result.log_likelihood <= find_case("zero_r11").log_likelihood + 5e-4);
        std::printf("  fixed_seam: ok\\n");
    }

    // A request whose free parameter set is empty is the pinned likelihood-
    // only branch even when design columns exist; the result must match the
    // direct physical objective exactly (no solver involved).
    {
        auto r = request(find_case("guard_regular"));
        const std::array<std::vector<double>, 4> empty_coefficients;
        const auto direct = kalos::evaluate_arma_physical_objective(
            r.series, r.design, kalos::ArmaParameterMap(r.blocks),
            empty_coefficients, 12, 1, 0, 0.0, 1e-10, 0.0, {});
        const auto result = kalos::estimate_sarima_fixed(r, {});
        assert(direct.status == "available");
        assert(result.log_likelihood == direct.adjusted_log_likelihood);
        std::printf("  guard_exact: ok\\n");
    }

    // Mismatched coefficient states are static input errors.
    {
        auto r = request(find_case("zero_r11"));
        r.blocks[0].fixed.pop_back();
        bool caught = false;
        try { kalos::estimate_sarima_fixed(r, {}); } catch (const std::invalid_argument&) { caught = true; }
        assert(caught);
    }

    // Deterministic reuse at each thread count, over a cancellation case and
    // over the ten-record Multiple chain (selection bits included). Bit-level
    // equality is only asserted within one thread count: cross-thread BLAS
    // reduction order legitimately changes roundoff.
    for (const char* id : {"urp_near_unit_zero", "multiple_full"}) {
        for (const char* count : {"1", "2"}) {
            threads(count);
            const auto a = kalos::estimate_sarima_fixed(request(find_case(id)), {});
            const auto b = kalos::estimate_sarima_fixed(request(find_case(id)), {});
            assert(a.status == b.status && a.log_likelihood == b.log_likelihood);
            for (std::size_t blk = 0; blk < 4; ++blk) assert(a.coefficients[blk] == b.coefficients[blk]);
            assert(a.passes.size() == b.passes.size());
            for (std::size_t i = 0; i < a.passes.size(); ++i) {
                assert(a.passes[i].stage == b.passes[i].stage);
                assert(a.passes[i].accepted == b.passes[i].accepted);
                assert(a.passes[i].gates_passed == b.passes[i].gates_passed);
                assert(a.passes[i].detail == b.passes[i].detail);
            }
        }
        std::printf("  reuse %%s: ok\\n", id);
    }
    std::printf("test_estimation_control: all checks passed\\n");
    return 0;
}
""" % body

with open(ROOT + "/tests/test_estimation_control.cpp", "w") as f:
    f.write(source)
print("wrote", len(ORDER), "cases")
