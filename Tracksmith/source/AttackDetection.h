#pragma once
#include "ElasticRender.h"
namespace AttackDetection {
inline std::vector<ElasticAnchor> detect(const ElasticRender::Audio& audio,double rate,float sensitivity,const std::function<bool()>& cancelled){
 if(!std::isfinite(rate)||rate<8000||rate>192000||audio[0].size()!=audio[1].size()||!std::isfinite(sensitivity)||sensitivity<0||sensitivity>1)throw std::runtime_error("Invalid attack analysis request.");
 const double length=static_cast<double>(audio[0].size())/rate;if(length<.5||length>30+1/rate)throw std::runtime_error("Choose a 0.5–30 second timing region.");
 const int hop=std::max(1,static_cast<int>(rate*.005)),window=hop*2;
 std::vector<double> energy,flux;double smooth=0,maximum=0,total=0;
 for(size_t first=0;first<audio[0].size();first+=hop){if(cancelled())throw std::runtime_error("Attack analysis cancelled.");double power=0;const auto end=std::min(audio[0].size(),first+window);for(size_t n=first;n<end;++n)power+=(audio[0][n]*audio[0][n]+audio[1][n]*audio[1][n])*.5;const double rms=std::sqrt(power/std::max<size_t>(1,end-first)),onset=std::max(0.0,rms-smooth);energy.push_back(rms);flux.push_back(onset);smooth=.85*smooth+.15*rms;maximum=std::max(maximum,onset);total+=onset;}
 struct Peak{double time,score;};std::vector<Peak> peaks;
 const double threshold=std::max({1.e-5,maximum*(.4-.32*sensitivity),total/std::max<size_t>(1,flux.size())*2.5});
 for(size_t i=1;i+1<flux.size();++i)if(flux[i]>=threshold&&energy[i]>1.e-4&&flux[i]>=flux[i-1]&&flux[i]>flux[i+1]){size_t first=i;while(first>0&&i-first<8&&energy[first-1]>.2*energy[i]&&flux[first-1]>.15*flux[i])--first;const double time=first*hop/rate;if(time>=.05&&time<=length-.05)peaks.push_back({time,flux[i]});}
 std::sort(peaks.begin(),peaks.end(),[](auto a,auto b){return a.score>b.score;});std::vector<ElasticAnchor> result{{0,0},{length,length}};
 const double spacing=.08+.08*(1-sensitivity);
 for(const auto peak:peaks){if(result.size()>=16)break;if(std::any_of(result.begin(),result.end(),[&](auto p){return std::abs(p.source-peak.time)<spacing;}))continue;result.push_back({peak.time,peak.time});}
 std::sort(result.begin(),result.end(),[](auto a,auto b){return a.source<b.source;});return result;
}
}
