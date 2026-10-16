#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>

class NativeClickProcessor final : public juce::AudioProcessor
{
public:
 static constexpr const char* identifier="tracksmith-native-click";
 std::atomic<bool> transportEnabled{true}, recording{false}, offline{false};
 juce::AudioProcessorValueTreeState parameters;
 static juce::AudioProcessorValueTreeState::ParameterLayout layout(){
  juce::AudioProcessorValueTreeState::ParameterLayout p;
  p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"accent",1},"Downbeat level",0.f,1.f,.75f));
  p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"beat",1},"Other beat level",0.f,1.f,.5f));
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sound",1},"Sound",juce::StringArray{"Digital","Wood","Rim"},0));
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"division",1},"Click value",juce::StringArray{"Quarter notes","Eighth notes","Sixteenth notes"},0));
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"When to click",juce::StringArray{"Play and record","Record only"},0));
  p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"export",1},"Include in mix export",false));return p;
 }
 NativeClickProcessor():AudioProcessor(BusesProperties().withOutput("Click",juce::AudioChannelSet::mono(),true)),parameters(*this,nullptr,"TRACKSMITH_CLICK",layout()){}
 const juce::String getName()const override{return "Tracksmith Click";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return .035;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
 bool isBusesLayoutSupported(const BusesLayout& b)const override{return b.inputBuses.isEmpty()&&b.getMainOutputChannelSet()==juce::AudioChannelSet::mono();}
 void prepareToPlay(double sr,int)override{rate=sr;reset();}void releaseResources()override{}void reset()override{remaining=0;expected=-1;lastTick=-1;previousDivision=-1;}
 void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override{
  audio.clear();midi.clear();auto* head=getPlayHead();const auto pos=head?head->getPosition():juce::Optional<juce::AudioPlayHead::PositionInfo>{};
  if(!pos||!pos->getIsPlaying()||!transportEnabled.load()||(parameters.getRawParameterValue("mode")->load()>.5f&&!recording.load())||(offline.load()&&parameters.getRawParameterValue("export")->load()<.5f)){reset();return;}
  const double tempo=pos->getBpm().orFallback(120.), ppq=pos->getPpqPosition().orFallback(0.);if(rate<=0||!std::isfinite(tempo)||tempo<=0||!std::isfinite(ppq)){reset();return;}
  const int division=1<<juce::jlimit(0,2,(int)parameters.getRawParameterValue("division")->load());
  const auto sig=pos->getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature{4,4});
  const double barBeats=std::max(1,sig.numerator)*4.0/std::max(1,sig.denominator), increment=tempo/(60*rate);
  if(expected<0||std::abs(ppq-expected)>increment*2||division!=previousDivision){remaining=0;const double tick=ppq*division;lastTick=(int64_t)std::floor(tick+1e-8);if(std::abs(tick-std::round(tick))<1e-7)--lastTick;}
  previousDivision=division;const int sound=juce::jlimit(0,2,(int)parameters.getRawParameterValue("sound")->load());
  for(int n=0;n<audio.getNumSamples();++n){const double beat=ppq+n*increment;const auto tick=(int64_t)std::floor(beat*division+1e-8);
   if(tick!=lastTick){lastTick=tick;const double inBar=std::fmod((double)tick/division,barBeats);const bool accent=std::abs(inBar)<1e-7;length=std::max(1,(int)(rate*(sound==0?.012:.028)));remaining=length;frequency=accent?1800.:1200.;amplitude=.25f*parameters.getRawParameterValue(accent?"accent":"beat")->load();}
   float value=0;if(remaining>0){const double t=(length-remaining)/rate,envelope=(double)remaining/length;value=amplitude*(float)(envelope*envelope*(sound==0?std::sin(juce::MathConstants<double>::twoPi*frequency*t):sound==1?std::sin(juce::MathConstants<double>::twoPi*frequency*.55*t):(.65*std::sin(juce::MathConstants<double>::twoPi*frequency*1.7*t)+.35*std::sin(juce::MathConstants<double>::twoPi*frequency*2.31*t))));--remaining;}for(int ch=0;ch<audio.getNumChannels();++ch)audio.setSample(ch,n,value);
  }expected=ppq+audio.getNumSamples()*increment;
 }
 void getStateInformation(juce::MemoryBlock& data)override{if(auto xml=parameters.copyState().createXml())copyXmlToBinary(*xml,data);}
 void setStateInformation(const void* data,int size)override{if(auto xml=getXmlFromBinary(data,size))if(xml->hasTagName("TRACKSMITH_CLICK"))parameters.replaceState(juce::ValueTree::fromXml(*xml));reset();}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
private:double rate=48000,expected=-1,frequency=1200;int64_t lastTick=-1;int previousDivision=-1,remaining=0,length=1;float amplitude=0;
};

class NativeClickEditor final : public juce::AudioProcessorEditor {
public:
 explicit NativeClickEditor(NativeClickProcessor& p):AudioProcessorEditor(p){
  setSize(520,352);for(auto* s:{&accent,&beat}){s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,64,28);s->setColour(juce::Slider::thumbColourId,juce::Colour(0xffe4e8ec));s->setColour(juce::Slider::trackColourId,juce::Colour(0xff8a9eae));addAndMakeVisible(s);}
  accent.setName("Downbeat level");beat.setName("Other beat level");accentAttachment=std::make_unique<SA>(p.parameters,"accent",accent);beatAttachment=std::make_unique<SA>(p.parameters,"beat",beat);
  sound.addItemList({"Digital","Wood","Rim"},1);division.addItemList({"Quarter notes","Eighth notes","Sixteenth notes"},1);mode.addItemList({"Play and record","Record only"},1);for(auto* c:{&sound,&division,&mode})addAndMakeVisible(c);sound.setName("Click sound");division.setName("Click value");mode.setName("When to click");soundAttachment=std::make_unique<CA>(p.parameters,"sound",sound);divisionAttachment=std::make_unique<CA>(p.parameters,"division",division);modeAttachment=std::make_unique<CA>(p.parameters,"mode",mode);addAndMakeVisible(exportClick);exportAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"export",exportClick);
 }
 void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff111316));g.setColour(juce::Colour(0xfff1f3f4));g.setFont(juce::FontOptions(25).withStyle("Bold"));g.drawText("TRACKSMITH CLICK",24,18,470,40,juce::Justification::centredLeft);g.setColour(juce::Colour(0xff9da6af));g.setFont(13);g.drawText("Follows session tempo  /  First beat accented",24,61,470,24,juce::Justification::centredLeft);for(auto row:std::array<std::pair<int,const char*>,5>{{{105,"Downbeat"},{149,"Other beats"},{197,"Sound"},{239,"Click value"},{281,"Playback"}}})g.drawText(row.second,24,row.first,126,28,juce::Justification::centredLeft);}
 void resized()override{accent.setBounds(150,105,342,28);beat.setBounds(150,149,342,28);sound.setBounds(150,197,342,28);division.setBounds(150,239,342,28);mode.setBounds(150,281,342,28);exportClick.setBounds(150,318,342,24);}
private:using SA=juce::AudioProcessorValueTreeState::SliderAttachment;using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;juce::Slider accent,beat;juce::ComboBox sound,division,mode;juce::ToggleButton exportClick{"Include click in mix export"};std::unique_ptr<SA> accentAttachment,beatAttachment;std::unique_ptr<CA> soundAttachment,divisionAttachment,modeAttachment;std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> exportAttachment;
};
inline juce::AudioProcessorEditor* NativeClickProcessor::createEditor(){return new NativeClickEditor(*this);}
