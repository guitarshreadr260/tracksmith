#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {
class MidiMappingPanel final : public juce::Component, private juce::Timer {
public:
 explicit MidiMappingPanel(AvrxGrooveProcessor& p):owner(p){
  for(int i=0;i<9;++i){auto& n=notes[(size_t)i];n.setRange(0,127,1);n.setSliderStyle(juce::Slider::IncDecButtons);n.setTextBoxStyle(juce::Slider::TextBoxLeft,false,100,26);n.setName(juce::String(GrooveGenerator::names[(size_t)i])+" MIDI note");n.onValueChange=[this,i]{owner.setMidiNote(i,(int)notes[(size_t)i].getValue());};addAndMakeVisible(n);}
  channel.setRange(1,16,1);channel.setSliderStyle(juce::Slider::IncDecButtons);channel.setTextBoxStyle(juce::Slider::TextBoxLeft,false,100,26);channel.onValueChange=[this]{owner.setMidiChannel((int)channel.getValue());};addAndMakeVisible(channel);
  addAndMakeVisible(reset);reset.onClick=[this]{owner.resetMidiMap(false);refresh();};addAndMakeVisible(close);close.onClick=[this]{setVisible(false);};refresh();startTimerHz(10);
 }
 void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff171a1d));g.setColour(juce::Colours::white);g.setFont(22);g.drawText("MIDI MAPPING",24,14,360,36,juce::Justification::centredLeft);g.setFont(13);g.setColour(juce::Colour(0xffb9bfc4));g.drawFittedText("Input notes and MIDI export use this map. Input accepts all channels. Duplicate assignments swap pads. Existing song MIDI keeps its recorded pitches.",24,52,getWidth()-48,48,juce::Justification::centredLeft,3);for(int i=0;i<9;++i){g.drawText(GrooveGenerator::names[(size_t)i],24,110+i*34,155,28,juce::Justification::centredLeft);g.drawText(juce::MidiMessage::getMidiNoteName(owner.getKit()->notes[(size_t)i],true,true,3),365,110+i*34,100,28,juce::Justification::centredLeft);}g.drawText("Export channel",24,424,155,28,juce::Justification::centredLeft);}
 void resized()override{for(int i=0;i<9;++i)notes[(size_t)i].setBounds(185,110+i*34,165,28);channel.setBounds(185,424,165,28);reset.setBounds(24,470,220,32);close.setBounds(300,470,150,32);}
private:
 void refresh(){auto k=owner.getKit();for(int i=0;i<9;++i)if(!notes[(size_t)i].isMouseButtonDown())notes[(size_t)i].setValue(k->notes[(size_t)i],juce::dontSendNotification);channel.setValue(k->midiChannel,juce::dontSendNotification);repaint();}
 void timerCallback()override{if(isVisible())refresh();}
 AvrxGrooveProcessor& owner;std::array<juce::Slider,9> notes;juce::Slider channel;juce::TextButton reset{"Reset General MIDI / Ch 10"},close{"Done"};
};
}
