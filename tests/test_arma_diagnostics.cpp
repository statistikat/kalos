// Item 21-H: unit qualification of the automatic-model diagnostics machine
// (kalos/arma_diagnostics.hpp): the AutoCovariances acf, the Ljung-Box
// statistic with its Chi2 upper-tail p-value, calcLbLength, calcResStat and
// the accept wiring on top of the shared ML-residual engine. Every scalar
// row is a raw-bits comparison against the pinned jdplus 3.9.0 JVM (capture
// validation/reference/LjungBoxGolden.java); the input series are exact
// recurrences replayed bit-identically from the embedded table.
#include "kalos/arma_diagnostics.hpp"

#include "jvm_arma_diagnostics.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using kalos::auto_correlation;
using kalos::calc_lb_length;
using kalos::calc_res_stat_rtval;
using kalos::ljung_box_test;
using kalos::model_controller_accept;
using kalos::ModelControllerRequest;

int failures = 0;

std::uint64_t to_bits(double d) {
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof(u));
    return u;
}

bool same(double got, std::uint64_t want) {
    return (std::isnan(golden_diag::bits(want)) && std::isnan(got))
        || to_bits(got) == want;
}

std::vector<std::vector<double>> load_series() {
    std::size_t count = 0;
    const golden_diag::Series* table = golden_diag::series_table(count);
    std::vector<std::vector<double>> out;
    out.resize(16);
    for (std::size_t s = 0; s < count; ++s) {
        std::vector<double> values(table[s].n);
        for (std::size_t i = 0; i < table[s].n; ++i) {
            values[i] = golden_diag::bits(table[s].values[i]);
        }
        out[table[s].id] = values;
    }
    return out;
}

double java_average(const std::vector<double>& x) {
    double s = 0.0;
    for (const double v : x) {
        s += v;
    }
    return s / static_cast<double>(x.size());
}

void check(const golden_diag::Row& r, const char* what, double got) {
    if (!same(got, r.v)) {
        std::printf("FAIL %s (%d,%d,%d): got %016llx want %016llx\n", what, r.i0,
                    r.i1, r.i2, static_cast<unsigned long long>(to_bits(got)),
                    static_cast<unsigned long long>(r.v));
        ++failures;
    }
}

template <class Check>
void check_rsa0_residual_test(const std::string& file, const std::string& key, Check check) {
    const std::string directory = KALOS_TRAMO_DIAGNOSTIC_DIR;
    std::ifstream residual_file(directory + "/rsa0-residuals-x86_64.txt");
    std::ifstream public_tests(directory + "/" + file);
    if (!residual_file || !public_tests) throw std::runtime_error("missing RSA0 diagnostic reference");
    std::map<std::string, std::vector<double>> residuals;
    std::string line, id;
    while (std::getline(residual_file, line)) {
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "case") row >> id;
        if (kind != "residuals.tsres") continue;
        int year, month, n;
        row >> year >> month >> n;
        auto& values = residuals[id];
        std::uint64_t bits;
        while (row >> std::hex >> bits) values.push_back(golden_diag::bits(bits));
        if (values.size() != static_cast<std::size_t>(n)) throw std::runtime_error("invalid residual count");
    }
    int cases = 0, edges = 0;
    while (std::getline(public_tests, line)) {
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "case") {
            row >> id;
            continue;
        }
        auto values = residuals.at(id);
        std::uint64_t statistic, pvalue;
        if (kind == "short") {
            std::size_t observations, count;
            if (!(row >> observations >> count >> std::hex >> statistic >> pvalue))
                throw std::runtime_error("invalid short RSA0 diagnostic row");
            values.clear();
            std::uint64_t bits;
            while (row >> std::hex >> bits) values.push_back(golden_diag::bits(bits));
            if (values.size() != count || count + 13 != observations)
                throw std::runtime_error("invalid short RSA0 residual count");
            ++edges;
        } else if (kind == key) {
            if (!(row >> std::hex >> statistic >> pvalue)) throw std::runtime_error("invalid JVM statistic");
            ++cases;
        } else {
            throw std::runtime_error("unrecognized RSA0 diagnostic reference row");
        }
        if (!check(values, statistic, pvalue)) {
            std::printf("FAIL public %s: %s (%zu residuals)\n", key.c_str(), id.c_str(), values.size());
            ++failures;
        }
    }
    if (cases != 6 || edges != 3) throw std::runtime_error("incomplete RSA0 diagnostic reference");
}

void check_rsa1_residual_diagnostics() {
    const std::string directory = KALOS_TRAMO_DIAGNOSTIC_DIR;
    struct Input {
        double ssq = 0;
        int effective = 0, estimated = 0;
        std::vector<double> residuals;
    };
    std::map<std::string, Input> inputs;
    std::string id, line, key;
    for (const auto& prefix : {std::string(), std::string("level_")}) {
        const auto name = prefix.empty() ? "rsa1-x86_64.txt" : "rsa1-level-effects-x86_64.txt";
        std::ifstream file(directory + "/" + name);
        if (!file) throw std::runtime_error("missing RSA1 fitted reference");
        while (std::getline(file, line)) {
            std::istringstream row(line);
            row >> key;
            if (key == "case") { row >> id; id = prefix + id; }
            if (key == "likelihood") {
                std::string ignored;
                std::uint64_t ssq;
                auto& fit = inputs[id];
                row >> ignored >> ignored >> ignored >> std::hex >> ssq >> std::dec >> fit.effective >> fit.estimated;
                fit.ssq = golden_diag::bits(ssq);
            } else if (key == "residuals") {
                int year, month, count;
                row >> year >> month >> count;
                auto& fit = inputs.at(id);
                std::uint64_t bits;
                while (row >> std::hex >> bits) fit.residuals.push_back(golden_diag::bits(bits));
                if (count != fit.effective || fit.residuals.size() != static_cast<std::size_t>(count))
                    throw std::runtime_error("invalid RSA1 residual count");
            }
        }
    }
    std::ifstream tests(directory + "/rsa1-residual-diagnostics-x86_64.txt");
    int checks = 0;
    while (std::getline(tests, line)) {
        std::istringstream row(line);
        row >> key;
        if (key == "case") { row >> id; continue; }
        const auto& fit = inputs.at(id);
        std::uint64_t statistic, pvalue;
        row >> std::hex >> statistic;
        if (key == "residuals.ser") {
            if (!same(kalos::tramo_residual_standard_error(fit.ssq, fit.effective, fit.estimated), statistic)) ++failures;
        } else {
            row >> pvalue;
            if (key == "residuals.lb") {
                const auto test = kalos::monthly_airline_residual_ljung_box(fit.residuals);
                if (!test.built || test.df != 22 || !same(test.value, statistic) || !same(test.p, pvalue)) ++failures;
            } else if (key == "residuals.doornikhansen") {
                const auto test = kalos::tramo_residual_normality(fit.residuals);
                if (!test || test->df != 2 || !same(test->value, statistic) || !same(test->p, pvalue)) ++failures;
            } else throw std::runtime_error("unrecognized public diagnostic");
        }
        ++checks;
    }
    if (inputs.size() != 8 || checks != 24) throw std::runtime_error("incomplete RSA1 diagnostic evidence");
}

} // namespace

int main() {
    check_rsa1_residual_diagnostics();
    check_rsa0_residual_test("rsa0-ljung-box-x86_64.txt", "residuals.lb",
        [](const auto& values, auto statistic, auto pvalue) {
            const auto test = kalos::rsa0_residual_ljung_box(values);
            return test.built && test.df == 22 && same(test.value, statistic) && same(test.p, pvalue);
        });
    check_rsa0_residual_test("rsa0-doornik-hansen-x86_64.txt", "residuals.doornikhansen",
        [](const auto& values, auto statistic, auto pvalue) {
            const auto test = kalos::rsa0_residual_normality(values);
            return test && test->df == 2 && same(test->value, statistic) && same(test->p, pvalue);
        });
    if (kalos::rsa0_residual_normality(std::vector<double>(7))) ++failures;
    const auto constant = kalos::rsa0_residual_normality(std::vector<double>(8, 1.0));
    if (!constant || !std::isnan(constant->value) || !std::isnan(constant->p)) ++failures;
    const auto series = load_series();
    std::size_t count = 0;
    const golden_diag::Row* rows = golden_diag::table(count);

    std::size_t n_acf = 0, n_lb = 0, n_len = 0, n_rt = 0, n_throws = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const golden_diag::Row& r = rows[i];
        if (r.threw) {
            continue;  // handled in the throw pass below
        }
        switch (r.tag) {
        case golden_diag::kAcf: {
            const std::vector<double>& x = series[r.i0];
            const double mean = r.i1 == 0 ? 0.0 : java_average(x);
            check(r, "acf", auto_correlation(x, mean, r.i2));
            ++n_acf;
            break;
        }
        case golden_diag::kLbStat:
        case golden_diag::kLbP: {
            const bool stat = r.tag == golden_diag::kLbStat;
            const std::vector<double>& x = series[r.i0];
            const auto lb = ljung_box_test(x, r.i1, r.i2);
            if (!lb.built) {
                std::printf("FAIL lb (%d,%d,%d): native threw, JVM built\n", r.i0,
                            r.i1, r.i2);
                ++failures;
                break;
            }
            check(r, stat ? "lb.value" : "lb.p", stat ? lb.value : lb.p);
            ++n_lb;
            break;
        }
        case golden_diag::kLbLen:
            check(r, "lbLen", static_cast<double>(calc_lb_length(r.i0, r.i1)));
            ++n_len;
            break;
        case golden_diag::kRtval:
            check(r, "rtval", calc_res_stat_rtval(series[r.i0]));
            ++n_rt;
            break;
        default:
            std::printf("FAIL unknown tag %u\n", r.tag);
            ++failures;
        }
    }
    // Throw rows: df = k - nhp <= 0 must map onto built == false.
    for (std::size_t i = 0; i < count; ++i) {
        const golden_diag::Row& r = rows[i];
        if (r.tag != golden_diag::kLbStat || !r.threw) {
            continue;  // the JVM threw once per build; the stat row carries it
        }
        const auto lb = ljung_box_test(series[r.i0], r.i1, r.i2);
        if (lb.built) {
            std::printf("FAIL lb-throw (%d,%d,%d): native built, JVM threw\n", r.i0,
                        r.i1, r.i2);
            ++failures;
        } else if (!std::isnan(lb.p)) {
            std::printf("FAIL lb-throw (%d,%d,%d): p must be NaN\n", r.i0, r.i1, r.i2);
            ++failures;
        }
        ++n_throws;
    }

    // Accept wiring on the identity MLJ (no ARMA): out = residuals, so every
    // field is checkable against the pinned scalar machines above.
    {
        const std::vector<double>& x = series[0];
        ModelControllerRequest req;
        req.residuals = x;
        req.period = 12;
        req.effective_observations = static_cast<int>(x.size());
        req.annual_frequency = 12;
        req.free_parameters = 1;
        req.lb_limit = 0.95;
        const auto res = model_controller_accept(req);
        const int k = calc_lb_length(12, static_cast<int>(x.size()));
        const auto lb = ljung_box_test(x, k, 1);
        double ssq = 0.0;
        for (const double v : x) {
            ssq += v * v;
        }
        bool ok = res.lb_built == lb.built && res.lb_length == k && same(res.lb_value, to_bits(lb.value))
            && same(res.rtval, to_bits(calc_res_stat_rtval(x)))
            && same(res.rvr, to_bits(std::sqrt(ssq / static_cast<double>(x.size()))))
            && res.ml_residuals.size() == x.size() && res.accepted == (lb.built && !((1.0 - lb.p) > 0.95));
        if (!ok) {
            std::printf("FAIL accept wiring (identity MLJ)\n");
            ++failures;
        }
        // A seasonal block with all-zero coefficients must expand identically
        // to the empty case (trailing-zero strip on both sides).
        ModelControllerRequest padded = req;
        padded.bphi = {0.0, 0.0};
        padded.btheta = {0.0};
        const auto res2 = model_controller_accept(padded);
        if (res2.accepted != res.accepted || res2.lb_length != res.lb_length
            || !same(res2.rvr, to_bits(res.rvr)) || !same(res2.rtval, to_bits(res.rtval))
            || !same(res2.lb_value, to_bits(res.lb_value))) {
            std::printf("FAIL accept wiring (zero seasonal blocks)\n");
            ++failures;
        }
        // Fail-closed envelope.
        bool threw = false;
        try {
            ModelControllerRequest bad = req;
            bad.effective_observations = static_cast<int>(x.size()) + 100;
            model_controller_accept(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) {
            std::printf("FAIL accept envelope (neff > n)\n");
            ++failures;
        }
    }

    std::printf("arma-diagnostics: %zu rows (acf %zu, lb %zu, lbLen %zu, rtval %zu, "
                "throws %zu), %d failures\n",
                count, n_acf, n_lb, n_len, n_rt, n_throws, failures);
    return failures == 0 ? 0 : 1;
}
