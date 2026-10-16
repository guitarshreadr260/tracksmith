#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class AudioPanel final : public juce::Component, private juce::Timer
{
public:
    explicit AudioPanel (AvrxGrooveProcessor& processor) : owner (processor)
    {
        for (auto* b : { &play, &together, &stop, &load, &analyse }) addAndMakeVisible (*b);
        addAndMakeVisible (strength);
        strength.setRange (0.5, 3, 0.05); strength.setValue (1);
        strength.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
        strength.setTooltip ("Higher detects softer attacks. Click Re-analyse to update the loaded audio.");
        strength.onValueChange = [this] { owner.setDetectionStrength (static_cast<float> (strength.getValue())); };
        analyse.onClick = [this] { owner.reanalyseTrack(); };
        addAndMakeVisible (metro); addAndMakeVisible (info);
        metro.onClick = [this] { owner.metronome.store (metro.getToggleState()); };
        play.onClick = [this] { owner.setTrackPlaying (! owner.isTrackPlaying()); };
        together.onClick = [this] { owner.playTogether(); };
        stop.onClick = [this] { owner.stopTogether(); };
        load.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Load a rhythmic audio reference", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            juce::Component::SafePointer<AudioPanel> safe (this);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe] (const juce::FileChooser& dialog) { if (safe && dialog.getResult().existsAsFile()) safe->owner.loadAndAnalyse (dialog.getResult()); });
        };
        info.setColour (juce::Label::textColourId, juce::Colours::lightgrey); startTimerHz (20);
    }
    void resized() override
    {
        load.setBounds (20, 520, 180, 36); play.setBounds (220, 520, 160, 36);
        together.setBounds (400, 520, 180, 36); stop.setBounds (600, 520, 160, 36);
        metro.setBounds (780, 520, 200, 36); strength.setBounds (205, 581, 540, 30); analyse.setBounds (780, 578, 200, 36); info.setBounds (20, 622, getWidth() - 40, 60);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white); g.setFont (24);
        g.drawText ("AUDIO REFERENCE", 20, 12, getWidth() - 40, 34, juce::Justification::centredLeft);
        g.setFont (15); g.setColour (juce::Colours::lightgrey);
        g.drawText ("Drop audio here. Click the waveform to move playback. Blue lines show detected attacks.", 20, 60, getWidth() - 40, 32, juce::Justification::centredLeft);
        g.drawText ("Detection strength", 20, 581, 175, 30, juce::Justification::centredLeft);
        const auto wave = owner.getWaveform();
        if (wave)
        {
            g.drawText (wave->name + "   /   " + juce::String (wave->seconds, 1) + " seconds", 20, 112, getWidth() - 40, 30, juce::Justification::centredLeft);
            const auto analysis = owner.getAnalysis();
            drawWaveform (g, waveformBounds(), *wave, wave->seconds, owner.trackPositionSeconds(), juce::Colour (0xff83c9bb), analysis ? analysis->onsets : std::vector<double>());
            g.drawText ("0:00", 32, 455, 140, 30, juce::Justification::centredLeft);
            g.drawText (juce::String (owner.trackPositionSeconds(), 1) + " / " + juce::String (wave->seconds, 1) + " s", getWidth() - 290, 455, 258, 30, juce::Justification::centredRight);
        }
        else drawWaveform (g, waveformBounds(), {}, 1, 0, juce::Colours::white);
        g.drawFittedText ("Analysis suggests a repeating 4/4 kick / snare / hat pattern; it does not separate instruments from a full mix. Listen, adjust Tempo on Drums, and edit the grid when needed. For live guitar accompaniment, use Live follow on Guitar. Imported audio remains visible even when a reliable tempo cannot be found.",
            20, 690, getWidth() - 40, 88, juce::Justification::topLeft, 4);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (waveformBounds().contains (e.getPosition()))
            if (auto wave = owner.getWaveform())
                owner.seekTrack (juce::jlimit (0.0, 1.0, (e.x - 32.0) / (getWidth() - 64.0)) * wave->seconds);
    }
    void refresh() { timerCallback(); }
private:
    juce::Rectangle<int> waveformBounds() const { return { 20, 160, getWidth() - 40, 290 }; }
    void timerCallback() override
    {
        strength.setValue (owner.detectionStrength.load(), juce::dontSendNotification); analyse.setEnabled (owner.hasTrack() && ! owner.isAnalysing());
        metro.setToggleState (owner.metronome.load(), juce::dontSendNotification);
        info.setText (owner.getAnalysisStatus(), juce::dontSendNotification);
        play.setButtonText (owner.isTrackPlaying() ? "Pause source" : "Play source");
        play.setEnabled (owner.hasTrack()); together.setEnabled (owner.hasTrack() && ! owner.isAnalysing()); load.setEnabled (! owner.isAnalysing()); repaint();
    }
    AvrxGrooveProcessor& owner;
    juce::TextButton play { "Play source" }, together { "Play together" }, stop { "Stop / rewind" }, load { "Load audio" }, analyse { "Re-analyse" };
    juce::Slider strength;
    juce::ToggleButton metro { "Metronome" };
    juce::Label info;
    std::unique_ptr<juce::FileChooser> chooser;
};
}
