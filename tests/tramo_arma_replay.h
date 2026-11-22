#ifndef KALOS_TESTS_TRAMO_ARMA_REPLAY_H
#define KALOS_TESTS_TRAMO_ARMA_REPLAY_H

// Shared replay surface for the item 23-D arma-selection unit: renders the
// kalos::TramoArmaSelectionResult as the probe lines the JVM captured
// (direct_sel, the five dpanel rows, arm_result, after and the ProcessingLog
// events with the CInfo detail rendering) and compares them token by token
// with bit-pattern equality for numeric tokens (Java's %.17g renders
// shortest round-trip decimals; both renderings reparse to the same bits -
// 23-B discipline). Used by tests/test_tramo_arma.cpp (generated golden) and
// tools/verify_tramo_arma_selection.cpp (evidence corpus).
#include "kalos/tramo_arma_selection.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace kalos::replay {

inline std::string g(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

inline bool token_equal(const std::string& a, const std::string& b) {
    if (a == b) {
        return true;
    }
    char* ea = nullptr;
    char* eb = nullptr;
    const double va = std::strtod(a.c_str(), &ea);
    const double vb = std::strtod(b.c_str(), &eb);
    if (ea == a.c_str() + a.size() && eb == b.c_str() + b.size()) {
        std::uint64_t ba = 0, bb = 0;
        std::memcpy(&ba, &va, sizeof(ba));
        std::memcpy(&bb, &vb, sizeof(bb));
        return ba == bb;
    }
    // The CInfo detail glues keys to values ("b2=-2.14..."); split at the
    // first '=' and compare both halves with the same rule.
    const std::size_t qa = a.find('=');
    const std::size_t qb = b.find('=');
    if (qa != std::string::npos && qa == qb && qa != 0
        && a.compare(0, qa + 1, b, 0, qb + 1) == 0) {
        return token_equal(a.substr(qa + 1), b.substr(qb + 1));
    }
    return false;
}

inline bool line_equal(const std::string& want, const std::string& got) {
    std::vector<std::string> wa, wb;
    const auto split = [](const std::string& s) {
        std::vector<std::string> out;
        std::size_t pos = 0;
        while (pos < s.size()) {
            const std::size_t next = s.find(' ', pos);
            if (next == std::string::npos) {
                out.push_back(s.substr(pos));
                break;
            }
            out.push_back(s.substr(pos, next - pos));
            pos = next + 1;
        }
        return out;
    };
    wa = split(want);
    wb = split(got);
    if (wa.size() != wb.size()) {
        return false;
    }
    for (std::size_t i = 0; i < wa.size(); ++i) {
        if (!token_equal(wa[i], wb[i])) {
            return false;
        }
    }
    return true;
}

inline std::vector<double> to_values(const std::uint64_t* bits, std::size_t count) {
    std::vector<double> out(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t b = bits[i];
        std::memcpy(&out[i], &b, sizeof(double));
    }
    return out;
}

inline const char* result_name(const TramoArmaSelectionResult& d) {
    switch (d.result) {
    case TramoArmaSelectionResult::Result::Changed:
        return "Changed";
    case TramoArmaSelectionResult::Result::Unchanged:
        return "Unchanged";
    case TramoArmaSelectionResult::Result::Unprocessed:
        return "Unprocessed";
    case TramoArmaSelectionResult::Result::Failed:
        return "Failed";
    }
    return "?";
}

inline std::string so_string(const TramoSarmaOrders& o) {
    return "(" + std::to_string(o.p) + "," + std::to_string(o.q) + ")("
        + std::to_string(o.bp) + "," + std::to_string(o.bq) + ")";
}

// The probe's echoed module-level lines, rebuilt from the decision, in the
// probe's emission order.
inline std::vector<std::string> produced(
    const std::string& id, const TramoArmaSelectionResult& d) {
    std::vector<std::string> out;
    if (d.has_selection) {
        out.push_back("direct_sel " + id + " " + so_string(d.selection));
        for (std::size_t i = 0; i < d.panel.size(); ++i) {
            const auto& row = d.panel[i];
            out.push_back("dpanel " + id + " " + std::to_string(i) + " "
                + (row.present ? so_string(row.orders) + " " + g(row.bic) : "null"));
        }
    }
    out.push_back("arm_result " + id + " " + result_name(d));
    out.push_back("after " + id + " spec (" + std::to_string(d.spec_p) + ","
        + std::to_string(d.spec_d) + "," + std::to_string(d.spec_q) + ")("
        + std::to_string(d.spec_bp) + "," + std::to_string(d.spec_bd) + ","
        + std::to_string(d.spec_bq) + ") mean 1");

    // ProcessingLog replay: the observer numbers events from 1, push/pop at
    // depth 0 and the section lines at depth 1; the info detail renders the
    // CInfo getters (best, the five model rows, the five BICs).
    int seq = 0;
    for (const std::string& evt : d.events) {
        ++seq;
        const std::string num = std::to_string(seq);
        if (evt == "push arma selection") {
            out.push_back("evt " + num + " push 0 arma selection det -");
        } else if (evt == "pop") {
            out.push_back("evt " + num + " pop 0 - det -");
        } else if (evt.rfind("remark ", 0) == 0) {
            out.push_back("evt " + num + " remark 1 " + evt.substr(7) + " det -");
        } else if (evt.rfind("info ", 0) == 0) {
            std::string detail = "armaInfo best=" + so_string(d.selection);
            for (std::size_t i = 0; i < d.panel.size(); ++i) {
                const auto& row = d.panel[i];
                detail += " m" + std::to_string(i) + "="
                    + (row.present ? so_string(row.orders) : "null");
                detail += " b" + std::to_string(i) + "=" + g(row.bic);
            }
            out.push_back("evt " + num + " info 1 " + evt.substr(5) + " det " + detail);
        } else {
            throw std::runtime_error("unknown event line: " + evt);
        }
    }
    return out;
}

} // namespace kalos::replay

#endif // KALOS_TESTS_TRAMO_ARMA_REPLAY_H
