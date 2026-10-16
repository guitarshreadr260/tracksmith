#include "PluginEditor.h"
namespace TracksmithGrooveFull {

static juce::Colour drumColour (int drum)
{
    if (drum == 0) return juce::Colour (0xffd6b27b);
    if (drum == 1) return juce::Colour (0xffca8d98);
    if (drum < 4) return juce::Colour (0xff83c9bb);
    return juce::Colour (0xff9aa8ce);
}

void DrumPad::setDetails (juce::String sample, bool selected, bool muted)
{
    sound = std::move (sample); chosen = selected; silent = muted;
    setTooltip (sound); repaint();
}

void DrumPad::paintButton (juce::Graphics& g, bool hover, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced (1);
    const auto colour = drumColour (drum);
    g.setColour (chosen || down ? colour.withAlpha (0.10f) : juce::Colour (0xff17191c));
    g.fillRoundedRectangle (area, 5);
    g.setColour (chosen || hover ? colour : juce::Colour (0xff384253));
    g.drawRoundedRectangle (area, 5, chosen ? 1.2f : 0.6f);
    g.setColour (juce::Colour (0xff8a9098)); g.setFont (9); g.drawText (juce::String (drum + 1).paddedLeft ('0', 2), 9, 6, 24, 16, juce::Justification::centredLeft);
    constexpr std::array<std::array<float, 4>, 9> crops {{{ .36f, .43f, .30f, .48f }, { .17f, .38f, .23f, .22f }, { .01f, .24f, .23f, .16f }, { .01f, .24f, .23f, .16f }, { .33f, .25f, .17f, .21f }, { .50f, .25f, .18f, .21f }, { .65f, .40f, .22f, .33f }, { .13f, .10f, .27f, .15f }, { .62f, .13f, .30f, .16f } }};
    const auto& crop = crops[static_cast<size_t> (drum)];
    if (artwork.isValid())
        g.drawImage (artwork, 4, 7, getWidth() - 8, 49, static_cast<int> (crop[0] * artwork.getWidth()), static_cast<int> (crop[1] * artwork.getHeight()), static_cast<int> (crop[2] * artwork.getWidth()), static_cast<int> (crop[3] * artwork.getHeight()));
    g.setColour (silent ? juce::Colours::grey : colour);
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    g.drawText (getName() + (silent ? " [M]" : ""), 4, 59, getWidth() - 8, 23, juce::Justification::centred);
    g.setColour (juce::Colour (0xff9b9fa6)); g.setFont (11.5f);
    g.drawFittedText (sound, 8, 84, getWidth() - 16, 20, juce::Justification::centred, 1);
}

std::array<int, GrooveGenerator::drumCount> DrumSequencer::lanes (const DrumKit& kit)
{
    std::array<int, GrooveGenerator::drumCount> result {};
    for (size_t n = 0; n < result.size(); ++n) result[n] = static_cast<int> (n);
    std::stable_sort (result.begin(), result.end(), [&] (int a, int b) { return kit.notes[static_cast<size_t> (a)] > kit.notes[static_cast<size_t> (b)]; });
    return result;
}
void DrumSequencer::paint (juce::Graphics& g)
{
    const auto kit = owner.getKit(); const auto mapping = lanes (*kit);
    const auto clip = g.getClipBounds();
    const int first = juce::jmax (0, clip.getX() / cellWidth), last = juce::jmin (kit->stepCount(), clip.getRight() / cellWidth + 1);
    g.fillAll (juce::Colour (0xff101214));
    for (int row = 0; row < GrooveGenerator::drumCount; ++row)
    {
        const int drum = mapping[static_cast<size_t> (row)];
        g.setColour (juce::Colour (row % 2 == 0 ? 0xff191b1e : 0xff141619));
        g.fillRect (clip.getX(), top + row * rowHeight, clip.getWidth(), rowHeight);
        g.setColour (juce::Colour (0xff272a2f)); g.drawHorizontalLine (top + (row + 1) * rowHeight, static_cast<float> (clip.getX()), static_cast<float> (clip.getRight()));
        for (int cell = first; cell < last; ++cell)
        {
            const float v = kit->pattern[static_cast<size_t> (drum)][static_cast<size_t> (cell)];
            if (v <= 0) continue;
            const juce::Rectangle<float> note (static_cast<float> (cell * cellWidth + 2), static_cast<float> (top + row * rowHeight + 3), cellWidth - 4.0f, rowHeight - 6.0f);
            g.setColour (kit->muted[static_cast<size_t> (drum)] ? juce::Colours::grey : drumColour (drum).withAlpha (0.4f + v * 0.6f)); g.fillRect (note);
            g.setColour (juce::Colour (0x55000000)); g.fillRect (note.withWidth (2));
            if (drum == selectedDrum && cell == selectedCell) { g.setColour (juce::Colours::white); g.drawRect (note, 1.5f); }
        }
    }
    g.setFont (11);
    for (int cell = first; cell < last; ++cell)
    {
        const bool bar = cell % kit->stepsPerBar() == 0, beat = cell % kit->division == 0;
        g.setColour (juce::Colour (bar ? 0xff656a72 : beat ? 0xff35393e : 0xff25282c));
        g.drawVerticalLine (cell * cellWidth, static_cast<float> (top), static_cast<float> (velocityTop + 16));
        if (bar || beat) g.drawText (juce::String (cell / kit->stepsPerBar() + 1) + "." + juce::String (cell % kit->stepsPerBar() / kit->division + 1), cell * cellWidth + 3, 0, cellWidth * kit->division - 4, top, juce::Justification::centredLeft);
        const auto v = kit->pattern[static_cast<size_t> (selectedDrum)][static_cast<size_t> (cell)];
        if (v > 0) { g.setColour (drumColour (selectedDrum)); g.fillRect (cell * cellWidth + 8.0f, velocityTop + 16.0f * (1 - v), 3.0f, 16.0f * v); }
    }
    const int current = owner.getPlayingStep();
    if (current >= 0) { g.setColour (juce::Colour (0xfff2d48c)); g.drawVerticalLine (current * cellWidth, 0, static_cast<float> (getHeight())); }
}
void DrumSequencer::mouseDown (const juce::MouseEvent& e)
{
    const auto kit = owner.getKit(); const int cell = e.x / cellWidth;
    if (cell < 0 || cell >= kit->stepCount()) return;
    if (e.y >= velocityTop)
    {
        if (kit->pattern[static_cast<size_t> (selectedDrum)][static_cast<size_t> (cell)] <= 0) return;
        selectedCell = cell; startY = e.y; dragDrum = selectedDrum; dragCell = cell;
        owner.setStepVelocity (selectedDrum, cell, juce::jlimit (0.05f, 1.0f, (velocityTop + 16 - e.y) / 16.0f)); repaint(); return;
    }
    const int row = (e.y - top) / rowHeight;
    if (e.y < top || row < 0 || row >= GrooveGenerator::drumCount) return;
    selectedDrum = lanes (*kit)[static_cast<size_t> (row)]; selectedCell = cell;
    if (onSelect) onSelect (selectedDrum);
    const float old = kit->pattern[static_cast<size_t> (selectedDrum)][static_cast<size_t> (cell)];
    if (e.mods.isRightButtonDown()) { owner.setStepVelocity (selectedDrum, cell, 0); dragCell = -1; repaint(); return; }
    startVelocity = old > 0 ? old : 0.75f; startY = e.y; dragDrum = selectedDrum; dragCell = cell;
    if (old <= 0) owner.setStepVelocity (selectedDrum, cell, startVelocity);
    repaint();
}
void DrumSequencer::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCell < 0) return;
    if (e.mods.isShiftDown() || e.getMouseDownY() >= velocityTop)
    {
        const float v = e.getMouseDownY() >= velocityTop ? (velocityTop + 16 - e.y) / 16.0f : startVelocity + (startY - e.y) / 100.0f;
        owner.setStepVelocity (dragDrum, dragCell, juce::jlimit (0.05f, 1.0f, v)); repaint(); return;
    }
    const auto kit = owner.getKit(); const int cell = juce::jlimit (0, kit->stepCount() - 1, e.x / cellWidth);
    const int row = juce::jlimit (0, GrooveGenerator::drumCount - 1, (e.y - top) / rowHeight);
    const int drum = lanes (*kit)[static_cast<size_t> (row)];
    if (cell != dragCell || drum != dragDrum)
    {
        const float velocity = kit->pattern[static_cast<size_t> (dragDrum)][static_cast<size_t> (dragCell)];
        owner.setStepVelocity (dragDrum, dragCell, 0); owner.setStepVelocity (drum, cell, velocity);
        selectedDrum = dragDrum = drum; selectedCell = dragCell = cell; if (onSelect) onSelect (drum); repaint();
    }
}
void DrumSequencer::mouseDoubleClick (const juce::MouseEvent&)
{ if (selectedCell >= 0) { owner.setStepVelocity (selectedDrum, selectedCell, 0); repaint(); } }

AvrxGrooveEditor::AvrxGrooveEditor (AvrxGrooveProcessor& processor)
    : AudioProcessorEditor (processor), owner (processor), sequencer (processor), midiDrag (processor), guitarPanel (processor), audioPanel (processor), multitrackPanel (processor), mixerPanel (processor), studioKit (processor), groovePanel (processor), referenceWave (processor, false), sampleWave (processor, true), channelStrip (processor)
{
    setLookAndFeel (&theme);
    addAndMakeVisible(followHost);followAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.parameters,"hostSync",followHost);
    const std::array<juce::String, 3> ids { "tempo", "swing", "level" };
    const std::array<juce::String, 3> names { "TEMPO", "SWING", "DRUM MASTER" };
    auto setup = [this] (juce::Slider& slider, double max)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
        slider.setRange (0, max, 0.01); addAndMakeVisible (slider);
    };
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        setup (sliders[i], 1);
        labels[i].setText (names[i], juce::dontSendNotification);
        labels[i].setFont (juce::FontOptions (11.0f)); labels[i].setColour (juce::Label::textColourId, juce::Colour (0xff9b9fa6));
        addAndMakeVisible (labels[i]);
        attachments[i] = std::make_unique<SliderAttachment> (owner.parameters, ids[i], sliders[i]);
    }
    for (auto* button : { &load, &together, &stop, &preview, &defaults, &sampleLoad, &sampleClear, &matchKey }) addAndMakeVisible (*button);
    addAndMakeVisible (run); addAndMakeVisible (mute);
    runAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (owner.parameters, "run", run);
    load.onClick = [this] { chooseAudio(); };
    together.onClick = [this] { owner.playTogether(); };
    stop.onClick = [this] { owner.stopTogether(); };
    preview.onClick = [this] { owner.setTrackPlaying (! owner.isTrackPlaying()); };
    defaults.onClick = [this] { owner.useDefaultGroove(); };
    sampleLoad.onClick = [this] { chooseSample(); };
    sampleClear.onClick = [this] { owner.clearSample (selectedPad, layer.getSelectedId() - 1); };
    matchKey.onClick = [this] { owner.matchSongKey (selectedPad, songKey.getSelectedId() - 1); selectPad (selectedPad); };
    mute.onClick = [this] { owner.setPadMuted (selectedPad, mute.getToggleState()); };
    for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
    {
        pads[static_cast<size_t> (drum)] = std::make_unique<DrumPad> (drum);
        auto& pad = *pads[static_cast<size_t> (drum)];
        addAndMakeVisible (pad);
        pad.onClick = [this, drum] { selectPad (drum); owner.auditionPad (drum); };
    }
    sequenceViewport.setViewedComponent (&sequencer, false); sequenceViewport.setScrollBarsShown (false, true);
    sequenceViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never); addAndMakeVisible (sequenceViewport);
    sequencer.onSelect = [this] (int drum) { selectPad (drum); };
    for (auto* slider : { &trackVolume, &roomVolume, &overheadVolume }) setup (*slider, 1);
    setup (padGain, 1.5); setup (noteNumber, 127); noteNumber.setRange (0, 127, 1);
    setup (layerGain, 1.5); setup (tuning, 24); setup (fineTune, 100);
    tuning.setRange (-24, 24, 1); fineTune.setRange (-100, 100, 1);
    trackVolume.setValue (owner.getTrackLevel(), juce::dontSendNotification);
    trackVolume.onValueChange = [this] { owner.setTrackLevel (static_cast<float> (trackVolume.getValue())); };
    roomVolume.onValueChange = overheadVolume.onValueChange = [this]
    { owner.setAmbience (static_cast<float> (roomVolume.getValue()), static_cast<float> (overheadVolume.getValue())); };
    padGain.onValueChange = [this] { owner.setPadGain (selectedPad, static_cast<float> (padGain.getValue())); };
    noteNumber.onValueChange = [this] { owner.setMidiNote (selectedPad, static_cast<int> (noteNumber.getValue())); };
    layerGain.onValueChange = [this] { owner.setLayerGain (selectedPad, layer.getSelectedId() - 1, static_cast<float> (layerGain.getValue())); };
    tuning.onValueChange = fineTune.onValueChange = [this] { owner.setTuning (selectedPad, static_cast<float> (tuning.getValue()), static_cast<float> (fineTune.getValue())); };
    for (auto* label : { &status, &kitStatus, &dragStatus, &padName })
    { label->setColour (juce::Label::textColourId, juce::Colour (0xff9b9fa6)); label->setFont (juce::FontOptions (12.0f)); addAndMakeVisible (*label); }
    mapping.addItem ("Logic / General MIDI", 1); mapping.addItem ("External GM (channel 10)", 2);
    mapping.setSelectedId (owner.getKit()->midiChannel == 10 ? 2 : 1, juce::dontSendNotification);
    mapping.onChange = [this] { owner.resetMidiMap (mapping.getSelectedId() == 1); selectPad (selectedPad); };
    addChildComponent(mapping);
    midiMappingPanel=std::make_unique<MidiMappingPanel>(owner);addChildComponent(*midiMappingPanel);
    addAndMakeVisible(editMapping);editMapping.onClick=[this]{midiMappingPanel->setBounds(410,160,480,520);midiMappingPanel->setVisible(true);midiMappingPanel->toFront(true);};
    for (int length = 1; length <= 16; ++length) bars.addItem (juce::String (length) + (length == 1 ? " bar" : " bars"), length);
    bars.setSelectedId (owner.getKit()->bars, juce::dontSendNotification);
    for (const auto& pair : std::array<std::pair<int, const char*>, 6> {{{ 1, "Quarter" }, { 2, "8th notes" }, { 4, "16th notes" }, { 8, "32nd notes" }, { 3, "8th triplets" }, { 6, "16th triplets" }}}) division.addItem (pair.second, pair.first);
    division.setSelectedId (owner.getKit()->division, juce::dontSendNotification);
    int id = 1; for (const auto* name : { "4/4", "3/4", "5/4", "6/8", "7/8", "9/8", "12/8" }) signature.addItem (name, id++);
    signature.setSelectedId (1, juce::dontSendNotification);
    constexpr std::array<int, 7> initialNumerators { 4, 3, 5, 6, 7, 9, 12 }, initialDenominators { 4, 4, 4, 8, 8, 8, 8 };
    for (size_t index = 0; index < initialNumerators.size(); ++index)
        if (owner.getKit()->numerator == initialNumerators[index] && owner.getKit()->denominator == initialDenominators[index]) signature.setSelectedId (static_cast<int> (index + 1), juce::dontSendNotification);
    bars.onChange = signature.onChange = division.onChange = [this] { changeSequence(); };
    for (auto* box : { &bars, &signature, &division, &layer, &rootNote, &songKey }) addAndMakeVisible (*box);
    for (int index = 1; index <= 4; ++index) layer.addItem ("Layer " + juce::String (index), index);
    layer.setSelectedId (1, juce::dontSendNotification); layer.onChange = [this] { selectPad (selectedPad); };
    rootNote.addItem ("Root: unknown", 1);
    const std::array<const char*, 12> pitchNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int pc = 0; pc < 12; ++pc) { rootNote.addItem ("Root: " + juce::String (pitchNames[static_cast<size_t> (pc)]), pc + 2); songKey.addItem ("Key: " + juce::String (pitchNames[static_cast<size_t> (pc)]), pc + 1); }
    songKey.setSelectedId (1, juce::dontSendNotification);
    rootNote.onChange = [this] { owner.setRootNote (selectedPad, rootNote.getSelectedId() - 2); };
    addAndMakeVisible (midiDrag);
    midiDrag.onStatus = [this] (juce::String text) { dragStatus.setText (text, juce::dontSendNotification); dragStatus.setTooltip (text); };
    dragStatus.setText ("Drag MIDI to a DAW or Explorer on this computer.", juce::dontSendNotification);
    for (juce::Component* control : std::array<juce::Component*, 15> { &sampleLoad, &sampleClear, &matchKey, &mute, &padGain, &noteNumber, &layerGain, &tuning, &fineTune, &layer, &rootNote, &songKey, &kitStatus, &padName, &sampleWave })
        if (std::find (sampleControls.begin(), sampleControls.end(), control) == sampleControls.end()) sampleControls.push_back (control);
    for (auto& pad : pads) sampleControls.push_back (pad.get());
    sampleControls.push_back (&channelStrip); addChildComponent (channelStrip);
    addAndMakeVisible (packSelector); sampleControls.push_back (&packSelector); refreshPacks();
    packSelector.onChange = [this] { if (packSelector.getSelectedId() == 1) owner.useBuiltInKit(); else if (packSelector.getSelectedId() >= 2 && packSelector.getSelectedId() - 2 < static_cast<int> (packFiles.size())) owner.loadDrumPack (packFiles[static_cast<size_t> (packSelector.getSelectedId() - 2)]); };
    addAndMakeVisible (studioKit); studioKit.onSelect = [this] (int drum) { selectPad (drum); }; studioKit.onEdit = [this] (int drum) { selectPad (drum); changeTab (5); };
    addAndMakeVisible (referenceWave); addChildComponent (sampleWave);
    for (int child = 0; child < getNumChildComponents(); ++child)
    {
        auto* control = getChildComponent (child);
        if (std::find (sampleControls.begin(), sampleControls.end(), control) == sampleControls.end()) drumControls.push_back (control);
    }
    for (auto* button : { &drumsTab, &guitarTab, &settingsTab, &audioTab, &multitrackTab, &samplesTab, &mixerTab, &groovesTab }) addAndMakeVisible (*button);
    groovesTab.onClick = [this] { changeTab (7); }; addChildComponent (groovePanel);
    drumsTab.onClick = [this] { changeTab (0); }; guitarTab.onClick = [this] { changeTab (1); }; settingsTab.onClick = [this] { changeTab (2); };
    mixerPanel.onEdit = [this] (int drum) { selectPad (drum); changeTab (5); };
    mixerTab.onClick = [this] { changeTab (6); }; addChildComponent (mixerPanel);
    samplesTab.onClick = [this] { changeTab (5); };
    multitrackTab.onClick = [this] { changeTab (4); };
    addChildComponent (multitrackPanel);
    audioTab.onClick = [this] { changeTab (3); };
    addChildComponent (audioPanel); addChildComponent (guitarPanel); addChildComponent (settingsPanel);
    settingsPanel.addAndMakeVisible (toneSetup); toneSetup.onClick = [this] { guitarPanel.openTone3000(); };
    settingsPanel.addAndMakeVisible (recordingFolderButton); settingsPanel.addAndMakeVisible (recordingFolderLabel);
    recordingFolderButton.onClick = [this] { chooseRecordingFolder(); };
    recordingFolderLabel.setFont (juce::FontOptions (12)); recordingFolderLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc4c6c9));
    settingsPanel.addAndMakeVisible (settingsInfo); settingsInfo.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    if (auto* devices = owner.getDeviceManager())
    {
        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (*devices, 0, 2, 0, 2, true, false, false, false);
        settingsPanel.addAndMakeVisible (*deviceSelector);
        settingsInfo.setText ("Choose audio input/output devices, enabled channels, sample rate and buffer size. Stop recording before changing routing.", juce::dontSendNotification);
    }
    else settingsInfo.setText ("Audio routing is controlled by the DAW when used as a plugin. The Standalone Settings tab provides PC input/output routing.", juce::dontSendNotification);
    setSize (1280, 900);
    addAndMakeVisible(useInSong);useInSong.onClick=[this]{if(owner.sendGrooveToSong)owner.sendGrooveToSong();};
    setWantsKeyboardFocus (true);
    std::function<void (juce::Component&)> registerKeys = [&] (juce::Component& c)
    { c.addKeyListener (this); for (int n = 0; n < c.getNumChildComponents(); ++n) registerKeys (*c.getChildComponent (n)); };
    registerKeys (*this);
    for (auto* tab : { &drumsTab, &samplesTab, &guitarTab, &audioTab, &multitrackTab, &settingsTab, &mixerTab, &groovesTab }) tab->getProperties().set ("navigation", true);
    const std::array<juce::TextButton*, 5> namedTabs { &drumsTab, &groovesTab, &samplesTab, &audioTab, &mixerTab };
    const std::array<const char*, 5> namesForTabs { "01  DRUM ENGINE", "02  GROOVE PLAYER", "03  SOUND LAB", "04  ANALYZER", "05  MIX CONSOLE" };
    for (size_t n = 0; n < namedTabs.size(); ++n) namedTabs[n]->getProperties().set ("navLabel", namesForTabs[n]);
    addAndMakeVisible (savePresetButton); addAndMakeVisible (loadPresetButton); addAndMakeVisible (presetMessage);
    presetMessage.setFont (juce::FontOptions (11.0f)); presetMessage.setColour (juce::Label::textColourId, juce::Colour (0xffbcb39e));
    savePresetButton.onClick = [this] { choosePreset (true); }; loadPresetButton.onClick = [this] { choosePreset (false); };
    resized(); selectPad (0); changeTab (0); timerCallback(); startTimerHz (20);
}

AvrxGrooveEditor::~AvrxGrooveEditor() { stopTimer(); if(owner.previewingGroove.load())owner.stopGroovePreview(); setLookAndFeel (nullptr); }

void AvrxGrooveEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0c0e));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff202327), 0, 0, juce::Colour (0xff111315), 200, 900, false)); g.fillRect (0, 0, 200, getHeight());
    g.setColour (juce::Colour (0xff45484d)); g.drawVerticalLine (199, 0, static_cast<float> (getHeight()));
    g.setColour (juce::Colour (0xffd6b27b)); g.setFont (juce::FontOptions (19).withStyle ("Bold")); g.drawText ("TRACKSMITH", 24, 22, 176, 36, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (24).withStyle ("Bold")); g.drawText ("GROOVE", 24, 58, 160, 32, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffa2a09c)); g.setFont (10); g.drawFittedText ("INTELLIGENT DRUM\nWORKSTATION", 24, 102, 162, 32, juce::Justification::centredLeft, 2);
    g.setColour (juce::Colour (0xff56524a)); g.drawHorizontalLine (149, 24, 176);
    if (activeTab == 0)
    {
        g.setColour (juce::Colour (0xffa2a09c)); g.setFont (10);
        g.drawText ("MONITOR & AMBIENCE", 24, 568, 160, 20, juce::Justification::centredLeft);
        g.drawText ("REFERENCE", 24, 614, 150, 18, juce::Justification::centredLeft);
        g.drawText ("ROOM", 24, 682, 150, 18, juce::Justification::centredLeft);
        g.drawText ("OVERHEADS", 24, 750, 150, 18, juce::Justification::centredLeft);
    }
    g.setColour (juce::Colour (0xff777d86)); g.setFont (10); g.drawText ("NATIVE  /  TRACKSMITH", 24, 854, 154, 22, juce::Justification::centredLeft);
    g.addTransform (juce::AffineTransform::translation (200, 0));
    g.setColour (juce::Colour (0xff111315)); g.fillRect (0, 0, 1080, 64);
    g.setColour (juce::Colour (0xff303339)); g.drawHorizontalLine (63, 24, 1056);
    constexpr std::array<const char*, 8> titles { "DRUM ENGINE", "GUITAR STUDIO", "AUDIO ROUTING", "PERFORMANCE ANALYSIS", "MULTITRACK RECORDER", "SAMPLE & SOUND LAB", "MIX CONSOLE", "GROOVE PLAYER" };
    g.setColour (juce::Colour (0xffe8e4dc)); g.setFont (juce::FontOptions (19).withStyle ("Bold")); g.drawText (titles[static_cast<size_t> (activeTab)], 24, 15, 600, 32, juce::Justification::centredLeft);

    if (activeTab == 5)
    {
        g.setColour (juce::Colour (0xffd6b27b)); g.setFont (juce::FontOptions (23).withStyle ("Bold"));
        g.drawText ("DRUM LAB  /  " + juce::String (GrooveGenerator::names[static_cast<size_t> (selectedPad)]).toUpperCase(), 32, 74, 760, 30, juce::Justification::centredLeft);
        g.setColour (juce::Colours::lightgrey); g.setFont (12);
        g.drawText ("LAYERS  >  TUNING  >  CONSOLE EQ  >  PARALLEL COMPRESSION", 32, 104, 1008, 24, juce::Justification::centredLeft);
        const auto label = [&] (const char* text, int x, int y, int width) { g.drawText (text, x, y, width, 18, juce::Justification::centredLeft); };
        label ("LAYER LEVEL", 516, 294, 170); label ("PAD LEVEL", 698, 294, 170); label ("MIDI NOTE", 880, 294, 160);
        label ("SEMITONES", 516, 346, 170); label ("FINE CENTS", 698, 346, 170);
        return;
    }
    if (activeTab != 0) return;
    auto panel = [&] (int y, int height) { g.setColour (juce::Colour (0xff15171a)); g.fillRoundedRectangle (20.0f, static_cast<float> (y), 1080 - 40.0f, static_cast<float> (height), 10); };
    g.setColour (juce::Colour (0xff17191c)); g.fillRoundedRectangle (20, 196, 380, 342, 7);
    panel (550, 248); panel (808, 76);
    g.setColour (juce::Colours::white); g.setFont (juce::FontOptions (23.0f).withStyle ("Bold"));
    g.setColour (juce::Colour (0xff83c9bb)); g.setFont (12.0f);
    auto caption = [&] (juce::String text, int x, int y, int width)
    { g.setColour (juce::Colour (0xff9b9fa6)); g.setFont (11.0f); g.drawText (text, x, y, width, 20, juce::Justification::centredLeft); };
    caption ("PERFORMANCE INPUT", 32, 200, 350);
    caption ("GROOVE EDITOR", 32, 555, 200);
    caption ("Drag / Shift: velocity / Right-click: delete", 724, 557, 322);
    const auto kit = owner.getKit();
    const auto lanes = DrumSequencer::lanes (*kit);
    for (int row = 0; row < GrooveGenerator::drumCount; ++row)
    {
        const int drum = lanes[static_cast<size_t> (row)]; const int note = kit->notes[static_cast<size_t> (drum)];
        const bool black = note % 12 == 1 || note % 12 == 3 || note % 12 == 6 || note % 12 == 8 || note % 12 == 10;
        g.setColour (juce::Colour (black ? 0xff111214 : 0xff303339)); g.fillRect (32, 605 + row * 18, 116, 17);
        g.setColour (drumColour (drum)); g.setFont (10.5f);
        g.drawText (juce::String (note) + "  " + GrooveGenerator::names[static_cast<size_t> (drum)], 36, 605 + row * 18, 108, 17, juce::Justification::centredLeft);
    }
    caption ("MIDI MAP", 34, 809, 250);
}

void AvrxGrooveEditor::resized()
{
    packSelector.setBounds (714, 77, 330, 30);
    useInSong.setBounds(604,10,184,30);
    savePresetButton.setBounds (804, 10, 120, 30); loadPresetButton.setBounds (936, 10, 120, 30);
    presetMessage.setBounds (24, 42, 1032, 18);
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        const int x = 24 + static_cast<int> (i) * 352;
        labels[i].setBounds (x, 66, 320, 18); sliders[i].setBounds (x, 84, 320, 28);
    }
    load.setBounds (24, 127, 160, 32); together.setBounds (194, 127, 142, 32);
    preview.setBounds (346, 127, 130, 32); stop.setBounds (486, 127, 142, 32);
    run.setBounds (644, 127, 170, 32); defaults.setBounds (856, 127, 200, 32);
    status.setBounds (24, 164, 375, 28);
    const std::array<juce::TextButton*, 5> tabs { &drumsTab, &groovesTab, &samplesTab, &audioTab, &mixerTab };
    for (size_t n = 0; n < tabs.size(); ++n) tabs[n]->setBounds (20, 172 + static_cast<int> (n) * 45, 160, 40);
    const int padWidth = (1080 - 64 - 8 * 8) / 9;
    for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
        pads[static_cast<size_t> (drum)]->setBounds (32 + drum * (padWidth + 8), 136, padWidth, 104);
    referenceWave.setBounds (32, 228, 356, 290);
    studioKit.setBounds (410, 174, 638, 364);
    sampleWave.setBounds (32, 254, 458, 178);
    channelStrip.setBounds (32, 484, 1080 - 64, 398);
    padName.setBounds (516, 254, 140, 30); layer.setBounds (666, 254, 126, 30);
    sampleLoad.setBounds (804, 254, 112, 30); sampleClear.setBounds (928, 254, 116, 30);
    layerGain.setBounds (516, 314, 168, 26); padGain.setBounds (698, 314, 168, 26); noteNumber.setBounds (880, 314, 164, 26);
    tuning.setBounds (516, 368, 168, 26); fineTune.setBounds (698, 368, 168, 26); mute.setBounds (880, 368, 164, 26);
    rootNote.setBounds (516, 408, 154, 26); songKey.setBounds (686, 408, 130, 26); matchKey.setBounds (834, 408, 210, 26);
    kitStatus.setBounds (32, 444, 1006, 30);
    bars.setBounds (244, 555, 112, 25); signature.setBounds (370, 555, 92, 25); division.setBounds (476, 555, 232, 25);
    sequenceViewport.setBounds (154, 583, 894, 216);
    sequencer.setSize (owner.getKit()->stepCount() * DrumSequencer::cellWidth, 200);
    trackVolume.setBounds (34, 756, 304, 28); roomVolume.setBounds (386, 756, 304, 28); overheadVolume.setBounds (738, 756, 304, 28);
    editMapping.setBounds (34, 838, 255, 30); midiDrag.setBounds (310, 826, 300, 48); dragStatus.setBounds (630, 814, 414, 64);
    groovePanel.setBounds (20, 82, 1080 - 40, getHeight() - 100);
    mixerPanel.setBounds (20, 82, 1080 - 40, getHeight() - 100);
    multitrackPanel.setBounds (20, 82, 1080 - 40, getHeight() - 100);
    audioPanel.setBounds (20, 82, 1080 - 40, getHeight() - 100);
    guitarPanel.setBounds (20, 82, 1080 - 40, getHeight() - 100);
    settingsPanel.setBounds (20, 82, 1080 - 40, getHeight() - 100);
    settingsInfo.setBounds (24, 12, settingsPanel.getWidth() - 48, 64);
    toneSetup.setBounds (24, 132, 190, 28);
    recordingFolderButton.setBounds (24, 88, 190, 32); recordingFolderLabel.setBounds (230, 78, settingsPanel.getWidth() - 254, 58);
    if (deviceSelector) deviceSelector->setBounds (24, 178, settingsPanel.getWidth() - 48, settingsPanel.getHeight() - 198);
    for (int n = 0; n < getNumChildComponents(); ++n)
    {
        auto* child = getChildComponent (n);
        if (std::find (tabs.begin(), tabs.end(), child) == tabs.end()) child->setTopLeftPosition (child->getX() + 200, child->getY());
    }
    followHost.setBounds(24,540,152,24);
    trackVolume.setBounds (24, 638, 152, 28); roomVolume.setBounds (24, 706, 152, 28); overheadVolume.setBounds (24, 774, 152, 28);
}

void AvrxGrooveEditor::selectPad (int drum)
{
    selectedPad = drum; studioKit.selectDrum (drum); sequencer.selectDrum (drum);
    sampleWave.selectLayer (drum, layer.getSelectedId() - 1); channelStrip.selectDrum (drum);
    const auto kit = owner.getKit(); const auto index = static_cast<size_t> (drum);
    padName.setText (GrooveGenerator::names[index], juce::dontSendNotification);
    padGain.setValue (kit->gains[index], juce::dontSendNotification);
    noteNumber.setValue (kit->notes[index], juce::dontSendNotification);
    mute.setToggleState (kit->muted[index], juce::dontSendNotification);
    layerGain.setValue (kit->layerGains[index][static_cast<size_t> (layer.getSelectedId() - 1)], juce::dontSendNotification);
    tuning.setValue (kit->tuning[index], juce::dontSendNotification); fineTune.setValue (kit->fineTuning[index], juce::dontSendNotification);
    rootNote.setSelectedId (kit->rootNotes[index] + 2, juce::dontSendNotification);
}

void AvrxGrooveEditor::timerCallback()
{
    const bool synced=owner.hostAvailable.load();sliders[0].setEnabled(!synced);labels[0].setText(synced?"DAW TEMPO: "+juce::String(owner.hostTempo.load(),1)+" BPM":"TEMPO",juce::dontSendNotification);
    presetMessage.setText (owner.presetStatus, juce::dontSendNotification); presetMessage.setTooltip (owner.presetStatus);
    const bool presetReady = ! owner.guitar.isRecording() && ! owner.isLoadingSample() && ! owner.isAnalysing() && ! owner.recorder.loading.load();
    savePresetButton.setEnabled (presetReady); loadPresetButton.setEnabled (presetReady);
    recordingFolderButton.setEnabled (! owner.guitar.isRecording());
    const auto folderPath = owner.recordingLocation.get().getFullPathName();
    recordingFolderLabel.setText ("Takes save automatically to:\n" + folderPath, juce::dontSendNotification); recordingFolderLabel.setTooltip (folderPath);

    owner.applyAnalysisTempo(); owner.applyLiveGroove();
    status.setText (owner.getAnalysisStatus() + "  |  Source " + juce::String (owner.trackPositionSeconds(), 1) + " s", juce::dontSendNotification);
    kitStatus.setText (owner.getKitStatus(), juce::dontSendNotification);
    load.setEnabled (! owner.isAnalysing()); together.setEnabled (! owner.isAnalysing());
    preview.setEnabled (owner.hasTrack() && ! owner.isAnalysing());
    preview.setButtonText (owner.isTrackPlaying() ? "Pause source" : "Play source");
    packSelector.setEnabled (! owner.isLoadingSample() && ! owner.guitar.isRecording() && ! owner.isAnalysing());
    sampleLoad.setEnabled (! owner.isLoadingSample()); sampleClear.setEnabled (! owner.isLoadingSample());
    midiDrag.setEnabled (! owner.isAnalysing() && ! owner.isLoadingSample());
    if (deviceSelector) deviceSelector->setEnabled (! owner.guitar.isRecording());
    const auto kit = owner.getKit();
    for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
    {
        const auto index = static_cast<size_t> (drum);
        int count = 0; for (int slot = 0; slot < 4; ++slot) if (kit->layer (drum, slot)) ++count;
        pads[index]->setDetails (! kit->velocityGroups[index].empty() ? juce::String (static_cast<int> (kit->velocityGroups[index].size())) + " velocity groups / round robin" : count > 0 ? juce::String (count) + (count == 1 ? " sample layer" : " sample layers") : "Built-in drum", drum == selectedPad, kit->muted[index]);
    }
    packSelector.setText (kit->packName.isEmpty() ? "Select drum pack" : kit->packName, juce::dontSendNotification);
    roomVolume.setValue (kit->roomLevel, juce::dontSendNotification);
    overheadVolume.setValue (kit->overheadLevel, juce::dontSendNotification);
    if (activeTab == 0) syncSequenceControls();
    channelStrip.repaint();
    referenceWave.repaint(); sampleWave.repaint();
    sequencer.repaint();
    if (sequencer.getWidth() != kit->stepCount() * DrumSequencer::cellWidth) sequencer.setSize (kit->stepCount() * DrumSequencer::cellWidth, 200);
}

void AvrxGrooveEditor::chooseAudio()
{
    chooser = std::make_unique<juce::FileChooser> ("Analyse reference audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<AvrxGrooveEditor> safe (this); const int destination = activeTab == 0 ? 0 : 3;
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, destination] (const juce::FileChooser& dialog) { if (safe && dialog.getResult().existsAsFile()) { safe->owner.loadAndAnalyse (dialog.getResult()); safe->changeTab (destination); } });
}

void AvrxGrooveEditor::chooseSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Load WAV for " + juce::String (GrooveGenerator::names[static_cast<size_t> (selectedPad)]), juce::File(), "*.wav");
    juce::Component::SafePointer<AvrxGrooveEditor> safe (this); const int drum = selectedPad, slot = layer.getSelectedId() - 1;
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, drum, slot] (const juce::FileChooser& dialog) { if (safe && dialog.getResult().existsAsFile()) safe->owner.loadSample (drum, dialog.getResult(), slot); });
}

bool AvrxGrooveEditor::isInterestedInFileDrag (const juce::StringArray& files)
{ return files.size() == 1; }
void AvrxGrooveEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    if (! isInterestedInFileDrag (files)) return;
    if(false && juce::File (files[0]).hasFileExtension ("nam")) { if (! owner.guitar.isRecording()) { owner.guitar.loadCapture (juce::File (files[0])); changeTab (1); } return; }
    if (juce::File (files[0]).hasFileExtension ("mid;midi")) { groovePanel.loadFile (juce::File (files[0])); changeTab (7); return; }
    if (activeTab == 7 && juce::File (files[0]).isDirectory()) { groovePanel.loadFolder (juce::File (files[0])); return; }
    if (activeTab == 4) { multitrackPanel.importFile (juce::File (files[0]), y - multitrackPanel.getY()); return; }
    if (activeTab == 5)
    {
        for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
            if (pads[static_cast<size_t> (drum)]->getBounds().contains (x, y))
            { selectPad (drum); if (! owner.isLoadingSample()) owner.loadSample (drum, juce::File (files[0]), layer.getSelectedId() - 1); return; }
        if (! owner.isLoadingSample()) owner.loadSample (selectedPad, juce::File (files[0]), layer.getSelectedId() - 1);
        return;
    }
    if (! owner.isAnalysing()) { const int destination = activeTab == 0 ? 0 : 3; owner.loadAndAnalyse (juce::File (files[0])); changeTab (destination); }
}

void MidiDragButton::paint (juce::Graphics& g)
{
    g.setColour (isEnabled() ? juce::Colour (0xff83c9bb) : juce::Colours::grey);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8);
    g.setColour (juce::Colour (0xff0b0c0e)); g.setFont (juce::FontOptions (15).withStyle ("Bold"));
    g.drawText ("DRAG MIDI TO DAW", getLocalBounds(), juce::Justification::centred);
}
void MidiDragButton::mouseDrag (const juce::MouseEvent& event)
{
    if (started || ! isEnabled() || event.getDistanceFromDragStart() < 5) return;
    started = true;
    try
    {
        const auto file = owner.createMidiDragFile();
        if (onStatus) onStatus (file.getFullPathName());
        if (! juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this))
            if (onStatus) onStatus ("Drag could not start. MIDI file: " + file.getFullPathName());
    }
    catch (const std::exception& error) { if (onStatus) onStatus (error.what()); }
}

void AvrxGrooveEditor::syncSequenceControls()
{
    const auto kit = owner.getKit();
    bars.setSelectedId (kit->bars, juce::dontSendNotification); division.setSelectedId (kit->division, juce::dontSendNotification);
    const auto meter = juce::String (kit->numerator) + "/" + juce::String (kit->denominator);
    int found = 0;
    for (int n = 0; n < signature.getNumItems(); ++n) if (signature.getItemText (n) == meter) found = signature.getItemId (n);
    if (found == 0) { found = signature.getNumItems() + 1; signature.addItem (meter, found); }
    signature.setSelectedId (found, juce::dontSendNotification);
}
void AvrxGrooveEditor::changeSequence()
{
    const auto meter = signature.getText();
    const int numerator = meter.upToFirstOccurrenceOf ("/", false, false).getIntValue();
    const int denominator = meter.fromFirstOccurrenceOf ("/", false, false).getIntValue();
    if (! owner.setSequence (bars.getSelectedId(), numerator, denominator, division.getSelectedId())) syncSequenceControls();
    resized(); repaint();
}
void AvrxGrooveEditor::changeTab (int tab)
{
    if(tab==1||tab==2||tab==4)tab=0;
    activeTab = tab;
    if (tab == 0) syncSequenceControls();
    const std::array<juce::TextButton*, 8> navigation { &drumsTab, &guitarTab, &settingsTab, &audioTab, &multitrackTab, &samplesTab, &mixerTab, &groovesTab };
    for (size_t n = 0; n < navigation.size(); ++n) navigation[n]->getProperties().set ("active", tab == static_cast<int> (n));
    for (auto* control : drumControls) control->setVisible (tab == 0);
    mapping.setVisible(false);editMapping.setVisible(tab==0);if(midiMappingPanel)midiMappingPanel->setVisible(false);
    for (auto* control : sampleControls) control->setVisible (tab == 5);
    samplesTab.setColour (juce::TextButton::buttonColourId, tab == 5 ? juce::Colour (0xff343b3b) : juce::Colour (0xff1d2024));
    groovePanel.setVisible (tab == 7);
    mixerPanel.setVisible (tab == 6);
    multitrackPanel.setVisible (tab == 4);
    multitrackTab.setColour (juce::TextButton::buttonColourId, tab == 4 ? juce::Colour (0xff343b3b) : juce::Colour (0xff1d2024));
    audioPanel.setVisible (tab == 3);
    audioTab.setColour (juce::TextButton::buttonColourId, tab == 3 ? juce::Colour (0xff343b3b) : juce::Colour (0xff1d2024));
    guitarPanel.setVisible (tab == 1); settingsPanel.setVisible (tab == 2);
    drumsTab.setColour (juce::TextButton::buttonColourId, tab == 0 ? juce::Colour (0xff343b3b) : juce::Colour (0xff1d2024));
    guitarTab.setColour (juce::TextButton::buttonColourId, tab == 1 ? juce::Colour (0xff343b3b) : juce::Colour (0xff1d2024));
    settingsTab.setColour (juce::TextButton::buttonColourId, tab == 2 ? juce::Colour (0xff343b3b) : juce::Colour (0xff1d2024));
    guitarTab.setVisible(false);settingsTab.setVisible(false);multitrackTab.setVisible(false);
    repaint();
}


void AvrxGrooveEditor::chooseRecordingFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Set the folder for all future recordings", owner.recordingLocation.get());
    juce::Component::SafePointer<AvrxGrooveEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe] (const juce::FileChooser& dialog) { if (safe && dialog.getResult().isDirectory()) safe->owner.setRecordingFolder (dialog.getResult()); });
}
bool AvrxGrooveEditor::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if(owner.wrapperType==juce::AudioProcessor::wrapperType_VST3)return false;
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown()) return false;
    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr) return false;
    if (key.getKeyCode() == juce::KeyPress::numberPad3)
    {
        if (false && ! recordHeld)
        {
            recordHeld = true;
            if (owner.guitar.isRecording()) return true;
            if (activeTab == 1) guitarPanel.startTake();
            else { multitrackPanel.startTake(); changeTab (4); }
        }
        return true;
    }
    if (key.getKeyCode() != juce::KeyPress::spaceKey) return false;
    if (! spaceHeld) { owner.toggleTransport (activeTab); spaceHeld = true; }
    return true;
}
bool AvrxGrooveEditor::keyStateChanged (bool, juce::Component*)
{
    if (! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey)) spaceHeld = false;
    if (! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::numberPad3)) recordHeld = false;
    return false;
}

void AvrxGrooveEditor::choosePreset (bool saving)
{
    chooser = std::make_unique<juce::FileChooser> (saving ? "Save Tracksmith Groove preset" : "Load Tracksmith Groove preset", juce::File(), "*.avrxpreset");
    const auto chooserFlags = juce::FileBrowserComponent::canSelectFiles | (saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode);
    chooser->launchAsync (chooserFlags, [safe = juce::Component::SafePointer<AvrxGrooveEditor> (this), saving] (const juce::FileChooser& dialog)
    {
        if (! safe || dialog.getResult() == juce::File()) return;
        if (saving) safe->owner.savePreset (dialog.getResult().withFileExtension ("avrxpreset"));
        else if (safe->owner.loadPreset (dialog.getResult())) { safe->selectPad (safe->selectedPad); safe->trackVolume.setValue (safe->owner.getTrackLevel(), juce::dontSendNotification); safe->syncSequenceControls(); }
        safe->timerCallback();
    });
}

void AvrxGrooveEditor::refreshPacks()
{
    packFiles.clear(); packSelector.clear(); packSelector.addItem ("Built-in drums", 1);
    if (owner.packFolder.isDirectory())
        for (const auto& entry : juce::RangedDirectoryIterator (owner.packFolder, true, "*.avrxpack", juce::File::findFiles))
        {
            auto xml = juce::XmlDocument::parse (entry.getFile());
            if (xml && xml->hasTagName ("AVRX_PACK")) { packFiles.push_back (entry.getFile()); packSelector.addItem (xml->getStringAttribute ("name", entry.getFile().getFileNameWithoutExtension()), static_cast<int> (packFiles.size()) + 1); }
        }
}
}
