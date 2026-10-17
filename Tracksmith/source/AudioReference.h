#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "ClipLayout.h"
#include "GrooveAnalysis.h"
class AudioReferenceWorker final : private juce::Thread,private juce::AsyncUpdater {
public:
 struct Request {std::array<std::vector<float>,2> audio;double rate=48000;TimelineClip clip;juce::String name;int revision=0;};
 struct Result {juce::File file;juce::String name,key,error;double seconds=0,bpm=0,peakDb=-120,rmsDb=-120,keyConfidence=0,tempoConfidence=0;int revision=0;};
 std::function<void(Result)> onFinished;
 AudioReferenceWorker():Thread("Audio reference analysis"){}
 ~AudioReferenceWorker()override{shutdown();}
 void shutdown(){onFinished={};signalThreadShouldExit();stopThread(-1);cancelPendingUpdate();}
 void cancel(){signalThreadShouldExit();}
 void pollCompletion(){handleUpdateNowIfNeeded();}
 bool start(Request request){if(isThreadRunning())return false;cancelPendingUpdate();job=std::move(request);return startThread();}
 static Result analyse(const Request& r,const std::function<bool()>& cancelled){
  Result out;out.name=r.name;out.revision=r.revision;
  try{
   const int frames=int(r.audio[0].size());if(!std::isfinite(r.rate)||r.rate<8000||r.rate>192000||frames<r.rate*.5||frames>r.rate*60||r.audio[1].size()!=r.audio[0].size())throw std::runtime_error("Use 0.5–60 seconds of valid timeline audio.");
   juce::AudioBuffer<float> audio(2,frames);double energy=0,peak=0;
   for(int n=0;n<frames;++n){if(n%4096==0&&cancelled())throw std::runtime_error("Reference analysis cancelled.");const float gain=ClipLayout::envelope(r.clip,r.clip.start+double(n)/r.rate);for(int c=0;c<2;++c){const auto sample=r.audio[c][size_t(n)]*gain;if(!std::isfinite(sample))throw std::runtime_error("Reference contains invalid samples.");audio.setSample(c,n,sample);energy+=double(sample)*double(sample);peak=std::max(peak,double(std::abs(sample)));}}
   out.seconds=frames/r.rate;out.peakDb=juce::Decibels::gainToDecibels(peak,-120.);out.rmsDb=juce::Decibels::gainToDecibels(std::sqrt(energy/(2*frames)),-120.);if(peak<.00001)throw std::runtime_error("This clip is silent or too quiet to use as a reference.");
   auto rhythm=GrooveAnalysis::analyse(audio,r.rate,1,cancelled);if(rhythm.valid){out.bpm=rhythm.bpm;out.tempoConfidence=rhythm.confidence;}
   // Chroma/profile correlation: a tentative key estimate, not transcription.
   constexpr int order=12,size=1<<order;juce::dsp::FFT fft(order);std::array<double,12> chroma{};std::vector<float> data(size*2);const int hop=std::max(size/2,frames/180);
   for(int start=0;start+size<=frames;start+=hop){if(cancelled())throw std::runtime_error("Reference analysis cancelled.");std::fill(data.begin(),data.end(),0);for(int n=0;n<size;++n){const float a=audio.getSample(0,start+n),b=audio.getSample(1,start+n);data[n]=(std::abs(a)>=std::abs(b)?a:b)*float(.5-.5*std::cos(juce::MathConstants<double>::twoPi*n/(size-1)));}fft.performFrequencyOnlyForwardTransform(data.data());for(int bin=1;bin<size/2;++bin){const double f=bin*r.rate/size;if(f<65||f>2500)continue;const int midi=int(std::round(69+12*std::log2(f/440.)));chroma[size_t((midi%12+12)%12)]+=std::sqrt(std::max(0.f,data[bin]));}}
   const std::array<double,12> major{6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88},minor{6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};double best=-2,second=-2;int winner=0;double mean=0;for(auto x:chroma)mean+=x/12;
   for(int mode=0;mode<2;++mode)for(int root=0;root<12;++root){const auto& profile=mode?minor:major;double pm=0;for(auto x:profile)pm+=x/12;double numerator=0,a=0,b=0;for(int n=0;n<12;++n){const auto x=chroma[size_t(n)]-mean,y=profile[size_t((n-root+12)%12)]-pm;numerator+=x*y;a+=x*x;b+=y*y;}const double score=a*b>0?numerator/std::sqrt(a*b):-2;if(score>best){second=best;best=score;winner=root+12*mode;}else second=std::max(second,score);}
   if(best>.45&&best-second>.02){const char* roots[]{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};out.key=juce::String(roots[winner%12])+(winner>=12?" Minor":" Major");out.keyConfidence=std::clamp((best-second)*4.,0.,1.);}
   auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Tracksmith Audio References");if(folder.createDirectory().failed())throw std::runtime_error("Cannot create local reference folder.");out.file=folder.getChildFile(juce::Uuid().toString()+".wav");auto stream=out.file.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),r.rate,2,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(audio,0,frames))throw std::runtime_error("Cannot write the reference WAV.");writer.reset();if(cancelled())throw std::runtime_error("Reference analysis cancelled.");
  }catch(const std::exception& e){out.error=juce::String::fromUTF8(e.what());if(out.file.existsAsFile())out.file.deleteFile();out.file={};}return out;
 }
private:
 Request job;juce::CriticalSection mutex;std::unique_ptr<Result> pending;
 void run()override{auto result=std::make_unique<Result>(analyse(job,[this]{return threadShouldExit();}));{const juce::ScopedLock lock(mutex);pending=std::move(result);}triggerAsyncUpdate();}
 void handleAsyncUpdate()override{std::unique_ptr<Result> result;{const juce::ScopedLock lock(mutex);result=std::move(pending);}if(result&&onFinished)onFinished(std::move(*result));}
};
