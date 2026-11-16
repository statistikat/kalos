#ifndef KALOS_ESTIMATION_CONTROL_HPP
#define KALOS_ESTIMATION_CONTROL_HPP

#include "arma_objective.hpp"
#include "arma_parameters.hpp"
#include "optimizer.hpp"
#include "regression_likelihood.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace kalos {

// The starting-point policy of the pinned jdplus RegSarimaComputer. The
// Hannan-Rissanen and Multiple starts require the Hannan-Rissanen
// initializer, which is separate item-21 work; selecting them is a rejected
// request, not a silent fallback. Promotion of any pinned value into
// configuration is held by the maintainer decision list of
// docs/item21-roadmap.md.
enum class EstimationStartingPoint { Zero, Default, HannanRissanen, Multiple };

// Pinned RegSarimaComputer control fields, kept as required request data:
// there is no default-constructed control and the core applies no implicit
// value. Numerically active in this unit: eps, starting_point. The remaining
// fields record the pinned call-site fidelity and are validated below.
struct EstimationControl {
    // Final optimization precision (pinned DEF_EPS 1e-7; the pinned
    // PROCESSOR singleton uses 1e-9). Forwarded to the solver as its
    // function tolerance; the solver keeps its qualified gradient criterion.
    double eps;
    // Pinned internal precision (DEF_INTERNAL_EPS 1e-4) of the Multiple-start
    // passes. Positive and finite; inert while the Multiple start is refused.
    double feps;
    // Pinned ml: objective in maximum-likelihood form. Only true is qualified
    // by the existing objective; false is rejected rather than approximated.
    bool maximum_likelihood;
    // Pinned mt (builder useParallelProcessing): a stored, never-read field
    // of the pinned processor. Accepted at either value; no Kalos effect.
    bool unit_root_search;
    // Pinned cdf (builder useCorrectedDegreesOfFreedom): reduces the
    // score/information sample by the mapping dimension. This unit exposes no
    // score or information, so either value is accepted with no effect.
    bool corrected_degrees_of_freedom;
    // Pinned fast (builder computeExactFinalDerivatives sets its negation):
    // derivative source of the pinned processor. The qualified Kalos solver
    // owns its derivative policy, so either value is accepted.
    bool fast_derivatives;
    EstimationStartingPoint starting_point;
    void validate() const;
};

// One fixed-order Sarima estimation request for the control layer. The
// blocks use the shared Kalos coefficient convention (AR 1 - phi(B), MA
// 1 + theta(B); [regular AR, regular MA, seasonal AR, seasonal MA] block
// order) and the mean/drift enter through explicit regression columns exactly
// as in the qualified regression-likelihood contract; no implicit mean exists
// here.
struct SarimaEstimationRequest {
    MaskedSeries series;
    RegressionDesign design;
    // Entries flagged fixed keep their initial value exactly. Free entries
    // must be admissible as map templates; their values never act as a
    // starting point because the pinned starting-point policy overwrites
    // every coefficient before optimization.
    std::array<ArmaParameterBlock, 4> blocks;
    int period;
    int d;
    int seasonal_d;
    EstimationControl control;
    // Qualified 20b solver controls; function_tolerance is replaced by eps.
    OptimizerControl optimizer;
};

// One decision record of the control layer: the final estimation pass and
// each pinned unit-root-cancellation restart attempt.
struct EstimationPassRecord {
    std::string stage;   // estimate, ur_regular or ur_seasonal.
    bool attempted;      // The pinned gates fired and an optimization ran.
    bool gates_passed;   // Both declared polynomials fell within UR_LIMIT 0.1.
    bool accepted;       // A candidate strictly improved the likelihood.
    std::string optimizer_status; // Empty when no optimization ran.
    std::size_t evaluations; // Objective evaluations of this pass.
    double log_likelihood; // Adjusted concentrated ML; NaN when unavailable.
};

struct SarimaEstimationResult {
    // converged | likelihood_only | the producing pass's solver status.
    std::string status;
    bool converged;
    // Final coefficients, Kalos convention; fixed entries preserved exactly.
    std::array<std::vector<double>, 4> coefficients;
    double log_likelihood; // NaN when the retained candidate is unavailable.
    std::vector<EstimationPassRecord> passes;
};

// Fixed-order estimation control over the qualified objective and optimizer:
// the pinned Zero/Default starting points, the eps-precision pass and the
// pinned unit-root-cancellation restart passes (regular pass gated on
// p, q, d and |1 + phi(B)|, |1 + theta(B)| at B = 1; seasonal pass gated on
// p, q, seasonal_d and the same polynomials at B = -1; a deflated restart is
// kept only when its likelihood is strictly better, and any candidate
// failure keeps the incumbent, mirroring the pinned exception branches). The
// pinned Point score/information are not produced by this unit. Invalid
// static inputs throw; unqualified controls fail explicitly.
SarimaEstimationResult estimate_sarima_fixed(
    const SarimaEstimationRequest& request, const Interrupt& interrupt = {});

} // namespace kalos
#endif
