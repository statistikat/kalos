// Item 21-E: unit qualification of the transcribed ArmaModule initial-ARMA
// identification (stage M part 1). The corpus sweep
// (tools/verify_initial_arma_identification.cpp against
// validation/evidence/native/initial-arma-identification) carries the full
// JVM-qualified machine; this unit covers the parts the corpus cannot pin at
// the fast-lane level: request validation, the pinned abort equivalences
// (short differenced cursor, zero-norm OLS response, unfilled estimations
// slots), interrupt propagation, the stored-never-read acceptWhiteNoise
// field, and the shared smoke replay - the 96-point d=1 bd=1 mean-corrected
// AR(2)-with-trend series whose JVM cursor (83 values), three-stage grid
// enumeration, every candidate BIC2/logLikelihood/landing, merged panel and
// refinement outcome are pinned from the pinned-observer corpus. Cursor and
// decisions are exact; candidate cells follow the sweep's tiers (exact, or
// inside the 1e-5 tight band, or the documented basin class: worst smoke
// deviation 5.2e-07 in BIC2). See
// docs/contracts/initial-arma-identification-contract.md.
#include <kalos/initial_arma_identification.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kScalarBand = 1e-5;
constexpr double kLlBandFactor = 5e-4;  // times the cursor length
constexpr double kParBand = 2e-3;
constexpr double kBasinBound = 2e-1;
constexpr double kLlBasinBound = 3.0;
constexpr double kParBasinBound = 2.0;

// The shared smoke series: AR(2) a=(0.5,-0.25) with a unit trend, n=96
// (seeded capture of the smoke probe; the cursor below is the JVM's own).
const double kSmokeSeries[] = {
    1.2214759276580487, 2.5095449739746165, 3.0852672888323630, 2.0236190634962510,
    2.2847414128582963, 1.4092767187474320, 2.0924601674490075, 1.4873148807228627,
    1.1923885143467507, 1.2855005939386113, 0.75588354840667420, 1.6711278637759912,
    1.7817079609387831, 1.8612291634258686, 1.7660735061448220, 1.9212509431403688,
    4.2514876233341200, 3.2250092293660470, 3.4602711039855950, 1.9556971381166073,
    2.0237421750353090, 1.7832580063567085, 1.3787975002754105, 1.8978253935441950,
    2.0520843186765100, 2.0501525371590743, 1.2270470480175544, 2.9201802265062380,
    2.7992081573031820, 2.6935333261725773, 2.6405015484765766, 2.7574832649056160,
    2.2499192989969505, 2.3637920544839535, 2.3742033094520110, 3.6415886374290810,
    2.2434000183474250, 2.3658492409101566, 2.4891065514513900, 3.6008209758515064,
    2.4273912691875514, 2.5269918747967353, 1.9670679080768250, 3.2056346746687030,
    2.9745430263953450, 3.3027714679131770, 2.2770757432605206, 1.9084718256834956,
    1.9978674766177122, 1.9842372293066797, 2.3779965672508605, 1.7092079889507725,
    1.5385219998876827, 1.6227162057247038, 1.2591056790864028, 0.68577248274354830,
    1.0755411886703135, 3.5587481715022267, 2.5464901393342316, 2.7293400682391296,
    2.7441762596179964, 1.8374417134923275, 3.1199629031360220, 3.1292817232121310,
    2.6968003377741865, 2.4830061516193480, 2.9346595581466810, 2.5343436709130875,
    1.5703040042554817, 2.0062324621560683, 1.8461807683617784, 1.2706749048350372,
    2.9346785626153356, 3.2784775103462440, 3.8304199965401030, 2.9941637809878276,
    3.0238591794490440, 2.0302065327078425, 2.9781417258119243, 2.2891297177113390,
    3.5663867096161800, 3.4056119534928797, 2.8856532959838460, 2.6216587568849660,
    3.6789695573804800, 1.9360899344323634, 1.9799320966774603, 2.8651603430970223,
    2.9830066159257100, 1.8187725506105645, 2.6196474022931335, 2.2364102612178467,
    2.0409922148105770, 2.7563639066769237, 3.3292199274196720, 2.1896477025951230,
};

// The JVM RegArimaUtility.olsResiduals cursor: d=1, bd=1, period=12, mean
// on, no user regressors (one-column pinned OLS, bit-exact).
const double kSmokeCursor[] = {
    -1.1965931078346563, -0.65892323614396686, 1.2287803983264849, 2.0810690668265321,
    -0.13905896386238278, -0.43596683808720155, -0.88747394314801664, 0.37492613928963964,
    -0.32164151227563448, 0.13711127544546522, -0.38426168610570649, 0.055633563964349181,
    -0.069498248009694924, -0.71599509586564725, 1.5499104774879631, -2.4392540134019809,
    0.93275829883229455, -0.27633891632072244, 1.633510418292853, -0.56365426683254072,
    0.36631166016042943, 0.42682649704418191, 0.76031217070311141, -1.5404928082191445,
    0.13633574007499308, 0.95831753567757938, -0.56946401809374125, -1.0405029014660729,
    0.21723017273461478, -0.49493745302908337, 1.133539786157665, 0.2884270536301336,
    0.22631042202565502, -1.0241522436258879, -1.6240345095592688, 1.4995390060106988,
    -0.12412473387893783, 0.28245676339777365, -1.7685482667053782, 1.0146984535956913,
    -0.003451663777336602, 0.20826817607643547, -1.7999452269399061, 0.63281509019494908,
    2.1669332773089076, 0.025392428479487448, 0.5634085824767493, -0.062604723560523462,
    -0.88114956281981016, 0.90071658769433982, 0.69006213437102282, -0.24984066038002836,
    -0.2860336559970334, 0.82721866916046005, 0.18497204510408763, -1.3418536365895448,
    -2.0353237889365006, 0.86416107436853118, -0.74640105643681276, 1.6611222023962577,
    1.2624882298514035, -0.71862396745500956, -0.83362029963355788, 0.47413151989398677,
    -0.76790372459153666, 0.50823652257157492, -0.27674138487216565, 2.2532513945572727,
    -0.58474847802906116, -0.34795222771991791, 0.3234660604226875, -0.59473812128995818,
    -2.0747238346841987, -0.49614558795393587, 1.7334391979666635, 0.10010561036229762,
    -0.15862668257911791, -0.1351056054266869, 0.31772960302012437, -1.4607203023172843,
    0.88810118398447313, 1.1047694142466082, -0.86362294973084286,
};

struct OracleCell {
    int p, q, bp, bq;
    const char* bic;   // JVM BIC2 (%.17g round-trip)
    const char* ll;    // JVM logLikelihood at the landing
    const char* params; // phi/bphi/theta/btheta blocks, raw JVM signs
};

// The 14 orders the module evaluates for the smoke request (three stages),
// every value from the pinned observer.
const OracleCell kSmokeOracle[] = {
    {3, 0, 0, 0, "2.8949657331676530", "-111.30339671086439", "0.26930077601580120;0.26297049098123000;0.30113994315045400///"},
    {3, 0, 0, 1, "2.6712698079115165", "-99.810595508836440", "0.38980110293262490;0.23886243550691225;0.32638511469605950///-0.70831221415747860"},
    {3, 0, 1, 0, "2.7620464996034038", "-103.57782821404977", "0.34307336488478546;0.23556422904042580;0.34755511208501233/0.45332851814419900//"},
    {3, 0, 1, 1, "2.7217382389819290", "-99.695615094360260", "0.37868622028640300;0.24466573284662907;0.31979933588574866/-0.096172421652132880//-0.79339044459052280"},
    {0, 0, 0, 1, "2.7349268044978117", "-109.08062177886260", "///-0.65839802709736330"},
    {0, 1, 0, 1, "2.6238117899141455", "-102.25992836974214", "//-0.54765791328693670/-0.76838437548453040"},
    {0, 2, 0, 1, "2.6327615779529640", "-100.42192426945482", "//-0.55330081061615000;-0.23242849654284045/-0.71374975343257920"},
    {1, 0, 0, 1, "2.6827122330490500", "-104.70429675984067", "0.33005424143830030///-0.76316874472915770"},
    {1, 1, 0, 1, "2.5972742632080350", "-98.949200707540260", "-0.42645902934118923//-0.91630058214319910/-0.68420984789072830"},
    {1, 2, 0, 1, "2.6492785585489638", "-98.897958660290500", "-0.49282592905447320//-0.99565576805332080;0.068568879482342680/-0.69212430954215640"},
    {2, 0, 0, 1, "2.7210612232882770", "-104.08635955087030", "0.36968874105188510;0.12544769321685004///-0.75479965663129200"},
    {2, 1, 0, 1, "2.6486459871513626", "-98.871706947290050", "-0.41308641941814960;-0.047506542617378760//-0.92272261328642490/-0.69649397290123430"},
    {2, 2, 0, 1, "2.6569401167353073", "-97.006493021125460", "0.45270368357159330;-0.47695522901404940//-0.052104443719578564;-0.80328991780049620/-0.74486926912329780"},
    {1, 1, 0, 0, "2.7615825632935990", "-107.97741546498946", "-0.51815123915022400//-0.99999999632538120/"}
};

// Parses the four JVM blocks and maps them to the Kalos blocks
// [ar, ma, sar, sma] with the AR blocks negated (Kalos stores 1 - phi(B)).
std::array<std::vector<double>, 4> jvm_blocks(const char* cell) {
    std::array<std::vector<double>, 4> jvm;
    const std::string s(cell);
    std::size_t pos = 0;
    for (int b = 0; b < 4; ++b) {
        const std::size_t next = s.find('/', pos);
        const std::string blk =
            s.substr(pos, next == std::string::npos ? std::string::npos
                                                    : next - pos);
        std::size_t v = 0;
        while (v < blk.size()) {
            const std::size_t semi = blk.find(';', v);
            if (semi != v) {
                jvm[b].push_back(std::strtod(
                    blk.substr(v, semi == std::string::npos
                                       ? std::string::npos
                                       : semi - v)
                        .c_str(),
                    nullptr));
            }
            if (semi == std::string::npos) break;
            v = semi + 1;
        }
        pos = next == std::string::npos ? s.size() : next + 1;
    }
    std::array<std::vector<double>, 4> out;
    out[0] = jvm[0];
    out[2] = jvm[1];
    for (double& v : out[0]) v = -v;
    for (double& v : out[2]) v = -v;
    out[1] = jvm[2];
    out[3] = jvm[3];
    return out;
}

kalos::InitialArmaIdentificationRequest pinned_request() {
    kalos::InitialArmaIdentificationRequest request;
    request.series.assign(std::begin(kSmokeSeries), std::end(kSmokeSeries));
    request.mean_correction = true;
    request.period = 12;
    request.d = 1;
    request.seasonal_d = 1;
    request.models_count = 5;   // the decoder call-site values (contract)
    request.max_p = 2;
    request.max_q = 2;
    request.max_bp = 1;
    request.max_bq = 1;
    request.balanced = false;
    request.mixed = true;
    request.accept_white_noise = false;
    request.eps = 1.0e-7;
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

template <class F> void aborts(F f) {
    bool threw = false;
    try {
        f();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

struct Boom : std::runtime_error {
    Boom() : std::runtime_error("boom") {}
};

} // namespace

int main() {
    // 1. Request validation: every pinned rejection is invalid_argument.
    rejects([] { kalos::InitialArmaIdentificationRequest().validate(); });
    {
        auto r = pinned_request();
        r.series.clear();
        rejects([&] { r.validate(); });
    }
    {
        auto r = pinned_request();
        r.series[3] = std::nan("");
        rejects([&] { r.validate(); });
    }
    {
        auto r = pinned_request();
        r.regressors = {std::vector<double>(r.series.size() - 1, 1.0)};
        rejects([&] { r.validate(); });
    }
    {
        auto r = pinned_request();
        r.period = 5;
        rejects([&] { r.validate(); });
    }
    {
        auto r = pinned_request();
        r.d = -1;
        rejects([&] { r.validate(); });
    }
    {
        auto r = pinned_request();
        r.models_count = 0;
        rejects([&] { r.validate(); });
    }
    {
        auto r = pinned_request();
        r.max_p = -1;
        rejects([&] { r.validate(); });
    }
    {
        auto r = pinned_request();
        r.eps = 0.0;
        rejects([&] { r.validate(); });
    }

    // 2. Pinned abort equivalences (runtime_error, never a silent fallback).
    aborts([] {
        auto r = pinned_request();          // 13 < d + bd * period + 1
        r.series.assign(13, 0.5);
        return kalos::identify_initial_arma(r);
    });
    aborts([] {
        auto r = pinned_request();          // zero-norm OLS response
        r.series.assign(96, 0.0);
        r.d = 0;
        r.seasonal_d = 0;
        return kalos::identify_initial_arma(r);
    });
    aborts([] {
        auto r = pinned_request();          // estimations slots unfillable
        r.models_count = 64;
        return kalos::identify_initial_arma(r);
    });

    // 3. Interrupt propagation.
    {
        const kalos::Interrupt boom = [] { throw Boom(); };
        bool threw = false;
        try {
            kalos::identify_initial_arma(pinned_request(), boom);
        } catch (const Boom&) {
            threw = true;
        }
        assert(threw);
    }

    // 4. The smoke replay.
    const kalos::InitialArmaIdentification smoke =
        kalos::identify_initial_arma(pinned_request());

    // 4a. Cursor: bit-identical to the JVM olsResiduals (one-column case).
    assert(smoke.residuals.size() == std::size(kSmokeCursor));
    for (std::size_t i = 0; i < smoke.residuals.size(); ++i) {
        assert(smoke.residuals[i] == kSmokeCursor[i]);
    }

    // 4b. Enumeration: the pinned three-stage order sequence, slot by slot.
    struct Stage { int stage, slot, p, q, bp, bq; };
    const Stage sequence[] = {
        {1, 0, 3, 0, 0, 0}, {1, 1, 3, 0, 0, 1}, {1, 2, 3, 0, 1, 0},
        {1, 3, 3, 0, 1, 1},
        {2, 0, 0, 0, 0, 1}, {2, 1, 0, 1, 0, 1}, {2, 2, 0, 2, 0, 1},
        {2, 3, 1, 0, 0, 1}, {2, 4, 1, 1, 0, 1}, {2, 5, 1, 2, 0, 1},
        {2, 6, 2, 0, 0, 1}, {2, 7, 2, 1, 0, 1}, {2, 8, 2, 2, 0, 1},
        {3, 0, 1, 1, 0, 0}, {3, 1, 1, 1, 0, 1},
    };
    assert(smoke.evaluations.size() == std::size(sequence));
    for (std::size_t i = 0; i < std::size(sequence); ++i) {
        const auto& e = smoke.evaluations[i];
        assert(e.stage == sequence[i].stage && e.slot == sequence[i].slot);
        assert(e.p == sequence[i].p && e.q == sequence[i].q);
        assert(e.bp == sequence[i].bp && e.bq == sequence[i].bq);
        assert(e.initial_source == "fitted");   // the HR initialization won
        assert(e.fit_status == "converged");
    }

    // 4c. Candidate cells: exact, tight band, or documented basin class.
    for (const auto& o : kSmokeOracle) {
        const kalos::ArmaCandidateEvaluation* first = nullptr;
        for (const auto& e : smoke.evaluations) {
            if (e.p == o.p && e.q == o.q && e.bp == o.bp && e.bq == o.bq
                && !first) {
                first = &e;
            }
        }
        assert(first != nullptr);
        const double want_bic = std::strtod(o.bic, nullptr);
        const double want_ll = std::strtod(o.ll, nullptr);
        const auto want = jvm_blocks(o.params);
        double dpar = 0.0;
        for (int b = 0; b < 4; ++b) {
            assert(first->coefficients[b].size() == want[b].size());
            for (std::size_t i = 0; i < want[b].size(); ++i) {
                dpar = std::max(dpar,
                    std::fabs(first->coefficients[b][i] - want[b][i]));
            }
        }
        const double dbic = std::fabs(first->bic - want_bic);
        const double dll = std::fabs(first->log_likelihood - want_ll);
        const bool tight = dbic <= kScalarBand
            && dll <= kLlBandFactor * double(std::size(kSmokeCursor))
            && dpar <= kParBand;
        const bool basin = first->fit_status == "converged"
            && dbic <= kBasinBound && dll <= kLlBasinBound
            && dpar <= kParBasinBound;
        assert(tight || basin || (dbic == 0.0 && dll == 0.0 && dpar == 0.0));
    }

    // 4d. Decision: the pinned selection and merged panel.
    assert(smoke.p == 1 && smoke.q == 1 && smoke.bp == 0 && smoke.bq == 1);
    const int panel[][4] = {{1, 1, 0, 1}, {0, 1, 0, 1}, {0, 2, 0, 1},
        {2, 1, 0, 1}, {1, 2, 0, 1}};
    const char* panel_bic[] = {"2.5972742632080350", "2.6238117899141455",
        "2.6327615779529640", "2.6486459871513626", "2.6492785585489638"};
    assert(smoke.preferred_models.size() == 5);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto& m = smoke.preferred_models[i];
        assert(m.p == panel[i][0] && m.q == panel[i][1]);
        assert(m.bp == panel[i][2] && m.bq == panel[i][3]);
        const double want = std::strtod(panel_bic[i], nullptr);
        const double dev = std::fabs(m.bic - want);
        assert(dev <= kScalarBand);  // exact except 1-2 ulps at (0,1,0,1)
    }

    // 4e. acceptWhiteNoise is stored, never read (pinned 3.9.0 bytecode):
    // either value, bit-identical results.
    {
        auto r = pinned_request();
        r.accept_white_noise = true;
        const kalos::InitialArmaIdentification other =
            kalos::identify_initial_arma(r);
        assert(other.p == smoke.p && other.q == smoke.q && other.bp == smoke.bp
            && other.bq == smoke.bq);
        assert(other.evaluations.size() == smoke.evaluations.size());
        for (std::size_t i = 0; i < other.evaluations.size(); ++i) {
            assert(other.evaluations[i].bic == smoke.evaluations[i].bic);
            assert(other.evaluations[i].log_likelihood
                == smoke.evaluations[i].log_likelihood);
        }
        assert(other.preferred_models.size() == smoke.preferred_models.size());
        for (std::size_t i = 0; i < other.preferred_models.size(); ++i) {
            assert(other.preferred_models[i].bic
                == smoke.preferred_models[i].bic);
        }
    }

    // 4f. The non-mixed route: every evaluated order stays inside the
    // pinned admissible set (mixed false drops p>0&&q>0 and bp>0&&bq>0
    // candidates at grid time).
    {
        auto r = pinned_request();
        r.mixed = false;
        const kalos::InitialArmaIdentification nomixed =
            kalos::identify_initial_arma(r);
        for (const auto& e : nomixed.evaluations) {
            assert(!(e.p > 0 && e.q > 0));
            assert(!(e.bp > 0 && e.bq > 0));
        }
    }

    return 0;
}
