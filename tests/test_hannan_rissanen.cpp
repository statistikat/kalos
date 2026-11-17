// Item 21-A.2: unit qualification of the transcribed HannanRissanen pipeline.
// The pinned oracle is deliberately not used here: the corpus (see
// validation/reference/capture-estimation-control-hr.R) carries the fitted
// pipeline, while the branches below that the corpus cannot reach
// deterministically at the pinned level — the zero-variance guard on an
// unbounded likelihood, the useDefault fill, stabilization of an explosive
// root, and input validation — are qualified against the pinned semantics
// directly. Random inputs come from a fixed-seed generator, so every case is
// deterministic; tolerances bound estimation error, not solver drift.
#include <kalos/hannan_rissanen.hpp>

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Deterministic PRNG (no library-dependent distributions): a 48-bit LCG
// mapped to uniforms, and uniforms summed into an approximate standard
// normal.
struct Lcg {
    unsigned long long state = 88172645463325252ULL;
    double uniform() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>((state >> 11) & 0xFFFFFFFFFFFFULL) /
               static_cast<double>(0x1000000000000ULL);
    }
    double normal() {
        double acc = 0.0;
        for (int i = 0; i < 12; ++i) acc += uniform();
        return acc - 6.0;
    }
};

kalos::HannanRissanenOrders orders(int p, int bp, int q, int bq, int period = 12) {
    return kalos::HannanRissanenOrders{p, bp, q, bq, period};
}

kalos::RegressionDesign empty_design() { return kalos::RegressionDesign{}; }

template <class F> void rejects(F f) {
    bool threw = false;
    try { f(); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
}

} // namespace

int main() {
    using kalos::hannan_rissanen_start;
    using Source = kalos::HannanRissanenStart::Source;

    // AR(1) recovery on a stationary series: the pipeline's AR section with
    // no MA orders is the pinned minspq least squares on the response lags,
    // negated into the Kalos convention.
    {
        Lcg rng;
        const int n = 800;
        std::vector<double> y(static_cast<std::size_t>(n));
        for (int t = 1; t < n; ++t)
            y[static_cast<std::size_t>(t)] = 0.7 * y[static_cast<std::size_t>(t - 1)] + rng.normal();
        const auto start = hannan_rissanen_start(y, empty_design(), orders(1, 0, 0, 0));
        assert(start.source == Source::Fitted);
        assert(!start.stabilized);
        assert(std::abs(start.coefficients[0][0] - 0.7) < 0.08);
        assert(start.coefficients[1].empty() && start.coefficients[2].empty() &&
               start.coefficients[3].empty());
    }

    // MA(1) recovery through the Levinson prefilter and bias correction; the
    // MA side keeps the pinned sign, so theta is directly comparable.
    {
        Lcg rng;
        const int n = 800;
        std::vector<double> e(static_cast<std::size_t>(n)), y(static_cast<std::size_t>(n));
        for (int t = 0; t < n; ++t) {
            e[static_cast<std::size_t>(t)] = rng.normal();
            y[static_cast<std::size_t>(t)] = e[static_cast<std::size_t>(t)] +
                (t > 0 ? -0.5 * e[static_cast<std::size_t>(t - 1)] : 0.0);
        }
        const auto start = hannan_rissanen_start(y, empty_design(), orders(0, 0, 1, 0));
        assert(start.source == Source::Fitted);
        assert(!start.stabilized);
        assert(std::abs(start.coefficients[2][0] + 0.5) < 0.15);
    }

    // Pinned stabilization: the least-squares root of an explosive AR(1) is
    // inverted through the unit circle (1/1.6), landing on 0.625.
    {
        Lcg rng;
        const int n = 600;
        std::vector<double> y(static_cast<std::size_t>(n));
        for (int t = 1; t < n; ++t)
            y[static_cast<std::size_t>(t)] = 1.6 * y[static_cast<std::size_t>(t - 1)] + rng.normal();
        const auto start = hannan_rissanen_start(y, empty_design(), orders(1, 0, 0, 0));
        assert(start.source == Source::Fitted);
        assert(start.stabilized);
        assert(std::abs(start.coefficients[0][0] - 0.625) < 0.08);
    }

    // Pinned rms < 1e-9 variance guard on the design residuals: the start is
    // zero on every block. This is the branch the pinned corpus cannot
    // witness (an exactly fitted design makes the likelihood unbounded and
    // the pinned optimizer's landing process-dependent), so it is qualified
    // here instead.
    {
        const int n = 60;
        std::vector<double> y(static_cast<std::size_t>(n), 0.25);
        kalos::RegressionDesign design;
        design.columns = 2;
        design.values.assign(2 * static_cast<std::size_t>(n), 0.0);
        for (std::size_t t = 0; t < static_cast<std::size_t>(n); ++t) {
            design.values[t] = 1.0;                                   // level
            design.values[static_cast<std::size_t>(n) + t] = 0.25;    // exact fit
        }
        design.coefficients.assign(2, 0.0);
        design.fixed.assign(2, 0);
        const auto start = hannan_rissanen_start(y, design, orders(1, 1, 1, 1));
        assert(start.source == Source::ZeroVariance);
        assert(!start.stabilized);
        for (const auto& block : start.coefficients)
            for (const double value : block) assert(value == 0.0);
        assert(start.coefficients[0].size() == 1 && start.coefficients[1].size() == 1 &&
               start.coefficients[2].size() == 1 && start.coefficients[3].size() == 1);
    }

    // Pinned useDefaultIfFailed fill: the response is shorter than the
    // pipeline warmup (largest design lag p + period*bp = 26), so the
    // regression throws inside the pinned catch-all and the builder default
    // fill comes back.
    {
        Lcg rng;
        std::vector<double> y(8);
        for (auto& value : y) value = rng.normal();
        const auto start = hannan_rissanen_start(y, empty_design(), orders(2, 2, 1, 1));
        assert(start.source == Source::Defaulted);
        assert(!start.stabilized);
        assert(start.coefficients[0].size() == 2 && start.coefficients[1].size() == 2 &&
               start.coefficients[2].size() == 1 && start.coefficients[3].size() == 1);
        for (const double value : start.coefficients[0]) assert(value == 0.1);
        for (const double value : start.coefficients[1]) assert(value == 0.1);
        for (const double value : start.coefficients[2]) assert(value == -0.2);
        for (const double value : start.coefficients[3]) assert(value == -0.2);
    }

    // The pinned initializer runs on OLS residuals of the differenced linear
    // model: with a strong regressor the AR recovery sees only the noise.
    {
        Lcg rng;
        const int n = 800;
        std::vector<double> y(static_cast<std::size_t>(n));
        std::vector<double> ar(static_cast<std::size_t>(n));
        for (int t = 1; t < n; ++t)
            ar[static_cast<std::size_t>(t)] = 0.6 * ar[static_cast<std::size_t>(t - 1)] + rng.normal();
        kalos::RegressionDesign design;
        design.columns = 1;
        design.values.resize(static_cast<std::size_t>(n));
        for (std::size_t t = 0; t < static_cast<std::size_t>(n); ++t) {
            const double x = static_cast<double>(t % 7) - 3.0;
            design.values[t] = x;
            y[t] = 5.0 * x + ar[t];
        }
        design.coefficients.assign(1, 0.0);
        design.fixed.assign(1, 0);
        const auto start = hannan_rissanen_start(y, design, orders(1, 0, 0, 0));
        assert(start.source == Source::Fitted);
        assert(std::abs(start.coefficients[0][0] - 0.6) < 0.1);
    }

    // Interrupt exceptions propagate instead of landing in the default fill.
    {
        struct Interrupted : std::runtime_error {
            Interrupted() : std::runtime_error("interrupted") {}
        };
        Lcg rng;
        std::vector<double> y(240);
        for (auto& value : y) value = rng.normal();
        bool threw = false;
        try {
            hannan_rissanen_start(y, empty_design(), orders(1, 0, 1, 0),
                                  [] { throw Interrupted(); });
        } catch (const Interrupted&) {
            threw = true;
        }
        assert(threw);
    }

    // Static input contract.
    rejects([] { hannan_rissanen_start({1}, empty_design(), orders(-1, 0, 0, 0)); });
    rejects([] { hannan_rissanen_start({1}, empty_design(), orders(0, 0, 0, 0, 0)); });
    rejects([] { hannan_rissanen_start({}, empty_design(), orders(1, 0, 0, 0)); });
    rejects([] {
        hannan_rissanen_start({1, std::numeric_limits<double>::quiet_NaN()}, empty_design(),
                              orders(1, 0, 0, 0));
    });
    rejects([] {
        kalos::RegressionDesign design;
        design.columns = 1; // values empty: shape mismatch
        hannan_rissanen_start({1, 2, 3}, design, orders(1, 0, 0, 0));
    });

    // All-zero orders produce a zero start without touching the pipeline.
    {
        const auto start = hannan_rissanen_start({1, 2, 1, 3}, empty_design(), orders(0, 0, 0, 0));
        assert(start.source == Source::Fitted);
        assert(!start.stabilized);
        for (const auto& block : start.coefficients) assert(block.empty());
    }

    std::puts("test_hannan_rissanen: OK");
}
