// Public adjusted-series diagnostics from both monthly seasonal-adjustment methods.
#include "kalos/combined_seasonality.hpp"
#include "kalos/tramo_seats_pipeline.hpp"
#include "kalos/x13_automatic.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
double decode(const std::string& s) {
    const auto raw = std::stoull(s, nullptr, 16);
    double value;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}
std::optional<kalos::AdjustedSeasonality> fit_product(const std::string& id) {
    if (id.compare(0, 6, "short_") == 0) return std::nullopt;
    const auto last = id.rfind('_');
    const auto name = id.substr(0, last);
    std::ifstream input(std::string(KALOS_ADJUSTED_INPUTS) + "/" + id.substr(last+1) + ".csv");
    int year, month;
    char comma;
    if (!(input >> year >> comma >> month) || comma != ',') throw std::runtime_error("missing model input");
    std::vector<double> values;
    for (double value; input >> value;) values.push_back(value);
    if (name == "x13_rsa0" || name == "x13_rsa1") {
        const auto product = (name == "x13_rsa0" ? kalos::x13_estimated_rsa0 : kalos::x13_estimated_rsa1)(values, year, month);
        return kalos::x13_adjusted_seasonality(product.fit, product.d + product.bd, product.mean);
    }
    if (name == "tramo_rsa0") {
        const auto product = kalos::run_tramo_seats_rsa0(values, year, month);
        return kalos::seats_adjusted_seasonality(product.decomposition.extraction.initial_components(), 2, true);
    }
    if (name.compare(0, 6, "tramo_") == 0) {
        const auto run = name == "tramo_rsa1" ? kalos::run_tramo_seats_rsa1 :
                         name == "tramo_rsa3" ? kalos::run_tramo_seats_rsa3 : kalos::run_tramo_seats_rsafull;
        const auto product = run(values, year, month, 0, -2);
        const auto& model = product.preprocessing;
        if (product.decomposition.extraction.initial_components().empty()) {
            if ((id != "tramo_rsa3_seas" && id != "tramo_rsafull_seas") ||
                product.decomposition.extraction.fact("status") != "error:jdplus.toolkit.base.core.arima.ArimaException")
                throw std::runtime_error(id + " unexpected SEATS extraction failure");
            return std::nullopt;
        }
        return kalos::seats_adjusted_seasonality(product.decomposition.extraction.initial_components(),
                                               model.orders.d + model.orders.bd, model.mean);
    }
    return std::nullopt;
}

}
int main() {
    std::ifstream input(KALOS_ADJUSTED_SEASONALITY_PANEL);
    std::string id;
    int order = 0, cases = 0, tests = 0;
    bool mean = false;
    kalos::AdjustedSeasonality result;
    double worst = 0., product_worst = 0.;
    int products = 0;
    std::optional<kalos::AdjustedSeasonality> product;
    for (std::string line; std::getline(input, line);) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "case") {
            int year, month;
            row >> id >> year >> month >> order >> std::boolalpha >> mean;
            product = fit_product(id);
            if (product) ++products;
            ++cases;
        } else if (key == "model_scale_sa") {
            std::vector<double> sa;
            for (std::string bits; row >> bits;) sa.push_back(decode(bits));
            result = kalos::monthly_adjusted_seasonality(sa, order, mean);
        } else if (key == "diagnostics.seas-sa-qs" || key == "diagnostics.seas-sa-f") {
            const auto& test = key == "diagnostics.seas-sa-qs" ? result.qs : result.seasonal_f;
            std::string raw_value, raw_probability;
            row >> raw_value;
            ++tests;
            if (raw_value == "unavailable") {
                if (test) throw std::runtime_error(id + " expected unavailable " + key);
                continue;
            }
            if (!test || !(row >> raw_probability)) throw std::runtime_error(id + " incomplete " + key);
            const auto* product_test = product ? (key == "diagnostics.seas-sa-qs" ? &product->qs : &product->seasonal_f) : nullptr;
            if (product_test && !*product_test) throw std::runtime_error(id + " missing fitted " + key);
            int index = 0;
            for (const double actual : {test->value, test->p_value}) {
                const double expected = decode(index == 0 ? raw_value : raw_probability);
                if (std::isnan(expected) && std::isnan(actual)) {
                    ++index;
                    continue;
                }
                const double scaled = std::abs(actual - expected) / std::max(1., std::abs(expected));
                if (actual != expected)
                    throw std::runtime_error(id + " " + key + " expected " + std::to_string(expected) +
                                             " actual " + std::to_string(actual));
                worst = std::max(worst, scaled);
                if (product_test) {
                    const double fitted = index == 0 ? (*product_test)->value : (*product_test)->p_value;
                    const double error = std::abs(fitted - expected) / std::max(1., std::abs(expected));
                    if (!(error < 1e-8)) throw std::runtime_error(id + " fitted " + key + " scaled " + std::to_string(error));
                    product_worst = std::max(product_worst, error);
                }
                ++index;
            }
        }
    }
    if (cases != 32 || tests != 64 || products != 16) throw std::runtime_error("incomplete adjusted seasonality reference");
    std::cout << cases << " reference cases, " << tests << " tests, maximum scaled error " << worst << "; " << products << " end-to-end fits, worst " << product_worst << "\n";
}
