#ifndef KALOS_TRAMO_SEATS_MODEL_VALIDATOR_HPP
#define KALOS_TRAMO_SEATS_MODEL_VALIDATOR_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace kalos {

// Item 24-A: the SEATS model-validation and canonical-decomposition DECISION
// mirror. This unit reproduces, operation for operation, what the LIVE pinned
// jdplus 3.9.0 path decides between the SeatsKernel.validate seam and the
// SeatsKernel.decomposeModel seam:
//
//   * DefaultModelValidator - the XL parameter-cutoff machine (simplifyModel
//     trailing trim with cutOff, changeMA/stabilizeMA, changeAR, the
//     fixMaUnitRoots projection), bit-exact;
//   * the toolkit selector chain (TrendCycleSelector + SeasonalSelector as
//     SeatsToolkit.of wires them from the DecompositionSpec knobs) and the
//     RootDecomposer linear-system structure (the FastMatrix assembly and the
//     LULinearSystemSolver2 transcription give bit-exact symmetric filters);
//   * the decomposability decision of UcarimaModel.setVarianceMax(-1, noisy):
//     the Spectrum$Minimizer minimum-sum test (minimizeByRoots with the
//     FastEigenValuesSolver closed forms / minimizeByGrid with the
//     denominator-bracketed Newton iteration) whose sign decides whether the
//     toolkit decomposer yields a UcarimaModel or null, and the SeatsKernel
//     decomposeModel state machine flags/events that follow ("arima parameters
//     cut off", "non decomposable model", "canonical decomposition failed",
//     "noisy model used", modelChanged/parametersCutOff).
//
// It does NOT implement what happens AFTER a null decomposition: the
// DefaultModelApproximator approximation modes are decision register D6 and
// stay unimplemented here - the unit reports approximation_required and stops
// (the live record pins what the kernel then did). The spectral
// factorization of the split components (SymmetricFilter.factorize -> the
// one-sided MA / variance facts), the variance allocation itself and the
// UcarimaModel post-allocation values are item 24-B scope; the unit exposes
// the raw (pre-allocation) component structure only.
//
// Envelope (register D11, monthly-first, faithful-to-capture): the unit
// boundary accepts the fixed-parameter (all-Fixed) SarimaSpec shape the probe
// captured, monthly (period 12) and quarterly (period 4) series; the unit
// REFUSES (std::invalid_argument, never silently reinterpreting):
//   - bphi != empty (stationary seasonal AR: its root set needs the
//     degree > 2 FastEigenValuesSolver.qr cascade, not ported);
//   - stationary AR degree > 2 (same closed-form coverage);
//   - any MA/AR input coefficient non-finite;
//   - requests whose Minimizer root search would need w.roots() of degree
//     > 2 (measured: every corpus case lands on wdeg <= 2);
//   - frequency outside {4, 12} (the captured corpus shapes).
// JVM-observed crashes on the live path (the fixMa deg-1 quirk feeding a
// MatrixException into the Legacy approximator, the Noisy-mode NPE) are
// outside this unit and recorded as data in the corpus, never reproduced.

// Pinned DecompositionSpec.ModelApproximationMode. The live builder default
// (and the corpus default) is Legacy; None nulls the toolkit approximator,
// Noisy switches setVarianceMax to the noisy-model branch.
enum class seats_approx_mode { none, legacy, noisy };

// The frozen fixed-parameter SARIMA request (the probe's `spec`/`phi`/...
// lines). Orders ARE the parameter vector lengths (the SarimaSpec builder
// contract). innovation_variance is the SeatsModelSpec.innovationVariance
// echo the decomposition scales the symmetric MA by (convolutionOf(ma, var)).
struct tramo_seats_model_request {
    int frequency = 12;   // series annual frequency; period is the declared
    // SarimaSpec period; the corpus always pairs them (12/12, 4/4).
    int period = 12;
    int d = 0;
    int bd = 0;
    std::vector<double> phi, bphi, theta, btheta;
    // (orders p/bp/q/bq are the vector sizes; p is the phi length.)
    bool log = false;            // echoed in the result; no model-path effect
    bool mean_correction = true;  // echoed (SeatsModel.of carries it)
    double innovation_variance = 0.0;
    // DecompositionSpec knobs (builder defaults: xl 0.95, epsphi 2, rmod 0.5,
    // smod 0.8, smodAtPi 0.8, mode Legacy).
    double xl = 0.95;
    double epsphi = 2.0;
    double rmod = 0.5;
    double smod = 0.8;
    double smod_pi = 0.8;
    seats_approx_mode mode = seats_approx_mode::legacy;
};

// One raw (pre-variance-allocation) RootDecomposer split component, as the
// probe read it through the getters: stationary AR, unit roots and the
// symmetric difference filter (the linear-system solution). The one-sided
// MA and innovation variance of a symmetric model need the 24-B spectral
// factorization and are NOT part of this mirror.
struct seats_raw_component {
    std::vector<double> sar;  // getStationaryAr().asPolynomial()
    std::vector<double> ur;   // getNonStationaryAr().asPolynomial()
    std::vector<double> sfd;  // symmetricMa().coefficientsAsPolynomial()
};

// The unit's decision-and-event record for one request.
struct tramo_seats_model_result {
    // ---- DefaultModelValidator output (the v.* fact block) --------------
    bool parameters_cut_off = false;   // validate() returned false
    double xl = 0.95;                  // validator's xl (getXl())
    // The validated model (m1): orders = parameter lengths + d/bd.
    int orders_p = 0, orders_d = 0, orders_q = 0;
    int orders_bp = 0, orders_bd = 0, orders_bq = 0;
    std::vector<double> phi, bphi, theta, btheta;
    double innovation_variance = 0.0;  // SeatsModel echo (unchanged)
    int period = 12;

    // ---- decomposition decision layer ------------------------------------
    // The raw selector-chain split of the VALIDATED model (r.* facts;
    // r.count 3, structural lines only).
    std::vector<seats_raw_component> raw_components;
    // true when the chain threw (LU MatrixException / singular system):
    // r.decompose "null" + the DefaultModelDecomposer catch -> um null.
    bool decomposition_threw = false;
    // um != null after setVarianceMax(-1, mode == Noisy): the toolkit
    // decomposer's answer (d.decompose "ok"/"null").
    bool variance_max_ok = false;
    // sum of the raw component spectral minima (setVarianceMax's floor);
    // exposed for the sweep report, decisions use its sign only.
    double variance_floor = 0.0;
    // The decomposeModel state machine outcome:
    bool approximation_required = false;  // um null and mode != None
    bool seats_exception = false;         // um null and mode == None
    bool noisy_model_used = false;        // Noisy branch changed the model
    bool model_changed = false;           // SeatsModel.setModelChanged
                                          // (approximation or noisy)
    // The kernel validate+decompose frame as the probe's observer recorded
    // it (push/pop nesting with the remark/warning/error messages the unit
    // itself emits; the estimation-frame events are 23-F/G scope).
    struct event {
        int seq;
        std::string kind;  // push, pop, warning, remark, error
        int depth;
        std::string msg;
        std::string detail;  // "-" or "orders ..." (the approximation remark)
    };
    std::vector<event> events;
};

// Runs the mirror. Throws std::invalid_argument outside the envelope above.
tramo_seats_model_result validate_tramo_seats_model(
    const tramo_seats_model_request& request);

}  // namespace kalos

#endif
