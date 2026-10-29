#pragma once
#include "AudioEngine.h"

// Separation and WAV decoding happen away from the audio and UI threads.
class SplitWorker final : private juce::Thread, private juce::AsyncUpdater {
public:
 struct Result { std::vector<LoadedWav> stems; juce::String error; juce::File folder; int revision=0; };
 std::function<void(Result)> onFinished;
 SplitWorker() : Thread("SPLIT separation") {}
 ~SplitWorker() override { shutdown(); }
 void shutdown() { onFinished={}; signalThreadShouldExit(); process.kill(); stopThread(-1); cancelPendingUpdate(); }
 void pollCompletion() { handleUpdateNowIfNeeded(); }
 bool start(juce::File file, int rev, int64_t budget, juce::File parent=juce::File()) {
  if (isThreadRunning()) return false;
  cancelPendingUpdate(); input=file; revision=rev; frameBudget=budget;
  if(parent==juce::File())parent=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Tracksmith SPLIT Results");
  output=parent.getChildFile(juce::Uuid().toString());
  existing=false;
  return startThread();
 }
 bool startExisting(juce::File folder,int rev,int64_t budget) {
  if(isThreadRunning())return false; cancelPendingUpdate(); input=folder; output=folder; revision=rev; frameBudget=budget; existing=true; return startThread();
 }
private:
 juce::File input, output; int revision=0; int64_t frameBudget=0;
 bool existing=false;
 juce::ChildProcess process; juce::CriticalSection lock; std::unique_ptr<Result> pending;
 void run() override {
  auto result=std::make_unique<Result>(); result->revision=revision; result->folder=output;
  try {
   juce::File folder;
   if(existing){
    folder=input;
    if(!folder.getChildFile("vocals.wav").existsAsFile()){
     auto model=folder.getChildFile("htdemucs_6s");auto children=model.findChildFiles(juce::File::findDirectories,false);
     if(children.size()!=1)throw std::runtime_error("Choose the folder containing vocals.wav and the other five stem WAVs.");
     folder=children[0];
    }
   }else{
   auto python=juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads/Tracksmith-SPLIT-Build1/Tracksmith-SPLIT-Build1/.venv/Scripts/python.exe");
   if (!python.existsAsFile()) throw std::runtime_error("SPLIT Python environment not found. Restore the Build 1 environment first.");
   if (!input.existsAsFile()) throw std::runtime_error("The selected audio file no longer exists.");
   juce::StringArray cmd{python.getFullPathName(),"-m","demucs","-n","htdemucs_6s","-d","cuda","--float32","-o",output.getFullPathName(),input.getFullPathName()};
   if (!process.start(cmd,juce::ChildProcess::wantStdOut|juce::ChildProcess::wantStdErr)) throw std::runtime_error("Could not start SPLIT.");
   const auto log=process.readAllProcessOutput();
   if (threadShouldExit()) throw std::runtime_error("SPLIT was cancelled.");
   if (process.getExitCode()!=0) throw std::runtime_error(("SPLIT failed: "+log.substring(juce::jmax(0,log.length()-1800))).toStdString());
   folder=output.getChildFile("htdemucs_6s").getChildFile(input.getFileNameWithoutExtension());
   }
   for (const auto* name : {"vocals","drums","bass","guitar","piano","other"}) {
    auto wav=decodeWav(folder.getChildFile(juce::String(name)+".wav"),juce::jmin<int64_t>(maximumAudioFileFrames,frameBudget),[this]{return threadShouldExit();});
    if (wav.error.isNotEmpty()) throw std::runtime_error((juce::String(name)+": "+wav.error).toStdString());
    frameBudget-=int64_t(wav.left.size()); wav.name=juce::String(name).toUpperCase(); result->stems.push_back(std::move(wav));
   }
  } catch (const std::exception& e) { result->stems.clear(); result->error=juce::String::fromUTF8(e.what()); }
  { const juce::ScopedLock guard(lock); pending=std::move(result); } triggerAsyncUpdate();
 }
 void handleAsyncUpdate() override { std::unique_ptr<Result> result; {const juce::ScopedLock guard(lock); result=std::move(pending);} if(result && onFinished) onFinished(std::move(*result)); }
};
