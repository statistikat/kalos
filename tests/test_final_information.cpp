// Item 21-H.0 unit qualification of the final-information producer
// (kalos/final_information.hpp): a bit-exact replay of the double-JVM
// corpus (validation/evidence/native/final-information, harvested into
// jvm_final_information.h) where the deviance callback is a raw-bit node
// table over exactly the evaluations the pinned stencil performed - a
// lookup miss means our stencil touched a node the JVM never evaluated, an
// uncovered node means it skipped one it did, so both directions of stencil
// drift fail the table. THROW nodes exercise the newval catch. Structural
// cases cover the step constant, the rejected base point, the empty
// parameter vector and determinism. The corpus is re-verified end-to-end
// from the csv files (table replay plus the kernel gates on the estimator
// rows) by the sweep tools/verify_final_information.cpp.
#include "kalos/final_information.hpp"

#include "jvm_final_information.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

using kalos::final_information;
using kalos::final_information_request;
using kalos::final_information_result;
using kalos::final_information_status;

int failures = 0;

std::uint64_t to_bits(double d) {
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof(u));
    return u;
}

double from_bits(std::uint64_t u) {
    double d = 0.0;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

void fail(const std::string& what) {
    std::printf("FAIL %s\n", what.c_str());
    ++failures;
}

std::string u64(std::uint64_t u) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(u));
    return buf;
}

// The replay oracle: the deviance callback answers from the captured node
// table by raw-bit coordinate key; anything else is stencil drift.
struct Replay {
    std::map<std::vector<std::uint64_t>, std::pair<std::uint64_t, int>> nodes;
    int misses = 0;
    int calls = 0;
    std::vector<char> hit; // node-table coverage, by index
};

void replay_case(const golden_fi::Case& c) {
    Replay rp;
    for (int i = 0; i < c.nodes_len; ++i) {
        const golden_fi::Node& n = c.nodes[i];
        std::vector<std::uint64_t> key(c.point, c.point + c.dim);
        for (int k = 0; k < c.dim; ++k) {
            key[k] = n.key[k];
        }
        rp.nodes.emplace(std::move(key),
                         std::make_pair(n.value, n.is_throw));
        rp.hit.push_back(0);
    }
    // index lookup for coverage
    std::map<std::vector<std::uint64_t>, int> index;
    for (int i = 0; i < c.nodes_len; ++i) {
        const golden_fi::Node& n = c.nodes[i];
        std::vector<std::uint64_t> key(c.point, c.point + c.dim);
        for (int k = 0; k < c.dim; ++k) {
            key[k] = n.key[k];
        }
        index[key] = i;
    }

    final_information_request req;
    req.orders.p = c.p;
    req.orders.bp = c.bp;
    req.orders.q = c.q;
    req.orders.bq = c.bq;
    for (int k = 0; k < c.dim; ++k) {
        req.point.push_back(from_bits(c.point[k]));
    }
    req.nobs = c.nobs;
    req.ar_limit = from_bits(c.ar_limit);
    req.ma_limit = from_bits(c.ma_limit);
    req.check_all = c.check_all != 0;
    req.epsilon = from_bits(c.epsilon);
    req.deviance = [&](const std::vector<double>& x) -> std::optional<double> {
        ++rp.calls;
        std::vector<std::uint64_t> key(x.size());
        for (std::size_t k = 0; k < x.size(); ++k) {
            key[k] = to_bits(x[k]);
        }
        const auto it = rp.nodes.find(key);
        if (it == rp.nodes.end()) {
            ++rp.misses;
            return std::nullopt; // keeps the replay finite; counted as a fail
        }
        rp.hit[static_cast<std::size_t>(index[key])] = 1;
        if (it->second.second) {
            return std::nullopt; // THROW node: the pinned evaluation failed
        }
        return from_bits(it->second.first);
    };

    const final_information_result r = final_information(req);

    const int want_status = c.status == 0 ? 0 : (c.status == 1 ? 1 : 2);
    const int got_status = r.status == final_information_status::available
                               ? 0
                               : (r.status == final_information_status::inverse_failed ? 1 : 2);
    if (got_status != want_status) {
        fail(std::string(c.id) + ": status " + std::to_string(got_status) +
             " != " + std::to_string(want_status));
    }
    if (rp.misses != 0) {
        fail(std::string(c.id) + ": " + std::to_string(rp.misses) +
             " deviance lookups outside the captured node table");
    }
    int uncovered = 0;
    for (const char h : rp.hit) {
        if (!h) {
            ++uncovered;
        }
    }
    if (uncovered != 0) {
        fail(std::string(c.id) + ": " + std::to_string(uncovered) +
             " captured nodes never evaluated");
    }
    if (c.status == 2) {
        if (!r.eps.empty() || !r.gradient.empty() || !r.hessian.empty()) {
            fail(std::string(c.id) + ": rejected case emitted stencil output");
        }
        return;
    }
    auto cmp = [&](const char* name, const std::uint64_t* want,
                   const std::vector<double>& got, int len) {
        if (static_cast<int>(got.size()) != len) {
            fail(std::string(c.id) + ": " + name + " length " +
                 std::to_string(got.size()) + " != " + std::to_string(len));
            return;
        }
        for (int k = 0; k < len; ++k) {
            const std::uint64_t g = to_bits(got[static_cast<std::size_t>(k)]);
            if (g != want[k]) {
                fail(std::string(c.id) + ": " + name + "[" + std::to_string(k) +
                     "] = " + u64(g) + " != " + u64(want[k]));
                return;
            }
        }
    };
    cmp("eps", c.eps, r.eps, c.dim);
    if (to_bits(r.fx) != c.fx) {
        fail(std::string(c.id) + ": fx " + u64(to_bits(r.fx)) + " != " + u64(c.fx));
    }
    cmp("grad", c.grad, r.gradient, c.dim);
    cmp("score", c.score, r.score, c.dim);
    cmp("hess", c.hess, r.hessian, c.dim * c.dim);
    cmp("info", c.info, r.information, c.dim * c.dim);
    if (c.status == 0) {
        cmp("asy", c.asy, r.asymptotic_covariance, c.dim * c.dim);
        cmp("diag", c.diag, r.asymptotic_covariance_diagonal, c.dim);
    } else if (!r.asymptotic_covariance.empty() ||
               !r.asymptotic_covariance_diagonal.empty()) {
        fail(std::string(c.id) + ": inverse-failed case emitted a covariance");
    }
}

void structural_cases() {
    // The step constant is the pinned literal Math.sqrt(2.220446E-16).
    if (to_bits(kalos::final_information_step()) != 0x3e4ffffffa0bc6d1ULL) {
        fail("final_information_step bit drift");
    }

    // A rejected base point short-circuits everything.
    {
        final_information_request req;
        req.orders.p = 1;
        req.point = {-0.5};
        req.deviance = [](const std::vector<double>&) { return std::nullopt; };
        const final_information_result r = final_information(req);
        if (r.status != final_information_status::final_point_rejected ||
            !r.gradient.empty()) {
            fail("rejected base point not short-circuited");
        }
    }

    // The empty parameter vector: the stencil degenerates to the bare fx
    // (JVM dim 0 never reaches the producer; the unit answers fail-open
    // with an empty available result).
    {
        final_information_request req;
        req.nobs = 10;
        req.deviance = [](const std::vector<double>&) -> std::optional<double> {
            return 5.0;
        };
        const final_information_result r = final_information(req);
        if (r.status != final_information_status::available ||
            r.fx != 5.0 || !r.eps.empty() || !r.information.empty() ||
            !r.asymptotic_covariance.empty()) {
            fail("empty-parameter degeneracy drifted");
        }
    }

    // Determinism: the same interior point replays bit-identically.
    {
        auto run = []() {
            final_information_request req;
            req.orders.q = 1;
            req.orders.bq = 1;
            req.point = {0.3, -0.4};
            req.nobs = 50;
            req.deviance = [](const std::vector<double>& x) -> std::optional<double> {
                double s = 1.0;
                for (const double v : x) {
                    s *= 1.0 + v * v;
                }
                return 10.0 + 3.0 * s;
            };
            return final_information(req);
        };
        const final_information_result a = run();
        const final_information_result b = run();
        if (a.status != final_information_status::available) {
            fail("analytic smoke case did not complete");
            return;
        }
        if (a.asymptotic_covariance.size() != 4) {
            fail("analytic smoke case covariance size");
            return;
        }
        for (std::size_t k = 0; k < a.asymptotic_covariance.size(); ++k) {
            if (to_bits(a.asymptotic_covariance[k]) !=
                to_bits(b.asymptotic_covariance[k])) {
                fail("analytic smoke case is not deterministic");
                break;
            }
        }
        // The Cholesky branch produced a symmetric covariance.
        if (to_bits(a.asymptotic_covariance[1]) !=
            to_bits(a.asymptotic_covariance[2])) {
            fail("analytic smoke covariance is not symmetric");
        }
    }
}

} // namespace

int main() {
    std::size_t count = 0;
    const golden_fi::Case* cases = golden_fi::table(count);
    for (std::size_t i = 0; i < count; ++i) {
        replay_case(cases[i]);
    }
    structural_cases();
    if (failures != 0) {
        std::printf("test_final_information: %d failures over %zu corpus cases\n",
                    failures, count);
        return 1;
    }
    std::printf("test_final_information: %zu corpus cases + structural checks clean\n",
                count);
    return 0;
}
