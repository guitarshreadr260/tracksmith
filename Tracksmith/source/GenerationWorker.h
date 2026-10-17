#pragma once
#include "AudioEngine.h"
// A separate local GPU process owns inference. This thread only submits/polls/downloads.
class GenerationWorker final : private juce::Thread, private juce::AsyncUpdater {
public:
 struct Request {juce::String prompt,key;double bpm=120,duration=16;int seed=-1;juce::File reference;double strength=.5;};
 struct Item {LoadedWav wav;juce::String asset;};
 struct Result {Request request;std::vector<Item> items;juce::String error;bool cancelled=false;};
 std::function<void(Result)> onFinished;
 std::atomic<int> phase{0};
 GenerationWorker():Thread("Music generation client"){}
 ~GenerationWorker()override{shutdown();}
 void shutdown(){onFinished={};signalThreadShouldExit();stopThread(-1);cancelPendingUpdate();}
 void cancel(){signalThreadShouldExit();}
 void pollCompletion(){handleUpdateNowIfNeeded();}
 bool start(Request request){if(isThreadRunning())return false;cancelPendingUpdate();job=std::move(request);phase=1;return startThread();}
 static juce::File folder(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Tracksmith/Generated Audio");}
private:
 Request job;juce::CriticalSection mutex;std::unique_ptr<Result> pending;
 static juce::var post(const juce::String& path,const juce::var& body){
  int code=0;auto stream=juce::URL("http://127.0.0.1:8001"+path).withPOSTData(juce::JSON::toString(body)).createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData).withExtraHeaders("Content-Type: application/json\r\n").withConnectionTimeoutMs(5000).withStatusCode(&code));
  if(!stream)throw std::runtime_error("Local generator is not responding. Start the generation engine, then retry.");
  auto bytes=stream->readEntireStreamAsString();if(bytes.length()>4*1024*1024)throw std::runtime_error("Generator response exceeded the limit.");auto value=juce::JSON::parse(bytes);
  if(code!=200||!value.isObject()||int(value.getProperty("code",200))!=200||value.getProperty("error",{}).toString().isNotEmpty())throw std::runtime_error(("Generator error: "+value.getProperty("error",value.getProperty("detail","Invalid response")).toString().substring(0,600)).toStdString());return value.getProperty("data",{});
 }
 void run()override{
  auto result=std::make_unique<Result>();result->request=job;
  try{
   auto* body=new juce::DynamicObject;juce::var request(body);body->setProperty("prompt",job.prompt+". Instrumental, no singing or spoken vocals.");body->setProperty("lyrics","[Instrumental]");body->setProperty("bpm",job.bpm);body->setProperty("key_scale",job.key);body->setProperty("time_signature","4");body->setProperty("audio_duration",job.duration);body->setProperty("thinking",false);body->setProperty("use_cot_caption",false);body->setProperty("use_cot_language",false);body->setProperty("batch_size",1);body->setProperty("audio_format","wav");body->setProperty("inference_steps",8);body->setProperty("use_random_seed",job.seed<0);body->setProperty("seed",job.seed);
   if(job.reference!=juce::File()){if(!job.reference.existsAsFile())throw std::runtime_error("The audio reference is no longer available. Drop the clip again.");body->setProperty("src_audio_path",job.reference.getFullPathName());body->setProperty("task_type","cover");body->setProperty("audio_cover_strength",job.strength);}
   const auto id=post("/release_task",request).getProperty("task_id",{}).toString();if(id.isEmpty())throw std::runtime_error("Generator did not return a task ID.");phase=2;
   const auto deadline=juce::Time::getMillisecondCounterHiRes()+20*60*1000;juce::var files;
   while(!threadShouldExit()){
    if(juce::Time::getMillisecondCounterHiRes()>deadline)throw std::runtime_error("Generation timed out. The engine may still be finishing its first model download; retry after it is ready.");
    auto* query=new juce::DynamicObject;query->setProperty("task_id_list",juce::Array<juce::var>{id});auto rows=post("/query_result",juce::var(query));const auto* items=rows.getArray();if(!items||items->isEmpty())throw std::runtime_error("Generator task status was missing.");const auto& task=items->getReference(0);const int status=int(task.getProperty("status",0));
    if(status==2)throw std::runtime_error(("Generation failed: "+task.getProperty("result","See generator log").toString().substring(0,600)).toStdString());
    if(status==1){auto value=task.getProperty("result",{});files=value.isString()?juce::JSON::parse(value.toString()):value;break;}wait(1500);
   }
   if(threadShouldExit()){result->cancelled=true;throw std::runtime_error("Stopped waiting. The local engine may finish its queued job in the background.");}
   const auto* list=files.getArray();if(!list||list->isEmpty())throw std::runtime_error("Generator returned no audio.");phase=3;
   if(folder().createDirectory().failed())throw std::runtime_error("Could not create generated audio folder.");
   const auto& item=list->getReference(0);const auto path=item.getProperty("file",{}).toString();if(!path.startsWith("/v1/audio?path="))throw std::runtime_error("Generator returned an unexpected audio URL.");
   int code=0;auto stream=juce::URL("http://127.0.0.1:8001"+path).createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(5000).withStatusCode(&code));if(!stream||code!=200)throw std::runtime_error("Could not download generated WAV.");
   const auto asset=juce::Uuid().toString()+".wav";const auto file=folder().getChildFile(asset);juce::TemporaryFile temporary(file);auto out=temporary.getFile().createOutputStream();if(!out)throw std::runtime_error("Cannot write generated WAV.");std::array<char,65536> bytes;int64_t total=0;
   while(!stream->isExhausted()&&!threadShouldExit()){const auto count=stream->read(bytes.data(),int(bytes.size()));if(count<=0)break;total+=count;if(total>64*1024*1024)throw std::runtime_error("Generated WAV exceeds the 64 MB prototype limit.");if(!out->write(bytes.data(),size_t(count)))throw std::runtime_error("WAV write failed.");}out.reset();
   if(threadShouldExit()){result->cancelled=true;throw std::runtime_error("Stopped waiting; result not imported.");}if(total<44||!temporary.overwriteTargetFileWithTemporary())throw std::runtime_error("Generated WAV was incomplete.");
   auto wav=decodeWav(file,4*1024*1024,[this]{return threadShouldExit();});if(wav.error.isNotEmpty())throw std::runtime_error(wav.error.toStdString());if(wav.left.empty()||double(wav.left.size())/wav.rate>65)throw std::runtime_error("Generated audio has an unexpected duration.");for(size_t i=0;i<wav.left.size();++i)if(!std::isfinite(wav.left[i])||!std::isfinite(wav.right[i]))throw std::runtime_error("Generated audio contains invalid samples.");
   auto* meta=new juce::DynamicObject;juce::var manifest(meta);meta->setProperty("engine","ACE-Step 1.5 / local turbo / 8 steps");meta->setProperty("prompt",job.prompt);meta->setProperty("bpm",job.bpm);meta->setProperty("key",job.key);meta->setProperty("requestedSeconds",job.duration);meta->setProperty("actualSeconds",double(wav.left.size())/wav.rate);meta->setProperty("seed",item.getProperty("seed_value",job.seed));meta->setProperty("file",asset);meta->setProperty("reference",job.reference.getFullPathName());meta->setProperty("referenceStrength",job.strength);file.withFileExtension(".json").replaceWithText(juce::JSON::toString(manifest));wav.name="Generated · "+job.prompt.substring(0,42);result->items.push_back({std::move(wav),asset});
  }catch(const std::exception& e){result->error=juce::String::fromUTF8(e.what());if(threadShouldExit())result->cancelled=true;}
  phase=0;{const juce::ScopedLock guard(mutex);pending=std::move(result);}triggerAsyncUpdate();
 }
 void handleAsyncUpdate()override{std::unique_ptr<Result> result;{const juce::ScopedLock guard(mutex);result=std::move(pending);}if(result&&onFinished)onFinished(std::move(*result));}
};
