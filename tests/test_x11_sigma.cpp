#include "kalos/x11_sigma.hpp"
#include "kalos/parallel.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void threads(const char* value) {
#ifdef _WIN32
    assert(_putenv_s("PERSEPHONE_NUM_THREADS",value)==0);
#else
    assert(setenv("PERSEPHONE_NUM_THREADS",value,1)==0);
#endif
}
template<class F> void invalid(F run) {
    bool caught=false;try{run();}catch(const std::invalid_argument&){caught=true;}assert(caught);
}
kalos::MaskedSeries series(std::vector<double> values) {
    kalos::MaskedSeries y;
    y.missing.assign(values.size(),0);
    y.values=std::move(values);
    return y;
}
bool equal_nan(const std::vector<double>& left,const std::vector<double>& right) {
    if(left.size()!=right.size()) return false;
    for(std::size_t i=0;i<left.size();++i) {
        if(std::isnan(left[i])!=std::isnan(right[i])) return false;
        if(!std::isnan(left[i])&&left[i]!=right[i]) return false;
    }
    return true;
}
bool equal_dispersion(const std::vector<kalos::X11SigmaWindow>& windows,
        const std::vector<double>& dispersions) {
    std::vector<double> actual;
    for(const auto& window:windows) actual.push_back(window.dispersion);
    return equal_nan(actual,dispersions);
}
// Pinned JDemetra+ 3.9.0 reference cases from
// validation/fixtures/native/x11-sigma (legacy policy; weights and dispersion
// windows after the final pass, captured through 17-significant-digit
// round-trip exact CSV files; see docs/contracts/x11-sigma-contract.md).
struct PinnedCase {
  const char* id;
  int frequency, n, start_period, backcasts, forecasts, passes;
  bool exclude, multiplicative;
  double lower_sigma, upper_sigma;
  std::vector<double> input;
  std::vector<std::uint8_t> missing;
  std::vector<double> weights, dispersions;
};
const PinnedCase pinned[] = {
  {"spike-4-40-1-0-0-in-add-1.5-2.5", 4, 40, 1, 0, 0, 2, false, false, 1.5, 2.5,
      {1.8778737845771547, 2.0202896003803525, 0.31763771869525148, 2.5919983233639732, 1.6583461295480248, -4.8284496223846496, -4.4262020855387609, 0.36512330274065818, 1.3243995446521029, 2.5943558052245121, -0.71255246769204383, 1.5904481823701533, 3.7118024966972802, -1.3018171969688748, -4.0971349675947453, -3.246170485711608, -1.96474400335341, 2.29737870839047, 1000000, 0.27192081749816266, 2.878333095413768, 1.1181101210087612, -0.62532355683151031, -3.3279715363476496, -5.6900826471636146, -0.65145727828376332, 3.2209401541643006, 0.36812141331018, 1.3076356295860065, 4.1731112404221866, 2.1411192142171354, 0.028380241314725163, -6.1822227917456543, -4.4301317731861785, 0.63118628201552895, 0.43170608759701845, -8, 2.3938287255351032, 1.799736740248022, 3.112322373021045},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 1, 1, 1, 0.60801247704283534, 0.76562980359098143, 1, 1, 1, 1, 1, 1, 1, 0.93558920138316948, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0.51207925293452283, 1, 1, 1, 1, 1, 1, 1, 0.67342250460920283, 1, 1, 1, 0.13634853427851659, 1, 1, 1},
      {2.552051513975004, 2.552051513975004, 2.552051513975004, 2.6189636195411179, 2.5939527525646815, 2.7147862126016884, 2.8623287198763752, 3.3845937592825561, 3.3845937592825561, 3.3845937592825561}},
  {"revive-4-28-1-0-0-in-add-1.5-2.5", 4, 28, 1, 0, 0, 2, false, false, 1.5, 2.5,
      {0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {0, 0, 0, 0, 0, 0, 0}},
  {"zeroish-4-32-3-0-0-in-add-1.5-2.5", 4, 32, 3, 0, 0, 2, false, false, 1.5, 2.5,
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0050000000000000001, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
      {0, 0, 0, 0, 0, 0, 0, 0, 0}},
  {"both-4-40-1-2-5-ex-add-1.5-2.5", 4, 40, 1, 2, 5, 2, true, false, 1.5, 2.5,
      {1.8778737845771547, 2.0202896003803525, 0.31763771869525148, 2.5919983233639732, 1.6583461295480248, -4.8284496223846496, -4.4262020855387609, 0.36512330274065818, 1.3243995446521029, 2.5943558052245121, -0.71255246769204383, 1.5904481823701533, 3.7118024966972802, -1.3018171969688748, -4.0971349675947453, -3.246170485711608, -1.96474400335341, 2.29737870839047, 3.3105393712187077, 0.27192081749816266, 2.878333095413768, 1.1181101210087612, -0.62532355683151031, -3.3279715363476496, -5.6900826471636146, -0.65145727828376332, 3.2209401541643006, 0.36812141331018, 1.3076356295860065, 4.1731112404221866, 2.1411192142171354, 0.028380241314725163, -6.1822227917456543, -4.4301317731861785, 0.63118628201552895, 0.43170608759701845, 1.1085741993235898, 2.3938287255351032, 1.799736740248022, 3.112322373021045},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 1, 1, 1, 0.63901741122768507, 0.79405178478236793, 1, 1, 1, 1, 1, 1, 1, 0.95846014516473432, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.61346405522069991, 1, 1, 1, 1, 1, 1, 1, 0.4502959379551788, 1, 1, 1, 1, 1, 1, 1},
      {2.5945700145265542, 2.5945700145265542, 2.5945700145265542, 2.6578196825359273, 2.6344154730851495, 2.7476434480378029, 3.0161538469012736, 3.0161538469012736, 3.0161538469012736, 3.0161538469012736}},
  {"mask-4-33-2-0-0-in-add-1.5-2.5", 4, 33, 2, 0, 0, 2, false, false, 1.5, 2.5,
      {1.8778737845771547, 2.0202896003803525, 0.31763771869525148, 2.5919983233639732, 1.6583461295480248, -4.8284496223846496, -4.4262020855387609, 0.36512330274065818, 1.3243995446521029, 2.5943558052245121, -0.71255246769204383, 1.5904481823701533, 3.7118024966972802, -1.3018171969688748, -4.0971349675947453, -3.246170485711608, -1.96474400335341, 2.29737870839047, 3.3105393712187077, 0.27192081749816266, 2.878333095413768, 1.1181101210087612, -0.62532355683151031, -3.3279715363476496, -5.6900826471636146, -0.65145727828376332, 3.2209401541643006, 0.36812141331018, 1.3076356295860065, 4.1731112404221866, 2.1411192142171354, 0.028380241314725163, -6.1822227917456543},
      {1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
      {1, 1, 1, 1, 1, 0.67157628494956856, 0.82389825021973961, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0.61690409454801332, 1, 1, 1, 1, 1, 1, 1, 0.45403350924413344},
      {2.6407717109769995, 2.6407717109769995, 2.6407717109769995, 2.7473769904560887, 2.7579516369117623, 2.7705559941427711, 3.0216637563119031, 3.0216637563119031, 3.0216637563119031}},
  {"mul-12-108-1-0-0-in-mul-1.5-2.5", 12, 108, 1, 0, 0, 2, false, true, 1.5, 2.5,
      {1.242053110654681, 1.1511580316390027, 1.242605768420411, 1.0049356431422656, 0.51352206348474994, 0.61606486178826136, 1.1866631802801293, 1.3332010388672995, 1.1408879032834365, 1.152588740737754, 1.0357555498454747, 0.5854212083727699, 0.56029229649840584, 1.1385760586696965, 1.4136182938590525, 1.1686909782447512, 1.0609264459535572, 1.0289667749619036, 0.67111308540256953, 0.53785123294521742, 1.0661109772422166, 1.4685269240305083, 1.2289387419394298, 0.98420488041471477, 0.98487423321811784, 0.75391609045613128, 0.55379392195987087, 0.98432549409552861, 1.488036899187253, 1.3097479234544769, 0.93613064537744206, 0.91096222445163322, 0.81772259938528546, 0.60580887372008907, 0.91006774791081035, 1.4690469531541017, 1.3952647460002003, 0.92489660150402198, 0.82045610527102908, 0.85009517890848119, 0.68466495215671097, 0.85873764302491018, 1.4158405159308964, 1.468712380652293, 0.95160735924936224, 0.72978043025818351, 0.84465731968483471, 0.77598553798485603, 0.84132640990570584, 1.3392621872405972, 1.5156158314021142, 1.0100672523300542, 0.65540215072261732, 0.80231915237359386, 0.86301173639372131, 0.86230358747729596, 1.2546330471798721, 1.5265843742456453, 1.0879715492632893, 0.61066566131822264, 0.73110254338813918, 0.92982349438245537, 0.91875758498400084, 1.1788055164391964, 1.4991167186086873, 1.1692719319623988, 0.60322805181169525, 0.64459918987672504, 0.96439947478140364, 1.000955021973265, 1.1269238716285699, 1.4380819086373708, 1.2372615564619045, 0.63358755174982007, 0.5593580940447862, 0.96092752768091083, 1.0942113319180955, 1.1095130992917932, 1.3547835269919324, 1.2777866950350751, 0.69498802567931983, 0.49170458826443403, 0.92092164779580366, 1.1817130680344905, 1.1304556599628017, 1.2647870769088492, 1.2819676278899927, 0.77471787647274415, 0.45460234793475318, 0.85293071980729418, 1.2477494166522396, 1.1862450623458984, 1.1849281562709943, 1.2479057831295008, 0.85655368558244305, 0.45516156608798725, 0.77089503580251661, 1.2807317669106328, 1.2666597232817067, 1.130076503002805, 1.1810489838241285, 0.92387888132054907, 0.49327226056359968, 0.69146648991945536, 1.2754209246957917, 1.3567275863010815, 1.1102779167508146, 1.0931445954690118},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 1, 1, 0.79871274800636904, 1, 1, 1, 1, 1, 1, 1, 0.96227535840732425, 1, 1, 1, 1, 1, 1, 0.88379554981065711, 1, 0.86149016586715266, 1, 1, 1, 1, 0.93954955538266238, 1, 0.79326081869748577, 1, 1, 1, 1, 1, 1, 0.85967154501669374, 1, 1, 1, 1, 1, 1, 1, 0.86085230074111374, 1, 1, 1, 1, 1, 1, 0.69422543909202372, 1, 1, 1, 1, 1, 1, 0.65581173177194918, 1, 1, 1, 1, 1, 1, 0.75604228223618108, 1, 1, 1, 1, 1, 1, 0.96930327697592489, 1, 1, 0.95334841825551209, 1, 1, 1, 1, 1, 1, 0.71588484452002454, 1, 1, 1, 1, 1, 1, 0.58565610795043144, 1, 1, 1, 1, 1, 1, 0.58761896175428119, 1, 1, 1, 1, 1, 1, 0.72138736892451949, 1, 1, 1, 1, 1},
      {0.28594697100397204, 0.28594697100397204, 0.28594697100397204, 0.28594883845074709, 0.28553721077056993, 0.28619771771111358, 0.2849005627099338, 0.2849005627099338, 0.2849005627099338}},
};

} // namespace
int main() {
    using namespace kalos;
    using Policy=X11SigmaPolicy;using State=X11SigmaWeightState;using Kind=X11SigmaWindowKind;
    // 1. Every pinned case reproduces bit-exactly under the legacy policy.
    for(const PinnedCase& pin:pinned) {
        const MaskedSeries y{pin.input,pin.missing};
        const auto result=x11_sigma_weights(y,pin.frequency,pin.start_period,Policy::legacy,
            pin.backcasts,pin.forecasts,pin.exclude,pin.lower_sigma,pin.upper_sigma,
            pin.multiplicative);
        assert(result.status=="available");
        assert(result.passes==pin.passes);
        assert(result.weights==pin.weights); // exact double equality
        assert(result.first_pass_weights.size()==pin.weights.size());
        assert(equal_dispersion(result.windows,pin.dispersions));
        assert(result.first_pass_windows.size()>=1);
        assert(!result.windows.empty());
        // The second pass only ever excludes rows whose first-pass weight is
        // exactly zero, so the recorded exclusion set must match.
        std::vector<std::size_t> zeros;
        for(std::size_t row=0;row<result.first_pass_weights.size();++row)
            if(result.first_pass_weights[row]==0) zeros.push_back(row);
        assert(result.first_pass_zeroed==zeros);
        const auto again=x11_sigma_weights(y,pin.frequency,pin.start_period,Policy::legacy,
            pin.backcasts,pin.forecasts,pin.exclude,pin.lower_sigma,pin.upper_sigma,
            pin.multiplicative);
        assert(again.weights==result.weights && again.windows.size()==result.windows.size());
        const auto accurate=x11_sigma_weights(y,pin.frequency,pin.start_period,
            Policy::accurate,pin.backcasts,pin.forecasts,pin.exclude,pin.lower_sigma,
            pin.upper_sigma,pin.multiplicative);
        assert(accurate.status=="available");
        for(const double value:accurate.weights) assert(value>=0 && value<=1);
        // The reserved default decision only separates the two rules on the
        // tiny-dispersion pinned cases; everywhere else they must agree.
        const bool tiny_family=std::string(pin.id).rfind("revive",0)==0||
            std::string(pin.id).rfind("zeroish",0)==0;
        assert((accurate.weights!=result.weights)==tiny_family);
    }
    {   // 2. Exact single-period geometry: one global window, all full weights.
        auto y=series(std::vector<double>(12,4));
        const auto result=x11_sigma_weights(y,12,1,Policy::legacy);
        assert(result.passes==1 && result.status=="available");
        assert(result.weights.size()==12 && result.windows.size()==1);
        assert(result.windows[0].dispersion==4 && result.windows[0].length==12);
        assert(result.windows[0].kind==Kind::global_single);
        for(const double value:result.weights) assert(value==1);
        for(const auto state:result.states) assert(state==State::full);
    }
    {   // 3. Zero-dispersion years separate the two reserved rules by state.
        auto y=series(std::vector<double>(8,0));
        const auto legacy=x11_sigma_weights(y,4,1,Policy::legacy);
        const auto accurate=x11_sigma_weights(y,4,1,Policy::accurate);
        for(std::size_t row=0;row<8;++row) {
            assert(legacy.weights[row]==1 && accurate.weights[row]==1);
            assert(legacy.states[row]==State::full_below_dispersion_cutoff);
            assert(accurate.states[row]==State::full); // zero deviation limit
        }
        auto spike=y; spike.values[5]=1e-6; // dispersion stays below 1e-5
        const auto legacy_spike=x11_sigma_weights(spike,4,1,Policy::legacy);
        const auto accurate_spike=x11_sigma_weights(spike,4,1,Policy::accurate);
        assert(legacy_spike.weights[5]==1);            // legacy dispersion cutoff
        assert(accurate_spike.weights[5]==0);          // scale limit of the ramp
        assert(accurate_spike.states[5]==State::zero);
    }
    {   // 4. Ramp, zero and second-pass exclusion inside a global single window.
        std::vector<double> values(24,0);
        for(std::size_t row=0;row<24;++row) values[row]=(row%2)?2:-2;
        values[9]=7;   // between the thresholds -> ramp
        values[21]=12; // beyond the upper threshold -> zero and excluded
        double sum=0; for(const double value:values) sum+=value*value;
        const double first=std::sqrt(sum/24.0);
        assert(first>2.8 && first<3.5); // keeps row 9 on the ramp, row 21 beyond
        const auto result=x11_sigma_weights(series(values),12,1,Policy::legacy);
        assert(result.passes==2);
        assert(result.first_pass_zeroed==std::vector<std::size_t>({21}));
        auto expected=[&](double dispersion,std::size_t row){
            const double deviation=std::abs(values[row]);
            const double lower=dispersion*1.5, upper=dispersion*2.5;
            if(deviation-upper>1e-15) return 0.0;
            if(deviation-lower>1e-15) return (upper-deviation)/(upper-lower);
            return 1.0;
        };
        for(std::size_t row=0;row<24;++row)
            assert(result.first_pass_weights[row]==expected(first,row));
        assert(result.first_pass_weights[9]>0 && result.first_pass_weights[9]<1);
        assert(result.states[21]==State::zero && result.first_pass_weights[21]==0);
        // Pass two drops row 21 only: the window keeps 24 rows but only 23
        // usable ones, and the loop below pins every final weight formula.
        const double second=std::sqrt((sum-144)/23.0);
        assert(equal_dispersion(result.windows,{second}));
        for(std::size_t row=0;row<24;++row) {
            const double deviation=std::abs(values[row]);
            const double lower=second*1.5, upper=second*2.5;
            double weight;
            if(deviation-upper>1e-15) weight=0;
            else if(deviation-lower>1e-15) weight=(upper-deviation)/(upper-lower);
            else weight=1;
            assert(result.weights[row]==weight);
        }
        assert(result.weights[21]==0); // row 21 stays beyond the tighter limits
    }
    {   // 5. Five-year window geometry with a boundary reuse and a tail window.
        std::vector<double> values;
        for(int row=0;row<61;++row) // five complete years plus a partial year
            values.push_back(((row%3)==0)?3.0:((row%3)==1?-2.0:0.5));
        double central=0,all=0;
        for(std::size_t row=0;row<60;++row) central+=values[row]*values[row];
        for(const double value:values) all+=value*value;
        const double central_dispersion=std::sqrt(central/60.0);
        const double tail_dispersion=std::sqrt(all/61.0);
        const auto result=x11_sigma_weights(series(values),12,1,Policy::legacy);
        assert(result.passes==1);
        assert(result.windows.size()==6);
        assert(result.windows[2].dispersion==central_dispersion);
        assert(result.windows[2].start==0 && result.windows[2].length==60);
        assert(result.windows[2].kind==Kind::central);
        for(std::size_t year=0;year<2;++year) {
            assert(result.windows[year].dispersion==central_dispersion);
            assert(result.windows[year].kind==Kind::boundary_reuse);
        }
        for(std::size_t year=3;year<6;++year) {
            assert(result.windows[year].dispersion==tail_dispersion);
            assert(result.windows[year].kind==Kind::trailing_extension);
            assert(result.windows[year].start==0 && result.windows[year].length==61);
        }
        for(const double value:result.weights) assert(value==1);
    }
    {   // 6. Missing inputs keep weight one and never disturb the windows.
        auto y=series(std::vector<double>(33,2));
        y.missing[7]=1;
        const auto result=x11_sigma_weights(y,12,2,Policy::legacy);
        assert(result.weights[7]==1 && result.states[7]==State::missing_input);
        for(const double value:result.weights) assert(value==1); // all deviations 2
        // Masked rows drop out of the dispersion denominator.
        auto dense=y; dense.missing[7]=0; dense.values[7]=0;
        const auto with_row=x11_sigma_weights(dense,12,2,Policy::legacy);
        assert(with_row.windows.size()==result.windows.size());
        assert(with_row.windows[0].usable==result.windows[0].usable+1);
        assert(with_row.windows[0].dispersion<result.windows[0].dispersion);
    }
    {   // 7. Projection spans follow the pinned drop-then-trim geometry.
        std::vector<double> values;
        for(int row=0;row<60;++row) values.push_back(std::sin(static_cast<double>(row)*.7));
        values[50]=6; // flagged once projections are inside the span, not after
        const auto kept=x11_sigma_weights(series(values),12,1,Policy::legacy);
        const auto dropped=x11_sigma_weights(series(values),12,1,Policy::legacy,
            12,12,true);
        assert(kept.passes==2 && kept.weights[50]==0);
        assert(kept.windows.size()==5 && kept.windows[0].start==0);
        assert(dropped.windows.size()==1); // three complete years -> one window
        assert(dropped.windows[0].start==12 && dropped.windows[0].length==36);
        assert(dropped.windows[0].kind==Kind::global_single);
        bool dropped_flagged=false;
        for(const double value:dropped.weights) dropped_flagged|=value<1;
        // Exclusion trims the dispersion windows only; the pinned detection
        // still flags the full series, so row 50 loses weight even when it
        // sits in the excluded projections.
        assert(dropped.weights[50]==0);
        const double kept_dispersion=std::sqrt(
            std::inner_product(values.begin(),values.end(),values.begin(),0.0)/60.0);
        assert(kept.first_pass_windows[0].dispersion==kept_dispersion);
        assert(dropped.first_pass_windows[0].dispersion!=kept.first_pass_windows[0].dispersion);
        assert(dropped_flagged);
        // Horizons may trim the span to empty but never past it.
        const auto empty=x11_sigma_weights(series(values),12,1,Policy::legacy,30,30,true);
        assert(empty.status=="no_effective_observations" && empty.passes==1);
        for(const double value:empty.weights) assert(value==1);
        invalid([&]{x11_sigma_weights(series(values),12,1,Policy::legacy,30,31,true);});
    }
    {   // 8. Input validation mirrors the pinned contract.
        auto y=series(std::vector<double>(12,1));
        invalid([&]{x11_sigma_weights(y,6,1,Policy::legacy);});       // frequency
        invalid([&]{x11_sigma_weights(y,12,0,Policy::legacy);});      // start period
        invalid([&]{x11_sigma_weights(y,12,13,Policy::legacy);});
        invalid([&]{x11_sigma_weights(y,12,1,Policy::legacy,0,0,false,0.5,2.5);});
        invalid([&]{x11_sigma_weights(y,12,1,Policy::legacy,0,0,false,2.5,2.5);});
        invalid([&]{x11_sigma_weights(y,12,1,Policy::legacy,0,0,false,3.0,2.5);});
        invalid([&]{x11_sigma_weights(y,12,1,Policy::legacy,-1,0);});
        invalid([&]{x11_sigma_weights(y,12,1,Policy::legacy,0,-1);});
        invalid([&]{x11_sigma_weights(series({1,2,3}),12,1,Policy::legacy);}); // < one period
        MaskedSeries broken{{1,2},{1,0}}; // storage for masked rows must be finite
        broken.values[0]=std::nan("");
        invalid([&]{x11_sigma_weights(broken,12,1,Policy::legacy);});
    }
    {   // 9. Interruption is honored before and during the passes.
        auto y=series(std::vector<double>(19*12,3));
        y.values[100]=90; // triggers the second pass
        int calls=0;
        const Interrupt stop=[&]{++calls;throw std::runtime_error("stop");};
        bool thrown=false;
        try { x11_sigma_weights(y,12,1,Policy::legacy,0,0,false,1.5,2.5,false,stop); }
        catch(const std::runtime_error&) { thrown=true; }
        assert(thrown && calls>=1);
        int counted=0;
        const Interrupt count=[&]{++counted;};
        const auto result=x11_sigma_weights(y,12,1,Policy::legacy,0,0,false,1.5,2.5,
            false,count);
        assert(result.passes==2 && counted>=1);
    }
    {   // 10. Serial and OpenMP runs agree exactly and the caller can reuse buffers.
        std::vector<double> values;
        for(int row=0;row<230;++row)
            values.push_back(3*std::sin(row*.7)+2*std::cos(row*1.4142135623730951));
        values[9]=1e3; values[150]=-8; values[201]=7;
        const MaskedSeries y{values,std::vector<std::uint8_t>(values.size(),0)};
        threads("1");
        const auto serial=x11_sigma_weights(y,12,3,Policy::legacy);
        threads("8");
        const auto parallel=x11_sigma_weights(y,12,3,Policy::legacy);
        threads("1");
        assert(serial.weights==parallel.weights);
        assert(serial.first_pass_weights==parallel.first_pass_weights);
        assert(serial.windows.size()==parallel.windows.size());
        for(std::size_t row=0;row<serial.windows.size();++row)
            assert(serial.windows[row].dispersion==parallel.windows[row].dispersion);
        assert(serial.states==parallel.states);
        assert(!in_parallel_region());
    }
    std::puts("test_x11_sigma ok");
    return 0;
}
