#include "kalos/linear_algebra.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

template <class F> void rejects(F f) {
    bool threw = false;
    try { f(); } catch (const std::exception&) { threw = true; }
    assert(threw);
}

int main() {
    using namespace kalos;
    rejects([] { DenseMatrix x(0, 1); });
    rejects([] { DenseMatrix x(std::numeric_limits<std::size_t>::max(), 2); });
    rejects([] { DenseMatrix x(65536, 65536); });
    rejects([] { DenseMatrix x(2, 2, {1, 2}); });
    rejects([] { DenseMatrix x(1, 1, {std::numeric_limits<double>::infinity()}); });
    DenseMatrix a(2, 2, {4, 1, 1, 3});
    DenseMatrix b(2, 1, {1, 2});
    const auto f = cholesky_factor(a);
    const auto x = cholesky_solve(f, b);
    assert(std::abs(x(0, 0) - 1.0 / 11) < 1e-14);
    assert(std::abs(x(1, 0) - 7.0 / 11) < 1e-14);
    rejects([&] { a(2, 0) = 1; });
    rejects([&] { a(0, 2) = 1; });
    a.values().pop_back();
    rejects([&] { cholesky_factor(a); });
    auto broken = f;
    broken.upper(1, 0) = 1;
    rejects([&] { cholesky_solve(broken, b); });
    broken = f;
    broken.upper(0, 0) = 0;
    rejects([&] { cholesky_solve(broken, b); });
    DenseMatrix wide(2, 3, {1, 0, 0, 1, 1, 1});
    const auto ls = qr_least_squares(wide, DenseMatrix(2, 1, {1, 1}));
    assert(ls.rank == 2);
    assert(std::abs(ls.coefficients(0, 0) - 1.0 / 3) < 1e-14);
    assert(std::abs(ls.coefficients(2, 0) - 2.0 / 3) < 1e-14);
    int callbacks = 0;
    qr_least_squares(wide, DenseMatrix(2, 1, {1, 1}), 1e-10, [&] { ++callbacks; });
    assert(callbacks > 0);
    rejects([&] { qr_least_squares(wide, b, 1e-10, [] { throw std::runtime_error("interrupted"); }); });
    assert(qr_least_squares(wide, b).rank == 2);
    assert(polynomial_roots({}).empty());
    assert(polynomial_roots({2.5}).empty());
    assert(polynomial_roots({1, -2, 0, 0}).size() == 1); // trailing zeros trimmed
    assert(std::abs(polynomial_roots({1, -2})[0].real - 0.5) < 1e-14);
    assert(polynomial_roots({1, -2})[0].imag == 0.0);
    assert(polynomial_roots({0, 2})[0].real == 0.0);
    assert(polynomial_roots({0, 0}).empty());
    const auto quad = polynomial_roots({1, -5, 6}); // (1 - 2x)(1 - 3x)
    assert(quad.size() == 2);
    const auto root_near = [&](double value) {
        for (const auto& root : quad) {
            if (std::abs(root.imag) < 1e-12 && std::abs(root.real - value) < 1e-9) return true;
        }
        return false;
    };
    assert(root_near(0.5) && root_near(1.0 / 3.0));
    const auto pair = polynomial_roots({1, 1, 1}); // complex pair exp(+-2 pi i / 3)
    assert(pair.size() == 2);
    for (const auto& root : pair) {
        assert(std::abs(std::hypot(root.real, root.imag) - 1.0) < 1e-9);
        assert(std::abs(root.real + 0.5) < 1e-9);
    }
    assert(std::abs(pair[0].imag + pair[1].imag) < 1e-9);
    const auto zero_lead = polynomial_roots({1, -1.5, 0.5, 0}); // (1 - x)(1 - x/2)
    assert(zero_lead.size() == 2);
    rejects([] { polynomial_roots({1, std::numeric_limits<double>::quiet_NaN()}); });
    rejects([] { polynomial_roots({1, std::numeric_limits<double>::infinity()}); });
}
