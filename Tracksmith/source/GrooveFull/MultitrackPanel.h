#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class MultitrackPanel final : public juce::Component, private juce::Timer
{
public:
    explicit MultitrackPanel (AvrxGrooveProcessor& processor) : owner (processor)
    {
        for (auto* button : { &play, &stop, &rewind, &record, &import }) addAndMakeVisible (*button);
        play.onClick = [this] { owner.toggleMultitrack(); };
        stop.onClick = [this] { owner.stopRecording(); owner.recorder.playing.store (false); owner.parameters.getParameter ("run")->setValueNotifyingHost (0); };
        rewind.onClick = [this] { if (! owner.guitar.isRecording()) owner.seekMultitrack (0); };
        record.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffae354b));
        record.onClick = [this] { startTake(); }; import.onClick = [this] { chooseImport(); };
        mode.addItem ("NAM sound", 1); mode.addItem ("Dry DI", 2); mode.setSelectedId (1); addAndMakeVisible (mode);
        preRoll.addItem ("Count-in: Off", 1); preRoll.addItem ("Count-in: 1 bar", 2); preRoll.addItem ("Count-in: 2 bars", 3); preRoll.setSelectedId (2); addAndMakeVisible (preRoll);
        mode.onChange = [this] { owner.recorderRecordMode.store (mode.getSelectedId()); };
        preRoll.onChange = [this] { owner.recorderCountIn.store (preRoll.getSelectedId() - 1); };
        for (int n = 0; n < MultitrackEngine::tracks; ++n)
        {
            const auto i = static_cast<size_t> (n);
            arm[i].setButtonText ("Arm " + juce::String (n + 1)); arm[i].setRadioGroupId (910); arm[i].setClickingTogglesState (true);
            arm[i].onClick = [this, n] { selected = n; owner.armedTrack.store (n); refresh(); };
            mute[i].setButtonText ("Mute"); solo[i].setButtonText ("Solo");
            mute[i].onClick = [this, i] { owner.recorder.strips[i].mute.store (mute[i].getToggleState()); };
            solo[i].onClick = [this, i] { owner.recorder.strips[i].solo.store (solo[i].getToggleState()); };
            clear[i].setButtonText ("Clear"); clear[i].onClick = [this, n] { if (! owner.guitar.isRecording()) owner.recorder.clear (n); };
            gain[i].setSliderStyle (juce::Slider::LinearHorizontal); gain[i].setRange (-60, 3.5, .1); gain[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20); gain[i].setTextValueSuffix (" dB"); gain[i].setName ("Track " + juce::String (n + 1) + " volume"); gain[i].setDoubleClickReturnValue (true, 0); gain[i].setTooltip ("Track volume: -60 dB silences the lane; double-click for unity.");
            pan[i].setSliderStyle (juce::Slider::LinearHorizontal); pan[i].setRange (-1, 1, 0.01); pan[i].setDoubleClickReturnValue (true, 0); pan[i].setName ("Track " + juce::String (n + 1) + " pan"); pan[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 88, 20); pan[i].textFromValueFunction = [] (double p) { return std::abs (p) < .005 ? juce::String ("Centre") : juce::String (p < 0 ? "L " : "R ") + juce::String (std::abs (p) * 100, 0); }; pan[i].valueFromTextFunction = [] (const juce::String& text) { if (text.containsIgnoreCase ("centre")) return 0.0; const double x = text.retainCharacters ("0123456789.-").getDoubleValue(); return text.containsIgnoreCase ("L") ? -std::abs (x) / 100 : text.containsIgnoreCase ("R") ? std::abs (x) / 100 : x; }; pan[i].updateText(); pan[i].setTooltip ("Pan: left / centre / right");
            gain[i].onValueChange = [this, i] { owner.recorder.strips[i].gain.store (juce::Decibels::decibelsToGain (static_cast<float> (gain[i].getValue()), -60.0f)); };
            pan[i].onValueChange = [this, i] { owner.recorder.strips[i].pan.store (static_cast<float> (pan[i].getValue())); };
            for (juce::Component* c : std::array<juce::Component*, 6> { &arm[i], &mute[i], &solo[i], &clear[i], &gain[i], &pan[i] }) addAndMakeVisible (*c);
        }
        clipStart.setRange (0, 3540, 0.01); clipStart.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 26);
        clipStart.onValueChange = [this] { owner.recorder.strips[static_cast<size_t> (selected)].start.store (clipStart.getValue()); };
        addAndMakeVisible (clipStart); addAndMakeVisible (info); info.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        refresh(); startTimerHz (15);
    }
    void importFile (const juce::File& file, int localY)
    {
        if (owner.guitar.isRecording()) return;
        if (localY >= laneTop && localY < laneTop + 4 * laneHeight) { selected = juce::jlimit (0, 3, (localY - laneTop) / laneHeight); owner.armedTrack.store (selected); }
        owner.recorder.loadFile (selected, file, false, false, true, owner.recorder.position.load()); refresh();
    }
    void startTake()
    {
        if (owner.guitar.isRecording()) return;
        const auto file = owner.recordingLocation.nextTake ("Track" + juce::String (selected + 1));
        if (file == juce::File()) { owner.recorder.setStatus ("Recording folder could not be created. Choose another folder in Settings."); return; }
        if (! owner.startMultitrackTake (selected, file, mode.getSelectedId() == 1, preRoll.getSelectedId() - 1))
            owner.recorder.setStatus ("Could not start recording. " + owner.guitar.status());
    }
    void resized() override
    {
        play.setBounds (20, 108, 160, 34); stop.setBounds (194, 108, 115, 34); rewind.setBounds (323, 108, 115, 34);
        record.setBounds (452, 108, 290, 34); import.setBounds (756, 108, 250, 34);
        mode.setBounds (20, 158, 200, 30); preRoll.setBounds (238, 158, 220, 30);
        for (int n = 0; n < 4; ++n)
        {
            const auto i = static_cast<size_t> (n); const int y = laneTop + n * laneHeight;
            arm[i].setBounds (20, y + 6, 100, 25);
            gain[i].setBounds (20, y + 40, 124, 46); pan[i].setBounds (156, y + 40, 122, 46);
            mute[i].setBounds (20, y + 88, 75, 20); solo[i].setBounds (104, y + 88, 75, 20); clear[i].setBounds (196, y + 88, 82, 20);
        }
        clipStart.setBounds (170, 704, 360, 30); info.setBounds (20, 744, getWidth() - 40, 52);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white); g.setFont (24); g.drawText ("MULTITRACK RECORDER", 20, 12, getWidth() - 40, 36, juce::Justification::centredLeft);
        g.setFont (14); g.setColour (juce::Colours::lightgrey);
        g.drawFittedText ("Arm a lane and record up to 60 seconds. Existing lanes play while you overdub. NAM sound records the amp tone; load your capture on Guitar first. Space starts/stops transport; during recording it stops the take.", 20, 55, getWidth() - 40, 44, juce::Justification::topLeft, 2);
        g.drawText ("Position " + juce::String (owner.recorder.position.load(), 1) + " s   /   Selected input " + juce::String (owner.guitar.inputChannel.load() + 1), 482, 157, 520, 32, juce::Justification::centredLeft);
        const double span = timelineSpan(); const float width = getWidth() - 354.0f;
        const float left = 312;
        const std::array<juce::Colour, 4> colours { juce::Colour (0xff9cbb72), juce::Colour (0xff6bbbd3), juce::Colour (0xffd47c91), juce::Colour (0xffac88cf) };
        g.setColour (juce::Colour (0xff1d2024)); g.fillRect (300, 205, getWidth() - 330, 28);
        for (int t = 0; t <= static_cast<int> (span); t += span > 120 ? 30 : 10)
        {
            const int x = static_cast<int> (left + t / span * width); g.setFont (11); g.setColour (juce::Colour (0xffa1a6af));
            g.drawText (juce::String (t) + "s", juce::jmin (x + 3, getWidth() - 68), 207, 40, 23, juce::Justification::centredLeft);
        }
        for (int n = 0; n < 4; ++n)
        {
            const auto i = static_cast<size_t> (n); const int y = laneTop + n * laneHeight;
            const auto colour = colours[i];
            g.setColour (juce::Colour (n == selected ? 0xff222529 : 0xff17191c)); g.fillRect (12, y, 274, laneHeight - 6);
            g.setColour (colour); g.fillRect (12, y, 3, laneHeight - 6); g.setFont (10);
            g.drawText ("TRACK " + juce::String (n + 1), 182, y + 7, 92, 18, juce::Justification::centredRight);
            g.setColour (juce::Colour (0xffa1a6af)); g.setFont (10); g.drawText ("VOLUME", 26, y + 28, 110, 14, juce::Justification::centredLeft); g.drawText ("L        PAN        R", 158, y + 28, 118, 14, juce::Justification::centred);
            g.setColour (juce::Colour (n % 2 ? 0xff15171a : 0xff191b1e)); g.fillRect (300, y, getWidth() - 330, laneHeight - 6);
            for (int tick = 0; tick <= 60; ++tick)
            {
                const float x = left + tick / 60.0f * width;
                g.setColour (juce::Colour (tick % 10 == 0 ? 0xff363a40 : tick % 5 == 0 ? 0xff2b2e34 : 0xff22252a));
                g.drawVerticalLine (static_cast<int> (x), static_cast<float> (y), y + laneHeight - 6.0f);
            }
            const auto clip = owner.recorder.getClip (n); const auto extra = owner.recorder.getExtra (n);
            const bool recording = owner.guitar.isRecording() && owner.recorder.recordingLane.load() == n;
            const int count = (clip ? 1 : 0) + (extra ? static_cast<int> (extra->size()) : 0) + (recording ? 1 : 0);
            const float height = 96.0f / juce::jmax (1, count); int row = 0;
            auto drawRegion = [&] (const WaveformData& display, const juce::String& name, double start)
            {
                const float top = y + 6.0f + row++ * height;
            if (! display.peaks.empty())
            {
                const float x = left + static_cast<float> (start / span * width);
                const float w = juce::jmin (left + width - x, static_cast<float> (display.seconds / span * width));
                if (w > 0)
                {
                    const auto region = juce::Rectangle<float> (x, top, w, height);
                    g.setColour (colour.withAlpha (owner.recorder.strips[i].mute.load() ? 0.08f : 0.20f)); g.fillRoundedRectangle (region, 3);
                    g.setColour (colour.withAlpha (0.55f)); g.drawRoundedRectangle (region, 3, 0.8f);
                    g.setColour (colour.withAlpha (0.28f)); g.fillRoundedRectangle (x, top, w, juce::jmin (22.0f, height), 3);
                    g.setColour (juce::Colour (0xffeceef0)); g.setFont (10);
                    g.drawFittedText (name, static_cast<int> (x + 6), static_cast<int> (top + 2), juce::jmax (1, static_cast<int> (w - 12)), 18, juce::Justification::centredLeft, 1);
                    const float centre = top + height * .65f, amplitude = height * .32f;
                    g.setColour (colour.withMultipliedAlpha (owner.recorder.strips[i].mute.load() ? 0.3f : 1.0f));
                    for (int pixel = 0; pixel < static_cast<int> (w); ++pixel)
                    {
                        const size_t first = static_cast<size_t> (pixel / w * display.peaks.size());
                        const size_t end = juce::jmin (display.peaks.size(), juce::jmax (first + 1, static_cast<size_t> ((pixel + 1) / w * display.peaks.size())));
                        float low = 0, high = 0;
                        for (size_t bin = first; bin < end; ++bin) { low = juce::jmin (low, display.peaks[bin][0]); high = juce::jmax (high, display.peaks[bin][1]); }
                        g.drawVerticalLine (static_cast<int> (x) + pixel, centre - juce::jlimit (0.0f, 1.0f, high) * amplitude, centre - juce::jlimit (-1.0f, 0.0f, low) * amplitude);
                    }
                }
            }
            };
            const double start = owner.recorder.strips[i].start.load();
            if (clip) drawRegion (clip->waveform, clip->file.getFileName() + (clip->namPrinted ? " / NAM" : ""), start);
            if (extra) for (const auto& region : *extra) drawRegion (region.clip->waveform, region.clip->file.getFileName() + (region.clip->namPrinted ? " / NAM" : ""), start + region.offset);
            if (recording) drawRegion (owner.guitar.takeWave.snapshot (false), owner.guitar.preRollSeconds() > 0 ? "COUNT-IN" : "RECORDING", owner.recorder.recordingStart.load());
            if (count == 0) { g.setColour (juce::Colour (0xff737982)); g.setFont (12); g.drawText ("Drop audio on this track", 326, y + 44, static_cast<int> (width) - 24, 24, juce::Justification::centredLeft); }

        }
        const float playheadX = left + static_cast<float> (juce::jlimit (0.0, 1.0, owner.recorder.position.load() / span)) * width;
        g.setColour (juce::Colour (0xfff0f1f2)); g.drawVerticalLine (static_cast<int> (playheadX), 229, laneTop + 4 * laneHeight - 6.0f);
        juce::Path marker; marker.addTriangle (playheadX - 5, 225, playheadX + 5, 225, playheadX, 231); g.fillPath (marker);
        g.drawText ("Lane start (seconds)", 20, 704, 150, 30, juce::Justification::centredLeft);
        g.drawFittedText ("New takes stay on the timeline. Overlaps play together. Save preset retains all clips; WAV files stay in your recording folder.", 554, 700, getWidth() - 574, 42, juce::Justification::centredLeft, 2);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x >= 300 && e.y >= 206 && e.y < laneTop + 4 * laneHeight && ! owner.guitar.isRecording())
        { owner.seekMultitrack (juce::jlimit (0.0, 1.0, (e.x - 312.0) / (getWidth() - 354.0)) * timelineSpan()); }
    }
    void refresh()
    {
        selected = owner.armedTrack.load(); if (lastPresetRevision != owner.presetRevision) { lastPresetRevision = owner.presetRevision; mode.setSelectedId (owner.recorderRecordMode.load(), juce::dontSendNotification); preRoll.setSelectedId (owner.recorderCountIn.load() + 1, juce::dontSendNotification); }
        owner.serviceRecording();
        play.setButtonText (owner.recorder.playing.load() ? "Pause / Space" : "Play / Space");
        record.setEnabled (! owner.guitar.isRecording() && ! owner.recorder.loading.load()); import.setEnabled (record.isEnabled());
        mode.setEnabled (! owner.guitar.isRecording()); preRoll.setEnabled (! owner.guitar.isRecording()); clipStart.setEnabled (! owner.guitar.isRecording());
        clipStart.setValue (owner.recorder.strips[static_cast<size_t> (selected)].start.load(), juce::dontSendNotification);
        for (int n = 0; n < 4; ++n) { const auto i = static_cast<size_t> (n); const auto& strip = owner.recorder.strips[i]; if (! gain[i].isMouseButtonDown()) gain[i].setValue (juce::Decibels::gainToDecibels (strip.gain.load(), -60.0f), juce::dontSendNotification); if (! pan[i].isMouseButtonDown()) pan[i].setValue (strip.pan.load(), juce::dontSendNotification); mute[i].setToggleState (strip.mute.load(), juce::dontSendNotification); solo[i].setToggleState (strip.solo.load(), juce::dontSendNotification); clear[static_cast<size_t> (n)].setEnabled (! owner.guitar.isRecording() && ! owner.recorder.loading.load()); arm[static_cast<size_t> (n)].setToggleState (n == selected, juce::dontSendNotification); arm[static_cast<size_t> (n)].setEnabled (! owner.guitar.isRecording()); }
        info.setText (owner.guitar.isRecording() ? owner.guitar.status() : owner.recorder.status(), juce::dontSendNotification); repaint();
    }
private:
    static constexpr int laneTop = 235, laneHeight = 114;
    double timelineSpan() const
    {
        double span = 60;
        for (int n = 0; n < 4; ++n) { const double start = owner.recorder.strips[static_cast<size_t> (n)].start.load(); if (auto clip = owner.recorder.getClip (n)) span = juce::jmax (span, start + clip->waveform.seconds); if (auto extra = owner.recorder.getExtra (n)) for (const auto& r : *extra) span = juce::jmax (span, start + r.offset + r.clip->waveform.seconds); }
        if (owner.guitar.isRecording() && owner.recorder.recordingLane.load() >= 0)
            span = juce::jmax (span, owner.recorder.recordingStart.load() + 60);
        return std::ceil (span / 10) * 10;
    }
    void timerCallback() override { refresh(); }
    void chooseImport()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import audio into selected lane", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        juce::Component::SafePointer<MultitrackPanel> safe (this); const int lane = selected;
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, lane] (const juce::FileChooser& dialog) { if (safe && dialog.getResult().existsAsFile()) safe->owner.recorder.loadFile (lane, dialog.getResult(), false, false, true, safe->owner.recorder.position.load()); });
    }
    AvrxGrooveProcessor& owner;
    int selected = 0, lastPresetRevision = -1;
    juce::TextButton play { "Play / Space" }, stop { "Stop" }, rewind { "Rewind" }, record { "RECORD armed lane / Num 3" }, import { "Import audio" };
    std::array<juce::ToggleButton, 4> arm, mute, solo;
    std::array<juce::TextButton, 4> clear;
    std::array<juce::Slider, 4> gain, pan;
    juce::ComboBox mode, preRoll;
    juce::Slider clipStart;
    juce::Label info;
    std::unique_ptr<juce::FileChooser> chooser;
};
}
