#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class RigTile final : public juce::Button
{
public:
    RigTile() : Button ("Rig block") {}
    RigType type = RigType::empty;
    bool active = true, selected = false;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto colour = juce::Colour (std::array<juce::uint32, 11> { 0xff737c83, 0xff87d7af, 0xffeab367, 0xffde7678, 0xff72b7e4, 0xff9e94e9, 0xff62cdbf, 0xffd5c9a9, 0xff6cc887, 0xffc998ec, 0xff75b5d3 }[static_cast<size_t> (type)]);
        const auto r = getLocalBounds().toFloat().reduced (4);
        g.setColour (juce::Colour (selected ? 0xff313b42 : over || down ? 0xff272f35 : 0xff171c21)); g.fillRoundedRectangle (r, 9);
        g.setColour (colour.withAlpha (active ? 1.0f : .35f)); g.drawRoundedRectangle (r, 9, selected ? 2.5f : 1.2f);
        const float cx = getWidth() * .5f, cy = 35;
        g.drawRoundedRectangle (cx - 21, cy - 21, 42, 42, 8, 1);
        if (type == RigType::empty) { g.drawLine (cx - 10, cy, cx + 10, cy, 2); g.drawLine (cx, cy - 10, cx, cy + 10, 2); }
        else if (type == RigType::capture) { g.drawRoundedRectangle (cx - 13, cy - 10, 26, 20, 3, 2); g.drawLine (cx - 10, cy - 3, cx + 10, cy - 3, 1); for (int n = 0; n < 3; ++n) g.fillEllipse (cx - 10 + n * 8, cy + 4, 3, 3); }
        else if (type == RigType::tone) { for (int n = 0; n < 3; ++n) { const float x = cx - 12 + n * 12; g.drawLine (x, cy - 13, x, cy + 13, 1.5f); g.fillEllipse (x - 3, cy - 8 + n * 5, 6, 6); } }
        else { juce::Path path; for (int n = 0; n <= 28; ++n) { const float x = cx - 14 + n, y = cy + std::sin (n * .4f) * 10; if (n == 0) path.startNewSubPath (x, y); else path.lineTo (x, y); } g.strokePath (path, juce::PathStrokeType (2)); }
        g.fillEllipse (getWidth() - 17.0f, 11, 5, 5);
        g.setColour (active ? juce::Colours::white : juce::Colours::grey); g.setFont (11);
        const auto name = type == RigType::bloomDelay ? "Bloom Delay" : type == RigType::shimmer ? "Shimmer" : type == RigType::capture ? "NAM Capture" : rigName (type);
        g.drawFittedText (name, 8, 61, getWidth() - 16, 20, juce::Justification::centred, 1);

    }
};

class GuitarPanel final : public juce::Component, private juce::Timer
{
public:
    explicit GuitarPanel (AvrxGrooveProcessor&);
    ~GuitarPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void openNamControls();
    void openTone3000();
    void startTake();
    void refresh() { timerCallback(); }
private:
    void timerCallback() override;
    void chooseNamPlugin();
    void chooseCapture();
    void showInspector();
    int selectedBlock = 2;
    std::array<RigTile, RigLayout::slots> blocks;
    juce::ComboBox blockType, swapSlot;
    juce::ToggleButton blockEnabled { "Block active" };
    juce::TextButton previousBlock { "< Move" }, nextBlock { "Move >" }, duplicate { "Duplicate" }, remove { "Remove" };
    std::array<juce::Slider, 6> effectControls;
    juce::Label blockHelp;
    AvrxGrooveProcessor& owner;
    juce::TextButton profiles { "Browse TONE3000" };
    juce::TextButton namControls { "Load .nam capture" }, namChoose { "Choose NAM VST3" }, record { "RECORD / Num 3" }, stop { "Stop recording" }, playTake { "Play take" };
    juce::ToggleButton monitor { "Monitor guitar" }, bypass { "Bypass rig" }, backing { "Backing groove" }, follow { "Live follow" }, metro { "Metronome" };
    int lastPresetRevision = -1;
    juce::ComboBox input, recordMode, preRoll;
    juce::Slider inputGain, outputGain, sensitivity;
    std::array<juce::Slider, 7> ampControls;
    juce::ToggleButton gate { "Gate" };
    juce::ComboBox pickup;
    juce::TextButton recalibrate { "Recalibrate" };
    juce::Label status, elapsed, followInfo;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DocumentWindow> namWindow, toneWindow;
};
}
