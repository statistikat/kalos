#include "kalos/estimation_control.hpp"

#include "kalos/arma_optimizer.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace kalos {
namespace {

// Pinned RegSarimaComputer.UR_LIMIT.
constexpr double ur_limit = 0.1;

double unavailable() { return std::numeric_limits<double>::quiet_NaN(); }

const char* stage_name(EstimationStartingPoint point) {
    switch (point) {
    case EstimationStartingPoint::Zero: return "zero";
    case EstimationStartingPoint::Default: return "default";
    case EstimationStartingPoint::HannanRissanen: return "hannan_rissanen";
    default: return "multiple";
    }
}

// The pinned SarimaModel stores the raw coefficients of the polynomial
// 1 + phi(B); the shared Kalos convention is 1 - phi(B) with MA 1 + theta(B)
// on both sides. The unit-root passes are transcribed literally on the
// pinned convention, so every entry crosses this sign flip.
std::array<std::vector<double>, 4> flip_ar(std::array<std::vector<double>, 4> coefficients) {
    for (const std::size_t b : {std::size_t(0), std::size_t(2)}) {
        for (auto& value : coefficients[b]) value = -value;
    }
    return coefficients; // The flip is an involution.
}

// Pinned tryUrpCancelling deflate: par[i] = -acc; acc += old, top-down, for
// the regular AR and regular MA blocks on the pinned arrays.
void deflate_regular(std::array<std::vector<double>, 4>& pinned) {
    for (const std::size_t block : {std::size_t(0), std::size_t(1)}) {
        auto& coefficients = pinned[block];
        double acc = 0.0;
        for (std::size_t i = coefficients.size(); i-- > 0;) {
            const double old = coefficients[i];
            coefficients[i] = -acc;
            acc += old;
        }
    }
}

// Pinned tryUrmCancelling deflate: par[i] = acc; acc = old - acc, top-down,
// for the same (regular) blocks; the seasonal blocks are never touched.
void deflate_seasonal(std::array<std::vector<double>, 4>& pinned) {
    for (const std::size_t block : {std::size_t(0), std::size_t(1)}) {
        auto& coefficients = pinned[block];
        double acc = 0.0;
        for (std::size_t i = coefficients.size(); i-- > 0;) {
            const double old = coefficients[i];
            coefficients[i] = acc;
            acc = old - acc;
        }
    }
}

// The pinned gate accumulators on one regular block: the plain sum for the
// unit-root pass and the toggle sum (-c0 + c1 - c2 + ...) for the -1 pass.
double pinned_gate_sum(const std::vector<double>& coefficients, bool alternating) {
    double acc = 0.0;
    bool negate = true; // The pinned toggle negates the first entry.
    for (const double value : coefficients) {
        acc += (alternating && negate ? -value : value);
        negate = !negate;
    }
    return acc;
}

bool within_limit(double pinned_sum) { return std::abs(1.0 + pinned_sum) <= ur_limit; }

// Maps physical coefficients through the qualified parameter map to the
// solver coordinates. Reflection-mode blocks use the map's own inverse
// construction (atanh of the reflection over 1 - margin); coefficient-mode
// blocks pass free entries directly. A non-admissible proposal yields false:
// the pinned raw-coordinate optimizer would throw there and its passes keep
// the incumbent, so the control layer does the same.
bool to_coordinates(const ArmaParameterMap& map,
    const std::array<std::vector<double>, 4>& physical, std::vector<double>& out) {
    out.clear();
    for (std::size_t b = 0; b < 4; ++b) {
        const auto mode = map.modes()[b];
        const auto& block = map.blocks()[b];
        if (mode == "empty" || mode == "fixed") continue;
        if (mode == "coefficient") {
            for (std::size_t j = 0; j < physical[b].size(); ++j) {
                if (!block.fixed[j]) out.push_back(physical[b][j]);
            }
            continue;
        }
        if (mode != "reflection") return false;
        const auto check = arma_admissibility(physical[b], b == 1 || b == 3, map.margin());
        if (check.status != "admissible") return false;
        for (const double reflection : check.reflections) {
            const double coordinate = std::atanh(reflection / (1 - map.margin()));
            if (!std::isfinite(coordinate)) return false;
            out.push_back(coordinate);
        }
    }
    return true;
}

struct PassOutcome {
    bool available = false;
    std::string optimizer_status;
    std::size_t evaluations = 0;
    double log_likelihood = unavailable();
    std::array<std::vector<double>, 4> coefficients;
};

PassOutcome run_pass(const SarimaEstimationRequest& request, const ArmaParameterMap& map,
    const std::vector<double>& coordinates, double precision, const Interrupt& interrupt) {
    PassOutcome outcome;
    OptimizerControl control = request.optimizer;
    control.function_tolerance = precision; // Pinned minimizer function precision.
    control.validate();
    OptimizationResult result = optimize_arma_objective(request.series, request.design, map,
        coordinates, request.period, request.d, request.seasonal_d, 0.0, 1e-10, 0.0, control,
        interrupt);
    outcome.optimizer_status = std::move(result.status);
    outcome.evaluations = static_cast<std::size_t>(result.evaluations);
    const auto point = std::static_pointer_cast<const ArmaOptimizationPoint>(result.final.detail);
    if (!point || result.final.status != "available" ||
        point->objective.status != "available") return outcome;
    outcome.available = true;
    outcome.log_likelihood = point->objective.adjusted_log_likelihood;
    outcome.coefficients = point->objective.parameters.coefficients;
    return outcome;
}

// One pinned unit-root-cancellation pass on the incumbent. The gates and the
// deflation reproduce the pinned bytecode branch by branch; a candidate is
// kept only on a strictly better likelihood and every failure keeps the
// incumbent (the pinned passes swallow all exceptions).
void run_cancelling_pass(SarimaEstimationResult& result, EstimationPassRecord record,
    const SarimaEstimationRequest& request, const ArmaParameterMap& map, bool seasonal,
    const Interrupt& interrupt, std::array<std::vector<double>, 4>& current, double& current_ll) {
    record.gates_passed = !current[0].empty() && !current[1].empty() &&
        (seasonal ? request.seasonal_d : request.d) > 0;
    if (record.gates_passed) {
        const auto pinned = flip_ar(current);
        record.gates_passed = within_limit(pinned_gate_sum(pinned[0], seasonal)) &&
            within_limit(pinned_gate_sum(pinned[1], seasonal));
        if (record.gates_passed) {
            auto deflated = flip_ar(current);
            if (seasonal) deflate_seasonal(deflated); else deflate_regular(deflated);
            record.attempted = true;
            std::vector<double> coordinates;
            if (to_coordinates(map, flip_ar(deflated), coordinates)) {
                const auto candidate = run_pass(request, map, coordinates, request.control.eps,
                    interrupt);
                record.optimizer_status = candidate.optimizer_status;
                record.evaluations = candidate.evaluations;
                record.log_likelihood = candidate.log_likelihood;
                if (candidate.available && candidate.log_likelihood > current_ll) {
                    record.accepted = true;
                    current = candidate.coefficients;
                    current_ll = candidate.log_likelihood;
                }
            }
            // An inadmissible deflated start mirrors a pinned exception: the
            // pass is attempted, the candidate is kept out and nothing throws.
        }
    }
    result.passes.push_back(std::move(record));
}

} // namespace

void EstimationControl::validate() const {
    if (!std::isfinite(eps) || eps <= 0 || !std::isfinite(feps) || feps <= 0) {
        throw std::invalid_argument("estimation precisions must be finite and positive");
    }
    if (!maximum_likelihood) {
        throw std::invalid_argument(
            "only the maximum-likelihood objective form is qualified for estimation");
    }
    if (starting_point != EstimationStartingPoint::Zero &&
        starting_point != EstimationStartingPoint::Default) {
        throw std::invalid_argument(std::string("starting point '") +
            stage_name(starting_point) +
            "' requires the Hannan-Rissanen initializer, which is not yet qualified");
    }
}

SarimaEstimationResult estimate_sarima_fixed(
    const SarimaEstimationRequest& request, const Interrupt& interrupt) {
    request.control.validate();
    for (const auto& block : request.blocks) {
        if (block.fixed.size() != block.initial.size()) {
            throw std::invalid_argument(
                "Sarima coefficient states must match their initial values");
        }
    }
    const ArmaParameterMap map(request.blocks);
    std::size_t free_count = 0;
    for (const auto& block : request.blocks) {
        for (const auto flag : block.fixed) free_count += flag ? 0 : 1;
    }
    validate_arma_objective_inputs(request.series, request.design, map, request.period,
        request.d, request.seasonal_d, 0.0, 1e-10, 0.0, interrupt);

    SarimaEstimationResult result;
    if (free_count == 0) { // Pinned parametersCount/dimension guard.
        std::array<std::vector<double>, 4> fixed_coefficients;
        for (std::size_t b = 0; b < 4; ++b) fixed_coefficients[b] = request.blocks[b].initial;
        const auto objective = evaluate_arma_physical_objective(request.series, request.design,
            map, fixed_coefficients, request.period, request.d, request.seasonal_d, 0.0, 1e-10,
            0.0, interrupt);
        result.status = "likelihood_only";
        result.converged = false;
        result.coefficients = objective.parameters.coefficients;
        result.log_likelihood = objective.status == "available" ? objective.adjusted_log_likelihood
                                                                : unavailable();
        return result;
    }

    const bool zero_start = request.control.starting_point == EstimationStartingPoint::Zero;
    std::array<std::vector<double>, 4> physical;
    for (std::size_t b = 0; b < 4; ++b) {
        const double fill = zero_start ? 0.0 : (b == 1 || b == 3 ? -0.2 : 0.1);
        physical[b].assign(request.blocks[b].initial.size(), fill);
        for (std::size_t j = 0; j < physical[b].size(); ++j) {
            if (request.blocks[b].fixed[j]) physical[b][j] = request.blocks[b].initial[j];
        }
    }
    std::vector<double> coordinates;
    if (!to_coordinates(map, physical, coordinates)) {
        throw std::invalid_argument("the pinned starting point is not admissible for this map");
    }

    EstimationPassRecord record{};
    record.stage = "estimate";
    record.attempted = true;
    record.gates_passed = true;
    const auto initial = run_pass(request, map, coordinates, request.control.eps, interrupt);
    record.optimizer_status = initial.optimizer_status;
    record.evaluations = initial.evaluations;
    record.log_likelihood = initial.log_likelihood;
    record.accepted = initial.available; // The retained initial candidate.
    result.passes.push_back(std::move(record));

    if (!initial.available) {
        result.status = initial.optimizer_status;
        result.converged = false;
        result.coefficients = std::move(physical);
        return result; // log_likelihood stays NaN: no candidate was retained.
    }
    auto current = initial.coefficients;
    auto current_ll = initial.log_likelihood;
    bool producing_converged = initial.optimizer_status == "converged_gradient";
    std::string producing_status = initial.optimizer_status;

    for (const bool seasonal : {false, true}) {
        run_cancelling_pass(result,
            EstimationPassRecord{seasonal ? "ur_seasonal" : "ur_regular",
                false, false, false, {}, 0, unavailable()},
            request, map, seasonal, interrupt, current, current_ll);
        if (result.passes.back().accepted) {
            producing_converged = result.passes.back().optimizer_status == "converged_gradient";
            producing_status = result.passes.back().optimizer_status;
        }
    }

    result.coefficients = std::move(current);
    result.log_likelihood = current_ll;
    result.converged = producing_converged;
    result.status = producing_converged ? "converged" : std::move(producing_status);
    return result;
}

} // namespace kalos
