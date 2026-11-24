// Item 23-E replay test: drives the transcribed tramo regression-test layer
// (kalos::tramo_model_statistics, kalos::tramo_test_tstats / tramo_t_accepts,
// kalos::run_tramo_regression_tests, kalos::tramo_ref_stats_gate and the
// AmiOptions helpers) over the generated golden JVM corpus
// (tests/jvm_tramo_regression_tests.h, corpus
// validation/evidence/native/tramo-regression-tests, double capture
// byte-identical). Every replay case rebuilds the request from the captured
// hand-over echoes and compares each consumed ModelStatistics row, both
// cl.tstats arms, every mean-test run decision, the full pass2 ladder
// after-state and the checkMu witnesses bit-for-bit (tests/
// tramo_regression_replay.h). Seam rows pin the fail-closed refusals: the
// quarterly MS row (D11) and the
// Default calendar-field rows (item 21-I.3). The synthetic block qualifies
// the pieces the corpus cannot reach (retry-ladder shapes, round-0 rule,
// reduceVa/lastSolution arms, the NaN T-bucket semantics, envelope
// refusals); see docs/contracts/tramo-regression-tests-contract.md.
#include "tramo_regression_replay.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace {
double read_bits(std::istream& in) {
    std::string bits;
    in >> bits;
    return kalos::replay::as_double(std::stoull(bits, nullptr, 16));
}

void check_fast_calendar(kalos::replay::Reporter& rep) {
    std::ifstream panel(KALOS_FAST_CALENDAR_PANEL);
    kalos::TramoRegressionTestRequest req;
    int fits = 0, decisions = 0;
    for (std::string line; std::getline(panel, line);) {
        std::istringstream row(line);
        std::string kind;
        row >> kind >> std::boolalpha;
        if (kind == "fit") {
            req = {};
            row >> req.mean >> req.trading_days_position >> req.trading_days_dimension
                >> req.length_of_period_position >> req.easter_position >> req.statistics.free_parameters
                >> req.statistics.dim;
            req.statistics.ssq = read_bits(row);
            req.auto_trading_days = req.trading_days_position >= 0;
            req.auto_length_of_period = req.length_of_period_position >= 0;
            req.auto_easter = req.easter_position >= 0;
            ++fits;
        } else if (kind == "coef" || kind == "diag") {
            auto& values = kind == "coef" ? req.statistics.coefficients : req.statistics.cov_diagonal;
            for (std::string word; row >> word;)
                values.push_back(kalos::replay::as_double(std::stoull(word, nullptr, 16)));
        } else if (kind == "result") {
            req.variable_threshold = read_bits(row);
            std::string status;
            bool mean, td, lp, easter, cleared;
            row >> status >> mean >> td >> lp >> easter >> cleared;
            const auto result = kalos::run_tramo_regression_tests(req);
            if (!row || status != (result.result == kalos::TramoRegressionTestResult::changed ? "Changed" : "Unchanged")
                || mean != result.mean || td != (req.auto_trading_days && !result.removed_trading_days)
                || lp != (req.auto_length_of_period && !result.removed_length_of_period)
                || easter != (req.auto_easter && !result.removed_easter) || cleared != result.estimation_cleared)
                rep.fail("fast calendar decision " + std::to_string(decisions));
            ++rep.checks;
            ++decisions;
        } else rep.fail("unexpected fast calendar record " + kind);
    }
    kalos::replay::check_int(rep, "fast calendar", "fitted models", fits, 12);
    kalos::replay::check_int(rep, "fast calendar", "calendar decisions", decisions, 130);
}
} // namespace

int main() {
    kalos::replay::Reporter rep;
    kalos::replay::replay_all(rep);
    check_fast_calendar(rep);
    if (rep.failures != 0) {
        std::printf("test_tramo_regression_tests: %d FAILURES of %d checks\n",
                    rep.failures, rep.checks);
        return 1;
    }
    std::printf("test_tramo_regression_tests: %d checks verified\n",
                rep.checks);
    return 0;
}
