#ifndef KALOS_TESTS_TRAMO_OUTLIER_SCAN_REPLAY_H
#define KALOS_TESTS_TRAMO_OUTLIER_SCAN_REPLAY_H

// Shared replay layer for the item 23-B round-0 outlier-scan corpus: parses
// the captured SCAN protocol line (the TramoOutlierScanProbe.java header
// grammar) into a kalos::TramoOutlierScanRequest, mirroring the probe's
// token handling: the d D p q bp bq order of the header, the yyyy factory
// mix, the "calc" pseudo-token resolved from the captured "in" fact line,
// the span selector already resolved to estimation-domain bounds (all ->
// [0, n), idx:l-h -> [l, h + 1], the epoch-anchored idxe selector -> the
// collapsed [0, 0)) and the CODE:idx(+CODE:idx) prespec list.

#include <kalos/tramo_outlier_scan.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tosr {

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

struct ParsedScan {
    kalos::TramoOutlierScanRequest req;
    bool cv_is_calc = false;
};

// tokens: SCAN id freq mean log d D p q bp bq fac tcrate prec ml maxround
// maxout span cv interp [pre]
inline ParsedScan parse_scan(const std::vector<std::string>& t,
    const std::vector<double>& values) {
    if (t.size() < 20 || t[0] != "SCAN") {
        throw std::runtime_error("malformed SCAN line");
    }
    ParsedScan ps;
    auto& r = ps.req;
    r.y = values;
    r.frequency = std::stoi(t[2]);
    r.mean = t[3] == "y";
    r.log = t[4] == "y";
    r.d = std::stoi(t[5]);
    r.bd = std::stoi(t[6]);
    r.p = std::stoi(t[7]);
    r.q = std::stoi(t[8]);
    r.bp = std::stoi(t[9]);
    r.bq = std::stoi(t[10]);
    if (t[11].size() != 4) {
        throw std::runtime_error("malformed factory mix: " + t[11]);
    }
    r.ao = t[11][0] == 'y';
    r.ls = t[11][1] == 'y';
    r.tc = t[11][2] == 'y';
    r.so = t[11][3] == 'y';
    r.tc_rate = tok(t[12]);
    r.precision = tok(t[13]);
    r.maximum_likelihood = t[14] == "y";
    r.max_round = std::stoi(t[15]);
    r.max_outliers = std::stoi(t[16]);
    const int n = static_cast<int>(values.size());
    if (t[17] == "all") {
        r.lower_bound = 0;
        r.upper_bound = n;
    } else if (t[17].rfind("idx:", 0) == 0) {
        const std::string body = t[17].substr(4);
        const auto dash = body.find('-');
        r.lower_bound = std::stoi(body.substr(0, dash));
        r.upper_bound = std::stoi(body.substr(dash + 1)) + 1;
    } else if (t[17].rfind("idxe:", 0) == 0) {
        // Epoch-anchored selector: the window lands before the series and
        // the module's bounds collapse.
        r.lower_bound = 0;
        r.upper_bound = 0;
    } else {
        throw std::runtime_error("malformed span token: " + t[17]);
    }
    if (t[18] == "calc") {
        ps.cv_is_calc = true;
    } else {
        r.cv = tok(t[18]);
    }
    r.interpolate = t[19] == "y";
    if (t.size() > 20 && t[20] != "-") {
        std::string rest = t[20];
        std::size_t start = 0;
        while (start < rest.size()) {
            const auto plus = rest.find('+', start);
            const std::string item = rest.substr(start,
                plus == std::string::npos ? std::string::npos : plus - start);
            const auto colon = item.find(':');
            if (colon == std::string::npos) {
                throw std::runtime_error("malformed prespec token: " + item);
            }
            const std::string code = item.substr(0, colon);
            const int pos = std::stoi(item.substr(colon + 1));
            kalos::TramoOutlierKind kind;
            if (code == "AO") {
                kind = kalos::TramoOutlierKind::Ao;
            } else if (code == "LS") {
                kind = kalos::TramoOutlierKind::Ls;
            } else if (code == "TC") {
                kind = kalos::TramoOutlierKind::Tc;
            } else if (code == "SO") {
                kind = kalos::TramoOutlierKind::So;
            } else {
                throw std::runtime_error("unknown prespec code: " + code);
            }
            r.prespec.emplace_back(pos, kind);
            if (plus == std::string::npos) {
                break;
            }
            start = plus + 1;
        }
    }
    return ps;
}

// Resolves the "calc" critical value from the captured "in" fact line
// ("in n 96 ... cv 3.4150000000000000 interp 1").
inline double cv_from_facts(const std::vector<std::string>& facts) {
    for (const auto& line : facts) {
        if (line.rfind("in ", 0) != 0) {
            continue;
        }
        const auto t = split(line);
        for (std::size_t i = 0; i + 1 < t.size(); ++i) {
            if (t[i] == "cv") {
                return tok(t[i + 1]);
            }
        }
    }
    throw std::runtime_error("no cv token in the in fact line");
}

} // namespace tosr

#endif
