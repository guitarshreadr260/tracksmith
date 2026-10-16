#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class Tone3000Panel final : public juce::Component, private juce::Timer
{
public:
    explicit Tone3000Panel (AvrxGrooveProcessor& p) : owner (p)
    {
        key.setName ("TONE3000 publishable key"); key.setTextToShowWhenEmpty ("Paste t3k_pub_ publishable key", juce::Colours::grey); key.setText (owner.tone3000.getKey());
        for (juce::Component* c : std::array<juce::Component*, 10> { &key, &saveKey, &account, &browse, &cancel, &disconnect, &model, &load, &details, &message }) addAndMakeVisible (*c);
        saveKey.onClick = [this] { owner.tone3000.setKey (key.getText()); refresh(); };
        browse.onClick = [this] { if (! owner.guitar.isRecording()) { owner.tone3000.browse(); refresh(); } };
        cancel.onClick = [this] { owner.tone3000.cancel(); };
        disconnect.onClick = [this] { owner.tone3000.disconnect(); refresh(); };
        load.onClick = [this] { if (! owner.guitar.isRecording()) owner.tone3000.download (model.getSelectedId() - 1); refresh(); };
        model.setTextWhenNothingSelected ("Choose a tone above, then select its model"); model.setName ("TONE3000 selected models");
        details.setColour (juce::Label::textColourId, juce::Colour (0xffcfdae2)); message.setColour (juce::Label::textColourId, juce::Colour (0xff91c9bc));
        setSize (720, 465); refresh(); startTimerHz (10);
    }
    void refresh()
    {
        const auto revision = owner.tone3000.modelRevision.load();
        if (revision != lastRevision)
        {
            lastRevision = revision; model.clear (juce::dontSendNotification); const auto models = owner.tone3000.models();
            for (size_t n = 0; n < models.size(); ++n) model.addItem (models[n].name, static_cast<int> (n + 1));
            if (! models.empty()) model.setSelectedId (1, juce::dontSendNotification);
        }
        const bool busy = owner.tone3000.busy.load(), recording = owner.guitar.isRecording();
        saveKey.setEnabled (! busy); key.setEnabled (! busy); browse.setEnabled (! busy && ! recording && Tone3000::validKey (owner.tone3000.getKey()));
        load.setEnabled (! busy && ! recording && model.getSelectedId() > 0); model.setEnabled (! busy); cancel.setEnabled (busy); disconnect.setEnabled (! busy);
        details.setText (owner.tone3000.title().isEmpty() ? "Amp + Cab / NAM A1\nSelect flow opens your browser for sign-in, discovery and auditioning. Download & load brings your chosen model into the NAM block." : owner.tone3000.title() + "\n" + (owner.tone3000.creator().isNotEmpty() ? "By @" + owner.tone3000.creator() + " / " : "") + "Amp + Cab / NAM A1", juce::dontSendNotification);
        message.setText (owner.tone3000.status(), juce::dontSendNotification);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff131a20)); g.setColour (juce::Colours::white); g.setFont (24); g.drawText ("AVRX  /  TONE3000", 24, 15, 672, 34, juce::Justification::centredLeft);
        g.setFont (13); g.setColour (juce::Colour (0xffb7c3ce));
        g.drawFittedText ("Discover community NAM captures and bring them straight into your rig. Sign-in and tone selection happen through TONE3000 in your browser.", 24, 58, 672, 45, juce::Justification::topLeft, 2);
        g.setColour (juce::Colour (0xff25333e)); g.fillRoundedRectangle (18, 111, 684, 103, 8);
        g.setColour (juce::Colours::lightgrey); g.setFont (12); g.drawText ("ONE-TIME SETUP  /  Account Settings > API Keys > publishable key", 30, 120, 660, 20, juce::Justification::centredLeft);
        g.setFont (11); g.drawText ("Use a t3k_pub_ key. The app rejects secret keys. Connection details stay on this PC.", 30, 187, 660, 18, juce::Justification::centredLeft);
        g.drawFittedText ("Downloaded captures stay in your AVRX user library. Loading a new capture preserves your effects. Stop recording before changing the amp.", 24, 407, 672, 40, juce::Justification::topLeft, 2);
    }
    void resized() override
    {
        key.setBounds (30, 149, 385, 29); saveKey.setBounds (425, 149, 112, 29); account.setBounds (542, 145, 145, 36);
        browse.setBounds (24, 228, 285, 34); cancel.setBounds (321, 228, 128, 34); disconnect.setBounds (462, 228, 225, 34);
        details.setBounds (24, 270, 672, 49); model.setBounds (24, 327, 470, 32); load.setBounds (506, 327, 181, 32); message.setBounds (24, 368, 672, 34);
    }
private:
    void timerCallback() override { refresh(); }
    AvrxGrooveProcessor& owner;
    int lastRevision = -1;
    juce::TextEditor key;
    juce::TextButton saveKey { "Save key" }, browse { "Browse TONE3000 / Sign in" }, cancel { "Cancel" }, disconnect { "Disconnect" }, load { "Download & load" };
    juce::HyperlinkButton account { "Account setup", juce::URL ("https://www.tone3000.com/api") };
    juce::ComboBox model;
    juce::Label details, message;
};

class Tone3000Window final : public juce::DocumentWindow
{
public:
    explicit Tone3000Window (AvrxGrooveProcessor& p) : DocumentWindow ("AVRX / TONE3000 captures", juce::Colour (0xff131a20), closeButton), owner (p)
    { setUsingNativeTitleBar (true); setContentOwned (new Tone3000Panel (p), true); centreWithSize (720, 465); setVisible (true); }
    void closeButtonPressed() override { owner.tone3000.cancel(); setVisible (false); }
private:
    AvrxGrooveProcessor& owner;
};
}
