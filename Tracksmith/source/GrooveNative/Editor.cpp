#include "Processor.h"
namespace TracksmithNativeGroove {
class DrumGrid final : public juce::Component
{
public:
 explicit DrumGrid(DrumsProcessor& p):processor(p){}
 std::function<void(int)> onSelect;
 void paint(juce::Graphics& g)override
 {
  g.fillAll(juce::Colour(0xff161b23));const int count=processor.bars()*16;const float width=static_cast<float>(getWidth()-110)/count;
  for(int d=0;d<9;++d){g.setColour(juce::Colours::lightgrey);g.setFont(12);g.drawText(GrooveGenerator::names[d],2,d*27,106,27,juce::Justification::centredLeft);
   for(int n=0;n<count;++n){const float value=processor.stepVelocity(d,n);g.setColour(value>0?juce::Colour(0xffd1e4ef):n%4==0?juce::Colour(0xff384759):juce::Colour(0xff29313f));g.fillRect(110+n*width,d*27.0f+3,width-2,21.0f);if(processor.displayedStep.load()==n){g.setColour(juce::Colours::white);g.drawRect(110+n*width,d*27.0f+3,width-2,21.0f,1.0f);}}}
 }
 void mouseDown(const juce::MouseEvent& e)override{if(!isEnabled())return;const int d=e.y/27,count=processor.bars()*16;const int cell=static_cast<int>((e.x-110.0f)/(getWidth()-110)*count);if(e.x<110&&d>=0&&d<9){processor.audition(d);if(onSelect)onSelect(d);return;}if(e.x>=110&&d<9&&d>=0&&cell>=0&&cell<count)processor.toggleStep(d,cell);repaint();}
private:DrumsProcessor& processor;
};
class DrumsEditor final : public juce::AudioProcessorEditor,private juce::Timer
{
public:
 explicit DrumsEditor(DrumsProcessor& p):AudioProcessorEditor(p),processor(p),grid(p),sequenceAttachment(p.parameters,"sequence",sequence),levelAttachment(p.parameters,"level",level),roomAttachment(p.parameters,"room",room),overheadAttachment(p.parameters,"overhead",overhead)
 {
  setSize(1100,730);title.setText("TRACKSMITH GROOVE · Groove / Sound Lab",juce::dontSendNotification);title.setFont(juce::FontOptions(23));addAndMakeVisible(title);
  for(auto* button:{&loadPack,&loadSample,&clearSample,&analyse,&exportMidi,&audition})addAndMakeVisible(button);
  loadPack.setButtonText("Load .avrxpack");loadSample.setButtonText("Load WAV layer");clearSample.setButtonText("Clear layer");analyse.setButtonText("Analyze reference audio");exportMidi.setButtonText("Export pattern MIDI");audition.setButtonText("Audition pad");
  loadPack.onClick=[this]{choose(1,"*.avrxpack");};loadSample.onClick=[this]{choose(2,"*.wav;*.aiff;*.flac");};analyse.onClick=[this]{choose(3,"*.wav;*.aiff;*.flac");};clearSample.onClick=[this]{processor.clearLayer(selected,layer.getSelectedId()-1);};audition.onClick=[this]{processor.audition(selected);};exportMidi.onClick=[this]{choose(5,"*.mid");};
  sequence.setButtonText("Play internal pattern (off = song MIDI)");addAndMakeVisible(sequence);
  for(int d=0;d<9;++d)pad.addItem(GrooveGenerator::names[d],d+1);pad.setSelectedId(1);pad.onChange=[this]{selected=pad.getSelectedId()-1;attachPad();};addAndMakeVisible(pad);
  for(int n=1;n<=4;++n)layer.addItem("Layer "+juce::String(n),n);layer.setSelectedId(1);addAndMakeVisible(layer);
  for(int n=1;n<=8;++n)bars.addItem(juce::String(n)+" bar"+(n>1?"s":""),n);bars.setSelectedId(processor.bars());bars.onChange=[this]{auto* parameter=processor.parameters.getParameter("bars");parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(bars.getSelectedId())));grid.repaint();};addAndMakeVisible(bars);
  for(auto* slider:{&level,&room,&overhead}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,20);addAndMakeVisible(slider);}
  outputLabel.setText("Output",juce::dontSendNotification);roomLabel.setText("Room",juce::dontSendNotification);overheadLabel.setText("Overhead",juce::dontSendNotification);for(auto* label:{&outputLabel,&roomLabel,&overheadLabel,&message})addAndMakeVisible(label);
  const std::array<const char*,9> names{"Gain","Pan","Tune","Low EQ","Low-mid EQ","High-mid EQ","High EQ","Threshold","Ratio"};
  for(int n=0;n<9;++n){labels[n].setText(names[n],juce::dontSendNotification);addAndMakeVisible(labels[n]);knobs[n].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);knobs[n].setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,20);addAndMakeVisible(knobs[n]);}
  const std::array<const char*,4> toggles{"Mute","Solo","EQ active","Comp active"};for(int n=0;n<4;++n){buttons[n].setButtonText(toggles[n]);addAndMakeVisible(buttons[n]);}
  grid.onSelect=[this](int drum){pad.setSelectedId(drum+1);};addAndMakeVisible(grid);attachPad();startTimerHz(20);
 }
 void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff1a202b));}
 void resized()override
 {
  title.setBounds(20,10,780,35);loadPack.setBounds(20,55,145,30);analyse.setBounds(175,55,200,30);exportMidi.setBounds(385,55,170,30);bars.setBounds(565,55,110,30);sequence.setBounds(700,55,370,30);
  grid.setBounds(20,100,getWidth()-40,245);pad.setBounds(20,365,150,30);layer.setBounds(180,365,110,30);loadSample.setBounds(300,365,150,30);clearSample.setBounds(460,365,110,30);audition.setBounds(580,365,130,30);
  for(int n=0;n<4;++n)buttons[n].setBounds(730+(n%2)*160,365+(n/2)*30,150,28);
  for(int n=0;n<9;++n){labels[n].setBounds(20+n*118,435,112,22);knobs[n].setBounds(20+n*118,459,112,106);}
  outputLabel.setBounds(20,590,70,25);level.setBounds(90,590,250,25);roomLabel.setBounds(365,590,60,25);room.setBounds(420,590,245,25);overheadLabel.setBounds(690,590,90,25);overhead.setBounds(780,590,290,25);message.setBounds(20,635,getWidth()-40,75);
 }
private:
 void attachPad(){sliderAttachments.clear();buttonAttachments.clear();const std::array<const char*,9> controls{"gain","pan","tune","eq0","eq1","eq2","eq3","threshold","ratio"};for(int n=0;n<9;++n)sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"pad"+juce::String(selected)+"_"+controls[n],knobs[n]));const std::array<const char*,4> toggles{"mute","solo","eq","comp"};for(int n=0;n<4;++n)buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"pad"+juce::String(selected)+"_"+toggles[n],buttons[n]));}
 void choose(int kind,const juce::String& pattern)
 {
  if(chooser)return;chooser=std::make_unique<juce::FileChooser>(kind==5?"Export drum pattern MIDI":"Choose drum file",juce::File(),pattern);
  const auto flags=kind==5?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles;
  chooser->launchAsync(flags,[safe=juce::Component::SafePointer<DrumsEditor>(this),kind](const juce::FileChooser& selected){if(!safe)return;const auto file=selected.getResult();if(file!=juce::File()){if(kind==5){const auto result=safe->processor.exportMidi(file.withFileExtension("mid"));safe->exportMessage=result.wasOk()?"MIDI exported.":result.getErrorMessage();}else safe->processor.requestFile(kind,file,safe->selected,safe->layer.getSelectedId()-1);}juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});});
 }
 void timerCallback()override{grid.setEnabled(!processor.isLoading());loadPack.setEnabled(!processor.isLoading());loadSample.setEnabled(!processor.isLoading());clearSample.setEnabled(!processor.isLoading());analyse.setEnabled(!processor.isLoading());grid.repaint();message.setText(exportMessage.isNotEmpty()?exportMessage:processor.status(),juce::dontSendNotification);}
 DrumsProcessor& processor;DrumGrid grid;int selected=0;juce::Label title,outputLabel,roomLabel,overheadLabel,message;juce::String exportMessage;
 juce::TextButton loadPack,loadSample,clearSample,analyse,exportMidi,audition;juce::ComboBox pad,layer,bars;juce::ToggleButton sequence;juce::Slider level,room,overhead;
 std::array<juce::Slider,9> knobs;std::array<juce::Label,9> labels;std::array<juce::ToggleButton,4> buttons;
 juce::AudioProcessorValueTreeState::ButtonAttachment sequenceAttachment;juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment,roomAttachment,overheadAttachment;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
 std::unique_ptr<juce::FileChooser> chooser;
};
juce::AudioProcessorEditor* DrumsProcessor::createEditor(){return new DrumsEditor(*this);}
}
