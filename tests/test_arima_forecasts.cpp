// Item 21-H.4a: the exact-diffuse ARIMA point-forecast/backcast unit
// (faithful port of the JVM ExactArimaForecasts consumer stack that
// X13Kernel.initialStep drives for nonzero horizons). The reference values
// are JVM probes captured with jshell against the runtime jars
// (jdplus-toolkit-base-{api,core}-3.9.0) on 2026-11-20: the input series is
// the pinned r60 sequence (Box-Muller stream, LCG seed 12345, printed at
// %.17g), the expected vectors are the printed ExactArimaForecasts outputs
// for the pinned SarimaModel points. Coefficients follow the JVM polynomial
// convention (probes built SarimaModel.builder(orders).parameters(...) with
// the literal values below). Acceptance is the measured tier documented in
// docs/contracts/arima-forecasts-contract.md (1e-9 for the forecast and
// backcast windows; the mean is a cancellation-dominated diagnostic at 1e-8
// and is NOT consumed by the pipeline route).
#include "kalos/arima_forecasts.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const double R60[60] = {
    1.7970310575111597, 2.2828566229590233, 0.66884445217028280,
    1.5624197125527770, 2.3621633432237736, 2.2910087123028540,
    3.0585653464931504, 1.6063926106630442, 1.5981100087900932,
    1.3673936058663378, 4.5506742298420450, 0.65576836553042140,
    1.4448253977745860, 2.3662566328415355, 0.92082731076034880,
    2.7635318976588400, 4.5045045352682680, 2.3554495403326414,
    2.8124097645958940, 4.2387251927637860, 2.4493064608517560,
    4.4164305526362870, 2.3609489629326257, 3.2537382154054750,
    2.5554104186352515, 2.7714071034981322, 1.1540733177124258,
    4.2553832105746140, 3.1613015920499240, 3.3816013974648427,
    4.2525923883954850, 3.1996194365730535, 2.6040909319242687,
    1.9532718355079428, 2.4487059090128627, 3.2202614397699200,
    2.9582182159748770, 2.6383895666132640, 3.3330350689611006,
    3.6070617179056756, 4.5528918963236560, 2.1920504454383170,
    3.9380852165013334, 3.8897248816223238, 3.2437650555887965,
    3.6043115178950320, 4.8352760431003325, 4.0002266710452865,
    3.8652383230385094, 3.5295826213387045, 3.4933467670311225,
    4.0347971137210960, 3.2850679198710320, 4.8949442390375370,
    3.3658994982782200, 6.2651631559692510, 3.1971243720693963,
    3.2408767505209477, 3.8060567126520346, 4.9706615324152390
};

void check(const char* name, const std::vector<double>& got,
           const std::vector<double>& exp, double tol) {
    assert(got.size() == exp.size());
    for (std::size_t i = 0; i < exp.size(); ++i) {
        const double diff = std::abs(got[i] - exp[i]);
        const double rel =
            exp[i] != 0.0 ? diff / std::abs(exp[i]) : diff;
        if (!(rel <= tol)) {
            throw std::runtime_error(std::string("arima forecasts probe: ") +
                                     name);
        }
    }
}

kalos::ArimaForecastModel airline(bool mean_correction) {
    kalos::ArimaForecastModel model;
    model.period = 12;
    model.d = 1; model.q = 1; model.bd = 1; model.bq = 1;
    model.theta = {0.4}; model.btheta = {-0.6};
    model.mean_correction = mean_correction;
    return model;
}

} // namespace

int main() {
    const std::vector<double> r(R60, R60 + 60);

    // --- structural surface --------------------------------------------
    {
        const auto model = airline(false);
        const auto empty = kalos::arima_forecasts(model, {}, 0, 3);
        assert(empty.status == "failed:arima_forecasts" &&
               empty.forecasts.empty() && empty.backcasts.empty());
        auto broken = r;
        broken[7] = std::numeric_limits<double>::quiet_NaN();
        const auto nan = kalos::arima_forecasts(model, broken, 2, 3);
        assert(nan.status == "failed:arima_forecasts");
        const auto zero = kalos::arima_forecasts(model, r, 0, 0);
        assert(zero.status == "available" && zero.forecasts.empty() &&
               zero.backcasts.empty());
    }

    // --- MA(1) theta = -0.5, stationary, h = 4 (h > dim path; the JVM
    // steady-state position is inside the series) -------------------------
    {
        kalos::ArimaForecastModel model;
        model.period = 12; model.q = 1; model.theta = {-0.5};
        const auto got = kalos::arima_forecasts(model, r, 0, 4);
        assert(got.status == "available");
        check("ma1", got.forecasts, {-4.3561857319498640, 0.0, 0.0, 0.0},
              1e-13);
    }

    // --- airline (0,1,1)(0,1,1), theta 0.4, btheta -0.6 ------------------
    {
        const auto off = kalos::arima_forecasts(airline(false), r, 0, 6);
        assert(off.status == "available");
        check("airline_false", off.forecasts,
              {5.6387923556403035, 5.5946285711116520, 5.2123193914871070,
               6.2540193831933500, 6.2639654302593420, 6.1336576383105650},
              1e-9);
        const auto on =
            kalos::arima_forecasts(airline(true), r, 3, 6);
        check("airline_true_f", on.forecasts,
              {5.6886838703243400, 5.7177525691990830, 5.4086758729779160,
               6.5236083480874970, 6.6067868785569640, 6.5497115700114685},
              1e-9);
        check("airline_true_b", on.backcasts,
              {1.9249451901108627, 2.8901069966685813, 1.5846363509426040},
              1e-9);
        // The reported mean follows the LAST run (JVM field semantics):
        // the backcast run above overwrote the forecast one.
        check("airline_true_mean", {on.mean}, {0.03311587058285648}, 1e-8);
    }

    // --- (2,1,1)(0,1,1), phi (0.31, -0.12), theta 0.55, btheta -0.62 -----
    {
        kalos::ArimaForecastModel model;
        model.period = 12;
        model.p = 2; model.d = 1; model.q = 1; model.bd = 1; model.bq = 1;
        model.phi = {0.31, -0.12};
        model.theta = {0.55}; model.btheta = {-0.62};
        model.mean_correction = true;
        const auto f = kalos::arima_forecasts(model, r, 0, 6);
        check("m4_true_f", f.forecasts,
              {5.3208760590054110, 5.4316955383336630, 5.1130307951234630,
               6.2470246817433015, 6.3541619374075130, 6.2512157768457840},
              1e-9);
        check("m4_true_mean_f", {f.mean}, {0.035474881252776314}, 1e-8);
        const auto b = kalos::arima_forecasts(model, r, 3, 0);
        check("m4_true_b", b.backcasts,
              {1.9787335795439884, 2.9159576300622870, 1.7012706068063550},
              1e-9);
        check("m4_true_mean_b", {b.mean}, {0.03547488125313336}, 1e-8);
    }
    return 0;
}
