#pragma once
#include "ElasticMap.h"
#include "../ThirdParty/SignalsmithStretch/signalsmith-stretch.h"
#include <array>
#include <functional>
#include <stdexcept>
namespace ElasticRender {
using Audio=std::array<std::vector<float>,2>;
struct Input {
 const Audio& data;int offset=0;
 struct Channel {const std::vector<float>& data;int offset;float operator[](int index)const{const auto position=offset+index;return position>=0&&position<static_cast<int>(data.size())?data[static_cast<size_t>(position)]:0;}};
 Channel operator[](int c)const{return {data[static_cast<size_t>(c)],offset};}
};
inline Audio render(const Audio& input,double rate,const std::vector<ElasticAnchor>& map,const std::function<bool()>& cancelled=[] {return false;}){
 const double length=map.empty()?0:map.back().source;
 if(!std::isfinite(rate)||rate<8000||rate>192000||input[0].size()!=input[1].size()||std::abs(static_cast<double>(input[0].size())/rate-length)>1.1/rate||!ElasticMap::valid(map,length))throw std::runtime_error("Invalid Time Warp map or source.");
 Audio output;for(auto& c:output)c.resize(input[0].size());
 bool identity=true;for(const auto p:map)if(std::abs(p.source-p.target)>1.e-8)identity=false;if(identity)return input;
 signalsmith::stretch::SignalsmithStretch<float> stretch;stretch.presetDefault(2,rate);stretch.setTransposeFactor(1);
 const double initialRate=(map[1].source-map[0].source)/(map[1].target-map[0].target);
 int inputIndex=stretch.outputSeekLength(static_cast<float>(initialRate));stretch.outputSeek(Input{input,0},inputIndex);
 const int frames=static_cast<int>(output[0].size()),processFrames=std::max(0,frames-stretch.intervalSamples());
 for(int offset=0;offset<processFrames;){
  if(cancelled())throw std::runtime_error("Time Warp render cancelled.");
  const int count=std::min(256,processFrames-offset);
  const auto processingTime=static_cast<double>(offset+count+stretch.outputLatency())/rate;
  const int nextInput=static_cast<int>(std::llround(ElasticMap::sourceAt(map,processingTime)*rate))+stretch.inputLatency();
  if(nextInput<inputIndex)throw std::runtime_error("Elastic map moves backwards at processing latency.");
  float* target[]={output[0].data()+offset,output[1].data()+offset};stretch.process(Input{input,inputIndex},nextInput-inputIndex,target,count);inputIndex=nextInput;offset+=count;
 }
 float* tail[]={output[0].data()+processFrames,output[1].data()+processFrames};
 const auto a=map[map.size()-2],b=map.back();stretch.flush(tail,frames-processFrames,static_cast<float>((b.source-a.source)/(b.target-a.target)));
 for(auto& channel:output)for(const auto sample:channel)if(!std::isfinite(sample))throw std::runtime_error("Time Warp returned an invalid sample.");
 // Avoid creating a stereo difference from exact dual-mono source material.
 if(input[0]==input[1])output[1]=output[0];
 if(cancelled())throw std::runtime_error("Time Warp render cancelled.");return output;
}
}
