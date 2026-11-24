#include "kalos/combined_seasonality.hpp"
#include "kalos/seats_extract_wk.hpp"
#include "kalos/x13_fit.hpp"
#include "kalos/x13_automatic.hpp"
#include "kalos/tramo_seats_pipeline.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
double decode(const std::string& hex) {
    const auto bits = std::stoull(hex, nullptr, 16);
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
struct Input {
    int year, month;
    std::vector<double> values;
};
std::map<std::string, Input> read_inputs() {
    std::map<std::string, Input> inputs;
    for (const char* filename : {"rsa3-x86_64.txt", "rsa3-ref-retry-x86_64.txt",
                                 "rsafull-x86_64.txt", "rsafull-late-calendar-x86_64.txt"}) {
        std::ifstream in(std::string(KALOS_COMBINED_SI_INPUTS) + "/" + filename);
        check(in.good(), filename);
        std::string id;
        for (std::string line; std::getline(in, line);) {
            std::istringstream row(line);
            std::string key;
            row >> key;
            if (key == "case") row >> id;
            if (key != "input") continue;
            const bool full_template = std::string(filename).compare(0, 7, "rsafull") == 0;
            auto& input = inputs[full_template ? "rsafull_" + id : id];
            std::size_t n;
            row >> input.year >> input.month >> n;
            for (std::string hex; row >> hex;) input.values.push_back(decode(hex));
            check(input.values.size() == n, id + " input length");
        }
    }
    for (const std::string id : {"air", "ao", "seas"}) {
        std::ifstream in(std::string(KALOS_COMBINED_X13_INPUTS) + "/" + id + ".csv");
        auto& input = inputs["x13_" + id];
        char comma;
        check(static_cast<bool>(in >> input.year >> comma >> input.month) && comma == ',', id + " stock X-13 domain");
        for (double value; in >> value;) input.values.push_back(value);
    }
    return inputs;
}
kalos::X11DecompositionMode mode(const std::string& id, bool multiplicative) {
    if (id.find("LogAdditive") != std::string::npos) return kalos::X11DecompositionMode::log_additive;
    if (id.find("PseudoAdditive") != std::string::npos) return kalos::X11DecompositionMode::pseudo_additive;
    return multiplicative ? kalos::X11DecompositionMode::multiplicative : kalos::X11DecompositionMode::additive;
}
std::optional<kalos::CombinedSiSeasonality> run_product(const std::string& id, const std::string& source,
        bool multiplicative, const std::map<std::string, Input>& inputs, bool stock) {
    if (source == "fixed_d8") {
        kalos::X13FitRequest request;
        request.input = inputs.at("fit_tr0_air").values;
        request.log = multiplicative;
        request.mode = mode(id, multiplicative);
        request.seasonal.assign(12, kalos::X11SeasonalRequest::s3x5);
        request.trend_length = 13;
        request.sigma = kalos::X11SigmaPolicy::legacy;
        const auto fit = kalos::x13_fit(request);
        check(fit.status == "available" && fit.mode_effective == request.mode, id + " fixed mode");
        return kalos::x13_si_seasonality(fit);
    }
    if (stock && source == "d8") {
        const auto& input = inputs.at("x13_" + id.substr(id.rfind('_') + 1));
        const auto run = id.compare(0, 8, "x13_rsa0") == 0 ? kalos::x13_estimated_rsa0 : kalos::x13_estimated_rsa1;
        const auto result = run(input.values, input.year, input.month);
        return kalos::x13_si_seasonality(result.fit);
    }
    if (id.compare(0, 8, "rsafull_") == 0) {
        const auto& input = inputs.at(id);
        const auto fit = kalos::run_tramo_seats_rsafull(input.values, input.year, input.month);
        check(fit.preprocessing.log == multiplicative, id + " RSAfull preprocessing transform");
        return kalos::seats_si_seasonality(fit.decomposition.extraction.final_components(), 12, fit.preprocessing.log);
    }
    if (source.compare(0, 3, "wk_") != 0) return std::nullopt;
    const auto& input = inputs.at(id.substr(id.find('_') + 1));
    if (id.compare(0, 4, "rsa0") == 0) {
        const auto fit = kalos::run_tramo_seats_rsa0(input.values, input.year, input.month);
        check(fit.preprocessing.model.log == multiplicative, id + " preprocessing transform");
        return kalos::seats_si_seasonality(fit.decomposition.extraction.final_components(), 12, fit.preprocessing.model.log);
    }
    const auto run = id.compare(0, 4, "rsa1") == 0 ? kalos::run_tramo_seats_rsa1 : kalos::run_tramo_seats_rsa3;
    const auto fit = run(input.values, input.year, input.month, 0, 24);
    check(fit.preprocessing.log == multiplicative, id + " preprocessing transform");
    return kalos::seats_si_seasonality(fit.decomposition.extraction.final_components(), 12, fit.preprocessing.log);
}
std::array<double, 6> values(const kalos::CombinedSiSeasonality& result) {
    return {result.stable_f, result.stable_p, result.moving_f, result.moving_p, result.kruskal_wallis, result.kruskal_p};
}

}

int main() {
    const auto inputs = read_inputs();
    std::string id, source;
    bool multiplicative = false;
    int year = 0, first = 0, period = 0, count = 0, statistics = 0;
    std::optional<kalos::CombinedSiSeasonality> actual, product;
    int product_count = 0, product_statistics = 0;
    double product_worst = 0;
    std::vector<double> si;
    std::map<std::string, int> summaries;
    double worst = 0;
    for (const char* path : {KALOS_COMBINED_SI_PANEL, KALOS_COMBINED_SI_FIXED, KALOS_COMBINED_STOCK}) {
      const bool stock = std::string(path) == KALOS_COMBINED_STOCK;
      std::ifstream in(path);
      check(in.good(), path);
      for (std::string line; std::getline(in, line);) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "case") {
            row >> id >> source >> std::boolalpha >> multiplicative;
            product = run_product(id, source, multiplicative, inputs, stock);
            if (stock || source == "fixed_d8" || source.compare(0, 3, "wk_") == 0) {
                check(product.has_value(), id + " native product diagnostic");
                ++product_count;
            }
        } else if (key == "si") {
            int length;
            row >> year >> first >> period >> length;
            si.clear();
            for (std::string hex; row >> hex;) si.push_back(decode(hex));
            check(static_cast<int>(si.size()) == length, id + " series length");
            actual = kalos::combined_si_seasonality(si, period, first, multiplicative);
        } else if (key == "statistics") {
            check(actual.has_value(), id + " numerical result");
            const auto measured = values(*actual);
            for (std::size_t i = 0; i < measured.size(); ++i) {
                const double value = measured[i];
                std::string hex;
                row >> hex;
                const double ref = decode(hex);
                if (std::isfinite(ref)) {
                    const double error = std::abs(ref - value) / (1 + std::abs(ref));
                    check(error <= 1e-12, id + " statistic " + hex + " got " + std::to_string(value));
                    worst = std::max(worst, error);
                } else check(std::isnan(ref) ? std::isnan(value) : ref == value, id + " statistic class");
                if (product) {
                    const double error = std::abs(values(*product)[i] - ref) / (1 + std::abs(ref));
                    check(error <= 1e-12, id + " product statistic " + std::to_string(i) + " reference " + std::to_string(ref) + " actual " + std::to_string(values(*product)[i]));
                    product_worst = std::max(product_worst, error);
                    ++product_statistics;
                }
                ++statistics;
            }
        } else if (key == "summary") {
            std::string summary;
            row >> summary;
            check((actual ? actual->summary : "unavailable") == summary, id + " summary " + summary);
            if (product) check(product->summary == summary, id + " native dashboard verdict");
            if (source == "d8") {
                kalos::X13FitResult fit;
                fit.status = "available";
                fit.frequency = period;
                fit.first_period = first;
                fit.backcast = 3;
                fit.forecast = 6;
                const bool log = id.find("LogAdditive") != std::string::npos;
                fit.mode_effective = mode(id, multiplicative);
                auto& d8 = fit.selection.d.d8;
                d8.values.assign(si.size() + 9, 0);
                d8.missing.assign(d8.values.size(), 1);
                for (std::size_t i = 0; i < si.size(); ++i) {
                    d8.values[3+i] = log ? std::log(si[i]) : si[i];
                    d8.missing[3+i] = 0;
                }
                auto computed = kalos::x13_si_seasonality(fit);
                check(computed && computed->summary == summary, id + " observed D8 result");
            } else if (source == "wk_irregular" || source == "wk_seasonal") {
                const auto component = source == "wk_irregular" ? kalos::seats_component::irregular : kalos::seats_component::seasonal;
                kalos::seats_component_series only{component, kalos::seats_information::value, year, first, si};
                auto computed = kalos::seats_si_seasonality({only}, period, multiplicative);
                check(computed && computed->summary == summary, id + " single WK component");
            }
            ++summaries[summary];
            ++count;
        } else throw std::runtime_error("unexpected combined-seasonality record");
      }
    }
    check(!kalos::seats_si_seasonality({},12,false) && !kalos::x13_si_seasonality({}), "absent components");
    check(count == 101 && statistics == 594 && product_count == 59 && product_statistics == 354 && summaries.size() == 4, "incomplete public SI panel");
    std::cout << count << " combined SI cases, " << statistics << " numerical checks, maximum scaled difference " << worst << "; " << product_count << " end-to-end products, maximum scaled difference " << product_worst << "\n";
}
