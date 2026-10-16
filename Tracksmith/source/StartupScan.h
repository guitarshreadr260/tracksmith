#pragma once
#include "PluginDiscovery.h"
#include "PluginCatalog.h"

// Discovery and plugin code run away from both the message and audio threads.
class StartupScan final : private juce::Thread
{
public:
    struct Progress { int done=0,total=0,failed=0,scanned=0; bool active=true; juce::String name; };
    std::function<void(Progress)> onProgress;
    StartupScan(juce::FileSearchPath roots={},juce::File state=PluginDiscovery::directory()):Thread("Startup VST3 scan"),searchRoots(roots),stateDirectory(state){}
    ~StartupScan() override { shutdown(); }
    void begin(){startThread();}
    void cancel(){signalThreadShouldExit();}
    void shutdown(){signalThreadShouldExit();stopThread(3000);}
private:
    juce::FileSearchPath searchRoots;
    juce::File stateDirectory;
    void report(Progress p){auto cb=onProgress;juce::MessageManager::callAsync([cb,p]{if(cb)cb(p);});}
    static juce::String fingerprint(const juce::File& f)
    {
        juce::String signature=f.getFullPathName()+":"+juce::String(f.getLastModificationTime().toMilliseconds())+":"+juce::String(f.getSize());
        if(f.isDirectory())for(const auto& child:f.findChildFiles(juce::File::findFiles,true))signature+=":"+child.getRelativePathFrom(f)+":"+juce::String(child.getSize())+":"+juce::String(child.getLastModificationTime().toMilliseconds());
        return juce::String::toHexString(signature.hashCode64());
    }
    void run() override
    {
        juce::KnownPluginList cache;
        stateDirectory.createDirectory();const auto cacheFile=stateDirectory.getChildFile("plugins.xml");
        if(auto tree=juce::XmlDocument::parse(cacheFile))cache.recreateFromXml(*tree);
        const auto stampFile=stateDirectory.getChildFile("startup-scan.xml");
        auto stamps=juce::XmlDocument::parse(stampFile);if(!stamps)stamps=std::make_unique<juce::XmlElement>("SCANS");
        juce::VST3PluginFormat format;auto paths=searchRoots.getNumPaths()?searchRoots:format.getDefaultLocationsToSearch();
        if(!searchRoots.getNumPaths())paths.add(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Stock Plugins"));
        for(const auto& d:cache.getTypes())paths.add(juce::File(d.fileOrIdentifier).getParentDirectory());
        auto files=format.searchPathsForPlugins(paths,true,false);files.removeDuplicates(false);files.sort(false);
        // Remove disappeared plugin entries from the insert browser.
        for(const auto& d:cache.getTypes())if(!juce::File(d.fileOrIdentifier).exists())cache.removeType(d);
        Progress progress;progress.total=files.size();report(progress);
        for(const auto& path:files)
        {
            if(threadShouldExit())break;
            const juce::File file(path);const auto stamp=fingerprint(file);
            progress.name=file.getFileName();report(progress);
            juce::XmlElement* old=nullptr;for(auto* e:stamps->getChildIterator())if(e->getStringAttribute("path")==path){old=e;break;}
            bool hasCached=false;for(const auto& d:cache.getTypes())if(d.fileOrIdentifier==path||(old&&old->getStringAttribute("catalogKey")==PluginCatalog::key(d))){hasCached=true;break;}
            if(old&&old->getStringAttribute("stamp")==stamp&&(hasCached||old->getBoolAttribute("failed")))
            {if(old->getBoolAttribute("failed"))++progress.failed;++progress.done;report(progress);continue;}
            const auto output=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("tracksmith-startup-scan",".xml",false);
            ++progress.scanned;
            juce::ChildProcess child;juce::StringArray args{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--scan-vst3",path,output.getFullPathName()};
            const auto started=juce::Time::getMillisecondCounterHiRes();
            if(child.start(args,0))while(child.isRunning()&&!threadShouldExit()&&juce::Time::getMillisecondCounterHiRes()-started<30000)wait(50);
            if(child.isRunning())child.kill();
            if(threadShouldExit()){output.deleteFile();break;}
            juce::KnownPluginList found;if(auto xml=juce::XmlDocument::parse(output))found.recreateFromXml(*xml);output.deleteFile();
            for(const auto& d:cache.getTypes())if(d.fileOrIdentifier==path)cache.removeType(d);
            for(const auto& d:found.getTypes())cache.addType(d);
            const bool failed=found.getNumTypes()==0;if(failed)++progress.failed;
            if(old)stamps->removeChildElement(old,true);
            auto* entry=stamps->createNewChildElement("PLUGIN");entry->setAttribute("path",path);entry->setAttribute("stamp",stamp);entry->setAttribute("failed",failed);
            if(!failed)entry->setAttribute("catalogKey",PluginCatalog::key(found.getTypes().getFirst()));
            if(auto xml=cache.createXml())xml->writeTo(cacheFile);stamps->writeTo(stampFile);
            ++progress.done;report(progress);
        }
        PluginCatalog::deduplicate(cache,juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Stock Plugins"));
        if(auto xml=cache.createXml())xml->writeTo(cacheFile);
        progress.active=false;progress.name=threadShouldExit()?"Scan paused — continue making music":"Your studio is ready";report(progress);
    }
};
