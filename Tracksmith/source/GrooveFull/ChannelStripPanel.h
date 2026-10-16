#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class StudioLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StudioLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId, juce::Colour (0xffdce0e4));
        setColour (juce::Slider::trackColourId, juce::Colour (0xff83c9bb));
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff303339));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff111315));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe5e7ea));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202327));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe5e7ea));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff181b1e));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff34373d));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff191b1f));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff343b3b));
        setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff83c9bb));
        setColour (juce::ToggleButton::textColourId, juce::Colour (0xffdce0e4));
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff565c64));
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool hover, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        if (static_cast<bool> (b.getProperties()["navigation"]))
        {
            const bool active = static_cast<bool> (b.getProperties()["active"]);
            if (hover || active) { g.setColour (juce::Colour (active ? 0xff1d2024 : 0xff131518)); g.fillRoundedRectangle (r, 4); }
            if (active) { g.setColour (juce::Colour (0xff83c9bb)); g.fillRect (r.getX() + 16, r.getBottom() - 2, r.getWidth() - 32, 2.0f); }
            return;
        }
        auto fill = colour;
        if (b.getToggleState()) fill = juce::Colour (0xff3c4745);
        g.setColour (fill.brighter (down ? 0.15f : hover ? 0.07f : 0).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle (r, 4);
        g.setColour (juce::Colour (hover ? 0xff666c74 : 0xff3c4148).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f)); g.drawRoundedRectangle (r, 4, 0.7f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool nav = static_cast<bool> (b.getProperties()["navigation"]);
        const bool active = static_cast<bool> (b.getProperties()["active"]);
        g.setFont (juce::FontOptions (nav ? 11.0f : 12.0f).withStyle ("Bold"));
        g.setColour (juce::Colour (nav && ! active ? 0xff9b9fa6 : 0xffe9ebed).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
        g.drawFittedText (nav ? b.getProperties()["navLabel"].toString() : b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 1);
    }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float pos, float, float, const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        const bool vertical = style == juce::Slider::LinearVertical;
        const float cx = x + width * 0.5f, cy = y + height * 0.5f;
        g.setColour (juce::Colour (0xff35393f));
        if (vertical) g.fillRoundedRectangle (cx - 2, static_cast<float> (y), 4, static_cast<float> (height), 2);
        else g.fillRoundedRectangle (static_cast<float> (x), cy - 2, static_cast<float> (width), 4, 2);
        g.setColour (slider.findColour (juce::Slider::trackColourId).withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.35f));
        if (vertical) g.fillRoundedRectangle (cx - 2, pos, 4, y + height - pos, 2);
        else g.fillRoundedRectangle (static_cast<float> (x), cy - 2, juce::jmax (0.0f, pos - x), 4, 2);
        g.setColour (juce::Colour (0xffdce0e4));
        if (vertical) { g.fillRoundedRectangle (cx - 13, pos - 5, 26, 10, 2); g.setColour (juce::Colour (0xff464b52)); g.drawHorizontalLine (static_cast<int> (pos), cx - 10, cx + 10); }
        else g.fillEllipse (pos - 4.5f, cy - 4.5f, 9, 9);
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto box = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y), static_cast<float> (width), static_cast<float> (height)).reduced (7);
        const float radius = juce::jmin (box.getWidth(), box.getHeight()) / 2, cx = box.getCentreX(), cy = box.getCentreY();
        const auto colour = slider.findColour (juce::Slider::rotarySliderFillColourId);
        for (int tick = 0; tick <= 10; ++tick)
        {
            const float a = startAngle + (endAngle - startAngle) * tick / 10;
            g.setColour (juce::Colour (0xff565b62)); g.drawLine (cx + std::sin (a) * radius, cy - std::cos (a) * radius, cx + std::sin (a) * (radius + 3), cy - std::cos (a) * (radius + 3), 0.6f);
        }
        juce::Path arc; arc.addCentredArc (cx, cy, radius - 2, radius - 2, 0, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff3a3f45)); g.strokePath (arc, juce::PathStrokeType (1.5f));
        const float angle = startAngle + position * (endAngle - startAngle);
        juce::Path active; active.addCentredArc (cx, cy, radius - 2, radius - 2, 0, startAngle, angle, true);
        g.setColour (colour); g.strokePath (active, juce::PathStrokeType (2));
        const bool hardware = static_cast<bool> (slider.getProperties()["hardware"]);
        g.setGradientFill (juce::ColourGradient (hardware ? colour.brighter (0.15f) : juce::Colour (0xff33373c), cx, cy - radius, hardware ? colour.darker (0.6f) : juce::Colour (0xff0b0c0e), cx, cy + radius, false));
        g.fillEllipse (cx - radius + 6, cy - radius + 6, (radius - 6) * 2, (radius - 6) * 2);
        g.setColour (juce::Colour (0xff60656c)); g.drawEllipse (cx - radius + 6, cy - radius + 6, (radius - 6) * 2, (radius - 6) * 2, 0.8f);
        g.setColour (juce::Colour (0xfff1f2f3)); g.drawLine (cx + std::sin (angle) * radius * 0.3f, cy - std::cos (angle) * radius * 0.3f, cx + std::sin (angle) * radius * 0.67f, cy - std::cos (angle) * radius * 0.67f, 2);
    }
};

class ChannelStripPanel final : public juce::Component
{
public:
    explicit ChannelStripPanel (AvrxGrooveProcessor& processor) : owner (processor)
    {
        updating = true;
        for (auto* b : { &eqOn, &compOn, &hpOn, &lowBell, &highBell }) addAndMakeVisible (*b);
        addAndMakeVisible (reset);
        reset.onClick = [this] { owner.setDrumShaping (selected, {}); refresh(); };
        eqOn.onClick = compOn.onClick = hpOn.onClick = lowBell.onClick = highBell.onClick = [this] { commit(); };
        const std::array<juce::Colour, 4> colours { juce::Colour (0xff70a1c1), juce::Colour (0xff78b8a0), juce::Colour (0xffd2bf78), juce::Colour (0xffdf9c72) };
        const std::array<double, 4> mins { 20, 100, 400, 1500 }, maxes { 500, 2000, 8000, 20000 };
        for (size_t n = 0; n < 4; ++n)
        {
            knob (frequency[n], mins[n], maxes[n], 1, " Hz", colours[n], true);
            knob (gain[n], -15, 15, 0.1, " dB", colours[n]);
            if (n == 1 || n == 2) knob (q[n - 1], 0.3, 4, 0.05, " Q", colours[n]);
        }
        knob (compression[0], -12, 24, 0.1, " dB", juce::Colour (0xffd6b27b));
        knob (compression[1], 0.02, 0.8, 0.01, " ms", juce::Colour (0xffd6b27b), true);
        knob (compression[2], 50, 1100, 1, " ms", juce::Colour (0xffd6b27b), true);
        knob (compression[3], -24, 24, 0.1, " dB", juce::Colour (0xffd6b27b));
        knob (compression[4], -48, 0, 0.1, " dB", juce::Colour (0xffd6b27b));
        knob (compression[5], 0, 100, 1, " %", juce::Colour (0xff83c9bb));
        for (int n = 0; n < 4; ++n)
        {
            const int ratio = std::array<int, 4> { 4, 8, 12, 20 }[static_cast<size_t> (n)];
            auto& button = ratios[static_cast<size_t> (n)]; button.setButtonText (juce::String (ratio)); button.setClickingTogglesState (true); button.setRadioGroupId (920);
            button.onClick = [this] { commit(); }; addAndMakeVisible (button);
        }
        for (auto* slider : { &hpFrequency, &eqMix })
        {
            slider->setSliderStyle (juce::Slider::LinearHorizontal); slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24); slider->onValueChange = [this] { commit(); }; addAndMakeVisible (*slider);
        }
        hpFrequency.setRange (20, 300, 1); hpFrequency.setTextValueSuffix (" Hz"); hpFrequency.setSkewFactorFromMidPoint (80);
        eqMix.setRange (0, 100, 1); eqMix.setTextValueSuffix (" %"); refresh();
    }
    void selectDrum (int drum) { selected = drum; refresh(); }
    void refresh()
    {
        updating = true; const auto s = owner.getKit()->shaping[static_cast<size_t> (selected)];
        eqOn.setToggleState (s.eqOn, juce::dontSendNotification); compOn.setToggleState (s.compressorOn, juce::dontSendNotification);
        hpOn.setToggleState (s.highPassOn, juce::dontSendNotification); lowBell.setToggleState (s.lowBell, juce::dontSendNotification); highBell.setToggleState (s.highBell, juce::dontSendNotification);
        for (size_t n = 0; n < 4; ++n) { frequency[n].setValue (s.frequency[n], juce::dontSendNotification); gain[n].setValue (s.gainDb[n], juce::dontSendNotification); }
        q[0].setValue (s.q[1], juce::dontSendNotification); q[1].setValue (s.q[2], juce::dontSendNotification);
        const std::array<float, 6> values { s.inputDb, s.attackMs, s.releaseMs, s.outputDb, s.thresholdDb, s.compressorMix * 100 };
        for (size_t n = 0; n < compression.size(); ++n) compression[n].setValue (values[n], juce::dontSendNotification);
        for (size_t n = 0; n < ratios.size(); ++n) ratios[n].setToggleState (s.ratio == std::array<float, 4> { 4, 8, 12, 20 }[n], juce::dontSendNotification);
        hpFrequency.setValue (s.highPassHz, juce::dontSendNotification); eqMix.setValue (s.eqMix * 100, juce::dontSendNotification); updating = false; repaint();
    }
    void resized() override
    {
        eqOn.setBounds (148, 8, 112, 26); reset.setBounds (438, 8, 130, 26); compOn.setBounds (716, 8, 100, 26);
        for (size_t n = 0; n < 4; ++n)
        {
            const int x = 16 + static_cast<int> (n) * 142;
            frequency[n].setBounds (x, 72, 122, 80); gain[n].setBounds (x, 174, 122, 80);
            ratios[n].setBounds (828 + static_cast<int> (n) * 43, 10, 39, 24);
        }
        q[0].setBounds (158, 268, 122, 80); q[1].setBounds (300, 268, 122, 80);
        lowBell.setBounds (26, 290, 112, 28); highBell.setBounds (452, 290, 112, 28);
        hpOn.setBounds (18, 364, 100, 24); hpFrequency.setBounds (112, 362, 220, 28); eqMix.setBounds (370, 362, 200, 28);
        for (size_t n = 0; n < compression.size(); ++n)
            compression[n].setBounds (612 + static_cast<int> (n % 3) * 128, 82 + static_cast<int> (n / 3) * 134, 116, 92);
    }
    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3e4140), 0, 0, juce::Colour (0xff242726), 586, 0, false)); g.fillRoundedRectangle (0, 0, 586, static_cast<float> (getHeight()), 5);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff363735), 598, 0, juce::Colour (0xff202322), static_cast<float> (getWidth()), 0, false)); g.fillRoundedRectangle (598, 0, getWidth() - 598.0f, static_cast<float> (getHeight()), 5);
        g.setColour (juce::Colour (0xff131516)); for (float x : { 7.0f, 576.0f, 605.0f, getWidth() - 10.0f }) { g.fillEllipse (x, 7, 4, 4); g.fillEllipse (x, getHeight() - 11.0f, 4, 4); }
        g.setColour (juce::Colour (0xffeee9de)); g.setFont (juce::FontOptions (16).withStyle ("Bold"));
        g.drawText ("CONSOLE EQ", 18, 8, 132, 28, juce::Justification::centredLeft); g.drawText ("FET COMP", 616, 8, 100, 28, juce::Justification::centredLeft);
        const std::array<const char*, 4> bands { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        const std::array<juce::Colour, 4> colours { juce::Colour (0xff70a1c1), juce::Colour (0xff78b8a0), juce::Colour (0xffd2bf78), juce::Colour (0xffdf9c72) };
        for (size_t n = 0; n < 4; ++n)
        {
            const int x = 16 + static_cast<int> (n) * 142; g.setColour (colours[n]); g.setFont (12);
            g.drawText (bands[n], x, 44, 122, 22, juce::Justification::centred);
            g.setColour (juce::Colours::lightgrey); g.setFont (10); g.drawText ("FREQUENCY", x, 58, 122, 18, juce::Justification::centred);
            g.drawText ("GAIN", x, 157, 122, 18, juce::Justification::centred);
            if (n == 1 || n == 2) g.drawText ("WIDTH / Q", x, 256, 122, 18, juce::Justification::centred);
        }
        g.setFont (11); g.drawText ("EQ BLEND", 370, 341, 180, 20, juce::Justification::centredLeft);
        const std::array<const char*, 6> titles { "INPUT", "ATTACK", "RELEASE", "OUTPUT", "THRESHOLD", "PARALLEL MIX" };
        for (size_t n = 0; n < titles.size(); ++n) { g.setFont (12); g.drawText (titles[n], 612 + static_cast<int> (n % 3) * 128, 58 + static_cast<int> (n / 3) * 134, 116, 24, juce::Justification::centred); }
        const float reduction = owner.getDrumReduction (selected);
        g.setColour (juce::Colour (0xff0c0d0f)); g.fillRoundedRectangle (618, 334, getWidth() - 644.0f, 48, 5);
        g.setColour (juce::Colour (0xffd6b27b)); g.fillRect (630.0f, 358.0f, (getWidth() - 668.0f) * juce::jlimit (0.0f, 1.0f, reduction / 24), 9.0f);
        g.setFont (11); g.drawText ("GAIN REDUCTION   " + juce::String (reduction, 1) + " dB", 632, 336, getWidth() - 672, 22, juce::Justification::centredLeft);
    }
private:
    void knob (juce::Slider& slider, double low, double high, double interval, juce::String suffix, juce::Colour colour, bool logarithmic = false)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 22);
        slider.setRange (low, high, interval); if (logarithmic) slider.setSkewFactorFromMidPoint (std::sqrt (low * high));
        slider.getProperties().set ("hardware", true); slider.setTextValueSuffix (suffix); slider.setColour (juce::Slider::rotarySliderFillColourId, colour); slider.setDoubleClickReturnValue (true, low < 0 ? 0 : low);
        slider.onValueChange = [this] { commit(); }; addAndMakeVisible (slider);
    }
    void commit()
    {
        if (updating) return;
        auto s = owner.getKit()->shaping[static_cast<size_t> (selected)]; s.eqOn = eqOn.getToggleState(); s.compressorOn = compOn.getToggleState(); s.highPassOn = hpOn.getToggleState(); s.lowBell = lowBell.getToggleState(); s.highBell = highBell.getToggleState();
        for (size_t n = 0; n < 4; ++n) { s.frequency[n] = static_cast<float> (frequency[n].getValue()); s.gainDb[n] = static_cast<float> (gain[n].getValue()); }
        s.q[1] = static_cast<float> (q[0].getValue()); s.q[2] = static_cast<float> (q[1].getValue());
        s.inputDb = static_cast<float> (compression[0].getValue()); s.attackMs = static_cast<float> (compression[1].getValue()); s.releaseMs = static_cast<float> (compression[2].getValue()); s.outputDb = static_cast<float> (compression[3].getValue()); s.thresholdDb = static_cast<float> (compression[4].getValue()); s.compressorMix = static_cast<float> (compression[5].getValue()) / 100;
        for (size_t n = 0; n < 4; ++n) if (ratios[n].getToggleState()) s.ratio = std::array<float, 4> { 4, 8, 12, 20 }[n];
        s.highPassHz = static_cast<float> (hpFrequency.getValue()); s.eqMix = static_cast<float> (eqMix.getValue()) / 100; owner.setDrumShaping (selected, s); repaint();
    }
    AvrxGrooveProcessor& owner;
    int selected = 0;
    bool updating = false;
    std::array<juce::Slider, 4> frequency, gain;
    std::array<juce::Slider, 2> q;
    std::array<juce::Slider, 6> compression;
    std::array<juce::TextButton, 4> ratios;
    juce::Slider hpFrequency, eqMix;
    juce::ToggleButton eqOn { "EQ on" }, compOn { "Comp on" }, hpOn { "High pass" }, lowBell { "Bell / Shelf" }, highBell { "Bell / Shelf" };
    juce::TextButton reset { "Reset strip" };
};
}
