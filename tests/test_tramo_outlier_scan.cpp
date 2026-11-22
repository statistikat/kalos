// Item 23-B replay test: drives kalos::scan_tramo_outliers (round-0 TRAMO
// outlier scan) over the generated golden JVM corpus
// (tests/jvm_tramo_outlier_scan.h) and additionally checks the static
// request envelope. The replay reconstructs each request from the captured
// SCAN protocol line, runs the round-0 machine and compares the decision
// status and the first selected [position, type] pair against the captured
// OutliersDetectionModule facts; the request echoes (critical value, factory
// count, resolved bounds) are checked on every case. The two divergent cases
// (so, spanout) carry the multi-round removal of the round-0 pick (register
// D3 evidence): the replay there pins the request echoes and the round-0
// selection itself. The pexao47 case pins the documented fail-closed throw
// of the warm re-estimation seam (non-constant design, D3 decision-brief
// evidence).
//
// Item 23-F F.2 extends this unit with the faithful multi-round machine: the
// replay of tests/jvm_tramo_outlier_rounds.h drives kalos::
// tramo_outlier_rounds over the Scratch23g dual-machine witness and compares
// the FULL trace - every FastOutliersDetector/SOD field dump bit-exactly,
// the add records, the manret note and the final manouts set - for all 37
// capture cases. The three 23-B divergent rows (so, spanout, spannone) flip
// here from divergent flags to positive multi-round assertions: the rounds
// machine reproduces the live module's empty "Unchanged" selections.
#include "kalos/tramo_outlier_rounds.hpp"
#include "kalos/tramo_outlier_scan.hpp"

#include "jvm_tramo_outlier_rounds.h"
#include "jvm_tramo_outlier_scan.h"
#include "tramo_outlier_scan_replay.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

#define CHECK(cond)                                                                            \
    if (!(cond)) {                                                                             \
        throw std::runtime_error("check failed: " #cond " (line " + std::to_string(__LINE__)   \
            + ")");                                                                            \
    }

using kalos::TramoOutlierKind;
using kalos::TramoOutlierScanDecision;
using kalos::TramoOutlierScanRequest;

std::vector<std::string> to_lines(const char* const* lines, std::size_t count) {
    return {lines, lines + count};
}

std::vector<double> to_values(const std::uint64_t* bits, std::size_t count) {
    std::vector<double> out(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t b = bits[i];
        std::memcpy(&out[i], &b, sizeof(double));
    }
    return out;
}

TramoOutlierScanDecision::Status status_of(const std::string& captured) {
    if (captured == "Failed") {
        return TramoOutlierScanDecision::Status::Failed;
    }
    if (captured == "Unchanged") {
        return TramoOutlierScanDecision::Status::Unchanged;
    }
    if (captured == "Changed") {
        return TramoOutlierScanDecision::Status::Selected;
    }
    throw std::runtime_error("unknown captured status: " + captured);
}

void replay_case(const golden_tos::Case& c) {
    const auto lines = to_lines(c.lines, c.nlines);
    if (lines.size() != 1) {
        throw std::runtime_error("case carries more than one protocol line");
    }
    auto ps = tosr::parse_scan(tosr::split(lines[0]), to_values(c.values, c.nvalues));
    if (ps.cv_is_calc) {
        ps.req.cv = tosr::cv_from_facts(to_lines(c.facts, c.nfacts));
    }

    if (c.seam_throw != nullptr && c.seam_throw[0] != '\0') {
        // Documented fail-closed construction (warm re-estimation over a
        // non-constant design): the seam throws, it never approximates.
        bool thrown = false;
        std::string message;
        try {
            kalos::scan_tramo_outliers(ps.req);
        } catch (const std::exception& e) {
            thrown = true;
            message = e.what();
        }
        CHECK(thrown);
        CHECK(message.rfind(c.seam_throw, 0) == 0);
        return;
    }

    const auto d = kalos::scan_tramo_outliers(ps.req);

    // Request echoes on every case.
    CHECK(d.critical_value == ps.req.cv);
    int nfac = 0;
    for (const bool on : {ps.req.ao, ps.req.ls, ps.req.tc, ps.req.so}) {
        nfac += on ? 1 : 0;
    }
    CHECK(d.factories == nfac);
    CHECK(d.lower_bound == ps.req.lower_bound);
    CHECK(d.upper_bound == ps.req.upper_bound);

    if (c.divergent) {
        // The live multi-round driver later empties the set (D3 evidence);
        // the round-0 machine still selects the pick the module observed.
        CHECK(d.status == TramoOutlierScanDecision::Status::Selected);
        return;
    }

    CHECK(d.status == status_of(c.status));
    const std::string first = c.outliers;
    if (!first.empty()) {
        if (first.rfind("[", 0) != 0) {
            throw std::runtime_error("malformed outliers grammar");
        }
        const auto close = first.find(']');
        const auto comma = first.find(',');
        const int pos = std::stoi(first.substr(1, comma - 1));
        const int type = std::stoi(first.substr(comma + 1, close - comma - 1));
        CHECK(d.status == TramoOutlierScanDecision::Status::Selected);
        CHECK(d.position == pos);
        CHECK(d.type == type);
    }
    if (d.status == TramoOutlierScanDecision::Status::Selected) {
        CHECK(std::isfinite(d.max_t));
        CHECK(std::fabs(d.max_t) >= d.critical_value);
        CHECK(std::isfinite(d.coefficient));
    }
}

// ---- Item 23-F F.2 multi-round replay -----------------------------------------

std::uint64_t bits_of(double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, sizeof(double));
    return b;
}

std::string first_bit_mismatch(const std::vector<double>& v,
    const std::uint64_t* bits, std::size_t n) {
    if (v.size() != n) {
        return "size " + std::to_string(v.size()) + " != " + std::to_string(n);
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (bits_of(v[i]) != bits[i]) {
            double want = 0.0;
            std::memcpy(&want, &bits[i], sizeof want);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "index %zu got %.17g#%016llx want %.17g#%016llx", i, v[i],
                static_cast<unsigned long long>(bits_of(v[i])), want,
                static_cast<unsigned long long>(bits[i]));
            return buf;
        }
    }
    return {};
}

void check_round_state(std::size_t i, const golden_tor::ExpState& e,
    const kalos::TramoOutlierRoundState& s) {
    const std::string at = "event " + std::to_string(i) + " state: ";
    const auto check = [&](bool ok, const char* what) {
        if (!ok) {
            throw std::runtime_error(at + what);
        }
    };
    check(e.round == s.round, "round");
    check(e.rflag == (s.rflag ? 1 : 0), "rflag");
    check(e.estep == (s.estimation_step ? 1 : 0), "estimation_step");
    check(e.bstep == (s.backward_step ? 1 : 0), "backward_step");
    check(e.exit_flag == (s.exit ? 1 : 0), "exit");
    check(static_cast<std::size_t>(e.outs) == s.outliers.size(), "outs count");
    for (int k = 0; k < e.outs; ++k) {
        check(e.outs_pos[k] == s.outliers[k][0], "outs position");
        check(e.outs_type[k] == s.outliers[k][1], "outs kind");
    }
    check(e.lastrem_len == (s.lastremoved ? 2 : -1), "lastremoved");
    check(e.coeff_null == (s.coeff_null ? 1 : 0), "coeff null");
    if (!e.coeff_null) {
        const auto m = first_bit_mismatch(s.coeff, e.coeff, e.coeff_n);
        check(m.empty(), ("coeff " + m).c_str());
    }
    check(e.tstats_null == (s.tstats_null ? 1 : 0), "tstats null");
    if (!e.tstats_null) {
        const auto m = first_bit_mismatch(s.tstats, e.tstats, e.tstats_n);
        check(m.empty(), ("tstats " + m).c_str());
    }
    check(e.res_null == (s.res_null ? 1 : 0), "res null");
    if (!e.res_null) {
        const auto m = first_bit_mismatch(s.res, e.res, e.res_n);
        check(m.empty(), ("res " + m).c_str());
    }
    {
        const auto m = first_bit_mismatch(s.pars, e.pars, e.pars_n);
        check(m.empty(), ("pars " + m).c_str());
    }
    check(e.xcount == static_cast<int>(s.xcount), "xcount");
}

void check_round_sod(std::size_t i, const golden_tor::ExpSod& e,
    const kalos::TramoOutlierRoundScanDump& d) {
    const std::string at = "event " + std::to_string(i) + " sod: ";
    const auto check = [&](bool ok, const char* what) {
        if (!ok) {
            throw std::runtime_error(at + what);
        }
    };
    check(e.el_null == (d.el_null ? 1 : 0), "el null");
    check(e.el_n == d.el_count, "el count");
    check(e.mad == bits_of(d.mad), "mad");
    check(e.lb == d.lower_bound, "lb");
    check(e.ub == d.upper_bound, "ub");
    check(e.t_null == (d.t_table_present ? 0 : 1), "T");
    check(e.alw_null == (d.allowed_present ? 0 : 1), "alw");
    check(e.alw_rows == d.allowed_rows, "alw rows");
    check(e.alw_cols == d.allowed_cols, "alw cols");
    check(e.alw_counts != nullptr || d.allowed_counts.empty(), "alw counts ptr");
    for (std::size_t k = 0; k < e.alw_cols && k < d.allowed_counts.size(); ++k) {
        check(static_cast<std::size_t>(e.alw_counts[k]) == d.allowed_counts[k],
            "alw count");
    }
    check(d.allowed_counts.size() == e.alw_cols, "alw count count");
    check(e.maxt_threw == (d.maxt_threw ? 1 : 0), "maxT threw");
    if (!e.maxt_threw) {
        check(e.maxt == bits_of(d.max_t), "maxT");
    }
    check(e.pick_pos == d.pick_position, "pick position");
    check(e.pick_type == d.pick_type, "pick type");
    if (e.has_cf) {
        check(e.cf == bits_of(d.coefficient), "pick coefficient");
    }
}

void replay_rounds_case(const golden_tor::Case& c) {
    auto ps = tosr::parse_scan(tosr::split(c.line), to_values(c.values, c.nvalues));
    // The mrd line wins for everything make()/calc() resolves (the critical
    // value, the SOD bounds - the fields the witness machine actually ran
    // with - and the crossed FIELD caps).
    std::memcpy(&ps.req.cv, &c.cv, sizeof(double));
    ps.req.lower_bound = c.lb;
    ps.req.upper_bound = c.ub;
    ps.req.max_round = c.maxround;
    ps.req.max_outliers = c.maxout;

    if (c.make_null) {
        const auto res = kalos::tramo_outlier_rounds(ps.req);
        CHECK(res.status == kalos::TramoOutlierRoundsResult::Status::Failed);
        CHECK(res.trace.empty());
        return;
    }

    const auto res = kalos::tramo_outlier_rounds(ps.req);
    if (res.trace.size() != c.nevents) {
        throw std::runtime_error("trace size " + std::to_string(res.trace.size())
            + " != expected " + std::to_string(c.nevents));
    }
    bool orders_checked = false;
    for (std::size_t i = 0; i < c.nevents; ++i) {
        const auto& e = c.events[i];
        const auto& ev = res.trace[i];
        if (std::strcmp(e.tag, ev.tag.c_str()) != 0) {
            throw std::runtime_error(
                "event " + std::to_string(i) + ": tag " + ev.tag + " != " + e.tag);
        }
        CHECK(e.has_state == (ev.has_state ? 1 : 0));
        if (e.has_state) {
            check_round_state(i, *e.st, ev.state);
            if (!orders_checked) {
                CHECK(e.st->orders[0] == ps.req.p);
                CHECK(e.st->orders[1] == ps.req.d);
                CHECK(e.st->orders[2] == ps.req.q);
                CHECK(e.st->orders[3] == ps.req.bp);
                CHECK(e.st->orders[4] == ps.req.bd);
                CHECK(e.st->orders[5] == ps.req.bq);
                CHECK(e.st->mean == (ps.req.mean ? 1 : 0));
                orders_checked = true;
            }
        }
        if (e.has_sod) {
            check_round_sod(i, *e.sod, ev.sod);
        }
        if (e.has_add) {
            CHECK(ev.add_position == e.add_pos);
            CHECK(ev.add_type == e.add_type);
            CHECK(bits_of(ev.add_coefficient) == e.add_cf);
        }
        if (e.has_manret) {
            CHECK(ev.manret_returned == (e.manret == 1));
            CHECK(ev.note == e.note);
            CHECK(ev.manret_round == e.manret_round);
            CHECK(res.note == e.note);
            CHECK(res.round == e.manret_round);
            CHECK(res.status
                == (e.manret == 1 ? kalos::TramoOutlierRoundsResult::Status::Ok
                                  : kalos::TramoOutlierRoundsResult::Status::Failed));
        }
    }

    // The final set: the witness manouts line, the live module's selection
    // (the probe's dual-machine agreement pinned manouts == real, 34/34).
    const auto& last = c.events[c.nevents - 1];
    CHECK(std::strcmp(last.tag, "manouts") == 0);
    if (res.outliers.size() != static_cast<std::size_t>(last.nouts)) {
        throw std::runtime_error("final set size "
            + std::to_string(res.outliers.size()) + " != manouts "
            + std::to_string(last.nouts));
    }
    for (int k = 0; k < last.nouts; ++k) {
        CHECK(res.outliers[k][0] == last.outs_pos[k]);
        CHECK(res.outliers[k][1] == last.outs_type[k]);
    }
    // D3 flip: the live result status and the final set agree on emptiness
    // for every captured case (the round-0 divergent rows included).
    if (std::strcmp(c.real_status, "Unchanged") == 0) {
        CHECK(res.outliers.empty());
    }
    if (std::strcmp(c.real_status, "Changed") == 0) {
        CHECK(!res.outliers.empty());
    }
    // Explicit flip assertions for the three 23-B divergent rows.
    if (std::strcmp(c.id, "sc_so") == 0 || std::strcmp(c.id, "sc_spanout") == 0) {
        CHECK(std::strcmp(c.real_status, "Unchanged") == 0);
        CHECK(res.status == kalos::TramoOutlierRoundsResult::Status::Ok);
        CHECK(res.note == "exit");
        CHECK(res.outliers.empty());
    }
    if (std::strcmp(c.id, "sc_spannone") == 0) {
        CHECK(res.status == kalos::TramoOutlierRoundsResult::Status::Ok);
        CHECK(res.note == "sodfalse");
        CHECK(res.outliers.empty());
    }
}

// ---- Static envelope ----------------------------------------------------------

TramoOutlierScanRequest base_request(std::vector<double> y) {
    TramoOutlierScanRequest r;
    r.y = std::move(y);
    r.frequency = 12;
    r.mean = true;
    r.log = true;
    r.p = 1;
    r.d = 1;
    r.q = 1;
    r.bp = 0;
    r.bd = 1;
    r.bq = 1;
    r.ao = r.ls = r.tc = r.so = true;
    r.tc_rate = 0.7;
    r.precision = 1e-7;
    r.maximum_likelihood = false;
    r.max_round = 50;
    r.max_outliers = 30;
    r.cv = 3.415;
    r.interpolate = true;
    r.lower_bound = 0;
    r.upper_bound = static_cast<int>(r.y.size());
    return r;
}

std::vector<double> series(int n, double base) {
    std::vector<double> y;
    y.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        y.push_back(base + 0.01 * i);
    }
    return y;
}

void test_envelope_throws() {
    auto r = base_request(series(96, 50.0));

    r.frequency = 4;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.d = 0;
    r.bd = 0;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.y[10] = std::nan("");
    r.interpolate = false;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.prespec.emplace_back(20, TramoOutlierKind::So);
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
    r = base_request(series(96, 50.0));

    r.y[3] = -1.0;
    try {
        kalos::scan_tramo_outliers(r);
        CHECK(false);
    } catch (const std::invalid_argument&) {
    }
}

void test_short_series_failed() {
    // n = 20: need = max(1 + 1 + 12, 1 + 12) + 1 + 3 + 12 = 30 > 20, the
    // module reports ProcessingResult.Failed (comatip < 0).
    auto r = base_request(series(20, 50.0));
    r.cv = 3.3;
    const auto d = kalos::scan_tramo_outliers(r);
    CHECK(d.status == TramoOutlierScanDecision::Status::Failed);
    CHECK(d.factories == 4);
    CHECK(d.critical_value == 3.3);
}

void test_collapsed_bounds_unchanged() {
    auto r = base_request(series(96, 50.0));
    r.lower_bound = 0;
    r.upper_bound = 0;
    const auto d = kalos::scan_tramo_outliers(r);
    CHECK(d.status == TramoOutlierScanDecision::Status::Unchanged);
    CHECK(d.lower_bound == 0 && d.upper_bound == 0);
}

void test_empty_factories_unchanged() {
    auto r = base_request(series(96, 50.0));
    r.ao = r.ls = r.tc = r.so = false;
    const auto d = kalos::scan_tramo_outliers(r);
    CHECK(d.status == TramoOutlierScanDecision::Status::Unchanged);
    CHECK(d.factories == 0);
}

} // namespace

int main() {
    try {
        test_envelope_throws();
        test_short_series_failed();
        test_collapsed_bounds_unchanged();
        test_empty_factories_unchanged();

        std::size_t count = 0;
        const golden_tos::Case* cases = golden_tos::table(count);
        for (std::size_t i = 0; i < count; ++i) {
            try {
                replay_case(cases[i]);
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string("case ") + cases[i].id + ": " + e.what());
            }
        }

        std::size_t rcount = 0;
        const golden_tor::Case* const* rcases = golden_tor::table(rcount);
        for (std::size_t i = 0; i < rcount; ++i) {
            try {
                replay_rounds_case(*rcases[i]);
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string("rounds case ") + rcases[i]->id + ": " + e.what());
            }
        }
    } catch (const std::exception& e) {
        std::puts(e.what());
        return 1;
    }
    return 0;
}
