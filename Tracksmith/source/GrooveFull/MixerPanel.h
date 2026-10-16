#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class MixerPanel final : public juce::Component, private juce::Timer
{
public:
    explicit MixerPanel (AvrxGrooveProcessor& processor) : owner (processor)
    {
        addAndMakeVisible (drumView); addAndMakeVisible (trackView);
        drumView.onClick = [this] { consoleMode = true; updateVisibility(); resized(); repaint(); };
        trackView.onClick = [this] { consoleMode = false; updateVisibility(); resized(); repaint(); };
        addAndMakeVisible (transport); transport.onClick = [this] { owner.toggleTransport (6); };
        for (size_t n = 0; n < 9; ++n)
        {
            edit[n].setButtonText (GrooveGenerator::names[n]); edit[n].onClick = [this, n] { if (onEdit) onEdit (static_cast<int> (n)); }; edit[n].setTooltip ("Open this drum's sample and sound-shaping strip"); addAndMakeVisible (edit[n]);
            for (size_t band = 0; band < 4; ++band)
            {
                miniKnob (eqGain[n][band], -15, 15, 0.1, " dB", bandColour (band));
                eqGain[n][band].setTooltip (juce::String (std::array<const char*, 4> { "Low", "Low mid", "High mid", "High" }[band]) + " EQ gain");
                eqGain[n][band].onValueChange = [this, n, band] { if (! updating) { auto settings = owner.getKit()->shaping[n]; settings.gainDb[band] = static_cast<float> (eqGain[n][band].getValue()); owner.setDrumShaping (static_cast<int> (n), settings); } };
            }
            miniKnob (inputDrive[n], -12, 24, 0.1, " dB", juce::Colour (0xffdfdfd5));
            miniKnob (parallel[n], 0, 100, 1, " %", juce::Colour (0xff80ab87));
            inputDrive[n].onValueChange = [this, n] { if (! updating) { auto settings = owner.getKit()->shaping[n]; settings.inputDb = static_cast<float> (inputDrive[n].getValue()); owner.setDrumShaping (static_cast<int> (n), settings); } };
            parallel[n].onValueChange = [this, n] { if (! updating) { auto settings = owner.getKit()->shaping[n]; settings.compressorMix = static_cast<float> (parallel[n].getValue()) / 100; owner.setDrumShaping (static_cast<int> (n), settings); } };
            eqOn[n].setButtonText ("EQ"); compOn[n].setButtonText ("COMP");
            for (auto* b : { &eqOn[n], &compOn[n] }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
            eqOn[n].onClick = [this, n] { auto settings = owner.getKit()->shaping[n]; settings.eqOn = eqOn[n].getToggleState(); owner.setDrumShaping (static_cast<int> (n), settings); };
            compOn[n].onClick = [this, n] { auto settings = owner.getKit()->shaping[n]; settings.compressorOn = compOn[n].getToggleState(); owner.setDrumShaping (static_cast<int> (n), settings); };
            fader (level[n], 1.5, "Drum level"); balance (pan[n], "Drum stereo balance"); switches (mute[n], solo[n]);
            level[n].onValueChange = [this, n] { if (! updating) owner.setPadGain (static_cast<int> (n), static_cast<float> (level[n].getValue())); };
            pan[n].onValueChange = [this, n] { if (! updating) owner.setPadPan (static_cast<int> (n), static_cast<float> (pan[n].getValue())); };
            mute[n].onClick = [this, n] { owner.setPadMuted (static_cast<int> (n), mute[n].getToggleState()); };
            solo[n].onClick = [this, n] { owner.setPadSolo (static_cast<int> (n), solo[n].getToggleState()); };
        }
        for (size_t n = 0; n < 8; ++n)
        {
            fader (lowerLevel[n], n < 4 ? 1.5 : 1.0, n < 4 ? "Recorder track level" : "Bus level");
            lowerLevel[n].onValueChange = [this, n]
            {
                if (updating) return;
                const auto value = static_cast<float> (lowerLevel[n].getValue());
                if (n < 4) owner.recorder.strips[n].gain.store (value);
                else if (n == 4) owner.setTrackLevel (value);
                else if (n == 5) owner.setAmbience (value, owner.getKit()->overheadLevel);
                else if (n == 6) owner.setAmbience (owner.getKit()->roomLevel, value);
                else owner.parameters.getParameter ("level")->setValueNotifyingHost (value);
            };
            if (n < 4)
            {
                balance (lowerPan[n], "Recorder track pan"); switches (lowerMute[n], lowerSolo[n]);
                lowerPan[n].onValueChange = [this, n] { if (! updating) owner.recorder.strips[n].pan.store (static_cast<float> (lowerPan[n].getValue())); };
                lowerMute[n].onClick = [this, n] { owner.recorder.strips[n].mute.store (lowerMute[n].getToggleState()); };
                lowerSolo[n].onClick = [this, n] { owner.recorder.strips[n].solo.store (lowerSolo[n].getToggleState()); };
            }
        }
        updateVisibility(); refresh(); startTimerHz (20);
    }
    void refresh()
    {
        transport.setButtonText (owner.recorder.playing.load() || owner.parameters.getRawParameterValue ("run")->load() > 0.5f ? "Stop / Space" : "Play / Space");
        drumView.setToggleState (consoleMode, juce::dontSendNotification); trackView.setToggleState (! consoleMode, juce::dontSendNotification);
        updating = true; const auto kit = owner.getKit();
        for (size_t n = 0; n < 9; ++n)
        {
            for (size_t band = 0; band < 4; ++band) eqGain[n][band].setValue (kit->shaping[n].gainDb[band], juce::dontSendNotification);
            inputDrive[n].setValue (kit->shaping[n].inputDb, juce::dontSendNotification); parallel[n].setValue (kit->shaping[n].compressorMix * 100, juce::dontSendNotification);
            eqOn[n].setToggleState (kit->shaping[n].eqOn, juce::dontSendNotification); compOn[n].setToggleState (kit->shaping[n].compressorOn, juce::dontSendNotification);
            level[n].setValue (kit->gains[n], juce::dontSendNotification); pan[n].setValue (kit->pan[n], juce::dontSendNotification);
            mute[n].setToggleState (kit->muted[n], juce::dontSendNotification); solo[n].setToggleState (kit->solo[n], juce::dontSendNotification);
            meters[n] = juce::jmax (owner.getPadPeak (static_cast<int> (n)), meters[n] * 0.87f);
        }
        for (size_t n = 0; n < 4; ++n)
        {
            lowerLevel[n].setValue (owner.recorder.strips[n].gain.load(), juce::dontSendNotification); lowerPan[n].setValue (owner.recorder.strips[n].pan.load(), juce::dontSendNotification);
            lowerMute[n].setToggleState (owner.recorder.strips[n].mute.load(), juce::dontSendNotification); lowerSolo[n].setToggleState (owner.recorder.strips[n].solo.load(), juce::dontSendNotification);
        }
        lowerLevel[4].setValue (owner.getTrackLevel(), juce::dontSendNotification); lowerLevel[5].setValue (kit->roomLevel, juce::dontSendNotification);
        lowerLevel[6].setValue (kit->overheadLevel, juce::dontSendNotification); lowerLevel[7].setValue (owner.parameters.getRawParameterValue ("level")->load(), juce::dontSendNotification);
        updating = false; repaint();
    }
    void resized() override
    {
        transport.setBounds (getWidth() - 220, 16, 200, 32);
        drumView.setBounds (20, 50, 168, 28); trackView.setBounds (198, 50, 188, 28);
        const int w = (getWidth() - 40) / 9;
        for (size_t n = 0; n < 9; ++n)
        {
            const int x = 20 + static_cast<int> (n) * w;
            edit[n].setBounds (x + 7, 101, w - 20, 24); eqOn[n].setBounds (x + 20, 142, w - 44, 22);
            for (size_t band = 0; band < 4; ++band) eqGain[n][band].setBounds (x + 7 + static_cast<int> (band % 2) * 49, 187 + static_cast<int> (band / 2) * 77, 46, 60);
            compOn[n].setBounds (x + 20, 339, w - 44, 22); inputDrive[n].setBounds (x + 7, 388, 46, 60); parallel[n].setBounds (x + 56, 388, 46, 60);
            pan[n].setBounds (x + 12, 499, w - 24, 22); level[n].setBounds (x + 14, 538, 54, 181);
            mute[n].setBounds (x + 10, 730, 38, 26); solo[n].setBounds (x + 54, 730, 38, 26);
        }
        const int lw = (getWidth() - 40) / 8;
        for (size_t n = 0; n < 8; ++n)
        {
            const int x = 20 + static_cast<int> (n) * lw;
            lowerLevel[n].setBounds (x + 32, 292, 58, 386);
            if (n < 4) { lowerPan[n].setBounds (x + 12, 244, lw - 24, 22); lowerMute[n].setBounds (x + 14, 702, 38, 26); lowerSolo[n].setBounds (x + 64, 702, 38, 26); }
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xffeceef0)); g.setFont (juce::FontOptions (24).withStyle ("Bold")); g.drawText ("STUDIO CONSOLE", 20, 12, 600, 34, juce::Justification::centredLeft);
        if (consoleMode)
        {
            const int w = (getWidth() - 40) / 9;
            for (size_t n = 0; n < 9; ++n)
            {
                const int x = 20 + static_cast<int> (n) * w; const auto accent = colour (n);
                hardwareStrip (g, x, 94, w - 6, 675, accent);
                const auto label = [&] (const juce::String& text, int top, int lx, int lw) { g.setColour (juce::Colour (0xffc9c8c1)); g.setFont (9); g.drawText (text, lx, top, lw, 18, juce::Justification::centred); };
                g.setColour (juce::Colour (0xff0c0e10)); g.fillRect (x + 8, 127, w - 22, 12); g.setColour (accent); g.setFont (8); g.drawText ("INPUT > EQ > COMP", x + 8, 126, w - 22, 14, juce::Justification::centred);
                for (size_t band = 0; band < 4; ++band) label (std::array<const char*, 4> { "LOW", "LOW MID", "HIGH MID", "HIGH" }[band], 171 + static_cast<int> (band / 2) * 77, x + 6 + static_cast<int> (band % 2) * 49, 48);
                label ("INPUT", 371, x + 6, 48); label ("MIX", 371, x + 55, 48);
                g.setColour (juce::Colour (0xff141617)); g.fillRect (x + 11, 463, w - 28, 18);
                g.setColour (juce::Colour (0xff8ecb89)); g.setFont (9); g.drawText ("GR " + juce::String (owner.getDrumReduction (static_cast<int> (n)), 1) + " dB", x + 11, 463, w - 28, 18, juce::Justification::centred);
                label ("L       PAN       R", 483, x + 9, w - 24);
                const float peak = juce::jlimit (0.0f, 1.0f, meters[n]); const float activity = peak > 0 ? juce::jlimit (0.0f, 1.0f, (20 * std::log10 (juce::jmax (peak, 0.001f)) + 60) / 60) : 0;
                g.setColour (juce::Colour (0xff0d0e10)); g.fillRect (x + 77, 542, 10, 147);
                for (int led = 0; led < 24; ++led) { const bool on = led < activity * 24; g.setColour (on ? juce::Colour (led > 20 ? 0xffdf7c66 : led > 16 ? 0xffddba6c : 0xff8bbd80) : juce::Colour (0xff242a24)); g.fillRect (x + 79, 684 - led * 6, 6, 4); }
                g.setFont (8); g.setColour (juce::Colour (0xffc2c1b9)); g.drawText ("0", x + 89, 538, 15, 14, juce::Justification::centredLeft); g.drawText ("-60", x + 86, 679, 24, 14, juce::Justification::centredLeft);
            }
        }
        else
        {
            const int lw = (getWidth() - 40) / 8;
            const std::array<const char*, 8> names { "Track 1", "Track 2", "Track 3", "Track 4", "Reference", "Room", "Overheads", "Drum Master" };
            for (size_t n = 0; n < 8; ++n)
            {
                const int x = 20 + static_cast<int> (n) * lw; hardwareStrip (g, x, 112, lw - 6, 637, n < 4 ? colour (n) : juce::Colour (0xff83c9bb));
                g.setColour (juce::Colour (0xffe7e5dd)); g.setFont (12); g.drawText (names[n], x + 6, 128, lw - 18, 28, juce::Justification::centred);
                g.setFont (10); g.setColour (juce::Colour (0xffb0b1ab)); g.drawText (n < 4 ? "RECORDER" : "BUS LEVEL", x + 6, 174, lw - 18, 22, juce::Justification::centred);
                if (n < 4) g.drawText ("L        PAN        R", x + 6, 220, lw - 18, 20, juce::Justification::centred);
            }
        }
        g.setFont (10); g.setColour (juce::Colour (0xff959b9f)); g.drawText ("Click a drum name for its full channel strip. M / S = mute / solo. Space plays recorder + groove. Changes are session-only.", 20, 775, getWidth() - 40, 24, juce::Justification::centredLeft);
    }
    std::function<void (int)> onEdit;

private:
    static juce::Colour colour (size_t n) { return juce::Colour (n == 0 ? 0xffd6b27b : n == 1 ? 0xffca8d98 : n < 4 ? 0xff83c9bb : 0xff9aa8ce); }
    static juce::Colour bandColour (size_t n) { return std::array<juce::Colour, 4> { juce::Colour (0xff5d8cae), juce::Colour (0xff84a66e), juce::Colour (0xffb1706d), juce::Colour (0xffd1d2cb) }[n]; }
    static void hardwareStrip (juce::Graphics& g, int x, int y, int w, int h, juce::Colour accent)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d4c), static_cast<float> (x), 0, juce::Colour (0xff252827), x + static_cast<float> (w), 0, false));
        g.fillRoundedRectangle (static_cast<float> (x), static_cast<float> (y), static_cast<float> (w), static_cast<float> (h), 3);
        g.setColour (juce::Colour (0x11444444)); for (int line = x + 2; line < x + w; line += 3) g.drawVerticalLine (line, static_cast<float> (y), static_cast<float> (y + h));
        g.setColour (juce::Colour (0xff666966)); g.drawRoundedRectangle (static_cast<float> (x), static_cast<float> (y), static_cast<float> (w), static_cast<float> (h), 3, 0.7f);
        g.setColour (accent); g.fillRect (x + 4, y, w - 8, 2);
        g.setColour (juce::Colour (0xff17191b)); g.fillEllipse (x + 4.0f, y + h - 6.0f, 3, 3); g.fillEllipse (x + w - 8.0f, y + h - 6.0f, 3, 3);
    }
    void miniKnob (juce::Slider& s, double lo, double hi, double step, const juce::String& suffix, juce::Colour c)
    { s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 46, 18); s.setRange (lo, hi, step); s.setTextValueSuffix (suffix); s.setColour (juce::Slider::rotarySliderFillColourId, c); s.getProperties().set ("hardware", true); addAndMakeVisible (s); }
    void updateVisibility()
    {
        for (size_t n = 0; n < 9; ++n) { level[n].setVisible (consoleMode); pan[n].setVisible (consoleMode); mute[n].setVisible (consoleMode); solo[n].setVisible (consoleMode); edit[n].setVisible (consoleMode); eqOn[n].setVisible (consoleMode); compOn[n].setVisible (consoleMode); inputDrive[n].setVisible (consoleMode); parallel[n].setVisible (consoleMode); for (auto& q : eqGain[n]) q.setVisible (consoleMode); }
        for (auto& s : lowerLevel) s.setVisible (! consoleMode);
        for (size_t n = 0; n < 4; ++n) { lowerPan[n].setVisible (! consoleMode); lowerMute[n].setVisible (! consoleMode); lowerSolo[n].setVisible (! consoleMode); }
    }
    void fader (juce::Slider& s, double max, const juce::String& tip)
    { s.setSliderStyle (juce::Slider::LinearVertical); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 20); s.setRange (0, max, 0.01); s.setDoubleClickReturnValue (true, max > 1 ? 1 : 0.7); s.setTooltip (tip); addAndMakeVisible (s); }
    void balance (juce::Slider& s, const juce::String& tip)
    { s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); s.setRange (-1, 1, 0.01); s.setDoubleClickReturnValue (true, 0); s.setTooltip (tip); addAndMakeVisible (s); }
    void switches (juce::TextButton& m, juce::TextButton& s)
    { m.setButtonText ("M"); s.setButtonText ("S"); m.setClickingTogglesState (true); s.setClickingTogglesState (true); m.setTooltip ("Mute"); s.setTooltip ("Solo"); addAndMakeVisible (m); addAndMakeVisible (s); }
    void timerCallback() override { refresh(); }
    bool consoleMode = true;
    juce::TextButton drumView { "Drum console" }, trackView { "Recorder & buses" };
    std::array<std::array<juce::Slider, 4>, 9> eqGain;
    std::array<juce::Slider, 9> inputDrive, parallel;
    std::array<juce::TextButton, 9> eqOn, compOn, edit;
    juce::TextButton transport { "Play / Space" };
    AvrxGrooveProcessor& owner;
    bool updating = true;
    std::array<juce::Slider, 9> level, pan;
    std::array<juce::TextButton, 9> mute, solo;
    std::array<float, 9> meters {};
    std::array<juce::Slider, 8> lowerLevel;
    std::array<juce::Slider, 4> lowerPan;
    std::array<juce::TextButton, 4> lowerMute, lowerSolo;
};
}
