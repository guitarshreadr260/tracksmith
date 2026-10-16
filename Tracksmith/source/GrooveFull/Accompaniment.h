#pragma once
#include "MidiGroove.h"
#include "GrooveAnalysis.h"
#include <algorithm>
#include <functional>
namespace TracksmithGrooveFull {
// Offline, audio-conditioned retrieval and variation. No neural weights or cloud service.
struct Accompaniment {
 struct Bar { std::array<std::array<float,16>,9> hits{};juce::String source;bool fill=false; };
 MidiGroove groove;int scanned=0,accepted=0;juce::String summary;
 static Accompaniment generate(const juce::AudioBuffer<float>& audio,double rate,double bpm,const juce::File& folder,const DrumKit& kit,const std::function<bool()>& cancelled){
  Accompaniment out;out.groove.bpm=bpm;out.groove.division=4;out.groove.numerator=4;out.groove.denominator=4;
  const double seconds=audio.getNumSamples()/rate;out.groove.bars=juce::jlimit(1,16,(int)std::ceil(seconds*bpm/240));
  std::vector<Bar> bank;std::vector<juce::File> files;
  if(folder.isDirectory())for(const auto& entry:juce::RangedDirectoryIterator(folder,true,"*",juce::File::findFiles)){if(cancelled())return out;if(entry.getFile().hasFileExtension("mid;midi")){files.push_back(entry.getFile());if(files.size()>=2000)break;}}
  std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.getFullPathName()<b.getFullPathName();});
  for(const auto& file:files){if(cancelled())return out;++out.scanned;const auto parsed=MidiGroove::read(file,kit,bpm,MidiGroove::libraryOptions(file));if(!parsed.valid||parsed.numerator!=4||parsed.denominator!=4)continue;++out.accepted;
   for(int bar=0;bar<parsed.bars;++bar){Bar candidate;candidate.source=parsed.name;candidate.fill=parsed.name.containsIgnoreCase("fill");for(int d=0;d<9;++d)for(int n=0;n<parsed.division*4;++n){int dest=juce::jlimit(0,15,(int)std::round(n*4.0/parsed.division));candidate.hits[d][dest]=std::max(candidate.hits[d][dest],parsed.pattern[d][bar*parsed.division*4+n]);}bank.push_back(std::move(candidate));}
  }
  if(bank.empty()){out.summary="No compatible 4/4 grooves in the selected library. Load a MIDI groove folder first.";return out;}
  const int steps=out.groove.bars*16;std::vector<double> energy(steps),bass(steps),colour(steps);double peak=0;std::array<double,2> low{},last{};const double alpha=1-std::exp(-2*juce::MathConstants<double>::pi*180/rate);
  for(int step=0;step<steps;++step){if(cancelled())return out;const int a=std::min(audio.getNumSamples(),(int)(step*rate*60/bpm/4)),b=std::min(audio.getNumSamples(),(int)((step+1)*rate*60/bpm/4));double total=0,lo=0,hi=0;
   for(int n=a;n<b;++n)for(int c=0;c<audio.getNumChannels();++c){const double v=audio.getSample(c,n);low[c]+=alpha*(v-low[c]);total+=v*v;lo+=low[c]*low[c];hi+=(v-last[c])*(v-last[c]);last[c]=v;}
   energy[step]=std::sqrt(total/std::max(1,(b-a)*audio.getNumChannels()));bass[step]=std::sqrt(lo/std::max(1,(b-a)*audio.getNumChannels()));colour[step]=std::sqrt(hi/std::max(1,(b-a)*audio.getNumChannels()));peak=std::max(peak,energy[step]);
  }
  if(peak<.0001){out.summary="Audio is silent or too quiet. No accompaniment generated.";return out;}
  std::array<std::array<double,16>,9> probability{};for(const auto& b:bank)for(int d=0;d<9;++d)for(int n=0;n<16;++n)probability[d][n]+=(b.hits[d][n]>0?1.:0.)/bank.size();
  uint32_t seed=2166136261u;for(auto e:energy)seed=(seed^(uint32_t)(e*1000000))*16777619u;auto random=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return (seed&0xffffff)/16777216.;};
  juce::StringArray used;int count=0;
  for(int bar=0;bar<out.groove.bars;++bar){if(cancelled())return out;double mean=0,previous=0;for(int n=0;n<16;++n){mean+=energy[bar*16+n]/16;if(bar)previous+=energy[(bar-1)*16+n]/16;}if(mean<peak*.018)continue;
   const double activity=juce::jlimit(0.,1.,mean/peak);const bool wantFill=(bar%4==3)||(bar>0&&std::abs(mean-previous)>peak*.3);
   std::vector<std::pair<double,int>> ranks;for(int k=0;k<(int)bank.size();++k){const auto& b=bank[k];double score=0;int hits=0;for(int n=0;n<16;++n){double accent=energy[bar*16+n]/peak,lowAccent=bass[bar*16+n]/peak;score+=b.hits[0][n]*(.65*accent+.35*lowAccent);score+=b.hits[1][n]*accent*.25;for(int d=0;d<9;++d)hits+=b.hits[d][n]>0;}score/=std::sqrt(std::max(1,hits));score-=std::abs(hits-(8+activity*24))*.012;score+=(b.fill==wantFill?.12:-.15);ranks.emplace_back(score,k);}
   const int top=std::min(6,(int)ranks.size());std::partial_sort(ranks.begin(),ranks.begin()+top,ranks.end(),[](auto a,auto b){return a.first>b.first;});const auto& chosen=bank[ranks[(int)(random()*top)].second];used.addIfNotAlreadyThere(chosen.source);
   for(int d=0;d<9;++d)for(int n=0;n<16;++n){const double start=(bar*16+n)*60/bpm/4;if(start>=seconds)continue;float v=chosen.hits[d][n];const double accent=energy[bar*16+n]/peak; // Preserve the drummer's pattern; adapt dynamics and sparse ornament probabilities.
    if(v==0&&d==0&&accent>.7&&probability[d][n]>.3&&random()<.25)v=.65f;
    if(v>0&&d>=2&&d<=3&&activity<.25&&n%2&&random()<.35)v=0;
    if(v>0&&count<2000){v=juce::jlimit(.08f,1.f,v*(float)(.55+.35*activity+.15*accent+.05*random()));out.groove.pattern[d][bar*16+n]=v;++count;}
   }
  }
  out.groove.mapped=count;out.groove.valid=count>0;out.groove.name="Audio + groove library";
  out.summary="Library-conditioned accompaniment | "+juce::String(out.accepted)+" grooves scanned | "+juce::String(out.groove.bars)+" bars | "+juce::String(count)+" hits | "+juce::String(bpm,1)+" BPM | Sources: "+used.joinIntoString(", ");if(seconds*bpm/240>16)out.summary+=" | First 16 bars only";return out;
 }
};
}
