#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
struct ElasticAnchor {double source=0,target=0;};
namespace ElasticMap {
constexpr double maximumLength=30,minimumSpacing=.05;
inline bool valid(const std::vector<ElasticAnchor>& map,double length){
 if(!std::isfinite(length)||length<.5||length>maximumLength||map.size()<2||map.size()>16)return false;
 if(std::abs(map.front().source)>1.e-7||std::abs(map.front().target)>1.e-7||std::abs(map.back().source-length)>1.e-7||std::abs(map.back().target-length)>1.e-7)return false;
 for(size_t i=0;i<map.size();++i){if(!std::isfinite(map[i].source)||!std::isfinite(map[i].target))return false;if(i){const double input=map[i].source-map[i-1].source,output=map[i].target-map[i-1].target;if(input<minimumSpacing||output<minimumSpacing||output/input<.75-1.e-7||output/input>1.5+1.e-7)return false;}}
 return true;
}
inline double sourceAt(const std::vector<ElasticAnchor>& map,double target){
 size_t i=1;while(i+1<map.size()&&target>map[i].target)++i;
 const auto a=map[i-1],b=map[i];return a.source+(target-a.target)*(b.source-a.source)/(b.target-a.target);
}
inline double targetAt(const std::vector<ElasticAnchor>& map,double source){
 size_t i=1;while(i+1<map.size()&&source>map[i].source)++i;const auto a=map[i-1],b=map[i];return a.target+(source-a.source)*(b.target-a.target)/(b.source-a.source);
}
}
