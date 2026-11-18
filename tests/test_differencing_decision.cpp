// Item 21-D: unit qualification of the transcribed DifferencingModule.
// The corpus sweep (tools/verify_differencing_decision.cpp against
// validation/evidence/native/differencing-decision) carries the JVM-qualified
// decisions; this unit covers what that corpus cannot reach at the pinned
// level: request validation, the qualified period domain, interrupt
// propagation, the stMean algebra with its length-dependent thresholds, the
// finalcond clamps, and two JVM-pinned replays (m_n80_rw, m_trend_seasonal)
// that guard the whole machine against regressions. Decision triples, the
// mlused flag and the nextstep-call count are pinned exact; tmean is banded
// because it lands through the Kalos estimator seam (21-B/21-C precedent).
#include <kalos/differencing_decision.hpp>

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

// The pinned decoder call-site constants (see the contract).
kalos::DifferencingDecisionRequest pinned_request(std::vector<double> series, int period = 12) {
    kalos::DifferencingDecisionRequest request;
    request.series = std::move(series);
    request.period = period;
    request.max_d = 2;
    request.max_seasonal_d = 1;
    request.ub1 = 1.0 / 1.0309278350515465;
    request.ub2 = 1.0 / 1.1363636363636365;
    request.cancel = 0.1;
    request.precision = 1e-7;
    return request;
}

template <class F> void rejects(F f) {
    bool threw = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// x = (a, 1, ..., 1) of length n whose stMean statistic s/sqrt(q) equals t:
// t^2 (a^2 + n - 1) = (a + n - 1)^2, positive-branch root.
double solve_shift(double t, int n) {
    const double m = static_cast<double>(n - 1);
    const double A = t * t - 1.0;
    const double B = -2.0 * m;
    const double C = t * t * m - m * m;
    return (-B + std::sqrt(B * B - 4.0 * A * C)) / (2.0 * A);
}

struct Boom : std::runtime_error {
    Boom() : std::runtime_error("boom") {}
};

static const double k_m_n80_rw[] = {
    1.3047856990107147,    -0.42947417397647425,    -1.9068252995817938,    -1.7226726301959303,
    0.27481540148048811,    -0.32567920530187111,    -0.26558159460693026,    0.012903171040298256,
    1.8549633699099815,    0.55561159851907527,    1.7482196519781898,    0.59500318358132165,
    2.9735795802916316,    2.7293925449399752,    3.9467442127393251,    4.1766553342883128,
    4.6200502739025788,    5.9695856328647539,    7.1611209147253749,    7.6752506592228924,
    6.0793297099583228,    6.780577039295812,    6.5903209836908108,    6.2040043600442702,
    6.3687039358421238,    6.7981942032178688,    5.2555780990428032,    3.9736627891188134,
    4.9969350863097155,    4.0640433494241899,    4.70674238035153,    6.1728793385623373,
    7.8488634423737205,    8.2504068018204499,    6.4982113646739714,    5.5903055309986671,
    5.7668635447838055,    5.2269284228596007,    5.4946786230917972,    6.2918642287394464,
    6.8774248778811433,    7.1230162311894274,    8.0771863884957664,    8.9305301022679995,
    9.3747712991618393,    9.0453303021752625,    8.6434677992189748,    6.715545914587385,
    6.0598323583283813,    5.2057473298933719,    5.795718473200516,    6.6098049408527553,
    7.1146238600799805,    7.7252286778353731,    6.8141444514738962,    6.3652058117295587,
    5.2084230518949957,    6.0179235759336835,    6.703401891066334,    7.3231729407689716,
    6.1945801165093997,    5.2170042953689242,    5.9517399980305576,    6.0113065708386229,
    5.8800745800427805,    5.5525668845358789,    4.2116372203415651,    4.3547941635507339,
    5.615294466362692,    4.5855235278404276,    5.7890951140232012,    5.6807198128848899,
    6.5378883033431094,    7.3196494029178574,    7.1475587151375741,    8.2759959559687566,
    7.3332954530485077,    7.9944018127241527,    8.5620594353432455,    8.3291853351224354,
};
static const double k_m_trend_seasonal[] = {
    1.8131921413730057,    3.5048095154226448,    3.1360662894399876,    3.4121130252257927,
    4.396500589924548,    3.3882662067633484,    2.3328811664827249,    2.0803235424164632,
    3.2135402472089289,    3.8130981541047992,    4.7674105723862326,    6.3967326822854877,
    6.2669655980643268,    8.5800769256038514,    12.151451243978121,    10.844986719644098,
    9.7410614964469353,    7.7822537717094997,    8.938864509504727,    8.8261509420165822,
    8.025471839120609,    9.0783375879792541,    8.8310601144012999,    12.707132760916673,
    12.652110441596221,    14.289955151117358,    17.142953806727846,    14.604602353590057,
    15.600706660393783,    15.690430943668986,    13.069834676326016,    15.057145760281326,
    14.042655148217102,    14.691264656418621,    16.864737545558782,    17.650091680588964,
    20.083827454607182,    21.039898046958243,    22.002025745306696,    18.798678733273654,
    21.692984832810026,    21.949101549562531,    20.800526398304751,    21.03441541356931,
    21.162067681336783,    21.714523978612903,    20.61071275786119,    24.053460296478317,
    26.509425341483194,    26.202729250292318,    27.968552931008482,    26.481688702817692,
    26.988658634355573,    26.170667183166387,    25.708895162451181,    27.63765132586958,
    25.862363486260794,    27.693173294570947,    27.984652425029797,    30.450814984333192,
    30.723882509459266,    32.645595369051343,    33.054925672159754,    32.79611799813847,
    32.378837521881238,    34.83312683596013,    32.415396347470114,    31.536761454107197,
    32.947026258000633,    34.097489187188089,    33.141597568952427,    35.696676736298642,
    36.996475445527196,    38.139700900785051,    39.984512126910616,    38.570027935473547,
    41.281522385383418,    40.498763067064829,    37.301251195171893,    37.598997395273329,
    39.964126588931528,    39.253207621281256,    40.381437383526546,    41.703270380261515,
    43.412733979274051,    43.957781426337696,    45.122021326583415,    46.021180762417451,
    45.150464100366911,    45.214714567731441,    46.55058534380013,    45.39989764971979,
    44.737392589965879,    44.604428895502622,    46.813319770251312,    45.963878631639467,
    49.591783682327744,    51.136898593295669,    52.469513872484676,    52.154039118121069,
    51.581362821450512,    52.155394965236781,    51.048662516333941,    50.773534499400625,
    50.039741178526356,    50.66515828396269,    53.112196190242379,    57.190493759119754,
    54.991332721780779,    57.059800182218261,    58.39500638445373,    56.709469028325053,
    58.600846914859176,    57.957122011842607,    56.166256436988242,    54.960944819137467,
    56.262835464872062,    56.544271691495922,    57.974037430118493,    59.735097713592218,
};

} // namespace

int main() {
    using kalos::decide_differencing;

    // ---- request validation ----
    rejects([] { decide_differencing(pinned_request({})); });
    rejects([] {
        decide_differencing(pinned_request({1, std::numeric_limits<double>::quiet_NaN()}));
    });
    rejects([] {
        decide_differencing(pinned_request({1, std::numeric_limits<double>::infinity()}));
    });
    // Qualified period domain: the JVM machine accepts any period, the Kalos
    // estimation seams are qualified for 4 and 12 only.
    for (const int period : {-1, 0, 1, 2, 3, 5, 6, 13}) {
        rejects([period] { decide_differencing(pinned_request({1, 2, 3, 4, 5, 6}, period)); });
    }
    rejects([] {
        auto request = pinned_request({1, 2, 3, 4, 5, 6});
        request.max_d = -1;
        decide_differencing(request);
    });
    rejects([] {
        auto request = pinned_request({1, 2, 3, 4, 5, 6});
        request.max_seasonal_d = -1;
        decide_differencing(request);
    });
    rejects([] {
        auto request = pinned_request({1, 2, 3, 4, 5, 6});
        request.ub1 = std::numeric_limits<double>::quiet_NaN();
        decide_differencing(request);
    });
    rejects([] {
        auto request = pinned_request({1, 2, 3, 4, 5, 6});
        request.cancel = std::numeric_limits<double>::infinity();
        decide_differencing(request);
    });
    rejects([] {
        auto request = pinned_request({1, 2, 3, 4, 5, 6});
        request.precision = 0.0;
        decide_differencing(request);
    });
    rejects([] {
        auto request = pinned_request({1, 2, 3, 4, 5, 6});
        request.precision = -1e-7;
        decide_differencing(request);
    });

    // ---- interrupt propagation: the seam throws out, never swallowed ----
    {
        auto request = pinned_request(std::vector<double>(k_m_n80_rw, k_m_n80_rw + 80));
        bool threw = false;
        try {
            decide_differencing(request, [] { throw Boom(); });
        } catch (const Boom&) {
            threw = true;
        }
        assert(threw);
    }

    // ---- stMean algebra under the zero clamps (order == 0 branch) ----
    // Alternating +/-1: s is exactly 0, so tmean == 0 and mu == false. The
    // finalcond clamps (max_d = max_seasonal_d = 0) force the stationary
    // branch, where the statistic is pure pinned algebra: s / sqrt(q).
    // JVM-verified through the clamped module: d = 0, bd = 0, mu = 0,
    // tmean = 0.
    {
        std::vector<double> series(100, 1.0);
        for (std::size_t i = 1; i < series.size(); i += 2) series[i] = -1.0;
        auto request = pinned_request(series);
        request.max_d = 0;
        request.max_seasonal_d = 0;
        const auto decision = decide_differencing(request);
        assert(decision.d == 0 && decision.seasonal_d == 0);
        assert(std::abs(decision.t_mean) < 1e-15);
        assert(!decision.mean_correction);
    }

    // ---- stMean threshold ladder (pinned stMean thresholds: n > 200 ->
    // 2.55, n > 80 -> 2.0, else 1.96). All four cases below were verified
    // against the pinned JVM through the clamped DifferencingModule (maxD 0,
    // maxBD 0): d = 0, bd = 0 with the printed (mu, tmean) ----
    {
        struct Case {
            double t;
            int n;
            bool mu;
        };
        const Case cases[] = {
            {1.97, 80, true},   // 1.97 > 1.96
            {1.97, 100, false}, // 1.97 <= 2.0
            {2.30, 200, true},  // 2.3 > 2.0
            {2.30, 201, false}, // 2.3 <= 2.55
        };
        for (const Case& c : cases) {
            std::vector<double> series(static_cast<std::size_t>(c.n), 1.0);
            series[0] = solve_shift(c.t, c.n);
            auto request = pinned_request(series);
            request.max_d = 0;
            request.max_seasonal_d = 0;
            const auto decision = decide_differencing(request);
            assert(decision.d == 0 && decision.seasonal_d == 0);
            assert(std::abs(decision.t_mean - c.t) < 1e-9);
            assert(decision.mean_correction == c.mu);
        }
    }
    // The (t = 1.97, n = 81) spike series is JVM-invalid: the pinned module
    // reaches m_err_sing in a mid-machine HannanRissanen pass and the
    // exception propagates out of process(DoubleSeq, int) (verified against
    // the pinned JVM; only the RegSarimaModelling entry point catches). The
    // seam reproduces the failure as a runtime_error.
    {
        std::vector<double> series(81, 1.0);
        series[0] = solve_shift(1.97, 81);
        auto request = pinned_request(series);
        request.max_d = 0;
        request.max_seasonal_d = 0;
        bool threw = false;
        try {
            decide_differencing(request);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }

    // ---- JVM-pinned replays (corpus ids m_n80_rw and m_trend_seasonal) ----
    // m_n80_rw: random walk, nextstep settles at round 2 with d = 1, bd = 0
    // and the drift test significant (iter = 2, mlused = true).
    {
        const auto decision = decide_differencing(pinned_request(
            std::vector<double>(k_m_n80_rw, k_m_n80_rw + 80)));
        assert(decision.d == 1 && decision.seasonal_d == 0);
        assert(decision.mean_correction);
        assert(decision.ml_used);
        assert(decision.nextstep_calls == 3);
        assert(std::abs(decision.t_mean - 2.5102257988485315) < 1e-3);
    }
    // m_trend_seasonal: trend plus seasonality, decided at step0 (iter = 0).
    {
        const auto decision = decide_differencing(pinned_request(
            std::vector<double>(k_m_trend_seasonal, k_m_trend_seasonal + 120)));
        assert(decision.d == 1 && decision.seasonal_d == 0);
        assert(decision.mean_correction);
        assert(!decision.ml_used);
        assert(decision.nextstep_calls == 1);
        assert(std::abs(decision.t_mean - 187.76340410419274) < 1e-3);
    }

    // ---- determinism: identical results back to back ----
    {
        const auto first = decide_differencing(pinned_request(
            std::vector<double>(k_m_n80_rw, k_m_n80_rw + 80)));
        const auto second = decide_differencing(pinned_request(
            std::vector<double>(k_m_n80_rw, k_m_n80_rw + 80)));
        assert(first.d == second.d && first.seasonal_d == second.seasonal_d);
        assert(first.mean_correction == second.mean_correction);
        assert(first.t_mean == second.t_mean);
        assert(first.ml_used == second.ml_used);
        assert(first.nextstep_calls == second.nextstep_calls);
        assert(first.trace.size() == second.trace.size());
    }

    std::puts("test_differencing_decision: OK");
}
