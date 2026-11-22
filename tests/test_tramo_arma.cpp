// Item 23-D replay test: drives kalos::select_tramo_arma (the live
// tramo.internal.ArmaModule arma selection) over the generated golden JVM
// corpus (tests/jvm_tramo_arma.h). Every replay case rebuilds the request
// from the captured protocol header and the olsResiduals bit pattern, runs
// the transcribed three-stage selector and compares the module-level fact
// lines - direct_sel (the selection), the five dpanel rows of the merged
// hrModels panel (orders plus the fast BIC), arm_result and the description
// update (after) - and the full ProcessingLog event sequence, rebuilt from
// the unit's events/panel/selection with the observer's depth and CInfo
// detail rendering (tests/tramo_arma_replay.h), against the captured probe
// lines, %.17g for bit pattern for bit pattern. The quarterly case asserts
// the D11 refusal and the constant-series case pins the upstream
// olsResiduals failure (no residual hand-over exists, record only); the
// pcnt/maxspec gate lines and the resvals hand-over ride along inert
// (docs/contracts/tramo-arma-selection-contract.md).
#include "kalos/tramo_arma_selection.hpp"

#include "jvm_tramo_arma.h"
#include "tramo_arma_replay.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(cond)                                                                            \
    if (!(cond)) {                                                                             \
        throw std::runtime_error("check failed: " #cond " (line " + std::to_string(__LINE__)   \
            + ")");                                                                            \
    }

#define CHECK_THROWS(expr)                                                                     \
    do {                                                                                       \
        bool threw = false;                                                                    \
        try {                                                                                  \
            (void)(expr);                                                                      \
        } catch (const std::invalid_argument&) {                                               \
            threw = true;                                                                      \
        }                                                                                      \
        CHECK(threw);                                                                          \
    } while (false)

namespace {

using kalos::replay::line_equal;
using kalos::replay::produced;
using kalos::replay::to_values;
using kalos::TramoArmaSelectionRequest;

int replay_case(const kalos::test::TArmaCase& c) {
    TramoArmaSelectionRequest r;
    r.residuals = to_values(c.res, c.nres);
    r.nobs = c.nobs;
    r.period = c.period;
    r.p0 = c.p0;
    r.d0 = c.d0;
    r.q0 = c.q0;
    r.bp0 = c.bp0;
    r.bd0 = c.bd0;
    r.bq0 = c.bq0;
    r.seasonal = c.seasonal != 0;
    r.accept_white_noise = c.wn != 0;
    const auto d = kalos::select_tramo_arma(r);
    const auto got = produced(c.id, d);
    if (got.size() != c.nwant) {
        std::string dump = std::string("case ") + c.id + " line count "
            + std::to_string(got.size()) + " want " + std::to_string(c.nwant);
        for (std::size_t i = 0; i < got.size(); ++i) {
            dump += "\n  got:  " + got[i];
        }
        for (std::size_t i = 0; i < c.nwant; ++i) {
            dump += std::string("\n  want: ") + c.want[i];
        }
        throw std::runtime_error(dump);
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (!line_equal(c.want[i], got[i])) {
            throw std::runtime_error(std::string("case ") + c.id + " line "
                + std::to_string(i) + "\n  want: " + c.want[i] + "\n  got:  " + got[i]);
        }
    }
    return 1;
}

} // namespace

int main() {
    // Envelope refusals (D11, register): empty and non-finite residual
    // blocks never reach the machine.
    TramoArmaSelectionRequest empty;
    empty.nobs = 96;
    CHECK_THROWS(kalos::select_tramo_arma(empty));
    TramoArmaSelectionRequest nan_request;
    nan_request.nobs = 96;
    nan_request.residuals = {1.0, std::numeric_limits<double>::quiet_NaN()};
    CHECK_THROWS(kalos::select_tramo_arma(nan_request));

    int replayed = 0;
    for (const auto& c : kalos::test::kArmaCases) {
        if (c.replay) {
            replayed += replay_case(c);
        } else if (c.period != 12) {
            // D11 refusal: the unit never runs the machine on a non-monthly
            // request.
            TramoArmaSelectionRequest r;
            r.residuals = to_values(c.res, c.nres);
            r.nobs = c.nobs;
            r.period = c.period;
            bool refused = false;
            try {
                kalos::select_tramo_arma(r);
            } catch (const std::invalid_argument&) {
                refused = true;
            }
            CHECK(refused);
            ++replayed;
        } else {
            // Record only: the pinned JVM failed upstream of the unit
            // (olsResiduals threw); the residual hand-over does not exist.
            CHECK(std::strlen(c.res_error) > 0);
        }
    }
    CHECK(replayed == 15);
    std::printf("test_tramo_arma: %d cases verified\n", replayed);
    return 0;
}
