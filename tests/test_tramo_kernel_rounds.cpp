// Item 23-F.3 replay test: drives kalos::tramo_run_kernel_rounds (the
// TramoKernel ami() round-driver seam) over the generated golden corpus
// (tests/jvm_tramo_kernel_rounds.h from kr-capture-1.out). The unit
// RECOMPUTES every kernel decision (the estimate site, the change
// accumulator and its drop, the round-0 accept, the refStats gate
// operands and verdict, the pass3 reset, the reduceVa arms, the iterate
// ends, the whole log structure, the state ladder); the heavy-stage
// outcomes ride as facts. Every double comparison is raw IEEE-754 bit
// equality. The D3 flip (23-F.2 SECTION 28.2) is asserted positively for
// kr_scso / kr_spanout / kr_spannone, the quarterly D11 refusal for
// kr_q_air, and the Ljung-Box p-value is recomputed bit-exactly from the
// captured residual vectors through the landed primitives.
#include <kalos/arma_diagnostics.hpp>
#include <kalos/detail/special_functions.hpp>
#include <kalos/tramo_kernel_rounds.hpp>
#include <kalos/tramo_regression_tests.hpp>
#include <kalos/tramo_transform_decision.hpp>

#include "jvm_tramo_kernel_rounds.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
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

std::string hex(std::uint64_t b) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(b));
    return std::string(buf);
}

std::string bits_of(double v) { return hex(to_bits(v)); }

std::string kind_name(TramoKernelLogKind k) {
    switch (k) {
        case TramoKernelLogKind::push: return "push";
        case TramoKernelLogKind::pop: return "pop";
        case TramoKernelLogKind::info: return "info";
        case TramoKernelLogKind::remark: return "remark";
        case TramoKernelLogKind::warning: return "warn";
        case TramoKernelLogKind::step: return "step";
    }
    return "?";
}

std::string events_string(const std::vector<TramoKernelLogEvent>& events) {
    std::string s;
    for (const TramoKernelLogEvent& e : events) {
        s += kind_name(e.kind);
        s += " ";
        s += std::to_string(e.level);
        s += " ";
        s += e.text;
        s += "\n";
    }
    return s;
}

std::string events_string(const std::vector<KrEvent>& events) {
    std::string s;
    for (const KrEvent& e : events)
        s += std::string(e.kind == 0 ? "push " : e.kind == 1 ? "pop "
                     : e.kind == 2 ? "info " : e.kind == 3 ? "remark "
                     : e.kind == 4 ? "warn " : "step ") +
             std::to_string(e.level) + " " + e.text + "\n";
    return s;
}

std::string snap_string(const TramoKernelRoundSnapshot& x) {
    return std::to_string(x.pass) + " " + std::to_string(x.round) + " " +
           std::to_string(x.pass3 ? 1 : 0) + " " +
           std::to_string(x.need_outliers ? 1 : 0) + " " +
           std::to_string(x.need_auto_modelling ? 1 : 0) + " " +
           bits_of(x.curva) + " " + bits_of(x.pcr) + " " +
           std::to_string(x.ref_auto ? 1 : 0) + " " +
           std::to_string(x.ref_stats ? 1 : 0) + " " +
           std::to_string(x.seasonal ? 1 : 0) + " " +
           std::to_string(x.original_seasonality_test);
}

std::string snap_string(const KrSnap& x) {
    return std::to_string(x.pass) + " " + std::to_string(x.round) + " " +
           std::to_string(x.pass3) + " " + std::to_string(x.need_out) + " " +
           std::to_string(x.need_auto) + " " + hex(x.curva) + " " +
           hex(x.pcr) + " " + std::to_string(x.ref_auto) + " " +
           std::to_string(x.ref_stats) + " " + std::to_string(x.seasonal) +
           " " + std::to_string(x.ost);
}

struct Diff {
    bool bad = false;
    std::string msg;
};

Diff diff_runs(const TramoKernelRoundsRun& run, const std::string& cid) {
    Diff d;
    const std::vector<KrEvent> want_events = expected_events(cid);
    const std::vector<KrSnap> want_snaps = expected_snaps(cid);
    if (events_string(run.events) != events_string(want_events)) {
        d.bad = true;
        d.msg = "event stream differs\n got:\n" + events_string(run.events) +
                "want:\n" + events_string(want_events);
        return d;
    }
    if (run.snapshots.size() != want_snaps.size()) {
        d.bad = true;
        d.msg = "snapshot count " + std::to_string(run.snapshots.size()) +
                " != " + std::to_string(want_snaps.size());
        return d;
    }
    for (std::size_t i = 0; i < want_snaps.size(); ++i)
        if (snap_string(run.snapshots[i]) != snap_string(want_snaps[i])) {
            d.bad = true;
            d.msg = "snapshot " + std::to_string(i) + "\n got: " +
                    snap_string(run.snapshots[i]) + "\nwant: " +
                    snap_string(want_snaps[i]);
            return d;
        }
    // The final ladder state runs past the last logged head (the gate
    // transitions run after the snapshot); the per-decision JVM echoes
    // cross-check it inside the unit.
    return d;
}

// The Ljung-Box witness: ModelStatistics.of runs LjungBox on the RAW
// residuals (AutoCovariances.autoCorrelationFunction(e, 0.0), no mean
// correction) with k = LjungBox.defaultAutoCorrelationsCount(freq, n)
// (the landed calc_lb_length transcription) and nhp = the row's free
// parameter count, the same statistic/p-value form the landed
// ljung_box_test machine proved bit-exact in 21-H. Replaying the
// captured e-vectors must reproduce the captured lbp bit-exactly.
std::string lb_witness() {
    std::string log;
    int rows = 0, ok = 0;
    for (const KrExpect& e : expectations()) {
        if (e.refuses)
            continue;
        TramoKernelRoundsRequest rq;
        build_case(e.id, rq);
        for (const TramoKernelSeam& s : rq.seams) {
            if (s.kind != TramoKernelSeamKind::round0 &&
                s.kind != TramoKernelSeamKind::pass2_gate)
                continue;
            const std::vector<double>& res = s.stats.residuals;
            if (res.empty())
                continue;
            ++rows;
            const int n = static_cast<int>(res.size());
            const int lag = calc_lb_length(12, n);
            double acc = 0.0;
            for (int i = 1; i <= lag; ++i) {
                const double r = auto_correlation(res, 0.0, i);
                acc += (r * r) / static_cast<double>(n - i);
            }
            const double stat =
                acc * static_cast<double>(n) * static_cast<double>(n + 2);
            const double p = detail::chi2_upper_p(
                static_cast<double>(lag - s.stats.free_parameters), stat);
            if (bits_of(p) == bits_of(s.stats.ljung_box_pvalue))
                ++ok;
            else
                log += std::string(e.id) + ": lbp " + bits_of(p) + " != " +
                       bits_of(s.stats.ljung_box_pvalue) + " (lag " +
                       std::to_string(lag) + ")\n";
        }
    }
    return log + "ljung-box witness: " + std::to_string(ok) + "/" +
           std::to_string(rows) + " rows bit-exact from the e-array\n";
}

void seasonal_ar_reversion() {
    TramoKernelRoundsRequest rq;
    build_case("kr_air", rq);
    for (auto& seam : rq.seams) {
        if (seam.kind != TramoKernelSeamKind::pass2_gate) continue;
        // Shape F reads bphi(1). SarimaModel.parameters puts that
        // coefficient before theta(1), which would keep this model.
        seam.arima_orders = {0, 1, 1, 1, 0, 1};
        seam.parameters = {-0.8, 0.4, 0.2};
        seam.reverted = true;
        seam.rule = "F";
        seam.gate_ljung_box_complement = seam.gate_reference_ljung_box_complement;
        const auto result = tramo_run_kernel_rounds(rq);
        for (const auto& decision : result.decisions) {
            if (decision.kind != TramoKernelSeamKind::pass2_gate) continue;
            CHECK(decision.reverted && decision.orders_after.d == 1 && decision.orders_after.bd == 1);
            return;
        }
    }
    throw std::runtime_error("missing seasonal AR comparison");
}

void seasonal_difference_drop() {
    // Both (1,1) and (2,0) use two differences; their monthly polynomial
    // degrees are 13 and 2. The JVM clears the prior outliers at this site.
    TramoKernelRoundsRequest rq;
    build_case("kr_ao", rq);
    for (std::size_t i = 1; i < rq.seams.size(); ++i) {
        if (rq.seams[i].kind != TramoKernelSeamKind::differencing_drop) continue;
        auto& diff = rq.seams[i - 1];
        CHECK(diff.kind == TramoKernelSeamKind::differencing);
        diff.orders.d = 2;
        diff.orders.bd = 0;
        CHECK(!tramo_run_kernel_rounds(rq).snapshots.empty());
        return;
    }
    throw std::runtime_error("missing seasonal difference-drop witness");
}

// The D3 flip table (23-F.2 SECTION 28.2) as POSITIVE expectations:
// {id, observed multi-round} per the divergent rows flipping to witness.
void d3_flip_checks() {
    struct Flip { const char* id; };
    const Flip flips[] = {
        {"kr_scso"}, {"kr_spanout"}, {"kr_spannone"},
    };
    for (const Flip& f : flips) {
        TramoKernelRoundsRequest rq;
        build_case(f.id, rq);
        const TramoKernelRoundsRun run = tramo_run_kernel_rounds(rq);
        if (run.snapshots.size() < 2)
            throw std::runtime_error(
                std::string("D3 flip: ") + f.id +
                " must drive multiple rounds");
    }
    // The F.2 divergent witnesses (scan-restart round-0 rewrites, the
    // pass3 reset) ride inside the multi-round replays: kr_gs2 /
    // kr_shortfail end their first pass on a pass3 reset.
    for (const char* id : {"kr_gs2", "kr_shortfail"}) {
        TramoKernelRoundsRequest rq;
        build_case(id, rq);
        const TramoKernelRoundsRun run = tramo_run_kernel_rounds(rq);
        bool saw_reset = false;
        for (const TramoKernelSeamDecision& d : run.decisions)
            if (d.kind == TramoKernelSeamKind::iterate_end &&
                d.flag == static_cast<int>(TramoKernelIterateEnd::pass3_reset))
                saw_reset = true;
        if (!saw_reset)
            throw std::runtime_error(std::string("D3: ") + id +
                                     " must show the pass3 reset");
    }
}

int run_case(const KrExpect& e) {
    TramoKernelRoundsRequest rq;
    build_case(e.id, rq);
    if (e.refuses) {
        bool threw = false;
        try {
            tramo_run_kernel_rounds(rq);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw)
            throw std::runtime_error(std::string(e.id) + ": the D11 refusal is missing");
        std::printf("%-14s D11-REFUSED\n", e.id);
        return 1;
    }
    const TramoKernelRoundsRun run = tramo_run_kernel_rounds(rq);
    if (to_bits(run.initial_curva) != e.init_curva)
        throw std::runtime_error(std::string(e.id) + ": initial curva " +
                                 bits_of(run.initial_curva) + " != " +
                                 hex(e.init_curva));
    if (to_bits(run.initial_pcr) != e.init_pcr)
        throw std::runtime_error(std::string(e.id) + ": initial pcr " +
                                 bits_of(run.initial_pcr) + " != " +
                                 hex(e.init_pcr));
    if (static_cast<int>(run.snapshots.size()) != e.iterations)
        throw std::runtime_error(std::string(e.id) + ": " +
                                 std::to_string(run.snapshots.size()) +
                                 " iterates, expected " +
                                 std::to_string(e.iterations));
    if (run.converged != (e.converged == 1))
        throw std::runtime_error(std::string(e.id) + ": convergence flag differs");
    const Diff d = diff_runs(run, e.id);
    if (d.bad)
        throw std::runtime_error(std::string(e.id) + ": " + d.msg);
    std::printf("%-14s OK iter=%zu\n", e.id, run.snapshots.size());
    return 1;
}

std::uint64_t hex_str(std::uint64_t b) { return b; }

}  // namespace

int main() {
    try {
        std::printf("%s", lb_witness().c_str());
        seasonal_ar_reversion();
        seasonal_difference_drop();
        d3_flip_checks();
        std::printf("D3 flip table: positive multi-round + pass3-reset "
                    "witnesses hold\n");
        int done = 0;
        for (const KrExpect& e : expectations())
            done += run_case(e);
        if (done != static_cast<int>(expectations().size()))
            throw std::runtime_error("not all cases replayed");
        std::printf("tramo kernel rounds replay: all %d cases match\n",
                    done);
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
