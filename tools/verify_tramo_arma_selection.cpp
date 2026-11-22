// Full-corpus verification for item 23-D: replays every case of the pinned
// evidence corpus (validation/evidence/native/tramo-arma-selection by
// default; an alternative corpus directory may be passed as the single
// argument) through kalos::select_tramo_arma and compares the probe lines
// (direct_sel, the five dpanel rows, arm_result, after and the full event
// log) against arma-facts.csv / arma-events.csv line for line with the
// shared replay renderer (tests/tramo_arma_replay.h). The quarterly row
// (cases with freq != 12) is an EXPECTED D11 REFUSAL (the unit must throw);
// rows without a resvals hand-over (the constant series, whose
// olsResiduals fails upstream) are record-only. The pcnt/maxspec gate lines
// and the resvals vectors ride along inert.
#include <kalos/tramo_arma_selection.hpp>

#include "../tests/tramo_arma_replay.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string& what) {
    std::cout << "FAIL " << what << std::endl;
    ++failures;
}

std::vector<std::string> read_lines(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open " + path);
    }
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            out.push_back(line);
        }
    }
    return out;
}

// The capture writes fully-quoted, comma-separated fields (R write.csv).
std::vector<std::string> csv_fields(const std::string& line) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < line.size()) {
        if (line[pos] == '"') {
            std::string field;
            std::size_t i = pos + 1;
            while (i < line.size()) {
                if (line[i] == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        field += '"';
                        i += 2;
                        continue;
                    }
                    break;
                }
                field += line[i++];
            }
            out.push_back(field);
            // skip the closing quote and the following comma
            i = (i < line.size() ? i + 1 : i);
            if (i < line.size() && line[i] == ',') {
                ++i;
            }
            pos = i;
        } else {
            const std::size_t end = line.find(',', pos);
            out.push_back(line.substr(pos, end == std::string::npos
                        ? std::string::npos
                        : end - pos));
            pos = end == std::string::npos ? line.size() : end + 1;
        }
    }
    return out;
}

struct CaseData {
    std::vector<std::string> inputs;
    std::vector<std::pair<std::string, std::string>> facts;
    std::vector<std::string> events; // full "evt ..." lines
};

std::vector<std::string> produced_want(const CaseData& c) {
    std::vector<std::string> want;
    for (const auto& kv : c.facts) {
        const std::string& key = kv.first;
        if (key == "direct_sel" || key == "dpanel" || key == "arm_result"
            || key == "after") {
            want.push_back(key + " " + kv.second);
        }
    }
    for (const auto& e : c.events) {
        want.push_back(e);
    }
    return want;
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir
        = argc > 1 ? argv[1] : "validation/evidence/native/tramo-arma-selection";
    std::map<std::string, CaseData> cases;
    std::vector<std::string> order;

    for (const auto& line : read_lines(dir + "/inputs.csv")) {
        const auto f = csv_fields(line);
        if (f.size() < 2 || f[0] == "id") {
            continue;
        }
        if (!cases.count(f[0])) {
            order.push_back(f[0]);
        }
        cases[f[0]].inputs.push_back(f[1]);
    }
    for (const auto& line : read_lines(dir + "/arma-facts.csv")) {
        const auto f = csv_fields(line);
        if (f.size() < 3 || f[0] == "id") {
            continue;
        }
        cases[f[0]].facts.push_back({f[1], f[2]});
    }
    for (const auto& line : read_lines(dir + "/arma-events.csv")) {
        const auto f = csv_fields(line);
        if (f.size() < 6 || f[0] == "id") {
            continue;
        }
        cases[f[0]].events.push_back("evt " + f[1] + " " + f[2] + " " + f[3] + " "
            + f[4] + " det " + f[5]);
    }

    int replayed = 0, refused = 0, record = 0;
    for (const auto& id : order) {
        const CaseData& c = cases[id];
        kalos::TramoArmaSelectionRequest r;
        bool has_res = false;
        for (const auto& line : c.inputs) {
            std::istringstream ls(line);
            std::string tag;
            ls >> tag;
            if (tag == "req") {
                // "req arma freq 12 seasonal 0 wn 0"
                std::string key;
                int freq = 12;
                ls >> key >> key >> freq >> key >> r.seasonal >> key
                   >> r.accept_white_noise;
                r.period = freq;
            } else if (tag == "in") {
                std::string tok;
                int n = 0;
                ls >> tok >> tok >> n >> tok >> r.nobs >> tok >> r.period >> tok;
                std::string spec;
                ls >> spec; // (p,d,q)(bp,bd,bq)
                int p0, d0, q0, bp0, bd0, bq0;
                std::sscanf(spec.c_str(), "(%d,%d,%d)(%d,%d,%d)", &p0, &d0, &q0, &bp0,
                    &bd0, &bq0);
                r.p0 = p0;
                r.d0 = d0;
                r.q0 = q0;
                r.bp0 = bp0;
                r.bd0 = bd0;
                r.bq0 = bq0;
            } else if (tag == "resvals") {
                std::string tok;
                int nres = 0;
                ls >> tok >> nres;
                std::string next;
                if (!(ls >> next)) {
                    continue;
                }
                if (next == "error") {
                    continue; // upstream failure: record only
                }
                r.residuals.push_back(std::strtod(next.c_str(), nullptr));
                while (ls >> tok) {
                    r.residuals.push_back(std::strtod(tok.c_str(), nullptr));
                }
                has_res = true;
            }
        }

        if (r.period != 12) {
            bool thrown = false;
            try {
                kalos::select_tramo_arma(r);
            } catch (const std::invalid_argument&) {
                thrown = true;
            }
            if (!thrown) {
                fail(id + ": non-monthly request was not refused");
            } else {
                ++refused;
            }
            continue;
        }
        if (!has_res) {
            ++record; // olsResiduals threw upstream; no hand-over exists
            continue;
        }

        const auto d = kalos::select_tramo_arma(r);
        const auto got = kalos::replay::produced(id, d);
        const auto want = produced_want(c);
        if (got.size() != want.size()) {
            fail(id + ": line count " + std::to_string(got.size()) + " vs "
                + std::to_string(want.size()));
            continue;
        }
        bool ok = true;
        for (std::size_t i = 0; i < got.size() && ok; ++i) {
            if (!kalos::replay::line_equal(want[i], got[i])) {
                fail(id + " line " + std::to_string(i) + "\n  want: " + want[i]
                    + "\n  got:  " + got[i]);
                ok = false;
            }
        }
        if (ok) {
            ++replayed;
        }
    }
    std::cout << "verify_tramo_arma_selection: " << replayed << " replayed, " << refused
              << " D11 refusals, " << record << " record-only, " << failures
              << " failures" << std::endl;
    return failures == 0 ? 0 : 1;
}
