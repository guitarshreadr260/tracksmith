#pragma once
#include "MidiMappingPanel.h"
#include "PluginProcessor.h"
#include "GuitarPanel.h"
#include "AudioPanel.h"
#include "MultitrackPanel.h"
#include "WaveformPanel.h"
#include "ChannelStripPanel.h"
#include "MixerPanel.h"
#include "StudioKitView.h"
#include "GroovePanel.h"
namespace TracksmithGrooveFull {

class MidiDragButton final : public juce::Component
{
public:
    explicit MidiDragButton (AvrxGrooveProcessor& processor) : owner (processor) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { started = false; }
    void mouseDrag (const juce::MouseEvent&) override;
    std::function<void (juce::String)> onStatus;
private:
    AvrxGrooveProcessor& owner;
    bool started = false;
};

class DrumPad final : public juce::Button
{
public:
    explicit DrumPad (int index) : Button (GrooveGenerator::names[static_cast<size_t> (index)]), drum (index) {}
    void setDetails (juce::String sample, bool selected, bool muted);
    void paintButton (juce::Graphics&, bool, bool) override;
private:
    int drum;
    juce::String sound { "Built-in drum" };
    bool chosen = false, silent = false;
    juce::Image artwork = juce::ImageCache::getFromMemory (TracksmithGrooveArtwork::groovestudiokit_png, TracksmithGrooveArtwork::groovestudiokit_pngSize);
};

class DrumSequencer final : public juce::Component
{
public:
    explicit DrumSequencer (AvrxGrooveProcessor& processor) : owner (processor) {}
    void paint (juce::Graphics&) override;
    static constexpr int cellWidth = 28, rowHeight = 18, top = 22, velocityTop = top + GrooveGenerator::drumCount * rowHeight;
    void selectDrum (int drum) { selectedDrum = drum; repaint(); }
    static std::array<int, GrooveGenerator::drumCount> lanes (const DrumKit&);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::function<void (int)> onSelect;
private:
    AvrxGrooveProcessor& owner;
    int selectedDrum = 0, selectedCell = -1, dragDrum = -1, dragCell = -1, startY = 0;
    float startVelocity = 0.75f;
};

class AvrxGrooveEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer,
                             public juce::FileDragAndDropTarget,
                             public juce::KeyListener
{
public:
    explicit AvrxGrooveEditor (AvrxGrooveProcessor&);
    ~AvrxGrooveEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged (bool, juce::Component*) override;
public:
    void showAnalyzer(){changeTab(3);}
    void showGrooves(){changeTab(7);}
private:
    void timerCallback() override;
    void chooseAudio();
    void choosePreset (bool saving);
    void refreshPacks();
    void chooseSample();
    void chooseRecordingFolder();
    void changeSequence();
    void syncSequenceControls();
    void changeTab (int);
    void selectPad (int);
    AvrxGrooveProcessor& owner;
    StudioLookAndFeel theme;
    std::unique_ptr<MidiMappingPanel> midiMappingPanel;
    juce::TextButton editMapping{"MIDI mapping..."};
    juce::ComboBox packSelector;
    std::vector<juce::File> packFiles;
    juce::ToggleButton followHost {"Follow DAW"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    juce::TextButton useInSong {"Use groove in song"};
    juce::TextButton savePresetButton { "Save preset" }, loadPresetButton { "Load preset" };
    juce::Label presetMessage;
    std::array<juce::Slider, 3> sliders;
    std::array<juce::Label, 3> labels;
    juce::ToggleButton run { "Run groove" }, mute { "Mute pad" };
    juce::TextButton load { "Analyse audio" }, together { "Play together" }, stop { "Stop / rewind" },
                     preview { "Play source" }, defaults { "Default pattern" },
                     sampleLoad { "Load WAV" }, sampleClear { "Clear layer" }, matchKey { "Match song key" },
                     drumsTab { "Drums" }, guitarTab { "Guitar" }, settingsTab { "Settings" }, audioTab { "Audio" }, multitrackTab { "Recorder" }, samplesTab { "Samples" }, mixerTab { "Mixer" }, groovesTab { "Grooves" };
    std::array<std::unique_ptr<DrumPad>, GrooveGenerator::drumCount> pads;
    DrumSequencer sequencer;
    juce::Viewport sequenceViewport;
    MidiDragButton midiDrag;
    GuitarPanel guitarPanel;
    AudioPanel audioPanel;
    MultitrackPanel multitrackPanel;
    MixerPanel mixerPanel;
    StudioKitView studioKit;
    GroovePanel groovePanel;
    bool spaceHeld = false, recordHeld = false;
    juce::Component settingsPanel;
    juce::Label settingsInfo, recordingFolderLabel;
    juce::TextButton recordingFolderButton { "Set recording folder" }, toneSetup { "TONE3000 connection" };
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    std::vector<juce::Component*> drumControls, sampleControls;
    WaveformPanel referenceWave, sampleWave;
    ChannelStripPanel channelStrip;
    int activeTab = 0;
    int selectedPad = 0;
    juce::Slider trackVolume, roomVolume, overheadVolume, padGain, noteNumber, layerGain, tuning, fineTune;
    juce::ComboBox mapping, bars, signature, division, layer, rootNote, songKey;
    juce::Label status, kitStatus, dragStatus, padName;
    std::unique_ptr<juce::FileChooser> chooser;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 3> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvrxGrooveEditor)
};
}
