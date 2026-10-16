#pragma once
#include "MidiGroove.h"
#include <functional>
namespace TracksmithGrooveFull {
struct NeuralPerformanceNote {int drum=0;double beat=0,length=.1;float velocity=.7f;};
// GrooVAE's canonical tom pitches encode low/mid/high classes, not the kit's GM pitches.
inline int neuralDrumRole(int pitch){switch(pitch){case 36:return 0;case 38:return 1;case 42:return 2;case 46:return 3;case 50:return 4;case 48:return 5;case 45:return 6;case 49:return 7;case 51:return 8;default:return -1;}}
struct NeuralResult {bool ok=false;juce::String status;std::vector<NeuralPerformanceNote> notes;};
inline NeuralResult generateNeural(const juce::AudioBuffer<float>& audio,double rate,const MidiGroove& reference,float influence,const std::function<bool()>& cancelled,const juce::File& runtime={}){
 NeuralResult result;if(cancelled()){result.status="Neural generation cancelled.";return result;}const auto folder=runtime==juce::File()?juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Native Instruments/Neural"):runtime;
 const auto node=folder.getChildFile("node.exe"),runner=folder.getChildFile("neural-runner.cjs");
 if(!node.existsAsFile()||!runner.existsAsFile()||!folder.getChildFile("GrooVAE/config.json").existsAsFile()){result.status="Neural runtime/model missing. Keep Native Instruments beside Tracksmith.exe.";return result;}
 const double beats=std::min(reference.bars*4.,audio.getNumSamples()/rate*reference.bpm/60.);const int steps=reference.bars*16;
 std::vector<double> energy(steps);double peak=0;
 for(int step=0;step<steps;++step){const int a=std::min(audio.getNumSamples(),(int)(step*rate*60/reference.bpm/4)),b=std::min(audio.getNumSamples(),(int)((step+1)*rate*60/reference.bpm/4));double e=0;for(int c=0;c<audio.getNumChannels();++c)for(int n=a;n<b;++n){const double v=audio.getSample(c,n);e+=v*v;}energy[step]=std::sqrt(e/std::max(1,(b-a)*audio.getNumChannels()));peak=std::max(peak,energy[step]);}
 if(peak<.0001){result.status="Select audible audio before neural generation.";return result;}
 auto* request=new juce::DynamicObject();request->setProperty("tempo",reference.bpm);request->setProperty("beats",beats);request->setProperty("influence",juce::jlimit(0.f,1.f,influence));juce::Array<juce::var> windows;
 for(int start=0;start<steps;start+=32){juce::Array<juce::var> taps,refs;for(int n=0;n<32&&start+n<steps;++n){const auto e=energy[start+n],before=start+n?energy[start+n-1]:0.;if(e>peak*.1&&(e>before*1.12||n%4==0))taps.add(n);bool ref=false;for(int d=0;d<2;++d)ref=ref||reference.pattern[d][start+n]>.2f;if(ref)refs.add(n);}
  if(refs.isEmpty())for(int n=0;n<32&&start+n<steps;++n)for(int d=0;d<9;++d)if(reference.pattern[d][start+n]>0){refs.add(n);break;}
  if(taps.isEmpty())taps=refs;auto* w=new juce::DynamicObject();w->setProperty("taps",taps);w->setProperty("referenceTaps",refs);windows.add(juce::var(w));
 }
 request->setProperty("windows",windows);const auto temporary=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Tracksmith-neural-"+juce::Uuid().toString());const auto input=temporary.withFileExtension("request.json"),output=temporary.withFileExtension("result.json");
 struct Cleanup{juce::File a,b;~Cleanup(){a.deleteFile();b.deleteFile();}}cleanup{input,output};
 if(!input.replaceWithText(juce::JSON::toString(juce::var(request),true))){result.status="Could not prepare local neural request.";return result;}
 juce::ChildProcess child;if(!child.start(juce::StringArray{node.getFullPathName(),runner.getFullPathName(),input.getFullPathName(),output.getFullPathName()},0)){result.status="Neural inference process could not start.";return result;}
 const auto deadline=juce::Time::getMillisecondCounterHiRes()+180000.;while(child.isRunning()){if(cancelled()||juce::Time::getMillisecondCounterHiRes()>deadline){child.kill();result.status=cancelled()?"Neural generation cancelled.":"Neural generation timed out.";return result;}juce::Thread::sleep(20);}
 if(!output.existsAsFile()||output.getSize()>1024*1024){result.status="Neural inference returned no usable result.";return result;}const auto response=juce::JSON::parse(output);if(!static_cast<bool>(response.getProperty("ok",false))){result.status="Neural generation failed: "+response.getProperty("error",{}).toString();return result;}
 auto values=response.getProperty("notes",{});const auto* notes=values.getArray();if(!notes||notes->isEmpty()||notes->size()>2000){result.status="Neural result exceeded note limits or was empty.";return result;}
 for(const auto& value:*notes){const int drum=neuralDrumRole((int)value.getProperty("pitch",-1));const double beat=(double)value.getProperty("start",-1.),length=(double)value.getProperty("length",.1),velocity=(double)value.getProperty("velocity",.7);if(drum<0||!std::isfinite(beat)||!std::isfinite(length)||!std::isfinite(velocity)||beat<0||beat>=beats||length<=0||velocity<=0||velocity>1)continue;result.notes.push_back({drum,beat,std::min(length,beats-beat),(float)velocity});}
 std::sort(result.notes.begin(),result.notes.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
 // Close retriggers after collapsing the neural model's drum classes into this kit.
 std::array<int,9> previous;previous.fill(-1);std::vector<NeuralPerformanceNote> clean;
 for(const auto& n:result.notes){auto& prior=previous[n.drum];if(prior>=0){auto& last=clean[prior];if(n.beat-last.beat<.01){last.velocity=std::max(last.velocity,n.velocity);continue;}last.length=std::min(last.length,n.beat-last.beat);}prior=(int)clean.size();clean.push_back(n);}result.notes=std::move(clean);
 result.ok=!result.notes.empty();result.status="GrooVAE neural accompaniment | "+juce::String((int)result.notes.size())+" notes | "+juce::String(reference.bpm,1)+" BPM | Reference influence "+juce::String((int)(influence*100))+"% | Local CPU "+response.getProperty("elapsedMs",0).toString()+" ms";return result;
}
}
