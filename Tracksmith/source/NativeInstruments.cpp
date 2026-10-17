#include "NativeInstruments.h"
#include "NativeClick.h"
#include "GrooveFull/PluginProcessor.h"
using Groove=TracksmithGrooveFull::AvrxGrooveProcessor;
class NativeGroove final : public Groove
{
public:
    double songBeat=0;
    void getStateInformation(juce::MemoryBlock& bytes)override{Groove::getStateInformation(bytes);if(auto xml=getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize()))){xml->setAttribute("songStartBeat",songBeat);copyXmlToBinary(*xml,bytes);}}
    void setStateInformation(const void* bytes,int size)override
    {
        auto xml=getXmlFromBinary(bytes,size);if(!xml)return;
        if(xml->hasTagName("AVRX_DRUMS")||xml->hasTagName("TRACKSMITH_GROOVE_DRUMS")){
            auto state=juce::ValueTree::fromXml(*xml);const bool early=state.hasType("TRACKSMITH_GROOVE_DRUMS");
            auto value=[&](juce::String id,float fallback){for(int i=0;i<state.getNumChildren();++i){auto p=state.getChild(i);if(p.getProperty("id").toString()==id)return static_cast<float>(p.getProperty("value",fallback));}return fallback;};
            const auto pack=state.getProperty("packPath").toString();if(juce::File::isAbsolutePath(pack)&&juce::File(pack).existsAsFile()){loadDrumPack(juce::File(pack));waitForBackgroundWork();}
            auto* output=parameters.getParameter("level");output->setValueNotifyingHost(output->convertTo0to1(early?juce::Decibels::decibelsToGain(value("level",-4)):value("level",.5f)));
            setAmbience(value("room",0),value("overhead",0));setSequence(static_cast<int>(value("bars",4)),4,4,4);
            for(int d=0;d<9;++d){const auto prefix="pad"+juce::String(d)+"_";setPadGain(d,early?juce::Decibels::decibelsToGain(value("gain"+juce::String(d),0)):value(prefix+"gain",1));setPadPan(d,value(prefix+"pan",0));setPadMuted(d,value(prefix+"mute",0)>.5f);setPadSolo(d,value(prefix+"solo",0)>.5f);setTuning(d,early?value("tune"+juce::String(d),0):value(prefix+"tune",0),0);
                TracksmithGrooveFull::DrumShaping shape;shape.eqOn=value(prefix+"eq",0)>.5f;shape.compressorOn=value(prefix+"comp",0)>.5f;shape.thresholdDb=value(prefix+"threshold",-18);shape.ratio=value(prefix+"ratio",4);for(int b=0;b<4;++b)shape.gainDb[b]=value(prefix+"eq"+juce::String(b),0);setDrumShaping(d,shape);
                for(int layer=0;layer<4;++layer){const auto path=state.getProperty("sample"+juce::String(d)+"_"+juce::String(layer)).toString();if(juce::File::isAbsolutePath(path)&&juce::File(path).existsAsFile()){loadSample(d,juce::File(path),layer);waitForBackgroundWork();}}
                if(!early){const auto cells=juce::StringArray::fromTokens(state.getProperty("pattern"+juce::String(d)).toString(),",","");for(int n=0;n<getKit()->stepCount();++n)setStepVelocity(d,n,n<cells.size()?cells[n].getFloatValue():0);}}
            parameters.getParameter("run")->setValueNotifyingHost(value("sequence",0)>.5f?1.f:0.f);nativeStateChanged=false;return;
        }
        songBeat=std::max(0.0,xml->getDoubleAttribute("songStartBeat",0));Groove::setStateInformation(bytes,size);nativeStateChanged=false;
    }
};
std::unique_ptr<juce::AudioProcessor> NativeInstruments::create(const juce::PluginDescription& d)
{
    if(d.pluginFormatName!="Tracksmith Native")return {};
    if(d.fileOrIdentifier==NativeClickProcessor::identifier)return std::make_unique<NativeClickProcessor>();
    if(d.fileOrIdentifier=="tracksmith-native-tone3000")return createTracksmithTone3000();
    if(d.fileOrIdentifier=="tracksmith-native-nam")return createTracksmithNativeNam();
    if(d.fileOrIdentifier=="tracksmith-native-groove-drums")return std::make_unique<NativeGroove>();return {};
}
void NativeInstruments::setNeuralOptions(juce::AudioProcessor* p,bool enabled,float influence){if(auto* g=dynamic_cast<Groove*>(p)){g->requestedNeural=enabled;g->referenceInfluence=juce::jlimit(0.f,1.f,influence);}}
void NativeInstruments::setGrooveLibrary(juce::AudioProcessor* p,const juce::File& folder){if(auto* g=dynamic_cast<Groove*>(p))g->grooveFolder=folder;}
void NativeInstruments::audition(juce::AudioProcessor* p,int drum){if(auto* kit=dynamic_cast<Groove*>(p))if(drum>=0&&drum<9)kit->auditionPad(drum);}
NativeInstruments::GrooveInfo NativeInstruments::grooveInfo(juce::AudioProcessor* p){GrooveInfo result;if(auto* kit=dynamic_cast<Groove*>(p)){result.busy=kit->isAnalysing()||kit->isLoadingSample();result.status=kit->getAnalysisStatus();result.kitName=kit->getKit()->packName;if(auto analysis=kit->getAnalysis()){result.valid=analysis->valid;result.bpm=analysis->bpm;result.confidence=analysis->confidence;result.hits=analysis->onsetCount;}}return result;}
bool NativeInstruments::analyseReference(juce::AudioProcessor* p,const juce::File& file,double offset,double length,double songBeat,double tempo){if(auto* kit=dynamic_cast<NativeGroove*>(p)){if(kit->isAnalysing()||kit->isLoadingSample())return false;kit->songBeat=songBeat;kit->requestedSongTempo=tempo;kit->loadAndAnalyse(file,true,offset,length);return true;}return false;}
std::vector<MidiNote> NativeInstruments::grooveNotes(juce::AudioProcessor* p){std::vector<MidiNote> notes;if(auto* processor=dynamic_cast<NativeGroove*>(p)){const auto kit=processor->getKit();int id=1;if(const auto performance=std::atomic_load(&processor->neuralPerformance)){for(const auto& n:*performance)notes.push_back({id++,kit->notes[n.drum],kit->midiChannel,processor->songBeat+n.beat,n.length,n.velocity});return notes;}for(int d=0;d<9;++d)for(int cell=0;cell<kit->stepCount();++cell){const float v=kit->pattern[d][cell];if(v>0)notes.push_back({id++,kit->notes[d],kit->midiChannel,processor->songBeat+static_cast<double>(cell)/kit->division,.5/kit->division,v});}}return notes;}
void NativeInstruments::bindGrooveToSong(juce::AudioProcessor* p,std::function<void()> callback){if(auto* kit=dynamic_cast<Groove*>(p))kit->sendGrooveToSong=std::move(callback);}
juce::Array<juce::var> NativeInstruments::grooveNoteMap(juce::AudioProcessor* p){juce::Array<juce::var> result;if(auto* kit=dynamic_cast<Groove*>(p))for(int note:kit->getKit()->notes)result.add(note);return result;}
void NativeInstruments::useSongMidi(juce::AudioProcessor* p){if(auto* kit=dynamic_cast<Groove*>(p))kit->stopGroovePreview();}

bool NativeInstruments::consumeGrooveChanges(juce::AudioProcessor* p){if(auto* kit=dynamic_cast<Groove*>(p))return kit->nativeStateChanged.exchange(false);return consumeTracksmithTone3000Changes(p);}
