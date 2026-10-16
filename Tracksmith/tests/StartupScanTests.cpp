#include "../source/StartupScan.h"
#include <iostream>
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    if(argc==4&&juce::String(argv[1])=="--scan-vst3"){PluginDiscovery::scan(argv[2],argv[3]);return 0;}
    require(argc==2,"Pass a stock VST3 bundle path");
    auto root=juce::File::getCurrentWorkingDirectory().getChildFile("work/startup-tests-"+juce::Uuid().toString());
    root.createDirectory();auto plugins=root.getChildFile("plugins");plugins.createDirectory();
    auto fixture=plugins.getChildFile(juce::File(argv[1]).getFileName());require(juce::File(argv[1]).copyDirectoryTo(fixture),"Copy stock scan fixture");
    auto run=[&]{StartupScan scan(juce::FileSearchPath(plugins.getFullPathName()),root.getChildFile("state"));bool done=false;StartupScan::Progress result;
        scan.onProgress=[&](auto p){if(!p.active){result=p;done=true;}};scan.begin();
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+45000;
        while(!done&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        scan.shutdown();require(done,"Startup scan completes");return result;};
    auto first=run();require(first.total==1&&first.done==1&&first.scanned==1&&first.failed==0,"First run discovers actual VST3");
    juce::KnownPluginList cache;auto xml=juce::XmlDocument::parse(root.getChildFile("state/plugins.xml"));require(xml!=nullptr,"Cache is written");cache.recreateFromXml(*xml);require(cache.getNumTypes()>0,"Plugin available to insert browser");
    auto original=cache.getTypes().getFirst();auto duplicate=original;duplicate.fileOrIdentifier=root.getChildFile("old/Copy.vst3").getFullPathName();juce::KnownPluginList duplicates;duplicates.addType(duplicate);duplicates.addType(original);auto distinct=original;distinct.uniqueId+=1;distinct.name="Different plugin";duplicates.addType(distinct);PluginCatalog::deduplicate(duplicates,plugins);require(duplicates.getNumTypes()==2,"Duplicate copies removed while distinct plugins remain");for(const auto& d:duplicates.getTypes())if(d.name==original.name)require(d.fileOrIdentifier==original.fileOrIdentifier,"Current bundled plugin preferred");
    auto second=run();require(second.scanned==0&&second.done==1,"Unchanged plugin uses cache");
    const auto binaries=fixture.findChildFiles(juce::File::findFiles,true,"*.vst3");require(!binaries.isEmpty(),"Find fixture binary");require(binaries[0].setLastModificationTime(juce::Time::getCurrentTime()+juce::RelativeTime::seconds(5)),"Update plugin binary timestamp");
    auto changed=run();require(changed.scanned==1&&changed.failed==0,"Changed plugin rescans");
    plugins.getChildFile("Invalid.vst3").replaceWithText("invalid test plugin");auto failed=run();require(failed.failed==1&&failed.scanned==1,"Invalid plugin is isolated and skipped");auto retry=run();require(retry.failed==1&&retry.scanned==0,"Unchanged failed plugin is not retried on every launch");
    std::cout<<"Startup discovery, plugin cache and changed-plugin rescan passed.\n";
}




