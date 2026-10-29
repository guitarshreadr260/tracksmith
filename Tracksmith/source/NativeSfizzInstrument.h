#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "sfizz.h"

// A first native sample-instrument bridge. The existing standalone engines and
// their editors are separate; this processor only hosts the bundled SFZ sound.
class NativeSfizzInstrument final : public juce::AudioProcessor {
public:
 explicit NativeSfizzInstrument(bool bass):AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),isBass(bass),parameters(*this,nullptr,"TRACKSMITH_SFZ",makeLayout()){}
 ~NativeSfizzInstrument() override { if(synth&&api.freeSynth)api.freeSynth(synth); }
 static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(){juce::AudioProcessorValueTreeState::ParameterLayout p;p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"level",1},"Level",0.0f,1.0f,0.7f));return p;}
 const juce::String getName()const override{return isBass?"Tracksmith Bass (Preview)":"Tracksmith Piano (Preview)";}
 bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 1.0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
 bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.inputBuses.isEmpty()&&l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
 void prepareToPlay(double sr,int block)override{
  if(synth&&api.freeSynth){api.freeSynth(synth);synth=nullptr;}ready=false;maxBlock=juce::jmax(1,block);
  auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Native Instruments").getChildFile("Tracksmith Sounds");
  if(!api.open(root.getChildFile("sfizz.dll")))return;synth=api.create();if(!synth)return;api.rate(synth,(float)sr);api.block(synth,maxBlock);
  auto sfz=root.getChildFile(isBass?"Bass/bass.sfz":"Piano/meta-piano.sfz");if(!sfz.existsAsFile())return;ready=api.load(synth,sfz.getFullPathName().toRawUTF8());
 }
 void releaseResources()override{} void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi)override{
  buffer.clear();if(!ready||!synth||buffer.getNumChannels()<2)return;
  const int count=buffer.getNumSamples();int cursor=0;
  auto render=[&](int end){while(cursor<end){int n=juce::jmin(maxBlock,end-cursor);float* channels[]={buffer.getWritePointer(0,cursor),buffer.getWritePointer(1,cursor)};api.render(synth,channels,2,n);cursor+=n;}};
  for(const auto meta:midi){int t=juce::jlimit(0,count,meta.samplePosition);render(t);auto msg=meta.getMessage();if(msg.isNoteOn())api.on(synth,0,msg.getNoteNumber(),(int)msg.getVelocity());else if(msg.isNoteOff())api.off(synth,0,msg.getNoteNumber(),0);else if(msg.isController())api.cc(synth,0,msg.getControllerNumber(),msg.getControllerValue());else if(msg.isPitchWheel())api.bend(synth,0,msg.getPitchWheelValue()-8192);}
  render(count);buffer.applyGain(parameters.getRawParameterValue("level")->load());
 }
 void getStateInformation(juce::MemoryBlock& data)override{if(auto xml=parameters.copyState().createXml())copyXmlToBinary(*xml,data);}
 void setStateInformation(const void* data,int size)override{if(auto xml=getXmlFromBinary(data,size))if(xml->hasTagName("TRACKSMITH_SFZ"))parameters.replaceState(juce::ValueTree::fromXml(*xml));}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override{return new juce::GenericAudioProcessorEditor(*this);}
private:
 struct Api {bool available=false;juce::DynamicLibrary dll;decltype(&sfizz_create_synth) create=nullptr;decltype(&sfizz_free) freeSynth=nullptr;decltype(&sfizz_load_file) load=nullptr;decltype(&sfizz_set_sample_rate) rate=nullptr;decltype(&sfizz_set_samples_per_block) block=nullptr;decltype(&sfizz_send_note_on) on=nullptr;decltype(&sfizz_send_note_off) off=nullptr;decltype(&sfizz_send_cc) cc=nullptr;decltype(&sfizz_send_pitch_wheel) bend=nullptr;decltype(&sfizz_render_block) render=nullptr;
  bool open(const juce::File& file){if(available)return true;if(!dll.open(file.getFullPathName()))return false;
#define LOAD_API(field,name) field=reinterpret_cast<decltype(field)>(dll.getFunction(name));if(!field)return false
  LOAD_API(create,"sfizz_create_synth");LOAD_API(freeSynth,"sfizz_free");LOAD_API(load,"sfizz_load_file");LOAD_API(rate,"sfizz_set_sample_rate");LOAD_API(block,"sfizz_set_samples_per_block");LOAD_API(on,"sfizz_send_note_on");LOAD_API(off,"sfizz_send_note_off");LOAD_API(cc,"sfizz_send_cc");LOAD_API(bend,"sfizz_send_pitch_wheel");LOAD_API(render,"sfizz_render_block");
#undef LOAD_API
  available=true;return true;}} api;
 bool isBass=false,ready=false;int maxBlock=512;sfizz_synth_t* synth=nullptr;juce::AudioProcessorValueTreeState parameters;
};
