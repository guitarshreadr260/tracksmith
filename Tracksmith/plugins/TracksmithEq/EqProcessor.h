#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "EqDsp.h"
class EqProcessor final : public juce::AudioProcessor {
public:
 juce::AudioProcessorValueTreeState parameters;
 std::atomic<float> inputPeak{0},outputPeak{0};std::atomic<double> displayRate{48000};
 EqProcessor();
 const juce::String getName()const override{return "Tracksmith EQ 7";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 1;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
 bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.getMainInputChannelSet()==b.getMainOutputChannelSet()&&(b.getMainInputChannelSet()==juce::AudioChannelSet::mono()||b.getMainInputChannelSet()==juce::AudioChannelSet::stereo());}
 void prepareToPlay(double rate,int)override;void releaseResources()override{}void reset()override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 juce::AudioProcessorParameter* getBypassParameter()const override{return parameters.getParameter("bypass");}
 static juce::String id(int band,const char* name){return "b"+juce::String(band)+"_"+name;}
 std::array<TracksmithEq::Band,7> bands()const;
 float value(const juce::String& name)const{return parameters.getRawParameterValue(name)->load();}
 void change(const juce::String& name,float plain){if(auto* p=parameters.getParameter(name))p->setValueNotifyingHost(p->convertTo0to1(plain));}
private:
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 TracksmithEq::Engine engine;
 struct BandValues {std::atomic<float>* enabled,*frequency,*q,*gain=nullptr,*mode=nullptr,*slope=nullptr;};std::array<BandValues,7> raw;
 std::atomic<float> *in,*out,*bypass,*invert;
};
