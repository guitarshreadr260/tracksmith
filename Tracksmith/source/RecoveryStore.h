#pragma once
#include "SessionFile.h"
#include <juce_events/juce_events.h>
namespace RecoveryStore {
constexpr int snapshotsPerRun=3,maximumRuns=10;
struct Policy {bool enabled=true,dirty=false,blocked=false,playing=false,writing=false,busy=false;double elapsed=0;juce::int64 revision=0,lastRevision=-1;
    bool eligible(bool force=false)const noexcept {return !blocked&&!playing&&!writing&&!busy&&(force||(enabled&&dirty&&revision!=lastRevision&&std::isfinite(elapsed)&&elapsed>=120000));}
};
struct Entry {juce::String id,name;juce::File snapshot,original,run;juce::int64 time=0,generation=0;};
inline juce::File defaultRoot(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Tracksmith/Recovery");}
inline juce::String newRun(){return "run-"+juce::Uuid().toString();}
inline bool validRun(const juce::File& run){return run.isDirectory()&&!run.isSymbolicLink()&&run.getFileName().startsWith("run-")&&juce::Uuid(run.getFileName().substring(4)).toString()==run.getFileName().substring(4);}
inline bool markClean(const juce::File& run){if(!validRun(run))return true;juce::TemporaryFile temporary(run.getChildFile("clean.flag"));return temporary.getFile().replaceWithText("Session saved or explicitly discarded.\n")&&temporary.overwriteTargetFileWithTemporary();}
inline std::vector<Entry> discover(const juce::File& root,const juce::String& excluded={}){
    std::vector<Entry> result;if(!root.isDirectory())return result;
    for(const auto& run:root.findChildFiles(juce::File::findDirectories,false,"run-*")){
        if(!validRun(run)||run.getFileName()==excluded||run.getChildFile("clean.flag").existsAsFile())continue;
        Entry best;bool found=false;
        for(int slot=0;slot<snapshotsPerRun;++slot){const auto file=run.getChildFile("snapshot-"+juce::String(slot)+".tracksmith");if(!file.existsAsFile()||file.isSymbolicLink()||file.getSize()>64*1024*1024)continue;
            auto document=juce::JSON::parse(file.loadFileAsString());const auto runId=document.getProperty("recoveryRun",{}),generation=document.getProperty("recoveryGeneration",{}),time=document.getProperty("recoveryTime",{}),original=document.getProperty("recoveryOriginal",{}),name=document.getProperty("recoveryName",{});
            if(!runId.isString()||runId.toString()!=run.getFileName()||!(generation.isInt()||generation.isInt64())||static_cast<juce::int64>(generation)<0||!(time.isInt()||time.isInt64())||static_cast<juce::int64>(time)<=0||!original.isString()||original.toString().length()>32768||(!original.toString().isEmpty()&&!juce::File::isAbsolutePath(original.toString()))||!name.isString()||name.toString().length()>128)continue;
            SavedSession validation;if(SessionFile::decode(document,file,validation).failed())continue;
            Entry entry{runId.toString(),name.toString(),file,original.toString().isEmpty()?juce::File():juce::File(original.toString()),run,static_cast<juce::int64>(time),static_cast<juce::int64>(generation)};
            if(!found||entry.generation>best.generation){best=std::move(entry);found=true;}
        }
        if(found)result.push_back(std::move(best));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.time>b.time;});return result;
}
inline void prune(const juce::File& root,const juce::String& current,int limit=maximumRuns){
    std::vector<juce::File> runs;for(const auto& run:root.findChildFiles(juce::File::findDirectories,false,"run-*"))if(validRun(run)&&run.getFileName()!=current)runs.push_back(run);
    std::sort(runs.begin(),runs.end(),[](const auto& a,const auto& b){return a.getLastModificationTime()<b.getLastModificationTime();});
    while(runs.size()>=static_cast<size_t>(std::max(1,limit))){const auto run=runs.front();runs.erase(runs.begin());for(int slot=0;slot<snapshotsPerRun;++slot){const auto file=run.getChildFile("snapshot-"+juce::String(slot)+".tracksmith");if(!file.isSymbolicLink())file.deleteFile();}const auto flag=run.getChildFile("clean.flag");if(!flag.isSymbolicLink())flag.deleteFile();run.deleteFile();}
}
inline juce::Result write(const juce::File& root,const juce::String& id,juce::int64 generation,const juce::File& original,const juce::String& name,SavedSession session){
    if(generation<0||id!="run-"+juce::Uuid(id.substring(4)).toString()||!id.startsWith("run-"))return juce::Result::fail("Invalid recovery destination.");
    const auto run=root.getChildFile(id);const auto made=run.createDirectory();if(made.failed()||!validRun(run))return juce::Result::fail("Cannot create the recovery folder.");
    const auto file=run.getChildFile("snapshot-"+juce::String(static_cast<int>(generation%snapshotsPerRun))+".tracksmith");
    auto document=SessionFile::encode(session,file);auto* object=document.getDynamicObject();object->setProperty("recoveryRun",id);object->setProperty("recoveryGeneration",generation);object->setProperty("recoveryTime",juce::Time::currentTimeMillis());object->setProperty("recoveryOriginal",original==juce::File()?juce::String():original.getFullPathName());object->setProperty("recoveryName",name.substring(0,128));
    auto result=SessionFile::write(file,document);if(result.wasOk()){run.getChildFile("clean.flag").deleteFile();prune(root,id);}return result;
}
}
struct RecoveryResult {bool scanning=false;juce::String id,error;juce::int64 revision=0,time=0;std::vector<RecoveryStore::Entry> entries;};
class RecoveryWorker final:private juce::Thread,private juce::AsyncUpdater {
public:
    std::function<void(RecoveryResult)> onFinished;
    RecoveryWorker():Thread("Session recovery"){}
    ~RecoveryWorker()override{shutdown();}
    void shutdown(){signalThreadShouldExit();stopThread(-1);cancelPendingUpdate();busy=false;}
    bool isBusy()const{return busy.load();}
    bool scan(juce::File folder,juce::String excluded){if(busy.exchange(true))return false;root=folder;id=excluded;scanning=true;return launch();}
    bool save(juce::File folder,juce::String run,juce::int64 sequence,juce::int64 revision,juce::File file,juce::String label,SavedSession snapshot){if(busy.exchange(true))return false;root=folder;id=run;generation=sequence;editRevision=revision;original=file;name=label;session=std::move(snapshot);scanning=false;return launch();}
private:
    bool launch(){if(startThread())return true;busy=false;return false;}
    void run()override{RecoveryResult result;result.scanning=scanning;result.id=id;result.revision=editRevision;
        try{if(scanning)result.entries=RecoveryStore::discover(root,id);else{const auto written=RecoveryStore::write(root,id,generation,original,name,std::move(session));if(written.failed())result.error=written.getErrorMessage();else result.time=juce::Time::currentTimeMillis();}}
        catch(const std::exception&){result.error="Recovery operation failed: insufficient memory or an unreadable file.";}
        if(threadShouldExit())return;{const juce::ScopedLock lock(resultLock);pending=std::move(result);}triggerAsyncUpdate();
    }
    void handleAsyncUpdate()override{RecoveryResult result;{const juce::ScopedLock lock(resultLock);result=std::move(pending);}busy=false;if(onFinished)onFinished(std::move(result));}
    std::atomic<bool> busy{false};bool scanning=false;juce::File root,original;juce::String id,name;juce::int64 generation=0,editRevision=0;SavedSession session;RecoveryResult pending;juce::CriticalSection resultLock;
};
