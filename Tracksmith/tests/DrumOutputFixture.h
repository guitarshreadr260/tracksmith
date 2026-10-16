#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <deque>
class DrumFixture:public juce::AudioProcessor{
 struct Hit{int64_t at;int pitch;};std::deque<Hit> hits;int64_t cursor=0;
public:
 DrumFixture():AudioProcessor(BusesProperties().withOutput("Kick",juce::AudioChannelSet::stereo(),true).withOutput("Snare",juce::AudioChannelSet::mono(),false).withOutput("Room",juce::AudioChannelSet::stereo(),false).withOutput("Unused",juce::AudioChannelSet::stereo(),false)){setLatencySamples(128);}
 const juce::String getName()const override{return "Multichannel drum fixture";}bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}double getTailLengthSeconds()const override{return 0;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
 bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
 void prepareToPlay(double,int)override{reset();}void releaseResources()override{}void reset()override{hits.clear();cursor=0;}
 bool isBusesLayoutSupported(const BusesLayout& layout)const override{return layout.inputBuses.isEmpty()&&layout.outputBuses.size()==4&&layout.outputBuses[0]==juce::AudioChannelSet::stereo();}
 void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override{for(auto event:midi)if(event.getMessage().isNoteOn())hits.push_back({cursor+event.samplePosition+128,event.getMessage().getNoteNumber()});audio.clear();for(int n=0;n<audio.getNumSamples();++n){while(!hits.empty()&&hits.front().at<=cursor+n){const auto hit=hits.front();hits.pop_front();if(hit.pitch==36){audio.setSample(0,n,.5f);audio.setSample(1,n,.25f);}if(hit.pitch==38)audio.setSample(2,n,.3f);audio.setSample(3,n,.1f);audio.setSample(4,n,.1f);}}cursor+=audio.getNumSamples();}
};
