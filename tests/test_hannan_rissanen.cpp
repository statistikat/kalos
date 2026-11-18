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
#include <kalos/detail/hannan_rissanen_pipeline.hpp>

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

static const double k_hr_ar_stationary[] = {
    2.2317276074196495,    2.266318771006115,    3.2255260791880258,    3.4684452852117165,
    0.16865649638300084,    0.1868044007314465,    -0.088433014489302991,    -0.83340785067100176,
    -0.39224730211195413,    -0.095842516383648713,    1.1848561168014589,    1.0650943674462705,
    0.97265621541045011,    1.1863394787978516,    2.2297539132273454,    1.1098023746845329,
    -0.0077511645274879193,    -1.9666462998948036,    -1.50727568772464,    -0.31544228754223336,
    -0.98876046004359219,    0.38897515442501329,    0.66216061176687413,    1.0307646100285122,
    2.6373084165450624,    2.343879450124589,    2.3631256249121266,    1.1707042056885586,
    1.3330538267216894,    0.52718055468227809,    -0.59821745740377885,    -1.4921673729578973,
    -1.1224575471520069,    -1.6362525970806439,    -1.6262910688835635,    -1.7731379158066956,
    -0.98893161722464185,    -1.7871512850653897,    -1.2256066726876864,    -0.50901011873679525,
    -0.24447904978543439,    1.4122387766711915,    0.55921761295128425,    -1.0805611030510023,
    -0.034786422011268137,    0.049266914683187654,    0.92561993267270792,    1.1123520793165369,
    -0.6644794652681858,    -0.9343639552355214,    -1.2344450352314698,    1.0427735471481774,
    0.075767793333069378,    -0.7928026805741728,    -1.7798572524892735,    0.19228021580237753,
    -0.092613875099222462,    -0.95585168967163703,    1.4975424991924697,    -0.90066017475068028,
    -1.2044413777328713,    0.6946859987178361,    1.0279196933256187,    -0.061488222075302357,
    0.077136810438581643,    0.64995946265265891,    2.1279337712573176,    1.0707996308318308,
    0.37553473572982754,    0.22958556254287463,    0.45855925282872895,    1.6427122169031312,
    0.87322917388648202,    2.8526750080603045,    2.3387231283731902,    0.62384669681311822,
    -0.83039645155196395,    -1.0456272953107835,    -0.73835652619431191,    -0.85355195735313405,
    0.94709338280817934,    -0.034527385789357545,    1.0365606416170707,    1.0077571073407654,
    1.4693331906327962,    1.5824293453395681,    0.15444291881502858,    -1.2255047834699517,
    -0.67810334826220453,    -0.94073957670197739,    1.6234598730133181,    0.46022976220366307,
    -0.44323374612270033,    -0.20733764308525621,    -1.1275741780254476,    -1.7070603419342585,
    -0.9256391298902219,    -0.81684155154425198,    -0.43693689073418829,    -0.73702453078089181,
    0.79067480408258528,    2.065463092204364,    3.599250083037048,    2.2234972454982227,
    0.45106341025256269,    0.44023254806139106,    1.6211327901610744,    0.64280167283264888,
    0.042096990380065813,    1.191361530791273,    0.91619735728225327,    1.8597819366289934,
    0.22931866109799093,    0.57185389785818919,    0.3445879220002187,    0.53250191065952579,
    0.21839205067976317,    -0.49022517926222708,    1.4035166974792324,    0.85128935692263097,
};

static const double k_hr_wn_shift100[] = {
    0.86696563952828387,    -0.32220114005180472,    0.18484090336678904,    -0.66967140761893451,
    -1.740225320554434,    0.35489187432216962,    1.2503042308675301,    0.56909017222922753,
    0.55489823878692834,    -0.66097613534556032,    0.46430623247243136,    0.2486436915989314,
    0.46167340436162108,    1.1121197338024273,    0.75676189043074871,    0.20342961397244397,
    0.2711665543902827,    -1.6838077843742572,    0.15158480490559745,    -0.70811452623086479,
    -0.35793580521377072,    -0.63645638229994073,    -1.8013843198755113,    1.6515174404986264,
    -0.75657456472902695,    -0.59848548789616429,    0.99383755308827304,    0.16101076376903137,
    1.1500850809191974,    2.7273742215354235,    -0.38369434722950907,    0.91319510389340608,
    -1.5444930733156599,    0.36551939500838776,    -0.58122817401655658,    1.1821945022796951,
    -0.33303074918828096,    -0.84801250677547069,    0.67142901728471616,    0.047176784568883301,
    0.72078306264126946,    -0.9213158821640155,    1.0715487857519879,    -0.6232583378007901,
    -0.73159836325109628,    -1.2813719632051079,    -0.28218518130067288,    1.0225580303412325,
    -0.55093284901295259,    -1.4277541930633908,    0.43533781557909607,    -0.76834279130738925,
    -1.8651651566826104,    0.32331233960979439,    1.6182400830255752,    -0.28374620162165803,
    1.5097542572771048,    2.1024945641890835,    -2.2672946136114773,    0.42678804635417161,
    1.394710680766073,    -0.16196261653219907,    1.8953228582060826,    -1.19096001219765,
    -0.44552513211920602,    0.41744259979802079,    -1.7577803516549768,    0.93952922550228024,
    -0.38908903421335345,    1.0304440570021474,    0.58228162352046064,    -0.55046510810534333,
    0.69880792683077986,    0.17109705741775683,    -0.78678839706922021,    -2.2140344799119065,
    0.95029488071475843,    -0.9818694548999074,    -0.45574113248486597,    1.0540050475720923,
    0.98993456406518399,    0.67272121274303509,    1.1024262140739136,    0.22774348146312995,
    2.4371229432262655,    -0.48489039976622711,    0.88004839747316099,    0.85879629680557334,
    -1.5110427210081023,    0.71158139143165577,    1.2241220436689151,    -1.1732100654157169,
    0.38860734024481458,    0.29533891110419225,    2.4073238924219886,    -0.26568350602515062,
    0.3742898717171429,    0.77458179699541096,    0.91602817926215052,    0.67315521970707981,
};

static const double k_hr_const_noise[] = {
    50.000000000092179,    50.000000000739171,    50.000000000926939,    50.000000000908557,
    50.000000000370555,    50.000000000148525,    50.000000000089273,    50.000000000161727,
    50.000000000859359,    50.000000000260016,    50.000000000014921,    50.000000000324974,
    50.0000000006996,    50.000000000915428,    50.000000000861228,    50.000000000364167,
    50.000000000626166,    50.000000000677659,    50.000000000273083,    50.000000000103036,
    50.000000000972712,    50.000000000936517,    50.000000000621142,    50.000000000206995,
    50.000000000564178,    50.000000000511527,    50.000000000312951,    50.000000000212992,
    50.000000000152994,    50.000000000920494,    50.000000000785619,    50.000000000109893,
    50.000000000429466,    50.000000000665949,    50.000000000958764,    50.000000000990433,
    50.000000000890765,    50.000000000870521,    50.000000000504897,    50.000000000204089,
    50.000000000705001,    50.000000000270823,    50.000000000455771,    50.000000000762441,
    50.000000000222272,    50.000000000963404,    50.000000000897529,    50.000000000756124,
    50.00000000005987,    50.000000000647766,    50.000000000730161,    50.000000000903569,
    50.000000000640263,    50.000000000660982,    50.000000000404199,    50.000000000805578,
    50.00000000082909,    50.000000000935927,    50.00000000095654,    50.000000000351228,
    50.000000000130335,    50.000000000341586,    50.000000000768303,    50.000000000599911,
    50.000000000238472,    50.000000000178467,    50.000000000924111,    50.000000000527642,
    50.000000000295174,    50.00000000091881,    50.000000000345253,    50.000000000230081,
    50.000000000461,    50.000000000517538,    50.000000000661444,    50.000000000943075,
    50.000000000303707,    50.000000000908173,    50.000000000004214,    50.000000000597495,
    50.000000000127187,    50.000000000880746,    50.000000000806615,    50.000000000753658,
    50.000000000357524,    50.000000000027264,    50.000000000544262,    50.000000000795289,
    50.000000000461235,    50.000000000229981,    50.00000000001458,    50.000000000072951,
    50.000000000014509,    50.000000000258844,    50.000000000198341,    50.000000000937369,
    50.0000000006753,    50.000000000811944,    50.000000000590731,    50.000000000379707,
    50.000000000231537,    50.00000000000253,    50.000000000777767,    50.000000000395858,
    50.000000000125304,    50.000000000576293,    50.000000000596309,    50.000000000258083,
    50.000000000542421,    50.000000000968768,    50.000000000065839,    50.00000000073004,
    50.000000000360004,    50.000000000851458,    50.0000000000298,    50.000000000094524,
    50.000000000957151,    50.000000000826937,    50.000000000588557,    50.000000000073044,
    50.000000000521382,    50.000000000787225,    50.000000000348059,    50.000000000119904,
    50.000000000891625,    50.000000000988408,    50.000000000211202,    50.000000000001393,
    50.000000000803176,    50.000000000073811,    50.000000000763926,    50.000000000365674,
    50.000000000797236,    50.000000000680693,    50.000000000863359,    50.000000000082579,
    50.000000000577842,    50.000000000323979,    50.000000000701476,    50.00000000096442,
    50.000000000093436,    50.000000000016037,    50.00000000002531,    50.000000000003482,
};

static const double k_hr_wn50_filtered[] = {
    -0.59747952997306397,    -0.107033723229013,    -0.93142225872045525,    -0.88738556043915129,
    0.45304551102668167,    0.097104073669644464,    0.30053647898500396,    2.1792079235087995,
    -0.1810213187915406,    -0.67195490151288484,    1.1902055682735324,    1.8972060077138733,
    1.5852038047492392,    0.87042506687490717,    -2.0342613721113678,    1.7770540572271996,
    -0.2221456898712752,    2.0363958184598965,    1.1257118465895428,    -0.58721732379080638,
    -0.81423942225170476,    1.7343724352043106,    -1.2952263439667204,    0.79073148441699237,
    -1.6105298268725139,    0.85336812523245287,    1.5737504085107348,    -2.7317607911252986,
    -1.1386302990054318,    -0.50641684538370357,    -0.48504457942449575,    -1.077621827537429,
    0.45076588118148037,    -1.1405337332184837,    0.39360878473835959,    0.23795677392549397,
    0.93431064521966123,    -0.99864882132715704,
};


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


    // ---- Raw-pipeline regressions (item 21-D A/B discovery) ----
    // The four series below run through detail::hannan_rissanen_pipeline with
    // orders 1/1/1/1 (period 12) after mean removal; the expected blocks are
    // the pinned JVM HannanRissanen.getModel() coefficients captured from the
    // reference build (double-capture identical). The original transcription
    // matched them only inside the 21-A landing band; the 21-D A/B isolation
    // exposed four bugs, all fixed, and these cases now agree to ~1e-13:
    //   1. LevinsonAlgorithm middle-slot update: the pinned bytecode computes
    //      the temporary a[m+1-i] + k*a[i] before writing either slot, so the
    //      middle slot gets P*(1+k), not the P*(1+2k) of the in-place order.
    //   2. npi(): max((int)(log n)^2, max(p + P*bp, 2*(q + P*bq))), NOT the
    //      product of the two maxima.
    //   3. updatemodel(): the btheta section starts at nP + q + block*(1+q),
    //      the old index omitted +q and returned theta for btheta.
    //   4. finalcorrection(): BackFilter.apply is causal with lower bound
    //      -degree: filtered[t] = sum_k poly[k]*data[t+degree-k].
    // The wn50 array is the (1-B^12)-filtered series the differencing machine
    // hands to the pipeline; the others are direct responses.
    {
        static const double k_expected[4][4] = {
            {-0.52962727531013910, 0.30098833214400350, 0.13655680916426130,
                0.34098160348970286}, // ar_stationary
            {0.20900014197236041, -0.64721875121508700, 0.065872547680653560,
                -0.63017789830950410}, // wn_shift100
            {-0.27431228531190460, 0.82946356671985550, -0.12419252068026822,
                0.73633489690196010}, // const_noise
            {0.21336709933160310, -0.25055156802161616, 0.10723250905214732,
                -0.72492381326989340}, // wn50 filtered
        };
        static const double* k_series[4] = {
            k_hr_ar_stationary, k_hr_wn_shift100, k_hr_const_noise, k_hr_wn50_filtered};
        static const std::size_t k_lengths[4] = {120, 100, 144, 38};
        for (int case_index = 0; case_index < 4; ++case_index) {
            std::vector<double> y(
                k_series[case_index], k_series[case_index] + k_lengths[case_index]);
            double mean = 0.0;
            for (const double value : y) mean += value;
            mean /= static_cast<double>(y.size());
            for (double& value : y) value -= mean;
            const auto raw = kalos::detail::hannan_rissanen_pipeline(y, orders(1, 1, 1, 1));
            assert(raw[0].size() == 1 && raw[1].size() == 1 && raw[2].size() == 1 &&
                   raw[3].size() == 1);
            const double blocks[4] = {raw[0][0], raw[1][0], raw[2][0], raw[3][0]};
            for (int i = 0; i < 4; ++i) {
                assert(std::abs(blocks[i] - k_expected[case_index][i]) < 1e-12);
            }
            // Bug 3 guard: the seasonal MA block must not mirror the regular one.
            assert(std::abs(raw[3][0] - raw[2][0]) > 1e-3);
        }
        rejects([] { kalos::detail::hannan_rissanen_pipeline({}, orders(1, 1, 1, 1)); });
        rejects([] { kalos::detail::hannan_rissanen_pipeline({1, 2, 3}, orders(-1, 1, 1, 1)); });
        rejects([] {
            kalos::detail::hannan_rissanen_pipeline(
                {1, std::numeric_limits<double>::quiet_NaN()}, orders(1, 1, 1, 1));
        });
    }

    std::puts("test_hannan_rissanen: OK");
}
