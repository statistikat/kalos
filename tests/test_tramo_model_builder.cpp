// Item 23-A unit qualification of the TRAMO model-builder decision mirror
// (kalos/tramo_model_builder.hpp): a replay of the double-captured JVM
// corpus (validation/evidence/native/tramo-model-builder, harvested into
// jvm_tramo_model_builder.h) through the shared probe-line driver
// (tramo_model_builder_replay.h). Three comparisons per case: the
// JVM-echoed request block against the request our driver builds from the
// same protocol lines (this pins the tramo_template expansion table and
// every request override against the live TramoSpec getters), the built
// ModelDescription fact lines (start rule, SARIMA spec, log/LoP/mean
// flags, missing plan, sorted variable list - all byte-identical strings
// with %.17g doubles), and the ProcessingLog warnings. The two non-monthly
// record cases must be REFUSED by the unit (register D11) even though the
// JVM built them. Structural cases cover the template-name matrix, the
// fail-closed rejection matrix and determinism. The corpus is re-verified
// end-to-end from the csv files by the sweep tools/verify_tramo_model_builder.cpp.
#include "kalos/tramo_model_builder.hpp"

#include "jvm_tramo_model_builder.h"
#include "tramo_model_builder_replay.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string& what) {
    std::printf("FAIL %s\n", what.c_str());
    ++failures;
}

double from_bits(std::uint64_t u) {
    double d = 0.0;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

std::string bits_of(double d) {
    char buf[32];
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof(u));
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(u));
    return buf;
}

template <typename T>
void expect_eq(const std::vector<T>& got, const std::vector<T>& want, const std::string& what) {
    if (got.size() != want.size()) {
        fail(what + ": " + std::to_string(got.size()) + " lines vs " +
             std::to_string(want.size()));
        for (std::size_t i = 0; i < want.size(); ++i)
            std::printf("  want[%zu] %s\n", i, want[i].c_str());
        for (std::size_t i = 0; i < got.size(); ++i)
            std::printf("  got[%zu]  %s\n", i, got[i].c_str());
        return;
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (!(got[i] == want[i])) {
            fail(what + " line " + std::to_string(i));
            std::printf("  want %s\n  got  %s\n", want[i].c_str(), got[i].c_str());
        }
    }
}

void expect_throw(const std::string& what, const std::function<void()>& run) {
    try {
        run();
        fail(what + ": expected a refusal");
    } catch (const std::invalid_argument&) {
        // the pinned fail-closed boundary
    } catch (const std::exception& e) {
        fail(what + ": unexpected exception: " + e.what());
    }
}

std::vector<std::string> to_lines(const char* const* arr, std::size_t n) {
    return std::vector<std::string>(arr, arr + n);
}

void replay_case(const golden_tmb::Case& c) {
    const std::string id = c.id;
    tmbreplay::parsed_case parsed;
    try {
        parsed = tmbreplay::parse_case(to_lines(c.lines, c.nlines));
    } catch (const std::exception& e) {
        fail(id + ": driver: " + e.what());
        return;
    }
    for (std::size_t i = 0; i < c.nvalues; ++i)
        parsed.req.values.push_back(from_bits(c.values[i]));

    // 1. request echo against the live TramoSpec getter lines.
    expect_eq(tmbreplay::format_req(parsed), to_lines(c.req, c.nreq), id + " req");

    if (c.reject) {
        // Non-monthly record case: the JVM built it; the unit refuses (D11).
        expect_throw(id + " refusal", [&] { kalos::build_tramo_model(parsed.req); });
        return;
    }

    // 2. built ModelDescription facts.
    kalos::tramo_build_result built;
    try {
        built = kalos::build_tramo_model(parsed.req);
    } catch (const std::exception& e) {
        fail(id + ": build threw: " + e.what());
        return;
    }
    expect_eq(tmbreplay::format_facts(built), to_lines(c.facts, c.nfacts), id + " facts");

    // 3. ProcessingLog warnings.
    expect_eq(tmbreplay::format_events(built), to_lines(c.events, c.nevents), id + " events");
}

void template_matrix() {
    const char* accepted[14] = {"tr0", "tr1", "tr2", "tr3", "tr4", "tr5", "trfull",
                                "TR0", "TR1", "TR2", "TR3", "TR4", "TR5", "TRfull"};
    const char* canonical[7] = {"TR0", "TR1", "TR2", "TR3", "TR4", "TR5", "TRfull"};
    for (int i = 0; i < 14; ++i) {
        const kalos::tramo_template_fields* f = kalos::tramo_template(accepted[i]);
        if (f == nullptr) {
            fail(std::string("template rejected: ") + accepted[i]);
            continue;
        }
        if (std::strcmp(f->name, canonical[i % 7]) != 0)
            fail(std::string("template row mismatch for ") + accepted[i]);
    }
    const char* refused[] = {"Trfull", "trFull", "TRfullx", "rsa0", "rsa1", "rsa2",
                             "rsa3", "rsa4", "rsa5", "rsafull", "TR", "tr6", "", " tr0"};
    for (const char* name : refused) {
        if (kalos::tramo_template(name) != nullptr)
            fail(std::string("template accepted: ") + name);
    }
}

kalos::tramo_build_request base_request() {
    const kalos::tramo_template_fields* f = kalos::tramo_template("TR1");
    kalos::tramo_build_request r;
    r.using_auto_model = f->using_auto_model;
    r.transform_function = f->transform_function;
    r.transform_adjust = f->transform_adjust;
    r.arima = f->arima;
    r.mean = f->mean;
    r.trading_days = f->trading_days;
    r.easter = f->easter;
    r.delta_tc = f->delta_tc;
    r.values.assign(96, 50.0);
    return r;
}

void rejection_matrix() {
    {
        auto r = base_request();
        r.annual_frequency = 4;
        expect_throw("quarterly frequency", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.values[3] = std::numeric_limits<double>::infinity();
        expect_throw("infinite value", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.values.assign(12, std::numeric_limits<double>::quiet_NaN());
        expect_throw("all-missing series", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.values.clear();
        expect_throw("empty series", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.start_month = 13;
        expect_throw("start month 13", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.using_auto_model = false;
        r.arima.period = 4;
        expect_throw("declared period 4", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.user_variables.push_back({"u1", true});
        expect_throw("resolved user variable", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.trading_days.used = true;
        r.trading_days.stock = true;
        expect_throw("stock trading days", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.trading_days.used = true;
        r.trading_days.type = kalos::tramo_trading_days_type::td7;
        r.trading_days.lop = kalos::tramo_lop::leap_year;
        expect_throw("TD7 registration", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.trading_days.used = true;
        r.trading_days.type = kalos::tramo_trading_days_type::td2;
        expect_throw("registered td with lop None", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.easter.used = true;
        r.easter.type = kalos::tramo_easter_type::standard;
        r.easter.julian = true;
        expect_throw("julian easter", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        r.easter.used = true;
        r.easter.type = kalos::tramo_easter_type::include_easter;
        expect_throw("include-easter registration", [&] { kalos::build_tramo_model(r); });
    }
    {
        auto r = base_request();
        kalos::tramo_outlier_request o;
        o.name = "x";
        o.code = "ao";
        o.position_month = 13;
        r.outliers.push_back(o);
        expect_throw("outlier month 13", [&] { kalos::build_tramo_model(r); });
    }
    // Gated calendar paths build cleanly and register nothing (tr2/tr4/tr5/
    // trfull evidence): used+test and used+automatic must NOT register.
    {
        auto r = base_request();
        r.trading_days = kalos::tramo_template("TR2")->trading_days;
        r.easter = kalos::tramo_template("TR2")->easter;
        const auto built = kalos::build_tramo_model(r);
        if (!built.variables.empty()) fail("tested calendar still registered variables");
    }
}

void interpolation_plan() {
    // Structural cross-check of the pinned fill rules on a hand-built case
    // (the corpus pins the same rules at the captured positions): interior
    // run (left + right) / 2, leading run = first finite value, trailing
    // run = the finite value to the left, missing list ascending.
    auto r = base_request();
    r.mean = kalos::tramo_mean_spec{};  // keep the output small; mean off
    r.values.resize(12);
    for (int i = 0; i < 12; ++i) r.values[static_cast<std::size_t>(i)] = i + 1;
    r.values[0] = std::numeric_limits<double>::quiet_NaN();
    r.values[1] = std::numeric_limits<double>::quiet_NaN();
    r.values[4] = std::numeric_limits<double>::quiet_NaN();
    r.values[5] = std::numeric_limits<double>::quiet_NaN();
    r.values[10] = std::numeric_limits<double>::quiet_NaN();
    r.values[11] = std::numeric_limits<double>::quiet_NaN();
    const auto built = kalos::build_tramo_model(r);
    const std::vector<int> want_missing{0, 1, 4, 5, 10, 11};
    if (built.missing != want_missing) fail("interpolation plan: missing list");
    const double want[] = {3.0, 3.0,  3.0, 4.0, 5.5, 5.5,
                           7.0, 8.0,  9.0, 10.0, 10.0, 10.0};
    for (std::size_t i = 0; i < 12; ++i) {
        if (bits_of(built.interpolated[i]) != bits_of(want[i]))
            fail("interpolation plan: slot " + std::to_string(i));
    }
}

void determinism() {
    auto r = base_request();
    r.user_variables.push_back({"u1", false});
    kalos::tramo_outlier_request o;
    o.name = "ao";
    o.code = "AO";
    o.position_year = 1994;
    o.position_month = 3;
    r.outliers.push_back(o);
    const auto a = tmbreplay::format_facts(kalos::build_tramo_model(r));
    const auto b = tmbreplay::format_facts(kalos::build_tramo_model(r));
    expect_eq(b, a, "determinism");
}

}  // namespace

int main() {
    std::size_t count = 0;
    const golden_tmb::Case* cases = golden_tmb::table(count);
    for (std::size_t i = 0; i < count; ++i) replay_case(cases[i]);
    template_matrix();
    rejection_matrix();
    interpolation_plan();
    determinism();
    if (failures == 0) {
        std::printf("ok: tramo model builder, %zu corpus cases + structural matrix\n", count);
        return 0;
    }
    std::printf("%d failures\n", failures);
    return 1;
}
