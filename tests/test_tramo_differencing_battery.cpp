// Item 23-C.2: unit qualification of the differencing BATTERY - the
// SeasonalityController passes, the RegularUnder(+2)/SeasonalUnder(+2) loop,
// the SeasonalOverDifferencingTest, the SeasonalityTests ladder and the
// SpectralPeaks gate - replayed natively (kalos::decide_tramo_differencing_
// battery) over the pinned estimation-state facts of
// validation/evidence/native/tramo-differencing-battery (see
// tools/gen_tramo_battery_golden.py for the corpus provenance and the
// double-capture byte-identity). The decision lines (seas1, the four steps
// with their post-step spec/mean/seasonal/ost, seas2, final) and the
// per-state ports (F p-values through the incBeta kernel, ladder scores,
// spectral-peak gates, over-differencing codes) are asserted bit for bit;
// candidate construction is asserted against the captured candidate stream
// and every Changed transfer against the post-step panel. What the replay
// cannot see fails closed - the negative probes at the bottom pin that.
#include "kalos/arma_diagnostics.hpp"
#include "kalos/detail/special_functions.hpp"
#include "kalos/tramo_differencing_battery.hpp"
#include "kalos/tramo_transform_decision.hpp"

#include "jvm_tramo_battery.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using kalos::BatteryCase;
using kalos::BatteryDecision;
using kalos::BatteryPanel;
using kalos::BatteryState;
using kalos::BatteryTests;
using kalos::PeakAr;
using kalos::PeakTukey;
using kalos::battery_has_seasonal_peaks;
using kalos::battery_ladder_score;
using kalos::battery_over_differencing_code;
using kalos::decide_tramo_differencing_battery;
using kalos::detail::f_upper_p;

int failures = 0;
long stat_states = 0, stat_ftests = 0, stat_scores = 0, stat_peaks = 0, stat_over = 0;
long stat_cands = 0, stat_steps = 0, stat_panels = 0, stat_frozen = 0;

void fail(const std::string& cid, const std::string& what) {
    std::printf("FAIL %s: %s\n", cid.c_str(), what.c_str());
    ++failures;
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < line.size()) {
        std::size_t j = line.find(' ', i);
        if (j == std::string::npos) {
            out.push_back(line.substr(i));
            break;
        }
        out.push_back(line.substr(i, j - i));
        i = j + 1;
        while (i < line.size() && line[i] == ' ') {
            ++i;
        }
    }
    return out;
}

double tok(const std::string& s) {
    return std::strtod(s.c_str(), nullptr);  // "NaN" / "null" -> 0.0
}

std::uint64_t bits(double d) {
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof(u));
    return u;
}

bool bits_eq(double a, double b) {
    if (std::isnan(a) && std::isnan(b)) {
        return true;
    }
    return bits(a) == bits(b);
}

bool panel_eq(const BatteryPanel& a, const BatteryPanel& b) {
    return bits_eq(a.ljung_box, b.ljung_box) && bits_eq(a.ljung_box_p, b.ljung_box_p)
           && bits_eq(a.season_box, b.season_box) && bits_eq(a.season_box_p, b.season_box_p)
           && bits_eq(a.bic, b.bic) && a.nobs == b.nobs && a.nout == b.nout
           && bits_eq(a.skew_abs, b.skew_abs) && bits_eq(a.skew_p, b.skew_p)
           && bits_eq(a.stable_mean_p, b.stable_mean_p)
           && bits_eq(a.stable_var_p, b.stable_var_p);
}

// The six observable states, in capture order.
const char* kSuffix[6] = {":e", ":s1", ":ru1", ":su1", ":ru2", ":su2"};

BatteryState* state_at(BatteryCase& bc, std::size_t k) {
    BatteryState* all[6] = {&bc.after_entry, &bc.after_pass1, &bc.after_ru1, &bc.after_su1,
                            &bc.after_ru2, &bc.after_su2};
    return all[k];
}

int state_index(const std::string& id, const std::string& tag) {
    for (int k = 0; k < 6; ++k) {
        if (tag == id + kSuffix[k]) {
            return k;
        }
    }
    return -1;
}

struct StepExp {
    std::string name, result, spec;
    int mean = 0, seasonal = 0, ost = 0;
};

struct Parsed {
    std::string id;
    BatteryCase bc;
    std::map<std::string, std::vector<double>> linearized;
    std::map<std::string, int> over;        // tag -> code (>= 0); absent on error
    std::map<std::string, int> resl_score;  // tag -> captured ladder score
    std::map<std::string, int> has_resl;    // tag -> JVM hasSeasonalPeaks
    std::map<std::string, int> has_ctl;     // tag -> the same, controller side
    std::map<std::string, bool> has_ctl_seen, has_resl_seen;
    std::vector<StepExp> steps;
    std::string seas1, seas1_spec, seas2, seas2_spec;
    int seas1_seasonal = 0, seas1_ost = 0, seas2_seasonal = 0, seas2_ost = 0;
    std::string final_spec;
    int final_mean = 0;
};

PeakAr parse_ar(const std::string& s) {
    if (s == "A") return PeakAr::A;
    if (s == "a") return PeakAr::a;
    if (s == "none") return PeakAr::none;
    return PeakAr::undef;
}

PeakTukey parse_tukey(const std::string& s) {
    if (s == "T") return PeakTukey::T;
    if (s == "t") return PeakTukey::t;
    if (s == "none") return PeakTukey::none;
    return PeakTukey::undef;
}

// Parses the ctlst/resl payload (from `at`: qsv V qsp P npv V npp P dlen N
// tukey set|null ar set|null arr null|N recs has H) into stage facts plus
// the JVM hasSeasonalPeaks verdict echoed next to them.
void parse_tests(const std::vector<std::string>& t, std::size_t at, BatteryTests& out,
                 int& has) {
    for (std::size_t i = at; i + 1 < t.size(); ++i) {
        if (t[i] == "qsv" && i + 3 < t.size()) {
            out.qs_null = t[i + 1] == "null";
            if (!out.qs_null) out.qs_v = tok(t[i + 1]);
            out.qs_p = tok(t[i + 3]);
        } else if (t[i] == "npv" && i + 3 < t.size()) {
            out.np_null = t[i + 1] == "null";
            if (!out.np_null) out.np_v = tok(t[i + 1]);
            out.np_p = tok(t[i + 3]);
        } else if (t[i] == "dlen") {
            out.differenced_length = std::atoi(t[i + 1].c_str());
        } else if (t[i] == "tukey") {
            out.tukey_null = t[i + 1] == "null";
        } else if (t[i] == "ar") {
            out.ar_null = t[i + 1] == "null";
        } else if (t[i] == "arr") {
            if (t[i + 1] != "null") {
                const int n = std::atoi(t[i + 1].c_str());
                for (int k = 0; k < n; ++k) {
                    const std::string& rec = t[i + 2 + static_cast<std::size_t>(k)];
                    const std::size_t slash = rec.find('/');
                    out.peaks.push_back({parse_ar(rec.substr(0, slash)),
                                         parse_tukey(rec.substr(slash + 1))});
                }
            }
        } else if (t[i] == "has") {
            has = std::atoi(t[i + 1].c_str());
        }
    }
}

void parse_panel(const std::vector<std::string>& t, std::size_t at, BatteryPanel& p) {
    for (std::size_t i = at; i + 1 < t.size(); ++i) {
        if (t[i] == "lb") p.ljung_box = tok(t[i + 1]);
        else if (t[i] == "lbp") p.ljung_box_p = tok(t[i + 1]);
        else if (t[i] == "slb") p.season_box = tok(t[i + 1]);
        else if (t[i] == "slbp") p.season_box_p = tok(t[i + 1]);
        else if (t[i] == "bic") p.bic = tok(t[i + 1]);
        else if (t[i] == "nobs") p.nobs = std::atoi(t[i + 1].c_str());
        else if (t[i] == "nout") p.nout = std::atoi(t[i + 1].c_str());
        else if (t[i] == "skp") p.skew_p = tok(t[i + 1]);
        else if (t[i] == "ska") p.skew_abs = tok(t[i + 1]);
        else if (t[i] == "smp") p.stable_mean_p = tok(t[i + 1]);
        else if (t[i] == "svp") p.stable_var_p = tok(t[i + 1]);
    }
}

struct Spec6 {
    int v[6] = {0, 0, 0, 0, 0, 0};
};

Spec6 parse_spec(const std::string& s) {
    Spec6 o;
    std::size_t k = 0;
    std::string num;
    for (char ch : s) {
        if (ch == ',' || ch == '@') {
            if (k < 6) o.v[k++] = std::atoi(num.c_str());
            num.clear();
        } else {
            num.push_back(ch);
        }
    }
    if (k < 6) o.v[k++] = std::atoi(num.c_str());
    return o;
}

std::string spec_token(const BatteryDecision::Snap& s, int freq) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d@%d", s.p, s.d, s.q, s.bp, s.bd, s.bq,
                  freq);
    return buf;
}

// Returns the index in the candidate stream for a "c<k>" tag, growing the
// stream lazily (the cand line precedes its coef/mstat lines).
std::size_t cand_slot(std::vector<kalos::BatteryCandidate>& v, const std::string& tag) {
    const std::size_t k = static_cast<std::size_t>(std::atoi(tag.c_str() + 1));
    while (v.size() <= k) {
        v.emplace_back();
    }
    return k;
}

Parsed parse_case(const kalos::test::TbCase& tc) {
    Parsed p;
    p.id = tc.id;
    p.bc.freq = tc.period;
    p.bc.seasonal = tc.seasonal != 0;
    p.bc.ost = tc.ost;
    for (std::size_t fi = 0; fi < tc.facts_len; ++fi) {
        const std::string line(tc.facts[fi]);
        const auto t = split(line);
        if (t.empty()) {
            continue;
        }
        const std::string& key = t[0];
        const std::string tag = t.size() > 1 ? t[1] : "";
        const int st = state_index(p.id, tag);
        if (key == "state") {
            if (st < 0 || t.size() > 2 && t[2] == "null") {
                continue;
            }
            BatteryState s;
            const Spec6 o = parse_spec(t[3]);
            s.p = o.v[0]; s.d = o.v[1]; s.q = o.v[2];
            s.bp = o.v[3]; s.bd = o.v[4]; s.bq = o.v[5];
            s.mean = t[5] == "1";
            s.nvar = std::atoi(t[7].c_str());
            *state_at(p.bc, static_cast<std::size_t>(st)) = s;
        } else if (key == "coef") {
            std::vector<double> phi, bphi, theta, btheta;
            int which = -1;
            for (std::size_t i = 2; i < t.size(); ++i) {
                if (t[i] == "phi") { which = 0; continue; }
                if (t[i] == "bphi") { which = 1; continue; }
                if (t[i] == "theta") { which = 2; continue; }
                if (t[i] == "btheta") { which = 3; continue; }
                if (which >= 0) {
                    (which == 0 ? phi : which == 1 ? bphi : which == 2 ? theta : btheta)
                        .push_back(tok(t[i]));
                }
            }
            if (st >= 0) {
                BatteryState* s = state_at(p.bc, static_cast<std::size_t>(st));
                s->phi = phi;
                s->bphi1 = bphi.empty() ? 0.0 : bphi[0];
                s->btheta1 = btheta.empty() ? 0.0 : btheta[0];
            }
        } else if (key == "lin" && st >= 0) {
            auto& values = p.linearized[tag];
            for (std::size_t i = 3; i < t.size(); ++i) values.push_back(tok(t[i]));
        } else if (key == "resid") {
            if (st < 0) {
                continue;
            }
            BatteryState* s = state_at(p.bc, static_cast<std::size_t>(st));
            for (std::size_t i = 3; i < t.size(); ++i) {
                s->resid.push_back(tok(t[i]));
            }
        } else if (key == "mstat") {
            BatteryPanel panel;
            parse_panel(t, 2, panel);
            if (!tag.empty() && tag[0] == 'c') {
                p.bc.candidates[cand_slot(p.bc.candidates, tag)].mstat = panel;
            } else if (st >= 0) {
                state_at(p.bc, static_cast<std::size_t>(st))->mstat = panel;
            }
        } else if (key == "cand") {
            // cand cK spec <s> mean <0|1> nvar <n> ok <0|1>
            const Spec6 o = parse_spec(t[3]);
            auto& c = p.bc.candidates[cand_slot(p.bc.candidates, tag)];
            c.p = o.v[0]; c.d = o.v[1]; c.q = o.v[2];
            c.bp = o.v[3]; c.bd = o.v[4]; c.bq = o.v[5];
            c.mean = t[5] == "1";
            c.ok = t[9] == "1";
        } else if (key == "ftest") {
            if (st < 0 || t.size() < 3 || t[2] == "null") {
                continue;
            }
            BatteryState* s = state_at(p.bc, static_cast<std::size_t>(st));
            s->ftest.present = true;
            s->ftest.value = tok(t[3]);
            s->ftest.pvalue = tok(t[5]);
            s->ftest.df1 = tok(t[7]);
            s->ftest.df2 = tok(t[9]);
        } else if (key == "ctlst") {
            if (st < 0) {
                continue;
            }
            BatteryState* s = state_at(p.bc, static_cast<std::size_t>(st));
            parse_tests(t, 2, s->ctlst, p.has_ctl[tag]);
            p.has_ctl_seen[tag] = true;
        } else if (key == "resl") {
            if (st < 0) {
                continue;
            }
            BatteryState* s = state_at(p.bc, static_cast<std::size_t>(st));
            p.resl_score[tag] = std::atoi(t[3].c_str());
            s->resl_score = p.resl_score[tag];
            parse_tests(t, 4, s->resl, p.has_resl[tag]);
            p.has_resl_seen[tag] = true;
        } else if (key == "over") {
            // over <tag> code <c> | over <tag> error <class> <msg...>
            if (t.size() > 3 && t[2] == "code") {
                p.over[tag] = std::atoi(t[3].c_str());
            } else {
                p.over[tag] = -1;  // observer-side exception: not asserted
            }
        } else if (key == "step") {
            // step <id> <name> <result> spec <s> mean <m> seasonal <x> ost <o>
            StepExp e;
            e.name = t[2];
            e.result = t[3];
            e.spec = t[5];
            e.mean = std::atoi(t[7].c_str());
            e.seasonal = std::atoi(t[9].c_str());
            e.ost = std::atoi(t[11].c_str());
            p.steps.push_back(e);
        } else if (key == "seas1") {
            // seas1 <id> <result> spec <s> seasonal <x> ost <o>
            p.seas1 = t[2];
            p.seas1_spec = t[4];
            p.seas1_seasonal = std::atoi(t[6].c_str());
            p.seas1_ost = std::atoi(t[8].c_str());
        } else if (key == "seas2") {
            p.seas2 = t[2];
            p.seas2_spec = t[4];
            p.seas2_seasonal = std::atoi(t[6].c_str());
            p.seas2_ost = std::atoi(t[8].c_str());
        } else if (key == "final") {
            // final <id> spec <s> mean <m>
            p.final_spec = t[3];
            p.final_mean = std::atoi(t[5].c_str());
        }
    }
    return p;
}

const char* result_name(BatteryDecision::Result r) {
    switch (r) {
    case BatteryDecision::Result::Unchanged: return "Unchanged";
    case BatteryDecision::Result::Changed: return "Changed";
    case BatteryDecision::Result::Unprocessed: return "Unprocessed";
    case BatteryDecision::Result::Failed: return "Failed";
    }
    return "?";
}

void replay(const kalos::test::TbCase& tc) {
    Parsed p = parse_case(tc);
    if (p.bc.candidates.empty()) {
        fail(p.id, "no candidates parsed");
        return;
    }
    BatteryDecision d;
    try {
        d = decide_tramo_differencing_battery(p.bc);
    } catch (const std::runtime_error& e) {
        fail(p.id, std::string("native replay threw: ") + e.what());
        return;
    }
    const int freq = p.bc.freq;

    // --- seas1 / steps / seas2 with their post-step snapshots ---------------
    const BatteryDecision::Result step_res[4] = {d.ru1, d.su1, d.ru2, d.su2};
    const int step_cand[5] = {d.cand_pass1, d.cand_ru1, d.cand_su1, d.cand_ru2, d.cand_su2};
    const char* names[4] = {"ru1", "su1", "ru2", "su2"};
    if (d.snaps.size() != 5) {
        fail(p.id, "expected 5 post-step snapshots");
    }
    if (p.seas1 != result_name(d.seas1)) {
        fail(p.id, "seas1: native " + std::string(result_name(d.seas1)) + " vs JVM " + p.seas1);
    }
    if (!d.snaps.empty() && (spec_token(d.snaps[0], freq) != p.seas1_spec
                             || d.snaps[0].seasonal != (p.seas1_seasonal != 0)
                             || d.snaps[0].ost != p.seas1_ost)) {
        fail(p.id, "seas1 snapshot: native " + spec_token(d.snaps[0], freq) + " seasonal="
             + std::to_string(d.snaps[0].seasonal) + " ost=" + std::to_string(d.snaps[0].ost)
             + " vs JVM " + p.seas1_spec + " seasonal=" + std::to_string(p.seas1_seasonal)
             + " ost=" + std::to_string(p.seas1_ost));
    }
    ++stat_steps;
    if (p.steps.size() != 4) {
        fail(p.id, "expected 4 step lines");
    }
    for (std::size_t i = 0; i < p.steps.size() && i < 4; ++i) {
        const StepExp& e = p.steps[i];
        const std::string want = std::string(names[i]) + " " + result_name(step_res[i]);
        if (e.name + " " + e.result != want) {
            fail(p.id, "step: native " + want + " vs JVM " + e.name + " " + e.result);
        }
        const BatteryDecision::Snap& s = d.snaps[i + 1];
        if (spec_token(s, freq) != e.spec || s.mean != (e.mean != 0)
            || s.seasonal != (e.seasonal != 0) || s.ost != e.ost) {
            fail(p.id, "step snapshot " + e.name + ": native " + spec_token(s, freq)
                 + " mean=" + std::to_string(s.mean) + " seasonal=" + std::to_string(s.seasonal)
                 + " ost=" + std::to_string(s.ost) + " vs JVM " + e.spec + " mean="
                 + std::to_string(e.mean) + " seasonal=" + std::to_string(e.seasonal)
                 + " ost=" + std::to_string(e.ost));
        }
        ++stat_steps;
    }
    if (p.seas2 != result_name(d.seas2)) {
        fail(p.id, "seas2: native " + std::string(result_name(d.seas2)) + " vs JVM " + p.seas2);
    }
    // The second pass must leave everything as the four steps left it.
    {
        const BatteryDecision::Snap& last = d.snaps.back();
        if (spec_token(last, freq) != p.seas2_spec || last.seasonal != (p.seas2_seasonal != 0)
            || last.ost != p.seas2_ost) {
            fail(p.id, "seas2: native left " + spec_token(last, freq) + " but JVM saw "
                 + p.seas2_spec);
        }
        if (spec_token(last, freq) != p.final_spec || last.mean != (p.final_mean != 0)) {
            fail(p.id, "final: native " + spec_token(last, freq) + " mean="
                 + std::to_string(last.mean) + " vs JVM " + p.final_spec + " mean="
                 + std::to_string(p.final_mean));
        }
    }
    // Every candidate beyond the initial estimation must be consumed, in
    // order (the JVM estimate() stream is exactly the battery's).
    {
        std::vector<int> want;
        for (std::size_t i = 1; i < p.bc.candidates.size(); ++i) {
            want.push_back(static_cast<int>(i));
        }
        if (d.consumed != want) {
            std::string got, exp;
            for (int x : d.consumed) got += std::to_string(x) + " ";
            for (int x : want) exp += std::to_string(x) + " ";
            fail(p.id, "candidate stream: consumed [" + got + "] vs captured [" + exp + "]");
        }
        stat_cands += static_cast<long>(want.size());
    }

    // --- per-state ports ------------------------------------------------------
    for (int k = 0; k < 6; ++k) {
        const std::string tag = p.id + kSuffix[k];
        BatteryState* st = state_at(p.bc, static_cast<std::size_t>(k));
        ++stat_states;
        if (st->resid.empty() && st->mstat.nobs == 0) {
            fail(p.id, "unparsed state " + tag);
            continue;
        }
        if (freq == 12) {
            const auto compare = [&](const auto& actual, const BatteryTests& expected, const char* source) {
                if (!bits_eq(actual.qs.value, expected.qs_v) || !bits_eq(actual.qs.p_value, expected.qs_p) ||
                    !bits_eq(actual.friedman.value, expected.np_v) || !bits_eq(actual.friedman.p_value, expected.np_p))
                    fail(p.id, std::string(source) + " seasonal statistics differ in " + tag);
            };
            compare(kalos::tramo_seasonality_statistics(st->resid), st->resl, "residual");
            const auto linearized = kalos::tramo_seasonality(p.linearized.at(tag));
            compare(linearized, st->ctlst, "linearized");
            const auto check_tukey = [&](const std::vector<double>& values, const BatteryTests& expected) {
                const auto peaks = kalos::tramo_monthly_tukey_peaks(values);
                if (peaks.empty() != expected.tukey_null)
                    fail(p.id, "Tukey availability in " + tag);
                if (!expected.peaks.empty()) {
                    if (peaks.size() != expected.peaks.size() || !std::equal(peaks.begin(), peaks.end(),
                            expected.peaks.begin(), [](auto peak, const auto& row) { return peak == row.tukey; }))
                        fail(p.id, "Tukey seasonal peaks in " + tag);
                }
            };
            check_tukey(st->resid, st->resl);
            check_tukey(linearized.differenced, st->ctlst);
        }
        const auto tests = kalos::tramo_comparator_residual_tests(st->resid);
        if (!bits_eq(tests.absolute_skewness, st->mstat.skew_abs) ||
            !bits_eq(tests.skewness_p, st->mstat.skew_p) ||
            !bits_eq(tests.stable_mean_p, st->mstat.stable_mean_p) ||
            !bits_eq(tests.stable_variance_p, st->mstat.stable_var_p)) {
            fail(p.id, "computed comparator residual tests differ in " + tag);
        }
        if (st->ftest.present) {
            const double p_native = f_upper_p(st->ftest.value, st->ftest.df1, st->ftest.df2);
            if (!bits_eq(p_native, st->ftest.pvalue)) {
                fail(p.id, "F p-value bit mismatch in " + tag);
            }
            ++stat_ftests;
        }
        if (p.has_resl_seen.count(tag)) {
            if (const int want = p.resl_score[tag]; want != battery_ladder_score(st->resl, freq)) {
                fail(p.id, "ladder score in " + tag + ": native "
                     + std::to_string(battery_ladder_score(st->resl, freq)) + " vs JVM "
                     + std::to_string(want));
            }
            if (p.has_resl[tag] != (battery_has_seasonal_peaks(st->resl.peaks) ? 1 : 0)) {
                fail(p.id, "peaks gate (resl) mismatch in " + tag);
            }
            ++stat_scores;
            ++stat_peaks;
        }
        if (p.has_ctl_seen.count(tag)) {
            if (p.has_ctl[tag] != (battery_has_seasonal_peaks(st->ctlst.peaks) ? 1 : 0)) {
                fail(p.id, "peaks gate (ctlst) mismatch in " + tag);
            }
            ++stat_peaks;
        }
        if (const int want = p.over.count(tag) ? p.over[tag] : -2; want >= 0) {
            if (const int code = battery_over_differencing_code(*st, freq); code != want) {
                fail(p.id, "over code in " + tag + ": native " + std::to_string(code) + " vs JVM "
                     + std::to_string(want));
            }
            ++stat_over;
        }
    }

    // --- transfers --------------------------------------------------------------
    // A step that Changed consumed a candidate and transferred its estimation:
    // the post-step panel must equal the candidate panel bit for bit. A step
    // that ran its model left the live estimation untouched: the pre- and
    // post-step panels must be identical (this also pins the seasonal-flip
    // Changed of the controller pass, which consumes no candidate).
    const int pre_tag[4] = {1, 2, 3, 4};   // :s1, :ru1, :su1, :ru2
    const int post_tag[4] = {2, 3, 4, 5};  // :ru1, :su1, :ru2, :su2
    for (int i = 0; i < 4; ++i) {
        const bool changed = step_res[i] == BatteryDecision::Result::Changed;
        if (changed) {
            if (step_cand[i + 1] < 0) {
                fail(p.id, std::string("Changed ") + names[i] + " consumed no candidate");
                continue;
            }
            const BatteryPanel& cpan =
                p.bc.candidates[static_cast<std::size_t>(step_cand[i + 1])].mstat;
            if (!panel_eq(cpan, state_at(p.bc, static_cast<std::size_t>(post_tag[i]))->mstat)) {
                fail(p.id, std::string("panel transfer mismatch at ") + names[i]);
            }
            ++stat_panels;
        } else {
            // A step may estimate its candidate and still be rejected by the
            // comparator (rw0 su1/su2); the live estimation is then frozen.
            if (!panel_eq(state_at(p.bc, static_cast<std::size_t>(pre_tag[i]))->mstat,
                          state_at(p.bc, static_cast<std::size_t>(post_tag[i]))->mstat)) {
                fail(p.id, std::string("panel moved across an Unchanged ") + names[i]);
            }
            ++stat_frozen;
        }
    }
    // The controller pass: a specification swap transfers its candidate, the
    // seasonal flip leaves the estimation frozen on the :e panel.
    if (d.seas1 == BatteryDecision::Result::Changed) {
        if (d.cand_pass1 >= 0) {
            const BatteryPanel& cpan =
                p.bc.candidates[static_cast<std::size_t>(d.cand_pass1)].mstat;
            if (!panel_eq(cpan, p.bc.after_pass1.mstat)) {
                fail(p.id, "panel transfer mismatch at the controller pass");
            }
            ++stat_panels;
        } else {
            if (!panel_eq(p.bc.after_entry.mstat, p.bc.after_pass1.mstat)) {
                fail(p.id, "panel moved across the seasonal-flip Changed");
            }
            ++stat_frozen;
        }
    } else {
        if (!panel_eq(p.bc.after_entry.mstat, p.bc.after_pass1.mstat)) {
            fail(p.id, "panel moved across an Unchanged controller pass");
        }
        ++stat_frozen;
    }
}

// --- fail-closed probes -------------------------------------------------------

BatteryCase probe_case() {
    BatteryCase c;
    c.freq = 12;
    c.seasonal = true;
    c.ost = 0;
    c.candidates.emplace_back();  // c0
    for (BatteryState* s : {&c.after_entry, &c.after_pass1, &c.after_ru1, &c.after_su1,
                            &c.after_ru2, &c.after_su2}) {
        s->mean = true;
        s->resid = {1.0, 2.0, 3.0, 4.0};
        s->mstat.nobs = 4;
        s->mstat.season_box_p = 0.5;
        s->ftest.present = true;
        s->ftest.pvalue = 0.5;
    }
    return c;
}

void negative_probes() {
    // (a) su2 proceeding over a calendar-bearing description fails closed.
    {
        BatteryCase c = probe_case();
        c.after_entry.ftest.pvalue = 0.001;  // hasSeasonality score 1 -> ctx.ost
        c.after_ru2.bd = 0;
        c.after_ru2.bq = 0;
        c.after_ru2.bp = 0;
        c.after_ru2.nvar = 2;
        bool hit = false;
        try {
            decide_tramo_differencing_battery(c);
        } catch (const std::runtime_error&) {
            hit = true;
        }
        if (!hit) {
            fail("negative", "su2 calendar guard did not fail closed");
        }
    }
    // (b) a step candidate missing from the pinned stream fails closed.
    {
        BatteryCase c = probe_case();
        c.after_ru1.resl.qs_p = 0.0001;
        c.after_ru1.resl.np_p = 0.0001;  // ladder score 2 -> su1 must estimate
        bool hit = false;
        try {
            decide_tramo_differencing_battery(c);
        } catch (const std::runtime_error&) {
            hit = true;
        }
        if (!hit) {
            fail("negative", "missing candidate snapshot did not fail closed");
        }
    }
    // (c) quasi-unit-root scans above degree 2 fail closed.
    {
        BatteryCase c = probe_case();
        c.after_pass1.d = 1;
        c.after_pass1.p = 3;
        c.after_pass1.phi = {0.5, 0.3, 0.2};
        c.after_pass1.resid = {1.0, 1.0, 1.0, 1.0};  // var 0 -> |t| = inf
        bool hit = false;
        try {
            decide_tramo_differencing_battery(c);
        } catch (const std::runtime_error&) {
            hit = true;
        }
        if (!hit) {
            fail("negative", "degree-3 quasi-unit-root scan did not fail closed");
        }
    }
    // (d) a null SeasonalFTest on the reference model fails closed.
    {
        BatteryCase c = probe_case();
        c.after_entry.ftest.present = false;
        bool hit = false;
        try {
            decide_tramo_differencing_battery(c);
        } catch (const std::runtime_error&) {
            hit = true;
        }
        if (!hit) {
            fail("negative", "absent SeasonalFTest did not fail closed");
        }
    }
}

void check_monthly_spectra() {
    std::ifstream input(KALOS_SEASONAL_PEAKS);
    std::size_t n;
    int seed;
    std::string expected_ar, expected_tukey;
    int count = 0;
    while (input >> n >> seed >> expected_ar >> expected_tukey) {
        std::uint32_t state = seed + 1;
        std::vector<double> x(n);
        for (std::size_t i = 0; i < n; ++i) {
            state = 1664525u * state + 1013904223u;
            const int month = (i + seed) % 12;
            const int seasonal = (7 * month + month * month) % 17 - 8;
            x[i] = (static_cast<int>(state >> 16) - 32768) / 4096.0 + seasonal * (seed % 4) / 2.0;
        }
        std::string actual;
        for (auto peak : kalos::tramo_monthly_tukey_peaks(x))
            actual += peak == PeakTukey::T ? '2' : peak == PeakTukey::t ? '1' : '0';
        if ((actual.empty() ? "-" : actual) != expected_tukey)
            fail("Tukey spectrum", "length " + std::to_string(n) + ", seed " + std::to_string(seed));
        ++count;
    }
    if (count != 168) fail("monthly spectrum", "incomplete classifier corpus");
}

} // namespace

int main() {
    std::size_t count = 0;
    const kalos::test::TbCase* cases = kalos::test::cases(count);
    long total_facts = 0;
    for (std::size_t i = 0; i < count; ++i) {
        replay(cases[i]);
        total_facts += static_cast<long>(cases[i].facts_len);
    }
    check_monthly_spectra();
    negative_probes();
    std::printf("tramo-differencing-battery: %zu cases, %ld fact lines, %ld states, %ld "
                "F p-values, %ld ladder scores, %ld peak gates, %ld over-codes, %ld decision "
                "lines, %ld candidates, %ld panel transfers, %ld frozen panels, %d failures\n",
                count, total_facts, stat_states, stat_ftests, stat_scores, stat_peaks,
                stat_over, stat_steps, stat_cands, stat_panels, stat_frozen, failures);
    return failures == 0 ? 0 : 1;
}
