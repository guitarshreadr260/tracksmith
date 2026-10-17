#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "MidiLane.h"
namespace NativeInstruments
{
inline juce::PluginDescription description(bool drums)
{
    juce::PluginDescription d;d.name=d.descriptiveName=drums?"Tracksmith Groove Drums":"Tracksmith NAM";
    d.pluginFormatName="Tracksmith Native";d.manufacturerName="Tracksmith";d.version="1";
    d.fileOrIdentifier=drums?"tracksmith-native-groove-drums":"tracksmith-native-nam";
    d.uniqueId=drums?0x54734744:0x54734e4d;d.isInstrument=drums;d.numInputChannels=drums?0:2;d.numOutputChannels=2;
    return d;
}
std::unique_ptr<juce::AudioProcessor> create(const juce::PluginDescription&);
inline juce::PluginDescription clickDescription(){auto d=description(true);d.name=d.descriptiveName="Tracksmith Click";d.fileOrIdentifier="tracksmith-native-click";d.uniqueId=0x5473434b;d.numOutputChannels=1;return d;}
inline juce::PluginDescription tone3000Description(){auto d=description(false);d.name=d.descriptiveName="TONE3000";d.fileOrIdentifier="tracksmith-native-tone3000";d.uniqueId=0x54735433;d.manufacturerName="TONE3000";return d;}
void setNeuralOptions(juce::AudioProcessor*,bool enabled,float influence);
void setGrooveLibrary(juce::AudioProcessor*,const juce::File&);
void audition(juce::AudioProcessor*,int drum);
struct GrooveInfo { bool busy=false,valid=false;double bpm=0,confidence=0;int hits=0;juce::String status,kitName; };
GrooveInfo grooveInfo(juce::AudioProcessor*);
bool analyseReference(juce::AudioProcessor*,const juce::File&,double offset,double length,double songBeat,double tempo=0);
std::vector<MidiNote> grooveNotes(juce::AudioProcessor*);
juce::Array<juce::var> grooveNoteMap(juce::AudioProcessor*);
void bindGrooveToSong(juce::AudioProcessor*,std::function<void()>);
void useSongMidi(juce::AudioProcessor*);
bool consumeGrooveChanges(juce::AudioProcessor*);
}
std::unique_ptr<juce::AudioProcessor> createTracksmithNativeNam();

std::unique_ptr<juce::AudioProcessor> createTracksmithTone3000();

bool consumeTracksmithTone3000Changes(juce::AudioProcessor*);
