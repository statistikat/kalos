#include "kalos/preparation.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

template<class F> void rejects(F f) {
    bool caught = false;
    try { f(); } catch (const std::exception&) { caught = true; }
    assert(caught);
}
int main() {
    using namespace kalos;
    for (const int frequency : {4, 12}) {
        TimeDomain original(-1, frequency, frequency, 6 * frequency);
        const auto window = estimation_window(original, original.start_ordinal()+2,
                                               original.start_ordinal()+2*frequency);
        assert(window.first == 2 && window.domain.length() == 2*frequency-1);
        assert(window.domain.at(0).first == 0 && window.domain.at(0).second == 2);
        rejects([&] { estimation_window(original, original.start_ordinal()-1); });
        rejects([&] { estimation_window(original, original.start_ordinal()+2, original.start_ordinal()+1); });
        std::vector<double> x(10*frequency);
        for (std::size_t i=0;i<x.size();++i) x[i]=static_cast<double>(i);
        x.front()=std::numeric_limits<double>::quiet_NaN();
        NamedRegressor supplied{"x",TimeDomain(-2,1,frequency,x.size()),x};
        const auto aligned=align_regressors(window.domain,{supplied},2,3);
        assert(aligned.observed_offset==2 && aligned.required.length()==2*frequency+4);
        const auto offset=aligned.required.start_ordinal()-supplied.domain.start_ordinal();
        assert(aligned.values.front()==offset && aligned.values.back()==offset+aligned.required.length()-1);
        auto bad=supplied;bad.values[static_cast<std::size_t>(offset)]=std::numeric_limits<double>::infinity();
        rejects([&] { align_regressors(window.domain,{bad},2,3); });
        rejects([&] { align_regressors(window.domain,{supplied,supplied},2,3); });
        rejects([&] { align_regressors(window.domain,{},-1,0); });
        rejects([&] { align_regressors(window.domain,{},0,std::numeric_limits<int>::max()); });
        for(int d=0;d<=2;++d)for(int sd=0;sd<=2;++sd) {
            if(!d && !sd)continue;
            auto design=prepare_regression(aligned,{{"x",2,true},{"drift",0,false}},d,sd,0,false,true,window.first);
            assert(design.regressors.names==std::vector<std::string>({"x","drift"}));
            assert(design.design.fixed==std::vector<std::uint8_t>({1,0}));
            assert(design.design.coefficients==std::vector<double>({2,0}));
            double factorial=1;for(int j=2;j<=d+sd;++j)factorial*=j;
            for(std::size_t i=0;i<static_cast<std::size_t>(window.domain.length());++i) {
                const double t=static_cast<double>(window.first+i+1);
                const double expected=std::pow(t,d+sd)/(factorial*std::pow(frequency,sd));
                assert(std::abs(design.design.values[i+window.domain.length()]-expected)<1e-12*std::max(1.0,expected));
            }
        }
        const auto mean=prepare_regression(aligned,{},0,0,0,true);
        assert(mean.generated==std::vector<std::string>({"mean"}));
        rejects([&] { prepare_regression(aligned,{},1,0,0,true); });
        rejects([&] { prepare_regression(aligned,{},0,0,0,false,true); });
        rejects([&] { prepare_regression(aligned,{{"absent",0,true}},0,0); });
        rejects([&] { prepare_regression(aligned,{},0,0,1,true); });
        rejects([&] { align_regressors(window.domain,{supplied},2,3,[]{throw std::runtime_error("cancel");}); });
        assert(align_regressors(window.domain,{supplied},2,3).values==aligned.values);
    }
    MaskedSeries terms{{0,-1,0,-3,-4},{1,0,1,0,0}};
    const auto a=likelihood_adjustment(terms,Transform::logarithm,2,10);
    assert(a.local_rows==std::vector<std::size_t>({3,4}));
    assert(a.source_rows==std::vector<std::size_t>({13,14}));
    assert(a.terms==std::vector<double>({0,0,0,-3,-4}) && a.log_jacobian==-7);
    assert(likelihood_adjustment(terms,Transform::none,2).local_rows==std::vector<std::size_t>({1,3,4}));
    assert(likelihood_adjustment({{0,0},{1,1}},Transform::logarithm,2).local_rows.empty());
}
