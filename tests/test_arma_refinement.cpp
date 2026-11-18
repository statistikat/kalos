// Item 21-F: unit qualification of the transcribed x13 AutoModellingModule
// ARMA refinement and comparison (stage M part 2). The corpus sweep
// (tools/verify_arma_refinement.cpp against
// validation/evidence/native/arma-refinement) carries the full
// JVM-qualified stage machine; this unit covers the parts the corpus
// cannot pin at the fast-lane level: request validation, the pinned
// airline-fallback path (the m_sw20 differencing failure), the guarded-
// region interrupt contract, the stored-never-read acceptWhiteNoise
// field, determinism, and pinned replays from the double-captured corpus:
// the m_wn96_a identity-path refinement (every panel BIC2 bit-identical),
// the m_walk96_trend linearizedData subtraction (bit-identical), and the
// m_i2_d2a d = 2 one-tap re-differencing filter (the (1 - B^2)(1 - B^12)
// application, bit-identical). Panel cells follow the sweep's tiers
// (exact or inside the 1e-5 tight band; the sweep measures 0 failures
// corpus-wide with 3 basin-class cells). See
// docs/contracts/arma-refinement-contract.md.
#include <kalos/arma_refinement.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// The decoder-pinned module scalars (X13SpecDecoder.readAutoModel).
kalos::ArmaRefinementRequest make_request(const std::vector<double>& series,
                                          const std::vector<double>& coef,
                                          bool mean, int period,
                                          const std::array<int, 6>& cur) {
    kalos::ArmaRefinementRequest r;
    r.series = series;
    r.mean = mean;
    r.regression_coefficients = coef;
    r.p = cur[0];
    r.q = cur[1];
    r.bp = cur[2];
    r.bq = cur[3];
    r.d = cur[4];
    r.seasonal_d = cur[5];
    r.period = period;
    r.max_d = 2;
    r.max_seasonal_d = 1;
    r.ub1 = 0.96999999999999990;
    r.ub2 = 0.87999999999999990;
    r.cancel = 0.10000000000000000;
    r.precision = 1.0000000000000000e-07;
    r.models_count = 5;
    r.max_p = 2;
    r.max_q = 2;
    r.max_bp = 1;
    r.max_bq = 1;
    r.balanced = false;
    r.mixed = true;
    r.accept_white_noise = false;
    r.eps = 1.0000000000000000e-07;
    return r;
}

// Pinned replay data from the double-captured corpus
// (validation/evidence/native/arma-refinement).
const double kWn96Series[] = {
    0.034573716201001727, -0.32382443587684712, 2.2612518005651996, -0.079015628660681833,
    0.068049091788605326, -1.0650564443591122, -0.35748406422351048, -1.0226681993295939,
    0.33684585061555095, 0.71913283869245614, -1.3160292937221103, -1.2206522734013061,
    -0.56290581377206228, -0.43085815910586012, 1.3298295418447443, -0.96640118909983308,
    0.52109460281528697, -0.96795237068946771, -0.056947585238506536, 1.1565397241792057,
    0.15582453182401035, 0.047177937179571347, -0.12582372544857801, 0.67655373431256716,
    1.022297990977177, 0.43956690776904705, -0.70443183026662337, 0.81065286812736648,
    0.29894891294401177, 1.0684434477704288, 1.0687642613510362, 0.56932240038839932,
    -0.65841489042769441, 1.7815503723838819, -1.4210500694152985, 1.4672852187295595,
    -0.5882318358953369, 1.2929350330014999, 0.86931857824411141, -1.9211079229979322,
    -0.83968138606142007, 0.56202660238672519, 0.58371968192654045, -0.50829942714902954,
    -0.20764900924621404, 0.64101663916539808, -1.027441284676939, 1.7052419926550535,
    0.34607880932432428, 0.29428621167434288, 0.87711150919628156, -0.63428019777116273,
    0.93584586123224656, -0.24327886823604039, -0.43601880999706405, -1.3866860877024076,
    0.43580822994956764, -1.237106043454425, -1.1081876281953693, 1.4577829607182928,
    -0.15070737536033954, -1.3670289137156968, 1.5345453960444675, -1.790577213500973,
    0.1668130080709172, -0.1784819949107142, 0.24743521375905886, 0.46584172184472744,
    2.0120689148557434, 0.011868538316638283, -0.999772180250867, 0.06233450444146816,
    0.15398812479503651, 0.32053781476830967, -0.23118686081886883, 0.56798098662088536,
    -0.048275472103785813, -0.46918229006060452, 0.88672144597152558, -0.11371566901074673,
    1.2824050990892006, -0.10688784143100688, -0.18675395936549946, 0.43679163628808043,
    -0.52143207391151414, -0.72359665048616828, 0.088978817980289948, 1.4834390173261023,
    -1.5369671673740395, 0.42538163121610628, -0.042325937082297323, 0.3126906539047577,
    -0.45200755916240581, -0.15038746415329907, 0.19060261341899157, 0.15363154463822365
};

const double kWalkTrendSeries[] = {
    -0.14354974032173459, -0.2304344682532915, -1.2006729380849033, -0.7370279750037857,
    0.4079247951300059, 0.21051114475854849, -1.53983897891996, -2.3891167386972501,
    -1.742694752407147, -0.12727359337219801, 0.81963190860323487, 0.37802310490444646,
    -2.1336898959296304, -3.4110443192337074, -4.0695029167725441, -3.602966031495122,
    -3.6565195002636597, -3.0377101332557839, -4.6797165074816345, -4.6944622624744667,
    -4.2852776842387987, -5.2309566657868256, -2.5717838346498838, -3.2054406364424897,
    -3.4955882310034205, -3.6135413396579099, -6.1271438482487435, -4.8459427436877478,
    -4.3794867895220797, -3.865491096167565, -6.7743892383957807, -5.9230922963137749,
    -5.8262137676902572, -5.3100774741512486, -4.9328301468719324, -5.4878983415063001,
    -4.6759327975539833, -5.5363679063064684, -5.340470681109692, -4.5884090016447185,
    -3.6331699400243274, -6.325149797613749, -5.9639658346801134, -4.7707963421504589,
    -4.7470714221251926, -4.9340985219123352, -4.4853840107837604, -1.6511313885100918,
    -2.0342731631042148, -1.3734608631177694, -0.93085959264207307, -1.8615679551347999,
    -2.0326487681966099, -2.8332476021746222, -1.9906956758793508, -3.6571089343874346,
    -3.1085672321640785, -4.0178207681103668, -5.9237912689816126, -6.8063900941169955,
    -7.4689817399822713, -8.4751158983780694, -10.351825679212574, -7.8948065389065336,
    -8.9714142780108332, -9.1554992777683797, -8.5946903653951274, -8.4604440260943132,
    -6.7484641616693883, -6.960108574944929, -7.6830077434512081, -7.6035548278668372,
    -8.4212403173494437, -8.766516865662787, -8.1066186040091797, -10.10433258201912,
    -9.0057723656668234, -8.9408240684007598, -7.9835867828110922, -7.6825875549803992,
    -8.4857722901234744, -7.0342205537351532, -6.6355772042204988, -5.0172500401922546,
    -5.8127834490658721, -6.8876136711412714, -8.3135626101676507, -6.3528072000789306,
    -7.6145195340081351, -7.6374291519069661, -8.3510678670553844, -8.7591336456957674,
    -7.9108909219808092, -7.5764504070035814, -8.4847604431087316, -9.0934411505624801
};

const double kSw20Series[] = {
    -0.35964308605490181, -0.73847000167301413, -0.30870892922325016, -0.94534166341734149,
    0.86895095369654374, 0.77558082133307216, 1.0822686679684841, -1.3923795773839311,
    0.4702806736881916, 2.1630915886135482, 0.1709036998663078, 2.3028253729583024,
    -1.3632180106145313, -1.1059476641637263, -0.51982416730261549, -1.1708141518390112,
    1.1300531651490116, 1.1483133216896679, 0.27847543495980476, -2.2409370111004354
};

const double kI2Series[] = {
    -0.79786983762856822, -1.8857145866594263, -3.7863710420755448, -4.9118932509809135,
    -7.6436508216379453, -11.285344846291579, -15.738370400166964, -19.289583628835821,
    -22.029865029041705, -22.939142327356947, -23.210623911221372, -21.639478082024127,
    -19.261086880773881, -17.143764051490876, -16.747868298522956, -17.338073078065527,
    -18.400934722345792, -19.763015137116806, -21.221741228520312, -21.635923188869391,
    -22.452220048196857, -22.711929848795769, -24.704794908034845, -25.33523875378329,
    -26.028052888631048, -24.630021518284025, -23.693585730924603, -21.488862772956715,
    -17.839679628853744, -15.357915096020035, -11.63642020025633, -8.7281718546379317,
    -6.4802154099530007, -4.8954988041746423, -1.3329200423649814, 2.2338085026075651,
    5.5068417977340394, 8.6474404735464212, 12.315017718886347, 16.702972682277863,
    19.976513442057282, 22.330170778717868, 24.532904848159443, 29.208189411280497,
    33.856295327212258, 38.345659861734767, 42.407165559401932, 46.719037556620009,
    50.965895038530483, 56.05382217428852, 62.486994169298129, 68.43074222799541,
    73.40151730011948, 78.399233356242263, 84.50874915843626, 91.633913846757423,
    96.263434027785863, 101.10513921122129, 106.466502560796, 112.6984152855801,
    118.34797206210155, 123.87789492954471, 128.69662461441794, 133.39116677662523,
    136.023244412417, 137.68145995400366, 138.61429365156093, 140.92144668032233,
    143.25074797897355, 144.86327415570273, 147.8660944694023, 152.32341354795514,
    156.30580250008768, 160.55504517021279, 165.29946613753887, 168.94935913590152,
    172.84105161502356, 178.23697079523134, 184.02421996191731, 190.36635678088075,
    195.83344865818373, 201.16327506753569, 205.29584147463777, 212.08508116659016,
    219.72279014896893, 225.6531780234011, 232.16237314592667, 237.91911101029677,
    243.39338862772297, 247.79985432223901, 252.73669007287521, 258.6318730302375,
    264.83710182290304, 269.9775062790294, 276.00744825121319, 281.94954032015249,
    287.7716232208287, 294.05224750645453, 300.44208317464671, 304.96595927435311,
    308.1970177035754, 311.54356385017701, 314.94867962407949, 316.99014106610031,
    316.8575140372194, 318.19135813326784, 319.85595621185263, 322.10202731208386,
    324.47003252606646, 327.0540133423695, 331.5014397810711, 336.32830519318992,
    341.19507073420226, 344.93839683291958, 349.1527825181272, 353.4205718647604,
    357.59457738877666, 362.8316910174056, 368.28802597797323, 374.5324092330813,
    380.64758411050218, 386.36259235489734, 392.13275024362741, 396.18225337640541,
    399.32651266075766, 404.47968418519008, 411.58989097247462, 417.22768376051351,
    422.94759940267124, 431.85674733112057, 439.60010869217649, 446.34894225178545,
    453.13590668155149, 460.59787412213797, 467.26795309317544, 472.41167423432131,
    476.88961354632181, 480.10178766876066, 483.58708723553275, 488.0136501433206,
    494.36597323046527, 502.58440509331155, 509.50232023388907, 516.43098620738192
};

const double kI2FilteredHead[] = {
    5.5017197866979010, 2.8318696377468378, 2.2042133557395650,
    3.9485095362593867, 5.2739130723545000, 6.1313307307916570
};



struct Boom : std::runtime_error {
    Boom() : std::runtime_error("boom") {}
};

void expect_invalid(const kalos::ArmaRefinementRequest& req, const char* what) {
    bool threw = false;
    try {
        kalos::refine_arma_model(req);
    } catch (const std::invalid_argument&) {
        threw = true;
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("wrong exception for ") + what + ": " + e.what());
    }
    assert(threw && "static input violation must throw invalid_argument");
}

} // namespace

int main() {
    using kalos::refine_arma_model;

    // 1. Request validation (std::invalid_argument, fail-closed).
    {
        const std::vector<double> y(kWn96Series, kWn96Series + 96);
        const std::vector<double> coef = {0.064085035897549670};
        expect_invalid(make_request({}, coef, true, 12, {0, 0, 0, 0, 0, 0}),
                       "empty series");
        {
            auto r = make_request(y, coef, true, 12, {0, 0, 0, 0, 0, 0});
            r.series[3] = std::nan("");
            expect_invalid(r, "non-finite series");
        }
        {
            auto r = make_request(y, coef, true, 12, {0, 0, 0, 0, 0, 0});
            r.period = 5;
            expect_invalid(r, "period outside {4, 12}");
        }
        {
            auto r = make_request(y, coef, true, 12, {0, 0, 0, 0, 0, 0});
            r.p = -1;
            expect_invalid(r, "negative order");
        }
        {
            auto r = make_request(y, coef, true, 12, {0, 0, 0, 0, 0, 0});
            r.models_count = 0;
            expect_invalid(r, "models_count < 1");
        }
        {
            auto r = make_request(y, coef, true, 12, {0, 0, 0, 0, 0, 0});
            r.eps = 0.0;
            expect_invalid(r, "non-positive eps");
        }
        {
            // One user column but only the mean coefficient.
            auto r = make_request(y, coef, true, 12, {0, 0, 0, 0, 0, 0});
            r.regressors = {std::vector<double>(96, 1.0)};
            expect_invalid(r, "coefficient count != (mean ? 1 : 0) + k");
        }
        {
            // Two coefficients for a mean-less model without regressors.
            auto r = make_request(y, {1.0, 2.0}, false, 12, {0, 0, 0, 0, 0, 0});
            expect_invalid(r, "coefficient count != (mean ? 1 : 0) + k (2)");
        }
        {
            auto r = make_request(y, {1.0}, false, 12, {0, 0, 0, 0, 0, 0});
            r.regressors = {std::vector<double>(95, 1.0)};
            r.regression_coefficients = {1.0};
            expect_invalid(r, "regressor length != series length");
        }
        {
            auto r = make_request(y, coef, true, 12, {0, 0, 0, 0, 0, 0});
            r.cancel = std::nan("");
            expect_invalid(r, "non-finite cancel");
        }
    }

    // 2. m_wn96_a replay: identity path, spec none, mean on. Every pinned
    // value from the double capture.
    {
        const std::vector<double> y(kWn96Series, kWn96Series + 96);
        const auto r = refine_arma_model(
            make_request(y, {0.064085035897549670}, true, 12,
                         {0, 0, 0, 0, 0, 0}));
        assert(!r.fallback && r.fallback_stage.empty());
        assert(r.linearized.size() == 96 && r.re_differenced.size() == 96);
        for (std::size_t i = 0; i < 96; ++i) {
            assert(r.linearized[i] == y[i]); // no user columns: identity
            assert(r.re_differenced[i] == y[i]);
        }
        assert(r.differencing.has_value());
        assert(r.differencing->d == 0 && r.differencing->seasonal_d == 0);
        assert(!r.differencing->mean_correction);
        assert(r.differencing->t_mean == 0.71183108918857650); // bit-exact
        assert(r.p == 1 && r.q == 0 && r.bp == 0 && r.bq == 0);
        assert(r.d == 0 && r.seasonal_d == 0 && !r.mean);
        assert(r.specification_changed && r.mean_changed && r.changed);
        assert(!r.evaluations.empty() && !r.refinement.empty());
        assert(r.preferred_models.size() == 5);
        const int order[5][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0},
                                 {1, 0, 0, 1}, {1, 0, 1, 0}};
        const double bic[5] = {2.6245678955335254, 2.6270973439955690,
                               2.6345110282485513, 2.6528757665230600,
                               2.6533466186326384};
        for (int k = 0; k < 5; ++k) {
            const auto& m = r.preferred_models[static_cast<std::size_t>(k)];
            assert(m.p == order[k][0] && m.q == order[k][1]
                   && m.bp == order[k][2] && m.bq == order[k][3]);
            // Sweep tier: these five cells are measured bit-identical.
            assert(m.bic == bic[k]);
        }
    }

    // 3. m_walk96_trend replay: the linearizedData subtraction of the user
    // column with the concentrated-likelihood coefficient (pinned op order:
    // z[i] = z[i] - coefficient * column[i]; the mean coefficient stays).
    {
        const std::vector<double> y(kWalkTrendSeries, kWalkTrendSeries + 96);
        auto req = make_request(y, {-1.3220919616935658, -0.078415701796687040},
                                true, 12, {0, 0, 0, 0, 0, 0});
        std::vector<double> trend(96);
        for (int i = 0; i < 96; ++i) {
            trend[static_cast<std::size_t>(i)] = static_cast<double>(i + 1);
        }
        req.regressors = {trend};
        const auto r = refine_arma_model(req);
        assert(!r.fallback);
        assert(r.linearized.size() == 96);
        const double b = -0.078415701796687040;
        for (std::size_t i = 0; i < 96; ++i) {
            assert(r.linearized[i] == y[i] - (b * trend[i])); // bit-exact
        }
        assert(r.linearized[0] == -0.065134038525047550);
        assert(r.linearized[1] == -0.073603064659917420);
        assert(r.differencing.has_value() && r.differencing->d == 0
               && r.differencing->seasonal_d == 0);
        assert(r.differencing->mean_correction); // mu = 1
        assert(r.differencing->t_mean == -5.8784302358590870); // bit-exact
        assert(r.p == 1 && r.q == 0 && r.bp == 0 && r.bq == 0);
        assert(r.mean); // mean kept through the identity differencing
        assert(r.specification_changed && !r.mean_changed && r.changed);
        assert(r.preferred_models.size() == 5);
        assert(r.preferred_models[0].p == 1 && r.preferred_models[0].q == 0);
        assert(std::fabs(r.preferred_models[0].bic - 3.1108955148405184)
               <= 1e-5);
    }

    // 4. m_i2_d2a replay: d = 2 with the pinned one-tap D polynomials,
    // (1 - B^2)(1 - B^12), the genuine AutoModellingModule re-differencing
    // (NOT (1 - B)^2) - the filter output is bit-identical.
    {
        const std::vector<double> y(kI2Series, kI2Series + 144);
        const auto r = refine_arma_model(
            make_request(y, {}, false, 12, {0, 1, 0, 1, 1, 1}));
        assert(!r.fallback);
        assert(r.differencing.has_value());
        assert(r.differencing->d == 2 && r.differencing->seasonal_d == 1);
        assert(!r.differencing->mean_correction);
        assert(r.differencing->t_mean == -1.5700302243040625); // bit-exact
        assert(r.re_differenced.size() == 144 - 14); // deg((1-B^2)(1-B^12))
        for (std::size_t i = 0; i < 6; ++i) {
            assert(r.re_differenced[i] == kI2FilteredHead[i]); // bit-exact
        }
        assert(r.p == 1 && r.q == 1 && r.bp == 1 && r.bq == 0);
        assert(r.d == 2 && r.seasonal_d == 1 && !r.mean);
        assert(r.specification_changed && !r.mean_changed && r.changed);
        assert(r.preferred_models.size() == 5);
        assert(r.preferred_models[0].p == 1 && r.preferred_models[0].q == 1
               && r.preferred_models[0].bp == 1);
        assert(std::fabs(r.preferred_models[0].bic - 3.2999247070702293)
               <= 1e-5);
    }

    // 5. m_sw20 replay: the pinned airline fallback (differencing stage
    // throw -> SarimaOrders.airline(12), mean off, no rethrow).
    {
        const std::vector<double> y(kSw20Series, kSw20Series + 20);
        const auto r = refine_arma_model(
            make_request(y, {}, false, 12, {0, 0, 0, 0, 0, 0}));
        assert(r.fallback && r.fallback_stage == "differencing");
        assert(r.p == 0 && r.q == 1 && r.bp == 0 && r.bq == 1);
        assert(r.d == 1 && r.seasonal_d == 1 && !r.mean);
        assert(r.specification_changed && !r.mean_changed && r.changed);
        assert(r.linearized.size() == 20); // linearizedData still ran
        for (std::size_t i = 0; i < 20; ++i) {
            assert(r.linearized[i] == y[i]);
        }
        assert(!r.differencing.has_value());
        assert(r.re_differenced.empty());
        assert(r.evaluations.empty() && r.preferred_models.empty()
               && r.refinement.empty());
    }

    // 6. The guarded-region interrupt contract: a throw at the entry
    // propagates; a throw from inside the guarded region arms the region so
    // the exception propagates instead of becoming the airline fallback.
    {
        const std::vector<double> y(kWn96Series, kWn96Series + 96);
        const auto base = make_request(y, {0.064085035897549670}, true, 12,
                                       {0, 0, 0, 0, 0, 0});
        bool threw = false;
        try {
            refine_arma_model(base, [] { throw Boom(); });
        } catch (const Boom&) {
            threw = true;
        }
        assert(threw && "entry interrupt must propagate");

        int calls = 0;
        threw = false;
        try {
            refine_arma_model(base, [&calls] {
                if (++calls == 4) {
                    throw Boom();
                }
            });
        } catch (const Boom&) {
            threw = true;
        }
        assert(threw && "post-arm exception must propagate, not fall back");
    }

    // 7. acceptWhiteNoise is a stored, never-read field of the pinned
    // bytecode: both values give the identical machine.
    {
        const std::vector<double> y(kWn96Series, kWn96Series + 96);
        auto a = refine_arma_model(
            make_request(y, {0.064085035897549670}, true, 12,
                         {0, 0, 0, 0, 0, 0}));
        auto r2 = make_request(y, {0.064085035897549670}, true, 12,
                               {0, 0, 0, 0, 0, 0});
        r2.accept_white_noise = true;
        const auto b = refine_arma_model(r2);
        assert(a.p == b.p && a.q == b.q && a.bp == b.bp && a.bq == b.bq);
        assert(a.preferred_models.size() == b.preferred_models.size());
        for (std::size_t k = 0; k < a.preferred_models.size(); ++k) {
            assert(a.preferred_models[k].bic == b.preferred_models[k].bic);
        }
    }

    // 8. Determinism: two runs are bit-identical (the corpus itself is
    // double-captured across JVM processes; this pins the native side).
    {
        const std::vector<double> y(kI2Series, kI2Series + 144);
        const auto req = make_request(y, {}, false, 12, {0, 1, 0, 1, 1, 1});
        const auto a = refine_arma_model(req);
        const auto b = refine_arma_model(req);
        assert(a.p == b.p && a.q == b.q && a.bp == b.bp && a.bq == b.bq
               && a.d == b.d && a.seasonal_d == b.seasonal_d
               && a.mean == b.mean && a.fallback == b.fallback);
        assert(a.differencing->t_mean == b.differencing->t_mean);
        assert(a.linearized == b.linearized);
        assert(a.re_differenced == b.re_differenced);
        assert(a.preferred_models.size() == b.preferred_models.size());
        for (std::size_t k = 0; k < a.preferred_models.size(); ++k) {
            assert(a.preferred_models[k].bic == b.preferred_models[k].bic);
            for (int blk = 0; blk < 4; ++blk) {
                assert(a.preferred_models[k].coefficients[static_cast<
                        std::size_t>(blk)]
                       == b.preferred_models[k].coefficients[static_cast<
                              std::size_t>(blk)]);
            }
        }
        assert(a.refinement.size() == b.refinement.size());
        for (std::size_t k = 0; k < a.refinement.size(); ++k) {
            assert(a.refinement[k].candidate == b.refinement[k].candidate
                   && a.refinement[k].rule == b.refinement[k].rule
                   && a.refinement[k].incumbent == b.refinement[k].incumbent
                   && a.refinement[k].moves == b.refinement[k].moves);
        }
    }

    return 0;
}
