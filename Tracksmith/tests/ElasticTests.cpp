#include "../source/ElasticRender.h"
#include <iostream>
#include <cstdlib>
static void require(bool condition,const char* message){if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int main(){
 constexpr double rate=48000,pi=3.14159265358979323846;ElasticRender::Audio input;
 for(auto& channel:input){channel.resize(96000);for(int n=0;n<96000;++n)channel[n]=static_cast<float>(.3*std::sin(2*pi*440*n/rate));}
 const std::vector<ElasticAnchor> identity{{0,0},{2,2}},map{{0,0},{1,1.2},{2,2}};
 require(ElasticRender::render(input,rate,identity)==input,"identity map is bit-identical");
 require(ElasticMap::valid(map,2)&&!ElasticMap::valid({{0,0},{1,1.8},{2,2}},2),"bounded segment ratios validated");
 const auto result=ElasticRender::render(input,rate,map);require(result[0].size()==input[0].size(),"exact frame count and pinned duration");
 int crossings=0;double energy=0;for(int n=24001;n<48000;++n){if(result[0][n-1]<=0&&result[0][n]>0)++crossings;energy+=result[0][n]*result[0][n];}
 require(std::abs(crossings*2-440)<6&&energy>100,"pitch retained while timing changes");
 double maxDifference=0,differenceEnergy=0,referenceEnergy=0;for(size_t n=0;n<result[0].size();++n){maxDifference=std::max(maxDifference,std::abs(static_cast<double>(result[0][n]-result[1][n])));differenceEnergy+=std::pow(result[0][n]-result[1][n],2);referenceEnergy+=std::pow(result[0][n],2);}std::cout<<"Stereo max delta "<<maxDifference<<" relative energy "<<differenceEnergy/referenceEnergy<<std::endl;
 for(size_t n=0;n<result[0].size();++n)require(std::isfinite(result[0][n])&&std::abs(result[0][n]-result[1][n])<1.e-5,"stereo coherence and finite output");
 auto stereo=input;for(auto& sample:stereo[1])sample*=.5f;const auto linked=ElasticRender::render(stereo,rate,map);double stereoError=0,stereoEnergy=0;for(size_t n=0;n<linked[0].size();++n){stereoError+=std::pow(linked[1][n]-.5*linked[0][n],2);stereoEnergy+=std::pow(linked[0][n],2);}require(stereoError/stereoEnergy<1.e-7,"linked stereo preserves channel level and phase relationship");
 int polls=0;bool cancelled=false;try{ElasticRender::render(input,rate,map,[&]{return ++polls>3;});}catch(const std::exception&){cancelled=true;}require(cancelled,"background renderer supports cancellation");
 auto burst=input;for(auto& channel:burst)for(int n=0;n<96000;++n){const double t=n/rate;channel[n]=static_cast<float>(.5*std::exp(-std::pow((t-1)/.015,2))*std::sin(2*pi*440*t));}
 const auto warped=ElasticRender::render(burst,rate,map);size_t peak=0;for(size_t n=0;n<warped[0].size();++n)if(std::abs(warped[0][n])>std::abs(warped[0][peak]))peak=n;
 require(std::abs(peak/rate-1.2)<.065,"note envelope follows the moved anchor within analysis-window tolerance");
 std::cout<<"PASS: Elastic identity, exact length, pitch preservation, stereo coherence, marker timing, ratio validation and cancellation.\n";
}
