#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
class PluginDiscovery final : private juce::Thread
{
public:
    using juce::Thread::isThreadRunning;
    PluginDiscovery():Thread("Plugin discovery"){}
    ~PluginDiscovery(){signalThreadShouldExit();stopThread(3000);}
    static juce::File directory(){auto f=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Tracksmith");f.createDirectory();return f;}
    static juce::File cacheFile(){return directory().getChildFile("plugins.xml");}
    static juce::File loadMarker(){return directory().getChildFile("loading-plugin.txt");}
    static juce::File quarantine(){return directory().getChildFile("interrupted-plugins.txt");}
    static bool blocked(const juce::String& id){return juce::StringArray::fromLines(quarantine().loadFileAsString()).contains(id);}
    static void recover(){auto id=loadMarker().loadFileAsString();if(id.isNotEmpty()){auto lines=juce::StringArray::fromLines(quarantine().loadFileAsString());lines.addIfNotAlreadyThere(id);quarantine().replaceWithText(lines.joinIntoString("\n"));loadMarker().deleteFile();}}
    static void allow(const juce::String& id){auto lines=juce::StringArray::fromLines(quarantine().loadFileAsString());lines.removeString(id);quarantine().replaceWithText(lines.joinIntoString("\n"));}
    static void scan(const juce::String& path,const juce::String& output){
        juce::AudioPluginFormatManager formats;formats.addFormat(new juce::VST3PluginFormat());
        juce::OwnedArray<juce::PluginDescription> found;formats.getFormat(0)->findAllTypesForFile(found,path);
        juce::KnownPluginList list;for(auto* d:found)list.addType(*d);if(auto xml=list.createXml())xml->writeTo(juce::File(output));
    }
    void begin(juce::File f,std::function<void(juce::String)> cb){stopThread(3000);file=f;callback=std::move(cb);startThread();}
private:
    juce::File file;std::function<void(juce::String)> callback;
    void run() override {
        auto output=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("tracksmith-scan",".xml",false);
        juce::ChildProcess child;juce::StringArray args{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),"--scan-vst3",file.getFullPathName(),output.getFullPathName()};
        const auto started=juce::Time::getMillisecondCounterHiRes();
        if(child.start(args,0))while(child.isRunning()&&!threadShouldExit()&&juce::Time::getMillisecondCounterHiRes()-started<30000)wait(50);
        if(child.isRunning())child.kill();
        auto xml=output.loadFileAsString();output.deleteFile();if(threadShouldExit())return;
        juce::MessageManager::callAsync([cb=callback,xml]{cb(xml);});
    }
};
