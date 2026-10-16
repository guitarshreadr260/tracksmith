#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DrumKit.h"
#include "GrooveAnalysis.h"
namespace TracksmithNativeGroove {
class DrumsProcessor final : public juce::AudioProcessor, private juce::Thread, private juce::Timer
{
public:
 DrumsProcessor();~DrumsProcessor() override;
 juce::AudioProcessorValueTreeState parameters;
 const juce::String getName() const override{return "Tracksmith Groove";}
 bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 3;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};}void changeProgramName(int,const juce::String&)override{}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 void prepareToPlay(double,int)override;void releaseResources()override{};void reset()override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 void audition(int drum){pendingPad=drum;}void toggleStep(int drum,int step);
 float stepVelocity(int drum,int step);int bars()const{return static_cast<int>(raw("bars"));}
 juce::String status();void requestFile(int kind,const juce::File&,int drum=0,int layer=0);
 void clearLayer(int drum,int layer);juce::Result exportMidi(const juce::File&);
 std::atomic<int> displayedStep{-1};std::array<std::atomic<float>,9> peaks{};
 bool isLoading()const{return loading.load();}
 float raw(const juce::String& id)const{return parameters.getRawParameterValue(id)->load();}
private:
 struct PadControl {std::atomic<float>* gain=nullptr;std::atomic<float>* pan=nullptr;std::atomic<float>* tune=nullptr;std::atomic<float>* mute=nullptr;std::atomic<float>* solo=nullptr;std::atomic<float>* eq=nullptr;std::atomic<float>* comp=nullptr;std::atomic<float>* threshold=nullptr;std::atomic<float>* ratio=nullptr;std::array<std::atomic<float>*,4> bands{};};
 std::array<PadControl,9> padControls;
 std::atomic<float>* sequenceValue=nullptr;std::atomic<float>* barsValue=nullptr;std::atomic<float>* levelValue=nullptr;std::atomic<float>* roomValue=nullptr;std::atomic<float>* overheadValue=nullptr;
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 void run()override;void timerCallback()override;void trigger(int,float);
 std::unique_ptr<DrumKit> kit=std::make_unique<DrumKit>(),pending;
 std::array<LayeredDrumVoice,9> voices;std::array<DrumChannelStrip,9> strips;DrumAmbience ambience;
 std::atomic<int> readers{0},pendingPad{-1};std::atomic<bool> updating{false};
 std::atomic<bool> loading{false};
 bool wasPlaying=false;double rate=48000,lastPpq=-100;int64_t lastCell=-1;float smoothLevel=.5f;
 juce::CriticalSection jobs;juce::String message{"Built-in drum voices ready. Load a WAV layer or an existing .avrxpack."},packPath;
 juce::File requested;int jobKind=0,jobDrum=0,jobLayer=0;bool jobPending=false;juce::ValueTree restored;
};
}
