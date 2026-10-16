#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace PluginCatalog
{
inline juce::String key(const juce::PluginDescription& d){return d.pluginFormatName.toLowerCase()+"|"+d.manufacturerName.toLowerCase()+"|"+d.name.toLowerCase()+"|"+juce::String(d.uniqueId)+"|"+juce::String(d.isInstrument?1:0);}
inline bool samePlugin(const juce::PluginDescription& a,const juce::PluginDescription& b)
{
    return a.pluginFormatName==b.pluginFormatName&&a.manufacturerName.equalsIgnoreCase(b.manufacturerName)
        &&a.name.equalsIgnoreCase(b.name)&&a.uniqueId==b.uniqueId&&a.isInstrument==b.isInstrument;
}
inline void deduplicate(juce::KnownPluginList& list,const juce::File& bundled)
{
    auto types=list.getTypes();juce::Array<juce::PluginDescription> unique;
    for(const auto& d:types){int found=-1;for(int i=0;i<unique.size();++i)if(samePlugin(unique.getReference(i),d)){found=i;break;}
        if(found<0)unique.add(d);else{const auto& previous=unique.getReference(found);const juce::File file(d.fileOrIdentifier),old(previous.fileOrIdentifier);
            const bool prefer=file.exists()&&(!old.exists()||(file.isAChildOf(bundled)&&!old.isAChildOf(bundled))||(!old.isAChildOf(bundled)&&file.getLastModificationTime()>old.getLastModificationTime()));if(prefer)unique.set(found,d);}}
    std::sort(unique.begin(),unique.end(),[](const auto& a,const auto& b){return (a.name+" "+a.manufacturerName).compareIgnoreCase(b.name+" "+b.manufacturerName)<0;});
    list.clear();for(const auto& d:unique)list.addType(d);
}
}
