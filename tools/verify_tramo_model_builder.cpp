// Full-sweep verification: the native item 23-A TRAMO model-builder mirror
// against the pinned jdplus 3.9.0 corpus
// (validation/evidence/native/tramo-model-builder; an alternative corpus
// directory may be passed as the single argument). Every case is replayed
// through the shared probe-line driver (tests/tramo_model_builder_replay.h,
// the same code the generated-golden unit test uses): the request our
// driver builds from the captured protocol lines must echo byte-identical
// "req" lines to the live TramoSpec getter block the JVM captured (this is
// the template-expansion-table gate), the rendered ModelDescription facts
// must equal builder-facts.csv line for line (decision fields exact,
// %.17g doubles bit-exact - the unit is pure bookkeeping plus the pinned
// (a + b) / 2.0 average, so the whole unit is bit-tier), and the
// ProcessingLog warnings must equal builder-events.csv. The two non-monthly
// record rows (cases.csv freq != 12) are EXPECTED REFUSALS: the request
// echo still has to match, but kalos::build_tramo_model must throw
// (register D11, monthly-first envelope).
#include <kalos/tramo_model_builder.hpp>

#include "../tests/tramo_model_builder_replay.h"

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

double from_bits(const std::string& tok) {
    if (tok == "nan") return std::numeric_limits<double>::quiet_NaN();
    return std::strtod(tok.c_str(), nullptr);
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

struct CorpusCase {
    std::string id;
    bool reject = false;
    std::vector<std::string> lines;  // src
    std::vector<std::string> req;
    std::vector<double> values;
    std::vector<std::string> facts;
    std::vector<std::string> events;
};

template <typename T>
void expect_eq(const std::vector<T>& got, const std::vector<T>& want, const std::string& what) {
    if (got.size() != want.size()) {
        fail(what + ": " + std::to_string(got.size()) + " lines vs " +
             std::to_string(want.size()));
        for (std::size_t i = 0; i < want.size(); ++i)
            std::cout << "  want[" << i << "] " << want[i] << std::endl;
        for (std::size_t i = 0; i < got.size(); ++i)
            std::cout << "  got[" << i << "]  " << got[i] << std::endl;
        return;
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (!(got[i] == want[i])) {
            fail(what + " line " + std::to_string(i));
            std::cout << "  want " << want[i] << std::endl << "  got  " << got[i] << std::endl;
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "validation/evidence/native/tramo-model-builder";
    std::map<std::string, CorpusCase> cases;
    std::vector<std::string> order;

    {
        auto rows = read_lines(dir + "/cases.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = tmbreplay::csv_fields(rows[i]);
            CorpusCase c;
            c.id = f[0];
            c.reject = f[2] != "12";  // freq column
            cases[c.id] = c;
            order.push_back(c.id);
        }
    }
    {
        auto rows = read_lines(dir + "/inputs.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = tmbreplay::csv_fields(rows[i]);
            auto& c = cases.at(f[0]);
            const std::string& line = f[1];
            if (line.rfind("src ", 0) == 0) {
                c.lines.push_back(line);
            } else if (line.rfind("req ", 0) == 0) {
                c.req.push_back(line);
            } else if (line.rfind("values ", 0) == 0) {
                // manual split on ';' (values contain no commas, tokens no spaces)
                const std::string payload = line.substr(7);
                std::size_t start = 0;
                while (start <= payload.size()) {
                    const std::size_t next = payload.find(';', start);
                    const std::size_t stop = next == std::string::npos ? payload.size() : next;
                    c.values.push_back(from_bits(payload.substr(start, stop - start)));
                    if (next == std::string::npos) break;
                    start = next + 1;
                }
            } else {
                fail("inputs.csv line outside the corpus grammar: " + line);
            }
        }
    }
    {
        auto rows = read_lines(dir + "/builder-facts.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = tmbreplay::csv_fields(rows[i]);
            if (f[1] == "case") continue;
            cases.at(f[0]).facts.push_back(f[1] + " " + f[2]);
        }
    }
    {
        auto rows = read_lines(dir + "/builder-events.csv");
        for (std::size_t i = 1; i < rows.size(); ++i) {
            auto f = tmbreplay::csv_fields(rows[i]);
            if (f[3] != "0" || f[5] != "-")
                fail("event detail/depth outside the renderable envelope: " + f[0]);
            cases.at(f[0]).events.push_back(f[2] + " " + f[4]);
        }
    }

    for (const auto& id : order) {
        const CorpusCase& c = cases.at(id);
        tmbreplay::parsed_case parsed;
        try {
            parsed = tmbreplay::parse_case(c.lines);
        } catch (const std::exception& e) {
            fail(id + ": driver: " + e.what());
            continue;
        }
        parsed.req.values = c.values;
        expect_eq(tmbreplay::format_req(parsed), c.req, id + " req");
        if (c.reject) {
            try {
                kalos::build_tramo_model(parsed.req);
                fail(id + ": expected refusal (non-monthly record case)");
            } catch (const std::invalid_argument&) {
            }
            std::cout << "case " << id << " refused as recorded" << std::endl;
            continue;
        }
        try {
            const auto built = kalos::build_tramo_model(parsed.req);
            expect_eq(tmbreplay::format_facts(built), c.facts, id + " facts");
            expect_eq(tmbreplay::format_events(built), c.events, id + " events");
        } catch (const std::exception& e) {
            fail(id + ": build threw: " + e.what());
        }
    }

    if (failures == 0) {
        std::cout << "PASS: tramo model builder, " << order.size() << " corpus cases"
                  << std::endl;
        return 0;
    }
    std::cout << failures << " failures" << std::endl;
    return 1;
}
