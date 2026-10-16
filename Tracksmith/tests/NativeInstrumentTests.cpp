#include "../source/NativeInstruments.h"
#include "../source/PluginRack.h"
#include "../source/SessionFile.h"
#include "../source/GrooveNative/Processor.h"
#include "../plugins/AvrxNam/NativeNam.h"
#include <iostream>
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    auto rack=std::make_unique<PluginRack>();rack->midiLanes[0].enabled=true;
    auto d=NativeInstruments::description(true);require(rack->install(0,0,d,NativeInstruments::create(d),{},false,48000).wasOk(),"Install compiled Groove drum section");
    auto* drums=rack->channels[0][0]->instance.get();juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
    audio.clear();drums->processBlock(audio,midi);require(audio.getMagnitude(0,256)==0,"Idle drum engine is silent");
    midi.addEvent(juce::MidiMessage::noteOn(10,36,.9f),64);drums->processBlock(audio,midi);
    require(audio.getMagnitude(0,64)==0&&audio.getMagnitude(64,192)>.001f,"Drum MIDI sample timing and audible output");
    drums->reset();midi.clear();drums->processBlock(audio,midi);require(audio.getMagnitude(0,256)==0,"Reset clears drum tails");
    NativeInstruments::audition(drums,1);drums->processBlock(audio,midi);require(audio.getMagnitude(0,256)>.001f,"Pad audition works without transport");
    {std::unique_ptr<juce::AudioProcessorEditor> editor(drums->createEditor());require(editor&&editor->getWidth()>=1000,"Full drum-only Groove editor opens natively");}
    juce::AudioProcessorParameter* level=nullptr;for(auto* candidate:drums->getParameters())if(PluginRack::parameterId(candidate)=="level")level=candidate;require(level!=nullptr,"Kit level parameter exists");require(PluginRack::parameterId(level)=="level","Native parameters expose stable automation IDs");level->setValueNotifyingHost(.3f);
    const auto saved=rack->save();require(saved.size()==1,"Native insert is included in session save");
    juce::MemoryBlock state;state.fromBase64Encoding(saved[0].getProperty("state",{}).toString());
    require(rack->install(0,0,d,NativeInstruments::create(d),state,false,48000).wasOk(),"Restore native drum state");juce::AudioProcessorParameter* restoredLevel=nullptr;for(auto* candidate:rack->channels[0][0]->instance->getParameters())if(PluginRack::parameterId(candidate)=="level")restoredLevel=candidate;require(restoredLevel&&std::abs(restoredLevel->getValue()-.3f)<.001f,"Drum control settings restore");
    auto nam=NativeInstruments::description(false);require(rack->install(1,0,nam,NativeInstruments::create(nam),{},false,48000).wasOk(),"NAM installs without a VST3 module");
    require(PluginRack::parameterId(rack->channels[1][0]->instance->getParameters()[0]).isNotEmpty(),"NAM parameters available to DAW");
    SavedSession session;session.tracks.resize(2);session.tracks[0].name="Drums";session.tracks[0].midiTrack=true;session.tracks[1].name="Guitar";session.inserts=rack->save();SavedSession restored;
    const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("work/native-instrument-test.tracksmith");
    require(SessionFile::decode(SessionFile::encode(session,file),file,restored).wasOk()&&restored.inserts.size()==2,"Session format preserves both native processors");
    for(const auto& item:restored.inserts){juce::PluginDescription desc;auto xml=juce::parseXML(item.getProperty("description",{}).toString());require(xml&&desc.loadFromXml(*xml),"Native description survives session roundtrip");juce::MemoryBlock bytes;bytes.fromBase64Encoding(item.getProperty("state",{}).toString());const int channel=static_cast<int>(item.getProperty("channel",-1));require(rack->install(channel,0,desc,NativeInstruments::create(desc),bytes,false,48000).wasOk(),"Restore both instruments without plugin scanning");}
    auto* groove=dynamic_cast<TracksmithNativeGroove::DrumsProcessor*>(rack->channels[0][0]->instance.get());require(groove!=nullptr,"Actual preserved drum-only processor is installed");
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("work/native-groove-samples");folder.createDirectory();const auto sample=folder.getChildFile("kick.wav");
    {juce::WavAudioFormat format;auto stream=sample.createOutputStream();std::unique_ptr<juce::OutputStream> output=std::move(stream);auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions().withSampleRate(48000).withNumChannels(1).withBitsPerSample(24));require(writer!=nullptr,"Create sampler test WAV");juce::AudioBuffer<float> wave(1,256);for(int n=0;n<256;++n)wave.setSample(0,n,.25f);require(writer->writeFromAudioSampleBuffer(wave,0,256),"Write sampler test WAV");}
    auto pump=[&]{const auto end=juce::Time::getMillisecondCounterHiRes()+5000;while(groove->isLoading()&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);juce::MessageManager::getInstance()->runDispatchLoopUntil(80);require(!groove->isLoading(),"Background drum loading completes");};pump();
    const auto pack=folder.getChildFile("kit.avrxpack");pack.replaceWithText("<AVRX_PACK version=\"1\" name=\"Test kit\"><Pad index=\"0\"><Velocity upper=\"1\"><Take file=\"kick.wav\"/></Velocity></Pad></AVRX_PACK>");groove->requestFile(1,pack);pump();require(!groove->status().contains("failed"),"Drum-only pack importer works");
    groove->reset();midi.clear();groove->audition(0);groove->processBlock(audio,midi);require(audio.getMagnitude(0,256)>.001f,"Loaded sample pack renders audio");
    groove->requestFile(2,sample,1,1);pump();juce::MemoryBlock packedState;groove->getStateInformation(packedState);auto packedXml=juce::AudioProcessor::getXmlFromBinary(packedState.getData(),static_cast<int>(packedState.getSize()));require(packedXml&&packedXml->getStringAttribute("packPath")==pack.getFullPathName()&&packedXml->getStringAttribute("sample1_1")==sample.getFullPathName(),"Pack and layered WAV references persist");
    require(groove->exportMidi(folder.getChildFile("pattern.mid")).wasOk(),"Native pattern MIDI export works");
    {auto copy=NativeInstruments::create(d);copy->setRateAndBufferSizeDetails(48000,256);copy->prepareToPlay(48000,256);copy->setStateInformation(packedState.getData(),static_cast<int>(packedState.getSize()));auto* previous=groove;groove=dynamic_cast<TracksmithNativeGroove::DrumsProcessor*>(copy.get());pump();groove->reset();groove->audition(0);groove->processBlock(audio,midi);require(audio.getMagnitude(0,256)>.001f&&!groove->status().contains("failed"),"Loaded sample pack restores in an independent native instance");groove=previous;}
    {auto legacy=NativeInstruments::create(d);juce::ValueTree old("TRACKSMITH_GROOVE_DRUMS"),parameter("PARAM");parameter.setProperty("id","level",nullptr);parameter.setProperty("value",-12.f,nullptr);old.addChild(parameter,-1,nullptr);juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(*old.createXml(),bytes);legacy->setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));auto* converted=dynamic_cast<TracksmithNativeGroove::DrumsProcessor*>(legacy.get());require(std::abs(converted->raw("level")-juce::Decibels::decibelsToGain(-12.f))<.006f,"Prototype 05 kit level migrates to full drum processor");}
    if(argc>=2){NativeNam model;juce::String error;require(model.load(juce::File(argv[1]),48000,256,error),"Default guitar capture loads");require(model.prepare(48000,256,error),"Default guitar capture prepares");for(int ch=0;ch<2;++ch)for(int n=0;n<256;++n)audio.setSample(ch,n,.05f*std::sin(n*.1f));model.process(audio,256);require(audio.getMagnitude(0,256)>0,"NAM default model renders audio");for(int n=0;n<256;++n)require(std::isfinite(audio.getSample(0,n)),"NAM output is finite");}
    if(argc>=3){juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> found;format.findAllTypesForFile(found,argv[2]);require(!found.isEmpty(),"Find existing VST3 effect");juce::String error;auto effect=rack->formats.createPluginInstance(*found[0],48000,256,error);require(effect!=nullptr,"Existing VST3 still loads");require(rack->install(1,1,*found[0],std::move(effect),{},false,48000).wasOk(),"Existing VST3 coexists with native NAM");require(!PluginRack::parameterId(rack->channels[1][1]->instance->getParameters()[0]).isEmpty(),"VST3 parameter IDs still available");}
    std::cout<<"Native NAM installation, Groove drum MIDI/audio, pad audition, silence, parameter IDs and state restore passed.\n";
}



