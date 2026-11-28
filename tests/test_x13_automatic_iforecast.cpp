// M14 (slice P5, i_f half): the thirteen-case double-JVM public X-13
// irregular-forecast oracle, validation/evidence/native/
// x13-automatic-iforecast/iforecast-x86_64.txt, replayed through the new
// public entry kalos::x13_automatic_irregular_forecast over the automatic
// (decoder-mirrored RSA3) and estimated-template (rsa0/rsa1/rsa2c) lanes, on
// the x13-refusal-jvm-profiles calendars.
//
// Definition pinned by the reference (X13Extractor.class lambda$new$28,
// javap-documented in the M14 probe dossier): i_f is a model-free constant
// placeholder — 1.0 when the decomposition mode answers
// DecompositionMode.isMultiplicative() (Multiplicative OR LogAdditive), +0.0
// otherwise (Additive, PseudoAdditive) — emitted on the finals trend-forecast
// (D12a) domain (same start, same count). No ARIMA/ARMA forecast, no mean
// correction (the mean=true stock_rsa3_ao capture still answers 1.0) and no
// back-transform (the log stock_rsa3_air capture answers exactly 1.0, not an
// exponentiated anything) enter the product.
//
// Tier policy (measured in the Phase-1 probe and the corrected-route driver,
// no invented budgets): the published i_f rows — dates, count and every one
// of the 144 words — are BIT claims, twelve of twelve cases word-exact
// (achieved-class table: I_F_LINES_BIT_IDENTICAL_12_OF_12). The extractor's
// gate chain is replayed arm by arm: the zero-horizon stock_rsa3_air_h0 block
// (the JVM D12a-null gate) pins the "no_requested_predictions" answer with an
// empty result, and the non-available-fit guard pins the throw. The d12a rows
// are claimed as DOMAIN claims only (start year, month, count): the corrected
// decoder-faithful mirror (the golden ao row, mixed=1) measures the domain
// CONTENT at worst 5.84e-14 relative drift (stock_rsa0_ao) — i_f consumes only
// the domain shape, which the table measures identical across all twelve cases
// and across the default-vs-bridge JVM profiles (the published i_f words
// themselves are bit-stable between the two profiles). The mode echo of each
// native landing must equal the capture's info line, and the constant must
// equal the JVM multiplicative predicate of that mode.

#include "kalos/x13_automatic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "jvm_x13_automatic.h"

namespace {

long long g_checks = 0;

void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
    ++g_checks;
}

double rdbits(std::uint64_t bits) {
    double v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

bool bit_equal(double a, double b) {
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

std::string hex_bits(double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, 8);
    std::ostringstream out;
    out << "0x" << std::hex << b;
    return out.str();
}

void check_bits(const std::string& where, double got, const std::string& hex) {
    check(bit_equal(got, rdbits(std::stoull(hex, nullptr, 16))),
          where + " got " + hex_bits(got) + " want " + hex);
}

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    for (std::string token; in >> token;) out.push_back(token);
    return out;
}

// ---- panel -------------------------------------------------------------------

struct Case {
    std::string id;
    std::string mode;
    bool log = false, mean = false;
    bool d12a_null = false, ifore_null = false;
    int d_year = 0, d_month = 0, d_count = 0;
    std::vector<std::string> d_words;
    int i_year = 0, i_month = 0, i_count = 0;
    std::vector<std::string> i_words;
};

bool parse_series(const std::vector<std::string>& row, int& year, int& month, int& count,
                  std::vector<std::string>& words) {
    if (row[1] == "null") return true;  // the null arm
    year = std::stoi(row[1]);
    month = std::stoi(row[2]);
    count = std::stoi(row[3]);
    words.assign(row.begin() + 4, row.end());
    check(static_cast<int>(words.size()) == count, row[0] + " word count vs declared size");
    return false;
}

std::vector<Case> read_panel() {
    std::ifstream in(KALOS_X13_IFORECAST_PANEL);
    check(static_cast<bool>(in), "missing x13-automatic-iforecast panel");
    std::vector<Case> panel;
    for (std::string line; std::getline(in, line);) {
        if (line.compare(0, 5, "case ") == 0) panel.push_back({line.substr(5)});
        else if (line.compare(0, 5, "info ") == 0) {
            check(!panel.empty(), "panel info row before any case");
            const auto row = fields(line);
            check(row.size() == 5, panel.back().id + " info row shape");
            panel.back().mode = row[1];
            panel.back().log = row[2] == "log=true";
            panel.back().mean = row[3] == "mean=true";
            check(row[4] == "valid=true", panel.back().id + " capture validity flag");
        } else if (line.compare(0, 5, "d12a ") == 0) {
            check(!panel.empty(), "panel d12a row before any case");
            const auto row = fields(line);
            panel.back().d12a_null = parse_series(row, panel.back().d_year, panel.back().d_month,
                                                  panel.back().d_count, panel.back().d_words);
        } else if (line.compare(0, 4, "i_f ") == 0) {
            check(!panel.empty(), "panel i_f row before any case");
            const auto row = fields(line);
            panel.back().ifore_null = parse_series(row, panel.back().i_year, panel.back().i_month,
                                                   panel.back().i_count, panel.back().i_words);
        } else if (!line.empty()) {
            check(!panel.empty(), "panel row before any case");
        }
    }
    check(panel.size() == 13, "incomplete x13-automatic-iforecast panel");
    for (const auto& c : panel)
        check(!c.mode.empty(), c.id + " incomplete panel block");
    return panel;
}

// ---- lane drivers (the mirror the P4 battery replay uses) --------------------

kalos::X13AutomaticRequest mirror_request(const golden_x13auto::Case& c,
                                          const std::vector<double>& input, int year, int month,
                                          int forecast) {
    kalos::X13AutomaticRequest r;
    r.input = input;
    r.frequency = 12;
    r.first_year = year;
    r.first_period = month;
    r.mode = static_cast<kalos::X11DecompositionMode>(c.mode);
    r.aicc_log_correction = rdbits(c.aicc_log_correction);
    r.max_d = 2;
    r.max_bd = 1;
    r.ub1 = rdbits(c.ub1);
    r.ub2 = rdbits(c.ub2);
    r.cancel = rdbits(c.cancel);
    r.differencing_precision = 1e-7;
    r.models_count = 5;
    r.max_p = 2;
    r.max_q = 2;
    r.max_bp = 1;
    r.max_bq = 1;
    r.balanced = c.balanced != 0;
    r.mixed = c.mixed != 0;
    r.arma_accept_white_noise = false;
    r.arma_eps = 1e-7;
    r.outliers_used = c.outliers_used != 0;
    for (int i = 0; i < c.n_types; ++i)
        r.outlier_types.push_back(static_cast<kalos::OutlierType>(c.otypes[i]));
    r.outlier_max_round = static_cast<std::size_t>(c.maxiter);
    r.outlier_max_outliers = 30;
    r.tcrate = rdbits(c.tcrate);
    r.precision = 1e-7;
    r.mean_precision = 1e-5;
    r.va = 0.;
    r.reduce_va = rdbits(c.reduce_va);
    r.ljung_box_limit = rdbits(c.ljung_box_limit);
    r.check_mu = true;
    r.accept_airline = false;
    r.ur_limit = 0.95;
    r.mean_test_initial = 1.96;
    r.mean_test_final = 1.0;
    r.tsig = 1.0;
    r.seasonal.assign(12, kalos::X11SeasonalRequest::msr);
    r.trend_length = 0;
    r.sigma = kalos::X11SigmaPolicy::accurate;
    r.lower_sigma = rdbits(c.lower_sigma);
    r.upper_sigma = rdbits(c.upper_sigma);
    r.backcast = 0;
    r.forecast = forecast;
    return r;
}

const golden_x13auto::Case* find_base(const std::string& id) {
    std::size_t count = 0;
    const golden_x13auto::Case* all = golden_x13auto::table(count);
    for (std::size_t i = 0; i < count; ++i)
        if (all[i].id == id) return &all[i];
    throw std::runtime_error("golden base case '" + id + "' missing");
}

struct Input {
    int year = 0, month = 0;
    std::vector<double> values;
};

Input read_csv(const std::string& id) {
    std::ifstream in(std::string(KALOS_X13_TEMPLATE_INPUTS) + "/" + id + ".csv");
    check(static_cast<bool>(in), id + ".csv input");
    Input input;
    char comma;
    check(static_cast<bool>(in >> input.year >> comma >> input.month) && comma == ',',
          id + " input calendar");
    for (double value; in >> value;) input.values.push_back(value);
    check(!input.values.empty(), id + " input values");
    return input;
}

kalos::X13AutomaticFitResult lane_fit(const std::string& id, const Input& input, int forecast) {
    return id.compare(0, 11, "stock_rsa3_") == 0
               ? kalos::x13_automatic_fit(
                     mirror_request(*find_base("ao"), input.values, input.year, input.month,
                                    forecast))
               : id.compare(0, 11, "stock_rsa0_") == 0
                   ? kalos::x13_estimated_rsa0(input.values, input.year, input.month)
                   : id.compare(0, 11, "stock_rsa1_") == 0
                       ? kalos::x13_estimated_rsa1(input.values, input.year, input.month)
                       : kalos::x13_estimated_rsa2c(input.values, input.year, input.month);
}

const char* mode_name(kalos::X11DecompositionMode mode) {
    return mode == kalos::X11DecompositionMode::multiplicative    ? "Multiplicative"
           : mode == kalos::X11DecompositionMode::log_additive    ? "LogAdditive"
           : mode == kalos::X11DecompositionMode::pseudo_additive ? "PseudoAdditive"
                                                                  : "Additive";
}

// The extractor's own predicate (DecompositionMode.isMultiplicative).
bool jvm_multiplicative(const std::string& mode) {
    return mode == "Multiplicative" || mode == "LogAdditive";
}

void fit_and_check(const Case& c, const Input& input) {
    const int forecast = c.id == "stock_rsa3_air_h0" ? 0 : 12;
    kalos::X13AutomaticFitResult result = lane_fit(c.id, input, forecast);
    check(result.status == "available", c.id + " lane status " + result.status);
    // The mode echo: the native effective mode equals the capture's mode, and
    // the log flag agrees with the multiplicative arm of the pinned forcing.
    check(std::string(mode_name(result.fit.mode_effective)) == c.mode,
          c.id + " mode echo " + mode_name(result.fit.mode_effective) + " want " + c.mode);
    check(result.log == c.log, c.id + " log flag echo");
    // The D12a domain the extractor reads (domain claim only: see the tier
    // policy above for the content-drift measurement).
    const long long total = 12LL * input.year + (input.month - 1) +
                            static_cast<long long>(input.values.size());
    const int start_year = static_cast<int>(total / 12);
    const int start_month = static_cast<int>(total % 12) + 1;
    const auto& finals = result.fit.finals;
    check(finals.status == "available", c.id + " finals status " + finals.status);
    if (c.d12a_null) {
        check(finals.d12a.empty(), c.id + " zero-horizon finals d12a must be empty");
        check(forecast == 0 && result.fit.forecast == 0, c.id + " zero-horizon echo");
    } else {
        check(finals.d12a.size() == static_cast<std::size_t>(c.d_count),
              c.id + " d12a count " + std::to_string(finals.d12a.size()));
        check(c.d_year == start_year && c.d_month == start_month,
              c.id + " d12a domain start " + std::to_string(c.d_year) + "-" +
                  std::to_string(c.d_month));
    }
    // The published product.
    const auto product = kalos::x13_automatic_irregular_forecast(result);
    if (c.ifore_null) {
        check(product.status == "no_requested_predictions" && product.values.empty(),
              c.id + " zero-horizon arm got " + product.status);
        return;
    }
    check(product.status == "available", c.id + " i_f status " + product.status);
    // Domain claims: count (and through it the dates, the start recomputed
    // above) agree with the capture row.
    check(product.values.size() == static_cast<std::size_t>(c.i_count),
          c.id + " i_f count " + std::to_string(product.values.size()));
    check(c.i_year == start_year && c.i_month == start_month,
          c.id + " i_f domain start " + std::to_string(c.i_year) + "-" +
              std::to_string(c.i_month));
    // Value claims: BIT tier, every word, plus the constant-mode invariant
    // implied by the reference definition.
    const double want = jvm_multiplicative(c.mode) ? 1. : 0.;
    for (std::size_t i = 0; i < product.values.size(); ++i) {
        check_bits(c.id + " i_f[" + std::to_string(i + 1) + "]", product.values[i], c.i_words[i]);
        check(bit_equal(product.values[i], want), c.id + " i_f constant invariant");
    }
}

}  // namespace

int main() {
    try {
        const auto panel = read_panel();
        for (const auto& c : panel) {
            const std::string stem = c.id == "stock_rsa3_air_h0"
                                         ? "air"
                                         : c.id.substr(c.id.find_last_of('_') + 1);
            const Input input = read_csv(stem);
            fit_and_check(c, input);
        }
        // The guard arm: a non-available fit must throw exactly as the y_ef
        // entry does (the honesty contract is about fit SHAPE, not a silent
        // "unavailable" for a half-built record).
        bool threw = false;
        try { (void)kalos::x13_automatic_irregular_forecast({}); }
        catch (const std::invalid_argument&) { threw = true; }
        check(threw, "non-available fit throws invalid_argument");
        // Determinism spot check: replaying one lane and one template answers
        // the same words.
        const Input air = read_csv("air");
        const auto first = kalos::x13_automatic_irregular_forecast(
            lane_fit("stock_rsa3_air", air, 12));
        const auto again = kalos::x13_automatic_irregular_forecast(
            lane_fit("stock_rsa3_air", air, 12));
        check(first.values.size() == again.values.size() &&
                  std::equal(first.values.begin(), first.values.end(), again.values.begin(),
                             bit_equal),
              "stock_rsa3_air i_f determinism");
        std::cout << "x13-automatic-iforecast: " << g_checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
