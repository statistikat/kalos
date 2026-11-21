// Full-sweep verification: the native item 24-A SEATS admissibility / root
// handling / decomposability mirror against the pinned jdplus 3.9.0 corpus
// (validation/evidence/native/tramo-seats-model; an alternative corpus
// directory may be passed as the single argument). Unlike the generated-golden
// unit test this tool reads the csv evidence DIRECTLY, so it also gates the
// evidence layer itself: cases.csv (decision matrix), inputs.csv (the "req"
// protocol lines), model-facts.csv (the 24-A assertable fact grammar) and
// model-events.csv (the kevt kernel stream) are parsed and replayed through
// the shared driver code (tests/tramo_seats_model_replay.h, the same
// parse_request / Checker the golden unit test runs). Validator facts, raw
// decomposition coefficients (bit-exact through the %.17g re-parse), the
// decomposability decision mirror, the kernel flags and the kevt frame
// (exact, exact-prefix for the legacy_approx D6 continuation) must all match.
// Five record cases carry the documented D11 refusal (degree > 2 root
// solving: d_d2, q_airline, q_d2, v_cut_bq2, v_simplify); for them the tool
// requires the unit to throw and counts the refusal. The final section is the
// determinism double-run plus the fail-closed envelope probes (non-{4,12}
// period, non-finite parameter, stationary bphi, degree > 2 phi/theta).
#include <kalos/tramo_seats_model_validator.hpp>

#include "../tests/tramo_seats_model_replay.h"

#include <cmath>
#include <cstdio>
#include <cstring>
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
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

// "a","b c","d""e" -> fields with the quotes removed.
std::vector<std::string> csv_fields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur += '"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                cur += ch;
            }
        } else if (ch == '"') {
            quoted = true;
        } else if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    out.push_back(cur);
    return out;
}

// Same grammar as tools/gen_tramo_seats_model_golden.py.
const char* const ASSERT_KEYS[] = {"v.changed", "v.xl",       "v.orders",  "v.phi",
                                   "v.bphi",    "v.theta",    "v.btheta",  "r.decompose",
                                   "r.count",   "d.decompose", "k.cutOff", "k.modelChanged"};
const char* const ASSERT_R_COLS[] = {"sar", "ur", "sfd"};

bool is_assert_key(const std::string& key) {
    for (const char* k : ASSERT_KEYS)
        if (key == k) return true;
    return false;
}

bool is_reject(const std::string& id) {
    const char* const ids[] = {"d_d2", "q_airline", "q_d2", "v_cut_bq2", "v_simplify"};
    for (const char* r : ids)
        if (id == r) return true;
    return false;
}

struct Stored {
    std::vector<std::string> req, facts, events;
    std::vector<const char*> reqp, factsp, eventsp;
    golden_tsm::Case c;
};

std::string hex_bits(double v) {
    std::ostringstream os;
    os << std::hex << replay_tsm::bits(v);
    return os.str();
}

std::string repr_result(const kalos::tramo_seats_model_result& r) {
    std::ostringstream os;
    auto dbl = [&](const char* n, const std::vector<double>& v) {
        os << n;
        for (double x : v) os << ' ' << hex_bits(x);
        os << ';';
    };
    os << (r.parameters_cut_off ? 1 : 0) << (r.model_changed ? 1 : 0)
       << (r.decomposition_threw ? 1 : 0) << (r.variance_max_ok ? 1 : 0)
       << (r.seats_exception ? 1 : 0) << (r.approximation_required ? 1 : 0)
       << (r.noisy_model_used ? 1 : 0);
    os << r.orders_p << r.orders_d << r.orders_q << r.orders_bp << r.orders_bd << r.orders_bq;
    dbl("x", {r.xl});
    dbl("v", {r.variance_floor});
    dbl("phi", r.phi);
    dbl("bphi", r.bphi);
    dbl("theta", r.theta);
    dbl("btheta", r.btheta);
    os << "comps" << r.raw_components.size() << ';';
    for (const auto& comp : r.raw_components) {
        dbl("sar", comp.sar);
        dbl("ur", comp.ur);
        dbl("sfd", comp.sfd);
    }
    for (const auto& e : r.events)
        os << "evt" << e.kind << e.depth << e.msg << ';';
    return os.str();
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "validation/evidence/native/tramo-seats-model";
    std::map<std::string, Stored> stored;
    std::vector<std::string> order;
    std::map<std::string, std::string> status, mode, ddecompose;

    {
        auto rows = read_lines(dir + "/cases.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = csv_fields(rows[i]);
            if (f.size() < 16) {
                fail("cases.csv row outside the corpus grammar: " + rows[i]);
                continue;
            }
            const std::string id = f[0];
            if (!stored.count(id)) {
                stored[id] = Stored();
                order.push_back(id);
            }
            status[id] = f[5];
            mode[id] = f[7];
            ddecompose[id] = f[10];
        }
    }
    {
        auto rows = read_lines(dir + "/inputs.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = csv_fields(rows[i]);
            if (f[1].rfind("req ", 0) == 0) stored.at(f[0]).req.push_back(f[1]);
        }
    }
    {
        auto rows = read_lines(dir + "/model-facts.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = csv_fields(rows[i]);
            const std::string key = f[1], val = f[2];
            if (is_assert_key(key)) {
                stored.at(f[0]).facts.push_back(key + " " + val);
            } else if (key == "r") {
                auto w = replay_tsm::tokens(val);
                if (w.size() >= 3 && w[0].rfind("c", 0) == 0) {
                    for (const char* col : ASSERT_R_COLS)
                        if (w[1] == col) {
                            stored.at(f[0]).facts.push_back(key + " " + val);
                            break;
                        }
                }
            }
        }
    }
    {
        auto rows = read_lines(dir + "/model-events.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = csv_fields(rows[i]);
            if (f[1] != "kevt") continue;
            stored.at(f[0]).events.push_back(f[3] + " " + f[4] + " " + f[5]);
        }
    }

    // Fix up the in-memory golden tables (stable storage).
    for (const std::string& id : order) {
        Stored& s = stored.at(id);
        for (const std::string& x : s.req) s.reqp.push_back(x.c_str());
        for (const std::string& x : s.facts) s.factsp.push_back(x.c_str());
        for (const std::string& x : s.events) s.eventsp.push_back(x.c_str());
        s.c.id = id.c_str();
        s.c.status = status.at(id).c_str();
        s.c.mode = mode.at(id).c_str();
        s.c.legacy_approx = (mode.at(id) == "Legacy" && ddecompose.at(id) == "null") ? 1 : 0;
        s.c.reject = is_reject(id) ? 1 : 0;
        s.c.nreq = s.reqp.size();
        s.c.req = s.reqp.data();
        s.c.nfacts = s.factsp.size();
        s.c.facts = s.factsp.data();
        s.c.nevents = s.eventsp.size();
        s.c.events = s.eventsp.data();
    }

    int checks = 0;
    std::vector<std::string> first_run;
    for (const std::string& id : order) {
        const golden_tsm::Case& c = stored.at(id).c;
        kalos::tramo_seats_model_request req;
        std::string err;
        if (!replay_tsm::parse_request(c, req, err)) {
            fail(id + ": request parse: " + err);
            continue;
        }
        if (c.reject) {
            ++checks;
            try {
                kalos::validate_tramo_seats_model(req);
                fail(id + ": D11 refusal expected, unit accepted");
            } catch (const std::exception&) {
            }
            std::cout << "case " << id << " refused as recorded (D11)" << std::endl;
            continue;
        }
        kalos::tramo_seats_model_result res;
        try {
            res = kalos::validate_tramo_seats_model(req);
        } catch (const std::exception& e) {
            fail(id + ": unit refused: " + e.what());
            continue;
        }
        std::vector<std::string> log;
        replay_tsm::Checker k{c, res, 0, 0, &log};
        replay_tsm::check_facts(k);
        replay_tsm::check_flags_and_events(k);
        for (const std::string& p : log) fail(p);
        checks += k.checks;
        first_run.push_back(id + "\t" + repr_result(res));
        std::cout << "case " << id << " " << k.checks << " checks" << std::endl;
    }

    // Determinism: a second identical run must reproduce every result.
    for (const std::string& id : order) {
        const golden_tsm::Case& c = stored.at(id).c;
        if (c.reject) continue;
        kalos::tramo_seats_model_request req;
        std::string err;
        if (!replay_tsm::parse_request(c, req, err)) continue;
        try {
            const auto again = kalos::validate_tramo_seats_model(req);
            ++checks;
            bool same = false;
            for (const std::string& row : first_run)
                if (row.rfind(id + "\t", 0) == 0 && row == id + "\t" + repr_result(again))
                    same = true;
            if (!same) fail(id + ": non-deterministic re-run");
        } catch (const std::exception& e) {
            fail(id + ": re-run threw: " + e.what());
        }
    }
    std::cout << "determinism double-run checked" << std::endl;

    // Fail-closed envelope probes (register D11).
    kalos::tramo_seats_model_request base;  // spec 0 1 1 0 1 1 12, theta 0.5, btheta 0.4
    base.phi = {};
    base.theta = {0.5};
    base.btheta = {0.4};
    auto probe = [&](const char* name, kalos::tramo_seats_model_request r) {
        ++checks;
        try {
            kalos::validate_tramo_seats_model(r);
            fail(std::string("envelope probe ") + name + ": unit accepted");
        } catch (const std::invalid_argument&) {
        } catch (const std::exception&) {
            fail(std::string("envelope probe ") + name + ": wrong exception type");
        }
    };
    {
        auto r = base;
        r.period = 6;
        probe("period 6", r);
    }
    {
        auto r = base;
        r.frequency = 6;
        probe("frequency 6", r);
    }
    {
        auto r = base;
        r.theta[0] = std::numeric_limits<double>::quiet_NaN();
        probe("NaN theta", r);
    }
    {
        auto r = base;
        r.bphi = {0.2};
        probe("stationary bphi", r);
    }
    {
        auto r = base;
        r.phi = {0.1, 0.1, 0.1};
        probe("degree-3 phi", r);
    }
    {
        auto r = base;
        r.btheta = {0.4, 0.2, 0.1};
        probe("degree-3 btheta", r);
    }
    std::cout << "envelope refusal probes checked" << std::endl;

    if (failures == 0) {
        std::cout << "PASS: tramo seats model, " << order.size() << " corpus cases, " << checks
                  << " checks" << std::endl;
        return 0;
    }
    std::cout << failures << " failures" << std::endl;
    return 1;
}
