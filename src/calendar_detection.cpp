#include "kalos/calendar_detection.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "kalos/detail/special_functions.hpp"

namespace kalos {
namespace {

// JVM Double.compare: NaN is the largest value, -0.0 < 0.0.
int double_compare(double a, double b) {
    if (a < b) return -1;
    if (a > b) return 1;
    if (a == b) {
        if (a == 0.0) {
            const bool a_neg = std::signbit(a);
            const bool b_neg = std::signbit(b);
            if (a_neg == b_neg) return 0;
            return a_neg ? -1 : 1;
        }
        return 0;  // both NaN
    }
    return a != a ? (b != b ? 0 : 1) : -1;  // NaN sorts last
}

// TradingDaysRegressionComparator.comparator(f): the lambda is
// compare(a, b) = a absent ? 1 : b absent ? -1
//                : Double.compare(key(a), key(b))   (nulls sort last).
int comparator_compare(const CalendarModelRow& a, const CalendarModelRow& b,
                       bool selector_aicc) {
    if (!a.present) return 1;
    if (!b.present) return -1;
    return double_compare(selector_aicc ? a.aicc : a.bic,
                          selector_aicc ? b.aicc : b.bic);
}

// The one p-value of the Wald algebra: f_upper_p over ints widened exactly
// like the bytecode's i2d on (dnx, dof).
double wald_p(int dnx, int dof, double f) {
    return detail::f_upper_p(f, static_cast<double>(dnx),
                             static_cast<double>(dof));
}

}  // namespace

int calendar_best_model(const std::vector<CalendarModelRow>& rows,
                        bool selector_aicc) {
    // The scan loop loads ests[i] before its bounds test, so an all-absent
    // array throws ArrayIndexOutOfBounds in the jar (its -1 return is
    // unreachable). Mirror the throw; machines translate it to Failed.
    std::size_t best = 0;
    while (best < rows.size() && !rows[best].present) ++best;
    if (best == rows.size()) {
        throw std::out_of_range(
            "TradingDaysRegressionComparator.bestModel over an all-null "
            "ladder (jar surface: ArrayIndexOutOfBounds -> Failed)");
    }
    for (std::size_t j = best + 1; j < rows.size(); ++j) {
        if (!rows[j].present) continue;
        if (comparator_compare(rows[best], rows[j], selector_aicc) > 0) {
            best = j;
        }
    }
    return static_cast<int>(best);
}

CalendarWaldTestResult calendar_wald_test(
    const std::vector<CalendarModelRow>& rows, double pmodel,
    double pconstraint) {
    if (rows.empty() || !rows.front().present || !rows.back().present) {
        throw std::invalid_argument(
            "TradingDaysRegressionComparator.waldTest needs present first "
            "and last rows (jar surface: NullPointerException -> Failed)");
    }
    CalendarWaldTestResult out;
    const std::size_t n = rows.size();
    const int last = static_cast<int>(n) - 1;
    const CalendarModelRow& ref = rows.back();
    const int dof = ref.degrees_of_freedom - 2;
    const double s2 = ref.ssq / static_cast<double>(dof);
    std::vector<int> nx(n, 0);
    std::vector<double> ssq(n, 0.0);
    nx[0] = rows[0].nx;
    nx[static_cast<std::size_t>(last)] = ref.nx;
    ssq[0] = rows[0].ssq;
    ssq[static_cast<std::size_t>(last)] = ref.ssq;
    for (std::size_t i = 1; i < static_cast<std::size_t>(last); ++i) {
        if (rows[i].present) {
            nx[i] = rows[i].nx;
            ssq[i] = rows[i].ssq;
        }
    }
    std::vector<double>& p = out.p;
    std::vector<double>& f_echo = out.f;
    p.assign(n, 0.0);
    f_echo.assign(n, 0.0);
    // The p-loop covers i in [1, last] INCLUDING absent rows (their cached
    // nx = 0 / ssq = 0 feed the arithmetic, matching the jar's arrays).
    for (int i = 1; i <= last; ++i) {
        const int dnx = nx[static_cast<std::size_t>(i)] - nx[0];
        if (dnx == 0) {
            p[static_cast<std::size_t>(i)] = 1.0;
            continue;
        }
        const double f = (ssq[0] - ssq[static_cast<std::size_t>(i)]) /
                         (static_cast<double>(dnx) * s2);
        f_echo[static_cast<std::size_t>(i)] = f;
        // ifle arm with dcmpl: f <= 0 (NaN excluded, NaN enters).
        if (!(f <= 0.0)) {
            p[static_cast<std::size_t>(i)] = wald_p(dnx, dof, f);
        } else {
            p[static_cast<std::size_t>(i)] = 1.0;
        }
    }
    int best = last;
    // Descend: while (best > 0 && compare(dcmpl, p[best], pmodel) > 0).
    // dcmpl pushes 1 for NaN, so a NaN p[best] KEEPS descending; the plain
    // C++ `>` would stop. !(p <= pmodel) is the exact mirror.
    while (best > 0 && !(p[static_cast<std::size_t>(best)] <= pmodel)) --best;
    if (best > 1) {
        for (int j = best - 1; j > 0; --j) {
            const std::size_t ju = static_cast<std::size_t>(j);
            const std::size_t bu = static_cast<std::size_t>(best);
            if (p[ju] >= pmodel) continue;  // dcmpg: NaN enters the arm
            const int dnx = nx[bu] - nx[ju];
            const double f = (ssq[ju] - ssq[bu]) /
                             (static_cast<double>(dnx) * s2);
            f_echo[ju] = f;
            if (f <= 0.0) continue;  // dcmpl: NaN enters the arm
            const double q = wald_p(dnx, dof, f);
            if (!(q <= pconstraint)) best = j;  // dcmpl: NaN swaps
        }
    }
    out.best = best;
    return out;
}

CalendarFtestInfo calendar_ftest_info(const CalendarModelRow& model0,
                                      const CalendarModelRow& model_wd,
                                      const CalendarModelRow& model_td,
                                      int desc_free_parameters) {
    CalendarFtestInfo info;
    info.ss0 = model0.ssq;
    info.ss1 = model_wd.ssq;
    info.ss6 = model_td.ssq;
    const double df1 = static_cast<double>(model_wd.degrees_of_freedom -
                                           desc_free_parameters);
    const double df6 = static_cast<double>(model_td.degrees_of_freedom -
                                           desc_free_parameters);
    info.ssmc1 = info.ss1 / df1;
    info.ssmc6 = info.ss6 / df6;
    const double f1 = (info.ss0 - info.ss1) / info.ssmc1;
    // dcmpg guard against 0: !(f > 0) -> 0.0 (NaN lands on 0.0).
    info.pftd1 = (f1 > 0.0) ? 1.0 - detail::f_upper_p(f1, 1.0, df1) : 0.0;
    const double f6 = (info.ss0 - info.ss6) / (info.ssmc6 * 6.0);
    info.pftd6 = (f6 > 0.0) ? 1.0 - detail::f_upper_p(f6, 6.0, df6) : 0.0;
    return info;
}

}  // namespace kalos
