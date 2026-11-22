// Full-sweep verification: the native item 21-I.1 calendar-generation mirror
// against the pinned jdplus 3.9.0 double captures
// (validation/evidence/native/calendar-generation/{gen.out,gen2.out}; an
// alternative corpus directory may be passed as the single argument). Unlike
// the golden unit this tool parses the raw capture grammar directly: every
// "row <family> <i> <%.17g values...>" line is replayed through the public
// calendar_generation API (td_count, mean_days, trading_days_contrast,
// length_of_period_column, easter_column, julian_easter_column, ramp_column,
// user_column) and BIT-compared (the captured %.17g decimals round-trip
// exactly, so strtod + bit equality is the pinning test). The scalar sections
// (computus rows, probability tables, meanDays vectors, tdCount matrices,
// name/description lines, the r_==c_ and eas_==easS_ equivalences) replay as
// separate claims. Failures print family/row/col with got/want bit patterns.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "kalos/calendar_generation.hpp"

namespace {

long long g_checks = 0, g_failures = 0;
std::vector<std::string> g_problems;

void note(const std::string& tag) {
    ++g_failures;
    if (g_problems.size() < 60) g_problems.push_back(tag);
}

std::string hexbits(double d) {
    std::uint64_t u;
    std::memcpy(&u, &d, 8);
    char b[24];
    std::snprintf(b, sizeof(b), "%016llx", static_cast<unsigned long long>(u));
    return b;
}

void chk(const std::string& tag, double got, double want) {
    ++g_checks;
    if (got == want && std::signbit(got) == std::signbit(want)) return;
    if (std::isnan(got) && std::isnan(want)) return;
    ++g_failures;
    if (g_problems.size() < 60)
        g_problems.push_back(tag + ": got " + hexbits(got) + " want " + hexbits(want));
}

std::vector<std::string> tokens(const std::string& s) {
    std::vector<std::string> t;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        std::size_t j = i;
        while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
        if (j > i) t.push_back(s.substr(i, j - i));
        i = j;
    }
    return t;
}

std::vector<double> bracket_doubles(const std::string& line) {
    std::string s = line.substr(line.find('[') + 1);
    s.erase(s.rfind(']'));
    for (auto& ch : s)
        if (ch == ',') ch = ' ';
    std::vector<double> v;
    for (auto& x : tokens(s)) v.push_back(std::strtod(x.c_str(), nullptr));
    return v;
}

// "easter=1985-04-07" or "jul(true)=1985-04-14" -> 407 (month*100+day)
int md_of(const std::string& s) {
    std::size_t p = s.find_last_of('-');
    return std::atoi(s.c_str() + p - 2) * 100 + std::atoi(s.c_str() + p + 1);
}

struct Capture {
    std::map<std::string, std::vector<std::vector<double>>> rows;
    std::map<std::string, std::vector<double>> means;             // tag -> vector
    std::map<std::string, std::vector<std::vector<double>>> tdc;  // tag -> 7 rows
    struct Comp { int year, easter, julT, julF; double pjul, prob; };
    std::vector<Comp> comp;
    std::map<int, double> pjul, prob;
    std::map<std::string, std::string> kv;  // scalar/name lines kept verbatim
    int nrowlines = 0;
};

void load(Capture& c, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        std::exit(2);
    }
    char buf[262144];
    while (std::fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        auto t = tokens(line);
        if (t.empty()) continue;
        if (t[0] == "row") {
            ++c.nrowlines;
            std::size_t i = static_cast<std::size_t>(std::atol(t[2].c_str()));
            std::vector<double> v;
            for (std::size_t j = 3; j < t.size(); ++j)
                v.push_back(std::strtod(t[j].c_str(), nullptr));
            auto& rows = c.rows[t[1]];
            if (rows.size() <= i) rows.resize(i + 1);
            rows[i] = std::move(v);
        } else if (t[0] == "mean12") {
            c.means["12"] = bracket_doubles(line);
        } else if (t[0] == "mean") {
            c.means[t[1][0] == '[' ? std::string() : t[1]] = bracket_doubles(line);
        } else if (t[0] == "tdc") {
            // gen.out keys rows by period label, gen2.out by domain tag; the
            // ISO labels sort chronologically, so insertion order is kept by
            // folding them all under "tdc".
            std::string key = t[1].find('/') != std::string::npos
                                  ? "tdc"
                                  : "tdc " + t[1];  // gen2 keys by domain tag
            std::vector<double> v = bracket_doubles(line);
            std::vector<int> iv;
            iv.reserve(v.size());
            for (double x : v) iv.push_back(static_cast<int>(x));
            std::vector<double> dv;
            dv.reserve(iv.size());
            for (int x : iv) dv.push_back(static_cast<double>(x));
            c.tdc[key].push_back(std::move(dv));
        } else if (t[0] == "eas") {
            Capture::Comp r;
            r.year = std::atoi(t[1].c_str());
            r.easter = md_of(t[2]);
            r.julT = md_of(t[4]);
            r.julF = md_of(t[5]);
            r.pjul = std::strtod(t[7].c_str(), nullptr);
            r.prob = std::strtod(t[9].c_str(), nullptr);
            c.comp.push_back(r);
        } else if (t[0] == "pjul") {
            c.pjul[std::atoi(t[1].c_str())] = std::strtod(t[2].c_str(), nullptr);
        } else if (t[0] == "prob") {
            c.prob[std::atoi(t[1].c_str())] = std::strtod(t[2].c_str(), nullptr);
        } else {
            c.kv[line] = line;
        }
    }
    std::fclose(f);
}

// ------------------------------------------------------------- case table --

struct Dom { int year, month, freq, len; };
const Dom DOMS[] = {
    {1990, 1, 12, 108},   // 0  A (gen + gen2)
    {1989, 4, 12, 90},    // 1  M1
    {1990, 3, 12, 48},    // 2  M2
    {1990, 1, 4, 28},     // 3  Q1
    {1991, 4, 4, 30},     // 4  Q2
    {1990, 1, 1, 20},     // 5  Y1
    {1991, 2, 12, 11},    // 6  M3 (leapless)
    {2006, 1, 12, 36},    // 7  D
    {1990, 1, 4, 36},     // 8  Q (gen q_td2/q_eas8 record)
    {1992, 1, 12, 48},    // 9  ua_S data domain
    {1988, 1, 12, 40},    // 10 ua_PRE
    {1997, 6, 12, 40},    // 11 ua_POST
    {1985, 1, 12, 24},    // 12 ua_BEF
    {1999, 1, 12, 24},    // 13 ua_AFT
    {1989, 1, 12, 200},   // 14 ua_COV
    {1991, 1, 4, 10},     // 15 ua_Q
    {1993, 1, 1, 10},     // 16 ua_Y
};
kalos::calendar_domain dom_at(int i) {
    return kalos::calendar_domain{DOMS[i].year, DOMS[i].month, DOMS[i].freq, DOMS[i].len};
}

enum Kind { K_TDC, K_MEAN, K_CONTRAST, K_LOP, K_EASTER, K_JULIAN, K_RAMP, K_USER };

struct Case {
    std::string id;      // capture family name (rows) or tag (means/tdc)
    int dom;
    Kind kind;
    int p0 = 0, p1 = 0, p2 = 0, p3 = 0;
    std::string src;     // K_USER: family providing the data
    int off = 0;         // K_USER: slice offset into that data
};

const kalos::day_clustering CL[9] = {
    kalos::day_clustering::td2,  kalos::day_clustering::td2c, kalos::day_clustering::td2d,
    kalos::day_clustering::td3,  kalos::day_clustering::td3c, kalos::day_clustering::td4,
    kalos::day_clustering::td4c, kalos::day_clustering::td6,  kalos::day_clustering::td7};
const char* TAGS[9] = {"TD2", "TD2c", "TDuser", "TD3", "TD3c", "TD4", "TD4c", "TD6", "TD7"};

std::vector<Case> build_cases() {
    std::vector<Case> v;
    auto add = [&](const std::string& id, int dom, Kind k, int p0 = 0, int p1 = 0,
                   int p2 = 0, int p3 = 0) {
        v.push_back(Case{id, dom, k, p0, p1, p2, p3, std::string(), 0});
    };
    // ---- gen.out primary corpus (domain A = dom 0, Q = dom 8) ----
    add("tdc", 0, K_TDC);
    for (int i = 0; i < 9; ++i) add(std::string("c_") + TAGS[i], 0, K_CONTRAST, i);
    add("q_td2", 8, K_CONTRAST, 0);
    add("lop_LeapYear", 0, K_LOP, 1);
    add("lop_LengthOfPeriod", 0, K_LOP, 2);
    for (int d : {1, 5, 8, 15}) add("eas_" + std::to_string(d), 0, K_EASTER, d, 1, 0);
    for (int d : {1, 5, 8, 15}) add("jul_" + std::to_string(d), 0, K_JULIAN, d, 1);
    add("ramp", 0, K_RAMP, 1992, 3, 1994, 6);
    add("ramp2", 0, K_RAMP, 1994, 6, 1992, 3);
    add("user_aligned", 0, K_USER, 1990, 1, 108);
    v.back().src = "user_aligned";
    add("user_short", 0, K_USER, 1992, 1, 48);
    v.back().src = "user_aligned";
    v.back().off = 0;   // clean(n)[t] is index-only: clean(48) == clean(108)[0..48)
    add("q_eas8", 8, K_EASTER, 8, 1, 0);
    // ---- gen2.out per-domain dumps ----
    struct DA { const char* tag; int dom; int freq; };
    const DA das[] = {{"A", 0, 12}, {"M1", 1, 12}, {"M2", 2, 12},
                      {"Q1", 3, 4}, {"Q2", 4, 4}, {"Y1", 5, 1}};
    for (auto& d : das) {
        add(std::string("mean ") + d.tag, d.dom, K_MEAN);
        if (d.freq > 3) {
            add(std::string("tdc ") + d.tag, d.dom, K_TDC);
            add(std::string("c2_") + d.tag, d.dom, K_CONTRAST, 0);
            add(std::string("c7_") + d.tag, d.dom, K_CONTRAST, 8);
            add(std::string("cu_") + d.tag, d.dom, K_CONTRAST, 2);
            add(std::string("c3_") + d.tag, d.dom, K_CONTRAST, 4);
            add(std::string("eas_") + d.tag, d.dom, K_EASTER, 8, 1, 0);
            add(std::string("jul_") + d.tag, d.dom, K_JULIAN, 8, 1);
        }
        add(std::string("ly_") + d.tag, d.dom, K_LOP, 1);
        add(std::string("lp_") + d.tag, d.dom, K_LOP, 2);
        add(std::string("rmp_") + d.tag, d.dom, K_RAMP, 1992, 3, 1994, 6);
    }
    add("ly_M3", 6, K_LOP, 1);
    add("lp_M3", 6, K_LOP, 2);
    struct EV { const char* id; int dur, corr, end, dom; };
    const EV evs[] = {{"e13n", 13, 0, 0, 0}, {"e28s", 28, 1, 0, 0},
                      {"e10p", 10, 2, 0, 0}, {"e25p", 25, 2, 0, 0},
                      {"e20t", 20, 3, 0, 0}, {"e8pm", 8, 1, 1, 0},
                      {"e8nm", 8, 1, -1, 0}, {"e1pm", 1, 0, 1, 0},
                      {"e15nm", 15, 1, -1, 0}, {"e8nm_D", 8, 1, -1, 7}};
    for (auto& e : evs) add(e.id, e.dom, K_EASTER, e.dur, e.corr, e.end);
    add("jul15f", 0, K_JULIAN, 15, 0);
    add("jul28", 0, K_JULIAN, 28, 1);
    add("jul1f", 0, K_JULIAN, 1, 0);
    add("jul8_Q1", 3, K_JULIAN, 8, 1);
    add("jul15_Q2", 4, K_JULIAN, 15, 1);
    // ua_ rows are the data itself (aligned dump over the data's own domain);
    // the u_ rows replay the factory placement of that data into dom.
    struct UV { const char* tag; int dom, dy, dm, len, uadom; };
    const UV uvs[] = {{"S", 0, 1992, 1, 48, 9},    {"PRE", 0, 1988, 1, 40, 10},
                      {"POST", 0, 1997, 6, 40, 11}, {"BEF", 0, 1985, 1, 24, 12},
                      {"AFT", 0, 1999, 1, 24, 13},  {"COV", 0, 1989, 1, 200, 14},
                      {"Q", 3, 1991, 1, 10, 15},    {"Y", 5, 1993, 1, 10, 16}};
    for (auto& u : uvs) {
        std::string ua = std::string("ua_") + u.tag;
        add(ua, u.uadom, K_USER, u.dy, u.dm, u.len);
        v.back().src = ua;
        add(std::string("u_") + u.tag, u.dom, K_USER, u.dy, u.dm, u.len);
        v.back().src = ua;
    }
    add("rmpBEF", 0, K_RAMP, 1985, 1, 1986, 1);
    add("rmpAFT", 0, K_RAMP, 2000, 1, 2001, 1);
    add("rmpEQ", 0, K_RAMP, 1992, 3, 1992, 3);
    add("rmpQ", 3, K_RAMP, 1992, 3, 1994, 6);
    add("rmpY", 5, K_RAMP, 1993, 1, 1997, 1);
    return v;
}

std::map<std::string, std::vector<double>> g_userdata;

void run_case(const Capture& cap, const Case& c) {
    kalos::calendar_domain d = dom_at(c.dom);
    switch (c.kind) {
        case K_TDC: {
            auto got = kalos::td_count(d);
            auto& want = cap.tdc.at(c.id);
            ++g_checks;
            if (got.size() != want.size()) {
                note(c.id + ": tdc row count " + std::to_string(got.size()) + " != " +
                     std::to_string(want.size()));
                return;
            }
            for (std::size_t w = 0; w < got.size(); ++w)
                for (std::size_t i = 0; i < got[w].size(); ++i)
                    chk(c.id + "[" + std::to_string(w) + "][" + std::to_string(i) + "]",
                        static_cast<double>(got[w][i]), want[w][i]);
            return;
        }
        case K_MEAN: {
            auto got = kalos::mean_days(d);
            std::string tag = c.id.substr(5);  // "mean <tag>"
            auto& want = cap.means.at(tag);
            ++g_checks;
            if (got.size() != want.size()) {
                note(c.id + ": mean width " + std::to_string(got.size()) + " != " +
                     std::to_string(want.size()));
                return;
            }
            for (std::size_t i = 0; i < got.size(); ++i)
                chk(c.id + "[" + std::to_string(i) + "]", got[i], want[i]);
            return;
        }
        case K_CONTRAST: {
            auto& rows = cap.rows.at(c.id);
            auto got = kalos::trading_days_contrast(CL[c.p0], d);
            ++g_checks;
            if (got.size() != rows[0].size()) {
                note(c.id + ": contrast width " + std::to_string(got.size()) + " != " +
                     std::to_string(rows[0].size()));
                return;
            }
            for (std::size_t i = 0; i < rows.size(); ++i)
                for (std::size_t j = 0; j < got.size(); ++j)
                    chk(c.id + "[" + std::to_string(i) + "][" + std::to_string(j) + "]",
                        got[j][i], rows[i][j]);
            return;
        }
        case K_LOP: {
            auto& rows = cap.rows.at(c.id);
            auto got = kalos::length_of_period_column(
                c.p0 == 1 ? kalos::calendar_lop::leap_year
                          : kalos::calendar_lop::length_of_period,
                d);
            for (std::size_t i = 0; i < rows.size(); ++i)
                chk(c.id + "[" + std::to_string(i) + "]", got[i], rows[i][0]);
            return;
        }
        case K_EASTER: {
            auto& rows = cap.rows.at(c.id);
            kalos::easter_correction corr = kalos::easter_correction::simple;
            switch (c.p1) {
                case 0: corr = kalos::easter_correction::none; break;
                case 1: corr = kalos::easter_correction::simple; break;
                case 2: corr = kalos::easter_correction::pre_computed; break;
                case 3: corr = kalos::easter_correction::theoretical; break;
            }
            auto got = kalos::easter_column(c.p0, corr, c.p2, d);
            for (std::size_t i = 0; i < rows.size(); ++i)
                chk(c.id + "[" + std::to_string(i) + "]", got[i], rows[i][0]);
            return;
        }
        case K_JULIAN: {
            auto& rows = cap.rows.at(c.id);
            auto got = kalos::julian_easter_column(c.p0, c.p1 == 1, d);
            for (std::size_t i = 0; i < rows.size(); ++i)
                chk(c.id + "[" + std::to_string(i) + "]", got[i], rows[i][0]);
            return;
        }
        case K_RAMP: {
            auto& rows = cap.rows.at(c.id);
            auto got = kalos::ramp_column(c.p0, c.p1, c.p2, c.p3, d);
            for (std::size_t i = 0; i < rows.size(); ++i)
                chk(c.id + "[" + std::to_string(i) + "]", got[i], rows[i][0]);
            return;
        }
        case K_USER: {
            auto& rows = cap.rows.at(c.id);
            auto dit = g_userdata.find(c.src);
            if (dit == g_userdata.end()) {
                note(c.id + ": user data " + c.src + " not built");
                return;
            }
            const std::vector<double>& data = dit->second;
            ++g_checks;
            if (c.p2 > static_cast<int>(data.size()) - c.off) {
                note(c.id + ": user data slice out of range");
                return;
            }
            std::vector<double> slice(data.begin() + c.off, data.begin() + c.off + c.p2);
            auto got = kalos::user_column(slice, c.p0, c.p1, d);
            for (std::size_t i = 0; i < rows.size(); ++i)
                chk(c.id + "[" + std::to_string(i) + "]", got[i], rows[i][0]);
            return;
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "validation/evidence/native/calendar-generation";
    Capture g1, g2;
    load(g1, dir + "/gen.out");
    load(g2, dir + "/gen2.out");

    auto cases = build_cases();
    // Build the user-variable data: every *_aligned/ua_* capture is the
    // factory output over the data's OWN domain (a full aligned copy), so it
    // IS the clean(n) input data, available for the placement replays.
    for (auto& c : cases) {
        if (c.kind != K_USER || c.src != c.id) continue;
        Capture& cap = g1.rows.count(c.id) ? g1 : g2;
        std::vector<double> own;
        for (auto& r : cap.rows.at(c.id)) own.push_back(r[0]);
        g_userdata[c.id] = own;
    }

    auto has_family = [](const Capture& cap, const Case& c) {
        if (c.kind == K_TDC) return static_cast<bool>(cap.tdc.count(c.id));
        if (c.kind == K_MEAN)
            return static_cast<bool>(cap.means.count(c.id.substr(5)));  // "mean <tag>"
        return static_cast<bool>(cap.rows.count(c.id));
    };
    for (auto& c : cases) {
        Capture& cap = has_family(g1, c) ? g1 : g2;
        try {
            run_case(cap, c);
        } catch (const std::exception& e) {
            note(c.id + ": escaped exception " + e.what());
        }
    }

    // meanDays(12) (both captures print it: "mean" and "mean12")
    {
        auto got = kalos::mean_days_yearly(12);
        chk("mean12 width", static_cast<double>(got.size()), 12.0);
        for (std::size_t i = 0; i < 12 && i < g1.means["12"].size(); ++i)
            chk("mean12[" + std::to_string(i) + "]", got[i], g1.means["12"][i]);
        for (std::size_t i = 0; i < 12 && i < g1.means[""].size(); ++i)
            chk("meanA12[" + std::to_string(i) + "]", got[i], g1.means[""][i]);
    }

    // computus rows (easter == easter2, both julian variants, out-of-range
    // probability calls return the 0.0 the jar prints for year arguments)
    for (auto& r : g1.comp) {
        int y = r.year;
        auto e = kalos::easter_date(y);
        ++g_checks;
        if (e.month * 100 + e.day != r.easter) note("easter " + std::to_string(y));
        auto jt = kalos::julian_easter_date(y, true);
        ++g_checks;
        if (jt.month * 100 + jt.day != r.julT) note("julT " + std::to_string(y));
        auto jf = kalos::julian_easter_date(y, false);
        ++g_checks;
        if (jf.month * 100 + jf.day != r.julF) note("julF " + std::to_string(y));
        chk("pjul(" + std::to_string(y) + ")", kalos::prob_julian_easter(y), r.pjul);
        chk("prob(" + std::to_string(y) + ")", kalos::prob_easter(y), r.prob);
    }

    // probability tables (gen2: d in [-1, 44] / [-1, 36] with the guards)
    for (auto& pr : g2.pjul)
        chk("pjul " + std::to_string(pr.first), kalos::prob_julian_easter(pr.first), pr.second);
    for (auto& pr : g2.prob)
        chk("prob " + std::to_string(pr.first), kalos::prob_easter(pr.first), pr.second);

    // equivalences pinned by the capture: raw == contrasts, default == Simple
    for (auto& fam : g1.rows) {
        if (fam.first.compare(0, 2, "r_") != 0) continue;
        std::string c = "c_" + fam.first.substr(2);
        ++g_checks;
        if (!g1.rows.count(c)) {
            note("missing contrast twin " + c);
            continue;
        }
        for (std::size_t i = 0; i < fam.second.size() && i < g1.rows[c].size(); ++i)
            for (std::size_t j = 0; j < fam.second[i].size() && j < g1.rows[c][i].size(); ++j) {
                ++g_checks;
                if (fam.second[i][j] != g1.rows[c][i][j] ||
                    std::signbit(fam.second[i][j]) != std::signbit(g1.rows[c][i][j]))
                    note("raw/contrast twin differs " + fam.first + " row " + std::to_string(i));
            }
    }
    for (auto& fam : g1.rows) {
        if (fam.first.compare(0, 5, "easS_") != 0) continue;
        std::string c = "eas_" + fam.first.substr(5);
        ++g_checks;
        if (!g1.rows.count(c)) {
            note("missing simple twin " + c);
            continue;
        }
        for (std::size_t i = 0; i < fam.second.size() && i < g1.rows[c].size(); ++i) {
            ++g_checks;
            if (fam.second[i][0] != g1.rows[c][i][0] ||
                std::signbit(fam.second[i][0]) != std::signbit(g1.rows[c][i][0]))
                note("easS_/eas_ twin differs " + fam.first + " row " + std::to_string(i));
        }
    }

    // name/description conventions (verbatim capture lines)
    {
        for (int i = 0; i < 9; ++i) {
            std::string want =
                std::string("nameOf ") + TAGS[i] + " = " + kalos::trading_days_name(CL[i]);
            ++g_checks;
            if (!g1.kv.count(want)) note("name line not in capture: " + want);
            std::string gtd = std::string("gtd ") + TAGS[i] + " dim=";
            ++g_checks;  // gtd lines exist for every clustering
            bool found = false;
            for (auto& l : g1.kv)
                if (l.first.compare(0, gtd.size(), gtd) == 0 &&
                    l.first.find(std::string(" type=") + TAGS[i]) != std::string::npos &&
                    l.first.find(std::string(" desc=") + kalos::trading_days_name(CL[i])) !=
                        std::string::npos)
                    found = true;
            if (!found) note("gtd line missing for " + std::string(TAGS[i]));
        }
        ++g_checks;
        if (!g1.kv.count("const ITradingDaysVariable.NAME=td") ||
            std::strcmp(kalos::trading_days_variable_name(), "td") != 0)
            note("ITradingDaysVariable.NAME");
        ++g_checks;
        if (!g1.kv.count("const ILengthOfPeriodVariable.NAME=lp") ||
            std::strcmp(kalos::length_of_period_variable_name(), "lp") != 0)
            note("ILengthOfPeriodVariable.NAME");
        for (int k = 1; k <= 8; ++k) {
            std::string want = "defaultName(" + std::to_string(k) + ")=td";
            ++g_checks;
            if (!g1.kv.count(want) || std::strcmp(kalos::trading_days_default_name(k), "td") != 0)
                note("defaultName " + std::to_string(k));
        }
        for (auto& t : kalos::trading_days_types()) {
            std::string want = std::string("tdt ") + t.tag + " nvar=" + std::to_string(t.nvar) +
                               " names=" + t.names + " contrastNames=" + t.contrast_names +
                               " desc=" + t.description;
            ++g_checks;
            if (!g1.kv.count(want)) note("tdt line differs: " + want);
        }
        for (int d : {1, 5, 8, 15}) {
            ++g_checks;
            if (!g1.kv.count("julas JulianEasterVariable(duration=" + std::to_string(d) +
                             ", gregorianDates=true) dim=1 desc=" +
                             kalos::julian_easter_description(d)))
                note("julas desc " + std::to_string(d));
        }
        ++g_checks;
        if (!g1.kv.count("ramp dim=1 desc=" + kalos::ramp_description(1992, 3, 1994, 6, 12)))
            note("ramp desc");
        ++g_checks;
        if (!g1.kv.count("ramp2 desc=" + kalos::ramp_description(1994, 6, 1992, 3, 12)))
            note("ramp2 desc");
        struct VD { const char* id, *corr; int end, dur; };
        const VD vds[] = {{"e13n", "None", 0, 13},        {"e28s", "Simple", 0, 28},
                          {"e10p", "PreComputed", 0, 10}, {"e25p", "PreComputed", 0, 25},
                          {"e20t", "Theoretical", 0, 20}, {"e8pm", "Simple", 1, 8},
                          {"e8nm", "Simple", -1, 8},      {"e1pm", "None", 1, 1},
                          {"e15nm", "Simple", -1, 15}};
        for (auto& e : vds) {
            std::string want = std::string("vdesc ") + e.id + " corr=" + e.corr + " end=" +
                               std::to_string(e.end) + " desc=" + kalos::easter_description(e.dur);
            ++g_checks;
            if (!g2.kv.count(want)) note("easter vdesc line: " + want);
        }
        ++g_checks;
        if (!g1.kv.count("lop None dim=1 desc=Length of period") ||
            !g1.kv.count("lop LeapYear dim=1 desc=Leap year") ||
            !g1.kv.count("lop LengthOfPeriod dim=1 desc=Length of period") ||
            std::strcmp(kalos::length_of_period_description(kalos::calendar_lop::none),
                        "Length of period") != 0 ||
            std::strcmp(kalos::length_of_period_description(kalos::calendar_lop::leap_year),
                        "Leap year") != 0 ||
            std::strcmp(kalos::length_of_period_description(kalos::calendar_lop::length_of_period),
                        "Length of period") != 0)
            note("lop descriptions");
        ++g_checks;
        if (!g1.kv.count("uv dim=1 desc=grp") || !g1.kv.count("user_aligned desc=grp") ||
            !g1.kv.count("q desc=td2"))
            note("user/quarterly desc lines");
    }

    // fail-closed envelope spot checks (the jar throws or silently refuses
    // where these refuse; a silent wrong answer is the failure mode)
    {
        auto refuse = [&](const char* what, const std::function<void()>& fn) {
            ++g_checks;
            bool threw = false;
            try {
                fn();
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            if (!threw) note(std::string("envelope: no refusal for ") + what);
        };
        kalos::calendar_domain ann{1990, 1, 1, 20};
        kalos::calendar_domain bad{1990, 2, 4, 12};  // unaligned quarterly
        refuse("annual TD contrast", [&] { kalos::trading_days_contrast(CL[0], ann); });
        refuse("unaligned domain", [&] { kalos::td_count(bad); });
        refuse("easter dur 0", [&] { kalos::easter_column(0, kalos::easter_correction::simple, 0, dom_at(0)); });
        refuse("easter dur 29", [&] { kalos::easter_column(29, kalos::easter_correction::simple, 0, dom_at(0)); });
        refuse("PreComputed dur 26",
               [&] { kalos::easter_column(26, kalos::easter_correction::pre_computed, 0, dom_at(0)); });
        refuse("julian dur 29", [&] { kalos::julian_easter_column(29, true, dom_at(0)); });
        refuse("annual easter", [&] { kalos::easter_column(8, kalos::easter_correction::simple, 0, ann); });
        refuse("annual TD", [&] { kalos::td_count(ann); });
        refuse("zero length", [&] { kalos::td_count(kalos::calendar_domain{1990, 1, 12, 0}); });
        refuse("unaligned user data", [&] {
            kalos::user_column(std::vector<double>(10, 1.0), 1990, 2, dom_at(3));
        });
    }

    std::printf(
        "verify_calendar_generation: %lld checks, %lld failures (%d cases, %d row lines)\n",
        g_checks, g_failures, static_cast<int>(cases.size()), g1.nrowlines + g2.nrowlines);
    for (std::size_t i = 0; i < g_problems.size() && i < 40; ++i)
        std::printf("  %s\n", g_problems[i].c_str());
    return g_failures == 0 ? 0 : 1;
}
