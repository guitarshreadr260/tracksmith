#pragma once
#include "ElasticRender.h"
#include "AudioEngine.h"
class ElasticWorker final : private juce::Thread,private juce::AsyncUpdater {
public:
 struct Request {ElasticRender::Audio audio;double rate=48000;std::vector<ElasticAnchor> map;juce::File folder;};
 std::function<void(LoadedWav)> onFinished;
 ElasticWorker():Thread("Time Warp render"){}
 ~ElasticWorker()override{shutdown();}
 void shutdown(){onFinished={};signalThreadShouldExit();stopThread(-1);cancelPendingUpdate();}
 void cancel(){signalThreadShouldExit();}
 void pollCompletion(){handleUpdateNowIfNeeded();}
 bool start(Request request){if(isThreadRunning())return false;cancelPendingUpdate();pending.reset();job=std::move(request);return startThread();}
 static LoadedWav execute(const Request& request,const std::function<bool()>& cancelled){
  LoadedWav result;
  try{
   juce::MemoryOutputStream fingerprint;fingerprint.writeString("Tracksmith Time Warp v1 / Signalsmith a670068 / Linear 146b26f");fingerprint.writeDouble(request.rate);
   for(const auto& c:request.audio)fingerprint.write(c.data(),c.size()*sizeof(float));for(const auto p:request.map){fingerprint.writeDouble(p.source);fingerprint.writeDouble(p.target);}
   const auto hash=juce::SHA256(fingerprint.getData(),fingerprint.getDataSize()).toHexString();const auto file=request.folder.getChildFile(hash+".wav");
   if(!request.folder.createDirectory().wasOk())throw std::runtime_error("Cannot create Time Warp media folder.");
   if(!file.existsAsFile()){
    const auto rendered=ElasticRender::render(request.audio,request.rate,request.map,cancelled);
    if(cancelled())throw std::runtime_error("Time Warp render cancelled.");
    juce::TemporaryFile temporary(file);auto stream=temporary.getFile().createOutputStream();if(!stream||!stream->openedOk())throw std::runtime_error("Cannot write Time Warp media.");
    std::unique_ptr<juce::OutputStream> output(std::move(stream));juce::WavAudioFormat format;
    auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions().withSampleRate(request.rate).withNumChannels(2).withBitsPerSample(32));
    const float* channels[]={rendered[0].data(),rendered[1].data()};
    if(!writer||!writer->writeFromFloatArrays(channels,2,static_cast<int>(rendered[0].size()))||!writer->flush())throw std::runtime_error("Time Warp WAV write failed.");writer.reset();
    if(cancelled())throw std::runtime_error("Time Warp render cancelled.");
    if(!temporary.overwriteTargetFileWithTemporary())throw std::runtime_error("Cannot finish Time Warp media file.");
   }
   result=decodeWav(file,16*1024*1024,cancelled);if(result.error.isEmpty()&&(result.left.size()!=request.audio[0].size()||std::abs(result.rate-request.rate)>.001))result.error="Time Warp media has an unexpected length or sample rate. Previous clip kept.";result.name="Time Warp · "+hash.substring(0,8)+".wav";
   if(cancelled())result.error="Time Warp render cancelled.";
  }catch(const std::exception& e){result.error=e.what();}return result;
 }
private:
 void run()override{auto result=std::make_unique<LoadedWav>(execute(job,[this]{return threadShouldExit();}));{const juce::ScopedLock lock(mutex);pending=std::move(result);}triggerAsyncUpdate();}
 void handleAsyncUpdate()override{std::unique_ptr<LoadedWav> result;{const juce::ScopedLock lock(mutex);result=std::move(pending);}if(result&&onFinished)onFinished(std::move(*result));}
 juce::CriticalSection mutex;std::unique_ptr<LoadedWav> pending;Request job;
};
