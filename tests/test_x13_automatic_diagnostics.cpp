// M7 (item 27 cutover gap): the nine-case double-JVM public automatic X-13
// residual diagnostics oracle, validation/evidence/native/
// x13-automatic-diagnostics/public-x86_64.txt, replayed BIT-exact through the
// automatic route's public getters (X13Results' fullResiduals plus the
// residuals.ser / residuals.lb / residuals.doornikhansen surface, transcribed
// as kalos::x13_automatic_residual_diagnostics over the final-model
// likelihood the automatic fits now populate).
//
// Case inventory: three stock X13Spec.RSA3 template fits on the
// x13-refusal-jvm-profiles CSVs (the decoder-equivalent RSA3 request mirrors
// the golden "ao" spec, the outlier type set {AO, LS, TC, SO} that
// X13AutomaticProbe shares with the automatic panels) and six decoder-
// equivalent automatic panels (X13AutomaticProbe request shapes from
// tests/jvm_x13_automatic.h with the X-11 mode and horizon overrides the
// probe applied, the same mirror automatic_si::check replays). The
// stock_rsa3_ao block is byte-identical to the ao_Undefined_0_0 block - the
// probe-level twin this transcription re-asserts fit-side.
//
// Tier policy: everything claimed here is a bit claim - the residual series
// (dates, count, every word), the standard error, the Ljung-Box statistic,
// p-value and degrees-of-freedom annotation, and the Doornik-Hansen triple.
// The Ljung-Box df mirror is the fitted regular+seasonal ARMA parameter
// count (24 - 2/3/4, pinned by the (0,1,1)(0,1,1), (1,0,1)(0,1,1) and
// (2,0,1)(0,1,1) landings printing 22.0/21.0/20.0). The likelihood line's
// first word is the JVM final-likelihood log-likelihood: on the automatic
// route the exported log_likelihood is the never-bit-claimed landing echo
// (the banded family in test_x13_automatic_oracle), so only the effective
// count, the parameter count and the ssq word are claimed here.
//
// Doornik-Hansen domain note (measured 2026-11-24): this capture ran the
// bridge profile, where Math.pow executes the intrinsic path, and the file's
// DH words match kalos::tramo_residual_normality (the host-libm pow arm, the
// pinned precedent of the tramo-seats-bridge residual diagnostics panels).
// The stock RSA1 panel of x13-estimated-templates ran the six-flag refusal
// profile (StrictMath pow semantics) and pins kalos::portable_residual_
// normality for the BYTE-IDENTICAL stock_rsa3_air / rsa1-air residual series
// (air value ...a962 vs ...a97e, p ...c00 vs ...bd4). Both variants are
// therefore replayed: the generic accessor is checked against this file, the
// frozen stock consumer (x13_stock_residual_diagnostics, whose domain is the
// estimated-template panel) against portable_residual_normality on the same
// fit. The y_ef rows claim the M13-P5 automatic-route forecast-error surface
// (kalos::x13_automatic_forecast_errors) as a TOLERANCE tier: the engine
// mirrors the pinned RegArimaForecasts.calcForecast semantics (exact diffuse
// prediction variance, FastDkFilter column sensitivities on the response's
// gains, unscaled final-likelihood covariance, LogNormal back-transform) and
// is proven to at most 9e-15 against the same-run dual-JVM captures, but it
// does not replicate the JVM's square-root filter arithmetic word for word,
// so each predicted word must agree to a relative tolerance of 1e-12 (never
// a bit claim); the measured worst disagreement over the nine-case panel is
// 1.9e-14. The coefficients row joins the bit claims: the exported
// final-likelihood regression coefficients (mean first, then outliers) are
// asserted word-exact against the capture.

#include "kalos/x13_automatic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "jvm_x13_automatic.h"

namespace {

long long g_checks = 0;
double g_worst_yef = 0.;

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

std::uint64_t to_bits(double v) {
    std::uint64_t b;
    std::memcpy(&b, &v, 8);
    return b;
}

std::string hex_bits(double v) {
    std::ostringstream out;
    out << "0x" << std::hex << to_bits(v);
    return out.str();
}

void check_bits(const std::string& where, double got, const std::string& hex) {
    check(bit_equal(got, rdbits(std::stoull(hex, nullptr, 16))),
          where + " got " + hex_bits(got) + " want " + hex);
}

bool vectors_bit_equal(const std::vector<double>& a, const std::vector<double>& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(),
                      [](double x, double y) { return bit_equal(x, y); });
}

// ---- oracle panel ----------------------------------------------------------

struct Block {
    std::string id;
    std::vector<std::string> lines;
};

std::vector<Block> read_panel() {
    std::ifstream in(KALOS_X13_AUTOMATIC_DIAGNOSTICS);
    check(static_cast<bool>(in), "missing x13-automatic-diagnostics panel");
    std::vector<Block> panel;
    for (std::string line; std::getline(in, line);) {
        if (line.compare(0, 5, "case ") == 0) panel.push_back({line.substr(5), {}});
        else if (!line.empty()) {
            check(!panel.empty(), "diagnostic row before any case");
            panel.back().lines.push_back(line);
        }
    }
    check(panel.size() == 9, "incomplete x13-automatic-diagnostics panel");
    return panel;
}

// ---- request mirror (identical to test_x13_automatic_oracle.cpp) -----------

kalos::X13AutomaticRequest mirror_request(const golden_x13auto::Case& c) {
    kalos::X13AutomaticRequest r;
    r.input.reserve(static_cast<std::size_t>(c.n));
    for (int i = 0; i < c.n; ++i) r.input.push_back(rdbits(c.values[i]));
    r.frequency = 12;
    r.first_year = c.first_year;
    r.first_period = c.first_period;
    r.mode = static_cast<kalos::X11DecompositionMode>(c.mode);
    r.aicc_log_correction = rdbits(c.aicc_log_correction);
    r.max_d = 2;
    r.max_bd = 1;
    r.ub1 = rdbits(c.ub1);   // module-space reciprocals, raw bits
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
    r.forecast = 0;
    return r;
}

const golden_x13auto::Case* find_base(const std::string& id) {
    std::size_t count = 0;
    const golden_x13auto::Case* all = golden_x13auto::table(count);
    for (std::size_t i = 0; i < count; ++i)
        if (all[i].id == id) return &all[i];
    throw std::runtime_error("golden base case '" + id + "' missing");
}

std::string last_field(std::string& id) {
    const auto split = id.rfind('_');
    if (split == std::string::npos) throw std::runtime_error("invalid automatic diagnostics case name");
    const auto field = id.substr(split + 1);
    id.resize(split);
    return field;
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

kalos::X13AutomaticRequest request_for(const std::string& id, const Input& stock_input) {
    if (id.compare(0, 11, "stock_rsa3_") == 0) {
        // The decoder-equivalent X13Spec.RSA3 request: the golden "ao" spec
        // (outlier types {AO, LS, TC, SO}) over the template CSV; the X-11
        // horizons play no role on the preprocessing surface. The probe's
        // stock_rsa3_ao block is byte-identical to the automatic panel's
        // ao_Undefined_0_0 block, pinning the mirror's request shape.
        auto request = mirror_request(*find_base("ao"));
        request.input = stock_input.values;
        request.first_year = stock_input.year;
        request.first_period = stock_input.month;
        return request;
    }
    std::string base = id;
    const int forecast = std::stoi(last_field(base));
    const int backcast = std::stoi(last_field(base));
    const std::string mode = last_field(base);
    auto request = mirror_request(*find_base(base));
    if (mode == "LogAdditive") request.mode = kalos::X11DecompositionMode::log_additive;
    else if (mode == "PseudoAdditive") request.mode = kalos::X11DecompositionMode::pseudo_additive;
    else if (mode != "Undefined") throw std::runtime_error(id + " unrecognized mode override");
    request.backcast = backcast;
    request.forecast = forecast;
    return request;
}

// ---- per-case replay ---------------------------------------------------------

void run_block(const Block& block, const Input& stock_input,
               std::map<std::string, kalos::X13AutomaticFitResult>& fits) {
    const std::string& id = block.id;
    const kalos::X13AutomaticRequest request = request_for(id, stock_input);
    const kalos::X13AutomaticFitResult result = kalos::x13_automatic_fit(request);
    check(result.status == "available", id + " status " + result.status);
    const kalos::X13StockResidualDiagnostics stats =
        kalos::x13_automatic_residual_diagnostics(result);
    const auto& surface = result.stock_likelihood.value();
    check(surface.full_residuals.size() == result.effective_observations,
          id + " residual length");
    for (const auto& line : block.lines) {
        std::istringstream row(line);
        std::string tag;
        row >> tag;
        if (tag == "model") {
            bool log, mean;
            int p, d, q, bp, bd, bq;
            row >> std::boolalpha >> log >> mean >> p >> d >> q >> bp >> bd >> bq;
            check(result.log == log && result.mean == mean && result.p == p &&
                      result.d == d && result.q == q && result.bp == bp &&
                      result.bd == bd && result.bq == bq,
                  id + " final description");
        } else if (tag == "likelihood") {
            std::size_t effective, estimated;
            std::string loglik, ssq;
            row >> effective >> estimated >> loglik >> ssq;
            check(effective == result.effective_observations &&
                      estimated == surface.estimated_parameters,
                  id + " likelihood dimensions");
            check_bits(id + " ssqErr", surface.residual_ssq, ssq);
        } else if (tag == "residuals") {
            int year, month, n;
            row >> year >> month >> n;
            check(n == static_cast<int>(surface.full_residuals.size()) &&
                      (year - request.first_year) * 12 + month - request.first_period ==
                          result.d + 12 * result.bd,
                  id + " residual dates");
            std::size_t i = 0;
            for (std::string hex; row >> hex; ++i) {
                check(i < surface.full_residuals.size(), id + " surplus residual");
                check_bits(id + " residual[" + std::to_string(i) + "]",
                           surface.full_residuals[i], hex);
            }
            check(i == surface.full_residuals.size(), id + " residual count");
        } else if (tag == "residuals.ser") {
            std::string hex;
            row >> hex;
            check_bits(id + " residuals.ser", stats.standard_error, hex);
        } else if (tag == "residuals.lb" || tag == "residuals.doornikhansen") {
            const bool lb = tag == "residuals.lb";
            std::string value, probability, distribution, with, degrees;
            double df;
            row >> value >> probability >> distribution >> with >> df >> degrees;
            check(distribution == "Chi2" && with == "with" && degrees == "degrees",
                  id + " " + tag + " distribution");
            check(stats.ljung_box.built && stats.normality.has_value(),
                  id + " " + tag + " built");
            check_bits(id + " " + tag + " value",
                       lb ? stats.ljung_box.value : stats.normality->value, value);
            check_bits(id + " " + tag + " p",
                       lb ? stats.ljung_box.p : stats.normality->p, probability);
            check((lb ? stats.ljung_box.df : stats.normality->df) == df,
                  id + " " + tag + " df");
        }
        if (tag == "coefficients") {
            std::size_t i = 0;
            for (std::string hex; row >> hex; ++i) {
                check(i < surface.coefficients.size(), id + " surplus coefficient");
                check_bits(id + " coefficient[" + std::to_string(i) + "]", surface.coefficients[i], hex);
            }
            check(i == surface.coefficients.size(), id + " coefficient count");
        } else if (tag.compare(0, 5, "y_ef_") == 0) {
            const int months = std::stoi(tag.substr(5));
            int year, month, count;
            row >> year >> month >> count;
            check(count == months &&
                      (year - request.first_year) * 12 + month - request.first_period ==
                          static_cast<int>(request.input.size()),
                  id + " y_ef horizon dates");
            const kalos::X13StockForecastErrors errors =
                kalos::x13_automatic_forecast_errors(result, months);
            check(errors.status == "available", id + " y_ef status " + errors.status);
            check(errors.standard_errors.size() == static_cast<std::size_t>(months),
                  id + " y_ef length");
            std::size_t i = 0;
            for (std::string hex; row >> hex; ++i) {
                check(i < errors.standard_errors.size(), id + " surplus y_ef");
                const double want = rdbits(std::stoull(hex, nullptr, 16));
                const double rel = std::fabs(errors.standard_errors[i] - want) / std::fabs(want);
                g_worst_yef = std::max(g_worst_yef, rel);
                check(rel <= 1e-12,
                      id + " y_ef[" + std::to_string(i) + "] rel " + std::to_string(rel));
            }
            check(i == errors.standard_errors.size(), id + " y_ef count");
        }
        // The arma row (banded landing echo) is pinned by
        // test_x13_automatic_oracle and carries no claim here.
    }
    fits.emplace(id, result);
    std::printf("%s %d residuals, ser 0x%llx, lb 0x%llx/0x%llx df %g, dh 0x%llx/0x%llx df %g\n",
                id.c_str(), static_cast<int>(surface.full_residuals.size()),
                static_cast<unsigned long long>(to_bits(stats.standard_error)),
                static_cast<unsigned long long>(to_bits(stats.ljung_box.value)),
                static_cast<unsigned long long>(to_bits(stats.ljung_box.p)),
                stats.ljung_box.df,
                static_cast<unsigned long long>(to_bits(stats.normality->value)),
                static_cast<unsigned long long>(to_bits(stats.normality->p)),
                stats.normality->df);
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> only(argv + 1, argv + argc);
    const auto panel = read_panel();
    const Input air = read_csv("air");
    const Input stock_ao = read_csv("ao");
    const Input seas = read_csv("seas");

    // The unavailable-domain guard fires before any surface is read.
    bool rejected = false;
    try { (void)kalos::x13_automatic_residual_diagnostics({}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "unavailable fit");

    std::map<std::string, kalos::X13AutomaticFitResult> fits;
    int ran = 0;
    for (const auto& block : panel) {
        if (!only.empty() &&
            std::find(only.begin(), only.end(), block.id) == only.end())
            continue;
        ++ran;
        std::printf("== %s\n", block.id.c_str());
        const Input stock_input = block.id == "stock_rsa3_air" ? air
                                      : block.id == "stock_rsa3_ao" ? stock_ao : seas;
        run_block(block, stock_input, fits);
    }
    if (ran == 0) {
        std::printf("no panel case matched the filter\n");
        return 1;
    }
    if (!only.empty()) {
        std::printf("%d cases replayed, %lld checks (filtered run)\n", ran, g_checks);
        return 0;
    }

    // Producer equivalence across the two routes on the three stock series.
    //  - AirPassengers: the stock RSA3 landing IS the kalos stock estimated
    //    route's rsa1 landing (same airline description, same residual
    //    series - the x13-estimated-templates panel already pins its
    //    residuals, ser and lb words for that fit). The generic accessor
    //    must reproduce the frozen stock consumer bit-exact on that fit
    //    through both routes; the Doornik-Hansen arms differ by the
    //    documented capture-domain split, and each is replayed from the
    //    shared residual series with its pinned variant.
    //  - ao/seas: no kalos stock estimated entry point exposes the RSA3
    //    type set, so the cross-route proof there is the file twin: the
    //    stock_rsa3_ao block is byte-identical to the ao_Undefined_0_0
    //    automatic-panel block, and the CSV input is bit-identical to the
    //    golden panel input, so the two fits must agree word for word.
    {
        const auto& mirror = fits.at("stock_rsa3_air");
        const auto& mirror_stats = kalos::x13_automatic_residual_diagnostics(mirror);
        const auto estimated = kalos::x13_estimated_rsa1(air.values, air.year, air.month);
        check(estimated.status == "available",
              "estimated rsa1 air status " + estimated.status);
        const auto& generic = kalos::x13_automatic_residual_diagnostics(estimated);
        const auto& frozen = kalos::x13_stock_residual_diagnostics(estimated);
        const auto& mirror_surface = mirror.stock_likelihood.value();
        const auto& estimated_surface = estimated.stock_likelihood.value();
        check(vectors_bit_equal(estimated_surface.full_residuals,
                                mirror_surface.full_residuals),
              "stock_rsa3_air residual series across routes");
        check(bit_equal(generic.standard_error, mirror_stats.standard_error) &&
                  bit_equal(generic.ljung_box.value, mirror_stats.ljung_box.value) &&
                  bit_equal(generic.ljung_box.p, mirror_stats.ljung_box.p) &&
                  generic.ljung_box.df == mirror_stats.ljung_box.df &&
                  bit_equal(generic.normality->value, mirror_stats.normality->value) &&
                  bit_equal(generic.normality->p, mirror_stats.normality->p),
              "stock_rsa3_air diagnostics across routes");
        check(bit_equal(frozen.standard_error, generic.standard_error) &&
                  bit_equal(frozen.ljung_box.value, generic.ljung_box.value) &&
                  bit_equal(frozen.ljung_box.p, generic.ljung_box.p) &&
                  frozen.ljung_box.df == generic.ljung_box.df,
              "stock_rsa3_air generic accessor vs the frozen stock consumer");
        const auto portable = kalos::portable_residual_normality(
            estimated_surface.full_residuals);
        check(portable && bit_equal(frozen.normality->value, portable->value) &&
                  bit_equal(frozen.normality->p, portable->p),
              "stock_rsa3_air stock consumer DH pins the StrictMath domain");
        const auto bridge = kalos::tramo_residual_normality(
            estimated_surface.full_residuals);
        check(bridge && bit_equal(generic.normality->value, bridge->value) &&
                  bit_equal(generic.normality->p, bridge->p),
              "stock_rsa3_air generic accessor DH pins the bridge domain");
    }
    {
        const auto& stock = fits.at("stock_rsa3_ao");
        const auto& panel_fit = fits.at("ao_Undefined_0_0");
        const auto& stock_values = stock.stock_likelihood.value().full_residuals;
        const auto& panel_values = panel_fit.stock_likelihood.value().full_residuals;
        check(vectors_bit_equal(stock_values, panel_values),
              "stock_rsa3_ao twin across request mirrors");
        check(vectors_bit_equal(stock.stock_likelihood.value().full_residuals,
                                stock_values), "stock_rsa3_ao determinism");
        const auto& ao_case = *find_base("ao");
        std::vector<double> golden_input;
        for (int i = 0; i < ao_case.n; ++i) golden_input.push_back(rdbits(ao_case.values[i]));
        check(vectors_bit_equal(golden_input, stock_ao.values),
              "ao.csv is bit-identical to the golden ao panel input");
    }
    {
        // The same 96-point input under the two specs must NOT collapse: the
        // seas CSV is the golden seas panel input (bit-checked below), yet
        // stock_rsa3_seas lands on the RSA3 model (2,0,1)(0,1,1) - proof the
        // mirror carries the RSA3 outlier set, not the golden seas spec.
        const auto seas_case = *find_base("seas");
        std::vector<double> golden_input;
        for (int i = 0; i < seas_case.n; ++i) golden_input.push_back(rdbits(seas_case.values[i]));
        check(vectors_bit_equal(golden_input, seas.values),
              "seas.csv is bit-identical to the golden seas panel input");
        check(fits.at("stock_rsa3_seas").p == 2 && fits.at("stock_rsa3_seas").q == 1 &&
                  fits.at("stock_rsa3_seas").bq == 1 && fits.at("stock_rsa3_seas").d == 0,
              "stock_rsa3_seas lands on the RSA3 description, not the seas-spec one");
    }

    // The forecast-error entry's guard domain (same convention as the
    // residual accessor above).
    bool rejected_forecast = false;
    try { (void)kalos::x13_automatic_forecast_errors({}, 0); }
    catch (const std::invalid_argument&) { rejected_forecast = true; }
    check(rejected_forecast, "forecast errors on an unavailable fit");
    bool negative_months = false;
    try { (void)kalos::x13_automatic_forecast_errors(fits.at("stock_rsa3_air"), -1); }
    catch (const std::invalid_argument&) { negative_months = true; }
    check(negative_months, "negative forecast horizon");
    const auto zero_months = kalos::x13_automatic_forecast_errors(fits.at("stock_rsa3_air"), 0);
    check(zero_months.status == "no_requested_predictions" && zero_months.standard_errors.empty(),
          "zero-month forecast request");

    std::printf("%d cases, %lld checks, worst y_ef rel %.3e\n", ran, g_checks, g_worst_yef);
    return 0;
}
