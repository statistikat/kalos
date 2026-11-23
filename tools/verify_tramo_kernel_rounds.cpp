// Full-corpus verification for item 23-F.3: replays every case of the
// generated golden table (tests/jvm_tramo_kernel_rounds.h, from
// validation/evidence/native/tramo-kernel-rounds/kr-capture-1.out) through
// kalos::tramo_run_kernel_rounds - the same comparisons the ctest applies -
// AND re-reads the raw corpus to cross-qualify the golden table against the
// captured JVM lines: the per-case identity flags, the init bits, every
// jar-side event, every rsnap snapshot, the seam-kind census and the
// pass2gate/round0/pass2 echo tokens. A corpus edit that moves any replayed
// fact therefore fails this sweep even before the unit replay runs. The
// quarterly case kr_q_air must throw (the D11 refusal).
//
// Direct-compile lane (not part of the CMake test list): run from the
// repository root, see docs/contracts/tramo-kernel-rounds-contract.md.
#include <kalos/arma_diagnostics.hpp>
#include <kalos/detail/special_functions.hpp>
#include <kalos/tramo_kernel_rounds.hpp>

#include "../tests/jvm_tramo_kernel_rounds.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace kalos;

const char* kCorpus =
    "validation/evidence/native/tramo-kernel-rounds/kr-capture-1.out";

int failures = 0;
long long checks = 0;

void fail(const std::string& what) {
    std::cout << "FAIL " << what << std::endl;
    ++failures;
}

#define SW_CHECK(cond, what)                                                 \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond))                                                         \
            fail(what);                                                      \
    } while (0)

std::uint64_t to_bits(double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, sizeof(double));
    return b;
}

const char* kind_name_local(TramoKernelLogKind k) {
    switch (k) {
        case TramoKernelLogKind::push: return "push";
        case TramoKernelLogKind::pop: return "pop";
        case TramoKernelLogKind::info: return "info";
        case TramoKernelLogKind::remark: return "remark";
        case TramoKernelLogKind::warning: return "warn";
        default: return "step";
    }
}

std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string t;
    while (in >> t) out.push_back(t);
    return out;
}

std::uint64_t g_bits(const std::string& tok) {
    return to_bits(std::strtod(tok.c_str(), nullptr));
}

// ---------------------------------------------------------------------------
// Raw corpus model (the same grammar the golden generator parses)
// ---------------------------------------------------------------------------

struct RawEvent {
    int kind, level;
    std::string text;
};

struct RawSnap {
    int pass, round, pass3, need_out, need_auto;
    std::uint64_t curva, pcr;
    int ref_auto, ref_stats, seasonal, ost;
};

struct RawGate {  // pass2gate / round0 / pass2 echo tokens, in stream order
    std::string kind;   // "round0" | "pass2gate" | "pass2"
    std::uint64_t v1, v2;  // round0: lbp, accept; pass2gate: lbp, se (0 if first);
                           // pass2: ret, pcr
    std::string why;
};

struct RawCase {
    std::string id;
    int freq = 0, n = 0;
    std::uint64_t init_curva = 0, init_pcr = 0;
    std::vector<RawEvent> events;   // jar-side run only
    std::vector<RawSnap> snaps;     // rsnap lines
    std::vector<std::string> seam_kinds;
    std::vector<RawGate> gates;
    int stats_rows = 0, stats_e = 0;
    bool identity_all1 = false;
};

std::vector<RawCase> read_corpus(const std::string& path) {
    std::vector<RawCase> cases;
    std::ifstream in(path);
    if (!in) throw std::runtime_error(std::string("cannot read ") + path);
    RawCase* cur = nullptr;
    int run = 0;
    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> t = split_ws(line);
        if (t.empty()) continue;
        if (t[0] == "KER" && t.size() > 3 && t[2] == "specname") {
            cases.emplace_back();
            cur = &cases.back();
            cur->id = t[1];
            cur->freq = std::atoi(t[5].c_str());
            cur->n = std::atoi(t[9].c_str());
            run = 0;
            continue;
        }
        if (cur == nullptr) continue;
        if (t[0] == "init") {
            cur->init_curva = g_bits(t[2]);
            cur->init_pcr = g_bits(t[4]);
        } else if (t[0] == "evt") {
            if (t[2] == "push" && t[3] == "0" && t[4] == "tramo" &&
                t[1] == "1" && !cur->events.empty())
                run = 1;
            if (run == 0) {
                // text = the line tail after "evt <n> <kind> <level> "
                // (the probe prints single-spaced tokens; the text may
                // contain spaces).
                const std::string::size_type pos =
                    4 + t[1].size() + 1 + t[2].size() + 1 + t[3].size() + 1;
                const std::string text =
                    pos < line.size() ? line.substr(pos) : std::string();
                const int k = t[2] == "push"   ? 0
                              : t[2] == "pop"  ? 1
                              : t[2] == "info" ? 2
                              : t[2] == "remark" ? 3
                              : t[2] == "warn"   ? 4
                                                 : 5;
                cur->events.push_back({k, std::atoi(t[3].c_str()), text});
            }
        } else if (t[0] == "rsnap") {
            RawSnap s{};
            for (std::size_t i = 1; i < t.size(); ++i) {
                const std::string::size_type eq = t[i].find('=');
                if (eq == std::string::npos) continue;
                const std::string k = t[i].substr(0, eq);
                const std::string v = t[i].substr(eq + 1);
                if (k == "pass") s.pass = std::atoi(v.c_str());
                else if (k == "round") s.round = std::atoi(v.c_str());
                else if (k == "pass3") s.pass3 = std::atoi(v.c_str());
                else if (k == "needOut") s.need_out = std::atoi(v.c_str());
                else if (k == "needAuto") s.need_auto = std::atoi(v.c_str());
                else if (k == "curva") s.curva = g_bits(v);
                else if (k == "pcr") s.pcr = g_bits(v);
                else if (k == "refAuto") s.ref_auto = std::atoi(v.c_str());
                else if (k == "refStats") s.ref_stats = std::atoi(v.c_str());
                else if (k == "seasonal") s.seasonal = std::atoi(v.c_str());
                else if (k == "ost") s.ost = std::atoi(v.c_str());
            }
            cur->snaps.push_back(s);
        } else if (t[0] == "seam") {
            cur->seam_kinds.push_back(t[2]);
            if (t[2] == "round0")
                cur->gates.push_back(
                    {"round0", g_bits(t[4]),
                     static_cast<std::uint64_t>(std::atoi(t[6].c_str())), ""});
            else if (t[2] == "pass2gate") {
                if (t[3] == "first")
                    cur->gates.push_back({"pass2gate", 0, 0, "first"});
                else
                    cur->gates.push_back({"pass2gate", g_bits(t[4]),
                                          g_bits(t[6]), t[18]});
            } else if (t[2] == "pass2") {
                cur->gates.push_back(
                    {"pass2", static_cast<std::uint64_t>(std::atoi(t[4].c_str())),
                     g_bits(t[6]), ""});
            }
        } else if (t[0] == "stats") {
            if (t[2] == "e")
                ++cur->stats_e;
            else
                ++cur->stats_rows;
        } else if (t[0] == "identity") {
            cur->identity_all1 =
                t.size() == 8 && t[1] == cur->id && t[2] == "events" &&
                t[3] == "1" && t[4] == "final" && t[5] == "1" &&
                t[6] == "all" && t[7] == "1";
        }
    }
    return cases;
}

int count_kind(const std::vector<std::string>& v, const std::string& k) {
    int c = 0;
    for (const std::string& s : v)
        if (s == k) ++c;
    return c;
}

// ---------------------------------------------------------------------------
// 1. Raw corpus cross-qualification against the golden table
// ---------------------------------------------------------------------------

void cross_qualify(const std::vector<RawCase>& raw) {
    SW_CHECK(raw.size() == expectations().size(),
             "corpus case count " + std::to_string(raw.size()) + " vs table " +
                 std::to_string(expectations().size()));
    for (std::size_t c = 0; c < raw.size() && c < expectations().size(); ++c) {
        const RawCase& r = raw[c];
        const KrExpect& e = expectations()[c];
        const std::string id = r.id;
        SW_CHECK(r.id == e.id, "case order " + id + " vs " + e.id);
        SW_CHECK(r.identity_all1, id + ": identity line not all-1");
        SW_CHECK(r.init_curva == e.init_curva && r.init_pcr == e.init_pcr,
                 id + ": init bits moved in the corpus");
        SW_CHECK(static_cast<int>(r.snaps.size()) == e.iterations,
                 id + ": rsnap count vs table iterations");
        SW_CHECK(r.freq == 12 || e.refuses == 1,
                 id + ": non-monthly case is not the refusal witness");

        const std::vector<KrEvent> want = expected_events(id);
        SW_CHECK(r.events.size() == want.size(),
                 id + ": jar event count " + std::to_string(r.events.size()) +
                     " vs table " + std::to_string(want.size()));
        for (std::size_t i = 0;
             i < r.events.size() && i < want.size(); ++i)
            SW_CHECK(r.events[i].kind == want[i].kind &&
                         r.events[i].level == want[i].level &&
                         r.events[i].text == want[i].text,
                     id + ": event " + std::to_string(i) + " moved");

        const std::vector<KrSnap> snaps = expected_snaps(id);
        for (std::size_t i = 0; i < r.snaps.size() && i < snaps.size(); ++i)
            SW_CHECK(r.snaps[i].pass == snaps[i].pass &&
                         r.snaps[i].round == snaps[i].round &&
                         r.snaps[i].pass3 == snaps[i].pass3 &&
                         r.snaps[i].need_out == snaps[i].need_out &&
                         r.snaps[i].need_auto == snaps[i].need_auto &&
                         r.snaps[i].curva == snaps[i].curva &&
                         r.snaps[i].pcr == snaps[i].pcr &&
                         r.snaps[i].ref_auto == snaps[i].ref_auto &&
                         r.snaps[i].ref_stats == snaps[i].ref_stats &&
                         r.snaps[i].seasonal == snaps[i].seasonal &&
                         r.snaps[i].ost == snaps[i].ost,
                     id + ": rsnap " + std::to_string(i) + " moved");

        // Seam census: the table's fact kinds must equal the corpus lines.
        TramoKernelRoundsRequest rq;
        build_case(id, rq);
        std::vector<std::string> table_kinds;
        for (const TramoKernelSeam& s : rq.seams) {
            table_kinds.push_back(
                s.kind == TramoKernelSeamKind::estimation  ? "estimate"
                : s.kind == TramoKernelSeamKind::differencing
                    ? "diff"
                : s.kind == TramoKernelSeamKind::differencing_drop
                    ? "diffdrop"
                : s.kind == TramoKernelSeamKind::arma      ? "arma"
                : s.kind == TramoKernelSeamKind::arma_q1   ? "armaq1"
                : s.kind == TramoKernelSeamKind::scan      ? "scan"
                : s.kind == TramoKernelSeamKind::estimation_model
                    ? "estmodel"
                : s.kind == TramoKernelSeamKind::estimation_reduce
                    ? "estmodelreduceva"
                : s.kind == TramoKernelSeamKind::regression_test
                    ? "regtest"
                : s.kind == TramoKernelSeamKind::round0    ? "round0"
                : s.kind == TramoKernelSeamKind::pass2_gate ? "pass2gate"
                : s.kind == TramoKernelSeamKind::pass2_return ? "pass2"
                : s.kind == TramoKernelSeamKind::seasonality_controller
                    ? "scontroller"
                : s.kind == TramoKernelSeamKind::model_controller
                    ? "controller"
                                                              : "iterate");
        }
        int diff = 0, scan = 0, arma = 0, estmodel = 0, gates = 0,
            ctrl = 0, est = 0, regt = 0, sctl = 0, ends = 0, p2 = 0,
            q1 = 0, drop = 0, rv = 0;
        for (const std::string& k : table_kinds) {
            if (k == "diff") ++diff;
            else if (k == "scan") ++scan;
            else if (k == "arma") ++arma;
            else if (k == "estmodel") ++estmodel;
            else if (k == "pass2gate") ++gates;
            else if (k == "controller") ++ctrl;
            else if (k == "estimate") ++est;
            else if (k == "regtest") ++regt;
            else if (k == "scontroller") ++sctl;
            else if (k == "iterate") ++ends;
            else if (k == "pass2") ++p2;
            else if (k == "armaq1") ++q1;
            else if (k == "diffdrop") ++drop;
            else if (k == "estmodelreduceva") ++rv;
        }
        SW_CHECK(diff == count_kind(r.seam_kinds, "diff") &&
                     scan == count_kind(r.seam_kinds, "scan") &&
                     arma == count_kind(r.seam_kinds, "arma") &&
                     estmodel == count_kind(r.seam_kinds, "estmodel") &&
                     gates == count_kind(r.seam_kinds, "pass2gate") &&
                     ctrl == count_kind(r.seam_kinds, "controller") &&
                     est == count_kind(r.seam_kinds, "estimate") &&
                     regt == count_kind(r.seam_kinds, "regtest") &&
                     sctl == count_kind(r.seam_kinds, "scontroller") &&
                     ends == count_kind(r.seam_kinds, "iterate") &&
                     p2 == count_kind(r.seam_kinds, "pass2") &&
                     q1 == count_kind(r.seam_kinds, "armaq1") &&
                     drop == count_kind(r.seam_kinds, "diffdrop") &&
                     rv == count_kind(r.seam_kinds, "estmodelreduceva"),
                 id + ": seam-kind census moved");

        // Gate echoes: the corpus tokens vs the fact fields.
        std::size_t gi = 0;
        for (const TramoKernelSeam& s : rq.seams) {
            if (s.kind != TramoKernelSeamKind::round0 &&
                s.kind != TramoKernelSeamKind::pass2_gate &&
                s.kind != TramoKernelSeamKind::pass2_return)
                continue;
            if (gi >= r.gates.size()) {
                fail(id + ": fewer corpus gate lines than table echoes");
                break;
            }
            const RawGate& g = r.gates[gi++];
            if (s.kind == TramoKernelSeamKind::round0)
                SW_CHECK(g.kind == "round0" &&
                             g.v1 == to_bits(s.stats.ljung_box_pvalue) &&
                             (g.v2 == 1) == s.accept,
                         id + ": round0 echo moved");
            else if (s.kind == TramoKernelSeamKind::pass2_gate) {
                if (g.why == "first")
                    SW_CHECK(s.rule == "first", id + ": first-gate marker");
                else
                    SW_CHECK(g.kind == "pass2gate" &&
                                 g.v1 == to_bits(s.gate_ljung_box_complement) &&
                                 g.v2 == to_bits(s.gate_standard_error) &&
                                 g.why == s.rule,
                             id + ": pass2gate echo moved");
            } else {
                SW_CHECK(g.kind == "pass2" &&
                             g.v1 == static_cast<std::uint64_t>(s.returned) &&
                             g.v2 == to_bits(s.returned_pcr),
                         id + ": pass2 return echo moved");
            }
        }
        SW_CHECK(gi == r.gates.size(), id + ": unused corpus gate lines");

        // stats rows / e-rows vs the carried rows.
        int rows = 0, eres = 0;
        for (const TramoKernelSeam& s : rq.seams)
            if (s.kind == TramoKernelSeamKind::round0 ||
                s.kind == TramoKernelSeamKind::pass2_gate) {
                ++rows;
                if (!s.stats.residuals.empty()) ++eres;
            }
        SW_CHECK(rows == r.stats_rows && eres == r.stats_e,
                 id + ": stats row census moved");
    }
}

// ---------------------------------------------------------------------------
// 2. Full replay (as the ctest) + the Ljung-Box witness recompute
// ---------------------------------------------------------------------------

void replay_all() {
    int lb_rows = 0, lb_ok = 0;
    for (const KrExpect& e : expectations()) {
        TramoKernelRoundsRequest rq;
        build_case(e.id, rq);
        if (e.refuses) {
            bool threw = false;
            try {
                tramo_run_kernel_rounds(rq);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            SW_CHECK(threw, std::string(e.id) + ": D11 refusal missing");
            continue;
        }
        TramoKernelRoundsRun run;
        try {
            run = tramo_run_kernel_rounds(rq);
        } catch (const std::exception& x) {
            fail(std::string(e.id) + ": threw " + x.what());
            continue;
        }
        SW_CHECK(to_bits(run.initial_curva) == e.init_curva &&
                     to_bits(run.initial_pcr) == e.init_pcr,
                 std::string(e.id) + ": init bits");
        SW_CHECK(static_cast<int>(run.snapshots.size()) == e.iterations &&
                     run.converged == (e.converged == 1),
                 std::string(e.id) + ": ladder shape");
        const std::vector<KrEvent> want_events = expected_events(e.id);
        const std::vector<KrSnap> want_snaps = expected_snaps(e.id);
        bool ev_ok = run.events.size() == want_events.size();
        for (std::size_t i = 0; ev_ok && i < run.events.size(); ++i)
            ev_ok = kind_name_local(run.events[i].kind) ==
                        (want_events[i].kind == 0   ? "push"
                         : want_events[i].kind == 1 ? "pop"
                         : want_events[i].kind == 2 ? "info"
                         : want_events[i].kind == 3 ? "remark"
                         : want_events[i].kind == 4 ? "warn"
                                                    : "step") &&
                    run.events[i].level == want_events[i].level &&
                    run.events[i].text == want_events[i].text;
        SW_CHECK(ev_ok, std::string(e.id) + ": event stream");
        bool sn_ok = run.snapshots.size() == want_snaps.size();
        for (std::size_t i = 0; sn_ok && i < run.snapshots.size(); ++i)
            sn_ok = run.snapshots[i].pass == want_snaps[i].pass &&
                    run.snapshots[i].round == want_snaps[i].round &&
                    run.snapshots[i].pass3 == (want_snaps[i].pass3 == 1) &&
                    run.snapshots[i].need_outliers ==
                        (want_snaps[i].need_out == 1) &&
                    run.snapshots[i].need_auto_modelling ==
                        (want_snaps[i].need_auto == 1) &&
                    to_bits(run.snapshots[i].curva) == want_snaps[i].curva &&
                    to_bits(run.snapshots[i].pcr) == want_snaps[i].pcr &&
                    run.snapshots[i].ref_auto == (want_snaps[i].ref_auto == 1) &&
                    run.snapshots[i].ref_stats ==
                        (want_snaps[i].ref_stats == 1) &&
                    run.snapshots[i].seasonal == (want_snaps[i].seasonal == 1) &&
                    run.snapshots[i].original_seasonality_test ==
                        want_snaps[i].ost;
        SW_CHECK(sn_ok, std::string(e.id) + ": snapshots");

        for (const TramoKernelSeam& s : rq.seams) {
            if (s.kind != TramoKernelSeamKind::round0 &&
                s.kind != TramoKernelSeamKind::pass2_gate)
                continue;
            const std::vector<double>& res = s.stats.residuals;
            if (res.empty()) continue;
            ++lb_rows;
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
            if (to_bits(p) == to_bits(s.stats.ljung_box_pvalue)) ++lb_ok;
        }
    }
    SW_CHECK(lb_rows > 0 && lb_ok == lb_rows,
             "ljung-box witness " + std::to_string(lb_ok) + "/" +
                 std::to_string(lb_rows));
    std::printf("ljung-box recompute: %d/%d rows bit-exact\n", lb_ok, lb_rows);
}

}  // namespace

int main() {
    try {
        const std::vector<RawCase> raw = read_corpus(kCorpus);
        cross_qualify(raw);
        replay_all();
    } catch (const std::exception& e) {
        std::cout << "FAIL exception: " << e.what() << std::endl;
        return 1;
    }
    std::cout << (failures == 0 ? "PASS" : "FAIL")
              << " verify_tramo_kernel_rounds: " << expectations().size()
              << " cases, " << checks << " checks, " << failures << " failures"
              << std::endl;
    return failures == 0 ? 0 : 1;
}
