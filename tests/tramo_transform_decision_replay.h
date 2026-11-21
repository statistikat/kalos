#ifndef KALOS_TESTS_TRAMO_TRANSFORM_DECISION_REPLAY_H
#define KALOS_TESTS_TRAMO_TRANSFORM_DECISION_REPLAY_H

// Shared replay layer for the item 23-B tramo-transform corpus: the
// probe-line driver (parses the oracle protocol lines captured in inputs.csv
// into the kalos request types, mirroring TramoTransformDecisionProbe.java
// line for line, including the "def"/"calc" pseudo-tokens and the fail-closed
// refusal of the robust-correction cases held by register D3) plus the fact
// and event formatters that render kalos results back into the captured line
// grammar ("key value" with the Java %.17g doubles, reusing the 23-A g17
// renderer). Both tests/test_tramo_transform_decision.cpp (generated golden
// header) and tools/verify_tramo_transform_decision.cpp (raw csv corpus)
// drive the units through this header.

#include <kalos/tramo_transform_decision.hpp>
#include <kalos/detail/fdlibm.hpp>

#include "tramo_model_builder_replay.h" // g17, to_lines helpers

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ttdreplay {

using tmbreplay::g17;

inline std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

inline double tok(const std::string& s) {
    return std::strtod(s.c_str(), nullptr);
}

// One parsed probe request of any of the three kinds.
struct ParsedCase {
    std::string kind; // calccv | season | transform
    std::size_t n = 0;        // calccv observation count
    int freq = 12;
    std::vector<double> values;
    // Transform request echo (mirrors the probe header line):
    bool mean = false, detect = false, specseas = false, robust = false;
    double log_preference = 0.0, precision = 0.0, cv = 0.0;
};

// Parses the captured protocol lines (the inputs.csv lines minus the values
// line); values come from the IEEE-754 bit table and are needed here for the
// deferred "calc" critical-value token.
inline ParsedCase parse_case(const std::vector<std::string>& lines,
    const std::vector<double>& values) {
    ParsedCase parsed;
    parsed.values = values;
    for (const std::string& line : lines) {
        const auto t = split(line);
        if (t.size() >= 2 && t[0] == "CALCCV") { // CALCCV id n
            parsed.kind = "calccv";
            parsed.n = static_cast<std::size_t>(std::stoul(t[2]));
        } else if (t.size() >= 3 && t[0] == "SEASON") { // SEASON id freq
            parsed.kind = "season";
            parsed.freq = std::stoi(t[2]);
            if (parsed.freq != 12) {
                throw std::invalid_argument("replay: non-monthly season case (D11)");
            }
        } else if (t.size() >= 10 && t[0] == "TRANSFORM") {
            parsed.kind = "transform";
            parsed.freq = std::stoi(t[2]);
            parsed.mean = t[3] == "y";
            parsed.detect = t[4] == "y";
            parsed.specseas = t[5] == "y";
            // The probe default: Math.log(0.95) on the pinned fdlibm surface.
            parsed.log_preference = t[6] == "def" ? kalos::detail::fdlibm_log(0.95) : tok(t[6]);
            parsed.precision = tok(t[7]);
            parsed.robust = t[8] == "y";
            parsed.cv = t[9] == "calc"
                ? kalos::tramo_critical_value(static_cast<std::size_t>(parsed.values.size()))
                            : tok(t[9]);
            if (parsed.freq != 12) {
                throw std::invalid_argument("replay: non-monthly transform case (D11)");
            }
            if (parsed.robust) {
                // The robust AO+LS correction chains the multi-round outlier
                // driver held by open register D3: no kalos request can carry
                // it, so the driver refuses rather than dropping the flag.
                throw std::invalid_argument("replay: robust correction refused (D3)");
            }
        }
    }
    if (parsed.kind.empty()) throw std::invalid_argument("replay: no protocol line");
    return parsed;
}

inline std::string bools(bool v) { return v ? "1" : "0"; }

// calccv: one "calccv n N v V" fact line.
inline std::vector<std::string> render_calccv(std::size_t n, double v) {
    return {"calccv n " + std::to_string(n) + " v " + g17(v)};
}

// The probe's Chi2 description: "Chi2 with <df> degrees of freedom " with
// the trailing space and the Java Double.toString of the integral df.
inline std::string chi2_desc(double df) {
    return "Chi2 with " + std::to_string(static_cast<long long>(df)) + ".0 degrees of freedom ";
}

// season: the raw-getter fact block in probe order.
inline std::vector<std::string> render_season(const ParsedCase& c,
    const kalos::TramoSeasonalityReport& r) {
    std::vector<std::string> out;
    out.push_back("in n " + std::to_string(c.values.size()) + " freq " + std::to_string(c.freq));
    out.push_back("season_verdict " + r.verdict + " " + std::to_string(r.to_int));
    out.push_back("qs_val " + g17(r.qs.value));
    out.push_back("qs_p " + g17(r.qs.p_value));
    out.push_back("qs_desc " + chi2_desc(2.0));
    out.push_back(std::string("qs_valid ") + (r.qs.valid ? "true" : "false"));
    out.push_back("np_val " + g17(r.friedman.value));
    out.push_back("np_p " + g17(r.friedman.p_value));
    out.push_back("np_desc " + chi2_desc(11.0));
    out.push_back(std::string("np_valid ") + (r.friedman.valid ? "true" : "false"));
    std::string diff = "diff n " + std::to_string(r.differenced.size());
    for (const double v : r.differenced) diff += " " + g17(v);
    out.push_back(diff);
    return out;
}

// transform: the module-fact block in probe order (the robust-only lines are
// unreachable: the driver refuses those requests).
inline std::vector<std::string> render_transform_facts(
    const ParsedCase& c, const kalos::TramoTransformDecisionResult& r) {
    std::vector<std::string> out;
    out.push_back("in n " + std::to_string(c.values.size()) + " freq " + std::to_string(c.freq)
        + " mean " + bools(c.mean) + " detect " + bools(c.detect)
        + " specseas " + bools(c.specseas) + " logpref " + g17(c.log_preference)
        + " prec " + g17(c.precision) + " robust " + bools(c.robust) + " cv " + g17(c.cv));
    if (c.detect) {
        out.push_back("season_verdict " + r.seasonality.verdict + " "
            + std::to_string(r.seasonality.to_int));
        out.push_back("qs " + g17(r.seasonality.qs.value) + " " + g17(r.seasonality.qs.p_value));
        out.push_back(
            "np " + g17(r.seasonality.friedman.value) + " " + g17(r.seasonality.friedman.p_value));
    }
    out.push_back("seasonal " + bools(r.seasonal));
    out.push_back("spec_pre " + r.spec_pre);
    out.push_back("loglevel_status " + r.loglevel_status);
    out.push_back("choosing_log " + bools(r.choosing_log));
    out.push_back("log_ll " + g17(r.log_ll));
    out.push_back("level_ll " + g17(r.level_ll));
    out.push_back("log_correction " + g17(r.log_correction));
    out.push_back("log_pref " + g17(r.log_preference));
    out.push_back("log_transform " + bools(r.log_transform));
    out.push_back("need_estimation 1"); // No estimation is ever attached here.
    return out;
}

// transform: the ProcessingLog line grammar, the probe Observer rendering:
// the downgrade remark (depth 0), the log-level scope with the negative
// remark (captured without its text), the chosen branch info, or the Failed
// warning.
inline std::vector<std::string> render_transform_events(
    const kalos::TramoTransformDecisionResult& r) {
    std::vector<std::string> out;
    for (const std::string& m : r.remarks) {
        if (m == "No identifiable seasonality") out.push_back("remark " + m);
    }
    out.push_back("push log-level test");
    if (r.nonpositive) {
        out.push_back("remark"); // The Observer records no text for remark(String).
    } else if (r.loglevel_status == "Failed") {
        out.push_back("warning log/level failed");
    } else {
        out.push_back("info loglevel info logs=" + g17(r.log_ll) + " levels=" + g17(r.level_ll)
            + " logpref=" + g17(r.log_preference));
    }
    out.push_back("pop");
    return out;
}

} // namespace ttdreplay

#endif
