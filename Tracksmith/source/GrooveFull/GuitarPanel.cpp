#include "GuitarPanel.h"
#include "Tone3000Panel.h"
namespace TracksmithGrooveFull {

class NamWindow final : public juce::DocumentWindow
{
public:
    explicit NamWindow (juce::AudioPluginInstance& plugin)
        : DocumentWindow ("NAM - load your capture here", juce::Colours::black, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
#if JUCE_MAJOR_VERSION >= 9
        setContentOwned (plugin.createEditorAndMakeActive(), true);
#else
        setContentOwned (plugin.createEditorIfNeeded(), true);
#endif
        centreWithSize (getWidth(), getHeight()); setVisible (true);
    }
    void closeButtonPressed() override { setVisible (false); }
};

GuitarPanel::GuitarPanel (AvrxGrooveProcessor& processor) : owner (processor)
{
    for (auto* button : { &namControls, &namChoose, &record, &stop, &playTake }) addAndMakeVisible (*button);
    for (auto* button : { &monitor, &bypass, &backing, &follow, &metro }) addAndMakeVisible (*button);
    namControls.onClick = [this] { chooseCapture(); };
    namChoose.onClick = [this] { chooseNamPlugin(); };
    record.onClick = [this] { startTake(); }; stop.onClick = [this] { owner.stopRecording(); };
    playTake.onClick = [this] { owner.recorder.togglePreview(); };
    monitor.onClick = [this] { owner.guitar.monitor.store (monitor.getToggleState()); };
    bypass.onClick = [this] { owner.guitar.bypassNam.store (bypass.getToggleState()); };
    backing.onClick = [this] { owner.parameters.getParameter ("run")->setValueNotifyingHost (backing.getToggleState() ? 1.0f : 0.0f); };
    follow.onClick = [this] { owner.setLiveFollow (follow.getToggleState()); };
    metro.onClick = [this] { owner.metronome.store (metro.getToggleState()); };
    preRoll.addItem ("Pre-roll: Off", 1); preRoll.addItem ("Pre-roll: 1 bar", 2); preRoll.addItem ("Pre-roll: 2 bars", 3); preRoll.setSelectedId (2); addAndMakeVisible (preRoll);
    sensitivity.setRange (0.5, 3, 0.05); sensitivity.setValue (1); sensitivity.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 24);
    sensitivity.onValueChange = [this] { owner.setDetectionStrength (static_cast<float> (sensitivity.getValue())); }; addAndMakeVisible (sensitivity);
    sensitivity.setTooltip ("Higher detects softer attacks. Lower focuses on strong strums.");
    record.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffae354b));
    input.addItem ("Active input 1", 1); input.addItem ("Active input 2", 2); input.setSelectedId (1);
    input.onChange = [this] { owner.guitar.inputChannel.store (input.getSelectedId() - 1); };
    recordMode.addItem ("Dry DI recording", 1); recordMode.addItem ("NAM sound recording", 2); recordMode.setSelectedId (2);
    recordMode.onChange = [this] { owner.guitarRecordMode.store (recordMode.getSelectedId()); };
    preRoll.onChange = [this] { owner.guitarCountIn.store (preRoll.getSelectedId() - 1); };
    addAndMakeVisible (input); addAndMakeVisible (recordMode);
    for (auto* slider : { &inputGain, &outputGain })
    {
        slider->setSliderStyle (juce::Slider::LinearHorizontal); slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
        slider->setRange (0, slider == &inputGain ? 4 : 2, 0.01); addAndMakeVisible (*slider);
    }
    inputGain.setValue (1); outputGain.setValue (0.5);
    inputGain.onValueChange = [this] { owner.guitar.setManualInputGain (static_cast<float> (inputGain.getValue())); };
    outputGain.onValueChange = [this] { owner.guitar.outputGain.store (static_cast<float> (outputGain.getValue())); };
    for (auto* label : { &status, &elapsed, &followInfo }) { label->setColour (juce::Label::textColourId, juce::Colours::lightgrey); addAndMakeVisible (*label); }
    pickup.addItem ("Manual input", 1); pickup.addItem ("Single-coil: auto", 2); pickup.addItem ("Passive humbucker: auto", 3); pickup.addItem ("Active pickup: auto", 4); pickup.setSelectedId (owner.guitar.pickupConfiguration.load() + 1, juce::dontSendNotification);
    pickup.onChange = [this] { owner.guitar.setPickupConfiguration (pickup.getSelectedId() - 1); }; recalibrate.onClick = [this] { owner.guitar.calibrateInput(); }; addAndMakeVisible (pickup); addAndMakeVisible (recalibrate);
    pickup.setTooltip ("Select pickup type, then play normally for 5 seconds. Uses measured signal level; manual trim overrides it.");
    const std::array<std::atomic<float>*, 7> values { &owner.guitar.ampDriveDb, &owner.guitar.bassDb, &owner.guitar.midDb, &owner.guitar.trebleDb, &owner.guitar.ampOutputDb, &owner.guitar.gateThresholdDb, &owner.guitar.gateReleaseMs };
    for (size_t n = 0; n < ampControls.size(); ++n)
    {
        auto& knob = ampControls[n]; knob.setName (std::array<const char*, 7> { "Drive", "Bass", "Mid", "Treble", "Amp output", "Gate threshold", "Gate release" }[n]);
        knob.setSliderStyle (n < 5 ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal); knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 22);
        knob.setRange (n == 5 ? -80 : n == 6 ? 10 : n == 0 || n == 4 ? -24 : -12, n == 5 ? 0 : n == 6 ? 500 : n == 0 ? 24 : 12, .1);
        knob.setTextValueSuffix (n == 6 ? " ms" : " dB"); knob.setDoubleClickReturnValue (true, n == 5 ? -60 : n == 6 ? 100 : 0);
        knob.setValue (values[n]->load(), juce::dontSendNotification); knob.onValueChange = [this, n, values] { values[n]->store (static_cast<float> (ampControls[n].getValue())); }; addAndMakeVisible (knob);
    }
    gate.onClick = [this] { owner.guitar.gateOn.store (gate.getToggleState()); }; gate.setTooltip ("Pre-amp gate: 1 ms attack, 30 ms hold and adjustable release. Dry DI is preserved."); addAndMakeVisible (gate);
    for (int n = 0; n < RigLayout::slots; ++n) { auto& tile = blocks[static_cast<size_t> (n)]; addAndMakeVisible (tile); tile.onClick = [this, n] { selectedBlock = n; showInspector(); }; }
    for (int n = 0; n < 11; ++n) blockType.addItem (rigName (static_cast<RigType> (n)), n + 1);
    addAndMakeVisible (profiles); profiles.setTooltip ("Choose a TONE3000 capture and download it directly into your NAM block."); profiles.onClick = [this] { openTone3000(); };
    addAndMakeVisible (swapSlot); swapSlot.setTextWhenNothingSelected ("Swap with slot..."); for (int n = 0; n < RigLayout::slots; ++n) swapSlot.addItem ("Slot " + juce::String (n + 1), n + 1); swapSlot.onChange = [this] { const int target = swapSlot.getSelectedId() - 1; if (owner.guitar.moveBlock (selectedBlock, target)) selectedBlock = target; swapSlot.setSelectedId (0, juce::dontSendNotification); showInspector(); };
    addAndMakeVisible (blockType); addAndMakeVisible (blockEnabled); addAndMakeVisible (blockHelp);
    blockType.onChange = [this] { if (! owner.guitar.editBlock (selectedBlock, static_cast<RigType> (blockType.getSelectedId() - 1))) owner.guitar.reportStatus ("Only one Gate, Drive, Capture, Tone EQ and Output block is allowed. Effects can be duplicated."); showInspector(); };
    blockEnabled.onClick = [this] { owner.guitar.enableBlock (selectedBlock, blockEnabled.getToggleState()); showInspector(); };
    for (auto* button : { &previousBlock, &nextBlock, &duplicate, &remove }) addAndMakeVisible (*button);
    previousBlock.onClick = [this] { if (owner.guitar.moveBlock (selectedBlock, selectedBlock - 1)) --selectedBlock; showInspector(); };
    nextBlock.onClick = [this] { if (owner.guitar.moveBlock (selectedBlock, selectedBlock + 1)) ++selectedBlock; showInspector(); };
    duplicate.onClick = [this] { if (! owner.guitar.duplicateBlock (selectedBlock)) owner.guitar.reportStatus ("Duplicate an effect into an empty slot. Remove another block to free a slot."); showInspector(); };
    remove.onClick = [this] { owner.guitar.editBlock (selectedBlock, RigType::empty); showInspector(); };
    for (size_t n = 0; n < effectControls.size(); ++n) { auto& knob = effectControls[n]; knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 22); addAndMakeVisible (knob); knob.onValueChange = [this, n] { owner.guitar.setBlockParameter (selectedBlock, static_cast<int> (n), static_cast<float> (effectControls[n].getValue())); }; }
    bypass.setTooltip ("Bypasses the entire rig; Input Trim and Monitor level remain active.");
    gate.setTooltip ("Gate controls operate at the Gate block's position. Dry DI remains unchanged.");
    showInspector();
    timerCallback(); startTimerHz (10);
}
GuitarPanel::~GuitarPanel() { stopTimer(); owner.tone3000.cancel(); toneWindow.reset(); namWindow.reset(); }
void GuitarPanel::openTone3000()
{
    if (! toneWindow) toneWindow = std::make_unique<Tone3000Window> (owner); toneWindow->setVisible (true); toneWindow->toFront (true);
}

void GuitarPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff11161b));
    g.setColour (juce::Colours::white); g.setFont (24);
    g.drawText ("GUITAR STUDIO", 20, 9, 350, 30, juce::Justification::centredLeft);
    g.setFont (12); g.setColour (juce::Colour (0xff7d929e)); g.drawText ("MODULAR RIG / ONE-MINUTE TAKES", 400, 12, getWidth() - 420, 25, juce::Justification::centredRight);
    g.setColour (juce::Colour (0xff1b232a)); g.fillRoundedRectangle (14, 43, getWidth() - 28.0f, 80, 8);
    g.setColour (juce::Colours::lightgrey); g.setFont (11);
    g.drawText ("INPUT TRIM", 20, 170, 90, 25, juce::Justification::centredLeft); g.drawText ("MONITOR", 540, 170, 85, 25, juce::Justification::centredLeft);
    g.setFont (12); g.drawText (owner.guitar.calibrating.load() ? "PLAY NORMALLY: " + juce::String (owner.guitar.calibrationProgress.load() * 100, 0) + "%" : "Manual trim overrides pickup calibration", 20, 237, 380, 24, juce::Justification::centredLeft);
    for (int n = 0; n < 2; ++n) { const float x = n == 0 ? 360.0f : 790.0f; const float peak = n == 0 ? owner.guitar.inputPeak.load() : owner.guitar.outputPeak.load(); g.setColour (juce::Colour (0xff29343d)); g.fillRect (x, 242.0f, 180.0f, 9.0f); g.setColour (n == 0 ? juce::Colour (0xff83c9bb) : juce::Colour (0xffd6b27b)); g.fillRect (x, 242.0f, 180.0f * juce::jmin (1.0f, peak), 9.0f); }
    g.setColour (juce::Colour (0xff7d929e)); g.drawText ("INPUT", 310, 235, 50, 23, juce::Justification::centredLeft); g.drawText ("RIG OUT", 730, 235, 60, 23, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff1b2229)); g.fillRoundedRectangle (14, 297, getWidth() - 28.0f, 275, 14);
    g.setColour (juce::Colour (0xff66818d)); g.setFont (11); g.drawText ("INPUT  >  ROW 1  >  ROW 2  >  OUTPUT     /     SELECT A BLOCK TO EDIT", 28, 305, getWidth() - 56, 21, juce::Justification::centredLeft);
    for (int row = 0; row < 2; ++row) { const float y = 383.0f + row * 116.0f; g.setColour (juce::Colour (0xff586b77)); g.drawLine (24, y, getWidth() - 24.0f, y, 2); }
    g.setColour (juce::Colour (0xff586b77)); g.drawLine (getWidth() - 23.0f, 383, getWidth() - 23.0f, 440, 1); g.drawLine (getWidth() - 23.0f, 440, 23, 440, 1); g.drawLine (23, 440, 23, 499, 1);
    g.setColour (juce::Colour (0xff1b2229)); g.fillRoundedRectangle (14, 582, getWidth() - 28.0f, 176, 10);
    const auto b = owner.guitar.getRig()->blocks[static_cast<size_t> (selectedBlock)];
    g.setColour (juce::Colours::lightgrey); g.setFont (12);
    if (b.type == RigType::capture) g.drawFittedText (owner.guitar.captureFile().existsAsFile() ? owner.guitar.captureFile().getFileName() : "Load a .nam capture using the top toolbar. Capture availability is shown in status below.", 180, 681, getWidth() - 212, 45, juce::Justification::centredLeft, 2);
    int x = 28;
    for (const auto& knob : ampControls) if (knob.isVisible()) { g.drawText (knob.getName(), x, 663, 125, 18, juce::Justification::centred); x += 138; }
    for (const auto& knob : effectControls) if (knob.isVisible()) { g.drawText (knob.getName(), x, 663, 125, 18, juce::Justification::centred); x += 138; }
}

void GuitarPanel::resized()
{
    namControls.setBounds (24, 51, 170, 28); namChoose.setBounds (202, 51, 170, 28); recordMode.setBounds (382, 51, 210, 28); preRoll.setBounds (602, 51, 185, 28); playTake.setBounds (797, 51, 220, 28);
    record.setBounds (24, 87, 170, 28); stop.setBounds (202, 87, 170, 28); elapsed.setBounds (382, 87, 350, 28); profiles.setBounds (742, 87, 275, 28);
    input.setBounds (20, 132, 200, 28); monitor.setBounds (240, 132, 170, 28); bypass.setBounds (440, 132, 140, 28); backing.setBounds (610, 132, 160, 28);
    inputGain.setBounds (110, 170, 390, 26); outputGain.setBounds (625, 170, 390, 26);
    pickup.setBounds (20, 204, 235, 27); recalibrate.setBounds (265, 204, 130, 27); follow.setBounds (420, 204, 130, 27); metro.setBounds (570, 204, 130, 27); sensitivity.setBounds (745, 204, 270, 27); sensitivity.setTooltip ("Live follow detection strength: higher detects softer attacks."); followInfo.setBounds (20, 265, getWidth() - 40, 25);
    const int cell = (getWidth() - 84) / 8;
    for (int n = 0; n < RigLayout::slots; ++n) blocks[static_cast<size_t> (n)].setBounds (28 + (n % 8) * (cell + 4), 338 + (n / 8) * 116, cell, 92);
    blockType.setBounds (28, 592, 200, 28); blockEnabled.setBounds (235, 592, 120, 28); previousBlock.setBounds (360, 592, 85, 28); nextBlock.setBounds (450, 592, 85, 28); swapSlot.setBounds (545, 592, 145, 28); duplicate.setBounds (700, 592, 135, 28); remove.setBounds (845, 592, 135, 28);
    blockHelp.setBounds (28, 624, getWidth() - 56, 34);
    showInspector();
    status.setBounds (20, 765, getWidth() - 40, 30);
}

void GuitarPanel::showInspector()
{
    const auto layout = owner.guitar.getRig(); const auto& b = layout->blocks[static_cast<size_t> (selectedBlock)];
    blockType.setSelectedId (static_cast<int> (b.type) + 1, juce::dontSendNotification); blockEnabled.setToggleState (b.enabled, juce::dontSendNotification);
    blockHelp.setText (rigDescription (b.type), juce::dontSendNotification); blockHelp.setColour (juce::Label::textColourId, juce::Colour (0xff9cb0bb));
    const bool editing = ! owner.guitar.isRecording();
    blockType.setEnabled (editing); blockEnabled.setEnabled (editing && b.type != RigType::empty); previousBlock.setEnabled (editing && selectedBlock > 0); nextBlock.setEnabled (editing && selectedBlock < RigLayout::slots - 1); remove.setEnabled (editing && b.type != RigType::empty); duplicate.setEnabled (editing && rigIsEffect (b.type));
    for (auto& knob : ampControls) knob.setVisible (false); for (auto& knob : effectControls) knob.setVisible (false); gate.setVisible (b.type == RigType::gate);
    int x = 28;
    for (int n = 0; n < 7; ++n)
    {
        const bool show = (b.type == RigType::drive && n == 0) || (b.type == RigType::tone && n >= 1 && n <= 3) || (b.type == RigType::output && n == 4) || (b.type == RigType::gate && n >= 5);
        if (show) { auto& knob = ampControls[static_cast<size_t> (n)]; knob.setVisible (true); knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 22); knob.setBounds (x, 681, 125, 73); x += 138; }
    }
    gate.setBounds (340, 685, 155, 28);
    swapSlot.setEnabled (editing);
    for (size_t n = 0; n < effectControls.size(); ++n)
    {
        const bool fx = rigIsEffect (b.type), echo = b.type == RigType::delay || b.type == RigType::bloomDelay, od = b.type == RigType::scream808;
        const bool show = n == 3 ? b.type != RigType::empty : fx && (n < 3 || (n == 4 && (b.type == RigType::shimmer || b.type == RigType::bloomDelay)) || (n == 5 && b.type == RigType::bloomDelay));
        if (! show) continue;
        auto& knob = effectControls[n]; knob.setVisible (true); knob.setBounds (x, 681, 125, 73); x += 138;
        knob.setName (n == 3 ? "Make-up Gain" : n == 4 ? b.type == RigType::shimmer ? "Octave Shimmer" : "Swell" : n == 5 ? "Diffusion" : n == 2 ? "Dry / Wet Mix" : n == 1 ? od ? "Tone" : echo ? "Feedback" : "Damping" : od ? "Overdrive" : echo ? "Time" : "Decay");
        knob.setRange (n == 3 ? -24 : n == 4 && b.type == RigType::bloomDelay ? 10 : n == 0 && ! od ? echo ? 10 : .2 : 0, n == 3 ? 24 : n == 4 && b.type == RigType::bloomDelay ? 1500 : n == 0 && ! od ? echo ? 2000 : 8 : n == 1 && echo ? .85 : 1, n == 3 ? .1 : (n == 0 && echo) || (n == 4 && b.type == RigType::bloomDelay) ? 1 : .01);
        knob.setTextValueSuffix (n == 3 ? " dB" : n == 4 && b.type == RigType::bloomDelay ? " ms" : n == 0 && ! od ? echo ? " ms" : " s" : ""); if (! knob.isMouseButtonDown()) knob.setValue (b.params[n], juce::dontSendNotification);
    }
    for (size_t n = 0; n < blocks.size(); ++n) { blocks[n].type = layout->blocks[n].type; blocks[n].active = layout->blocks[n].enabled && (layout->blocks[n].type != RigType::gate || owner.guitar.gateOn.load()); blocks[n].selected = static_cast<int> (n) == selectedBlock; blocks[n].setTooltip (rigDescription (blocks[n].type)); blocks[n].repaint(); }
    repaint();
}

void GuitarPanel::timerCallback()
{
    owner.serviceTone3000();
    profiles.setEnabled (! owner.guitar.isRecording());
    owner.guitar.serviceInputCalibration();
    gate.setButtonText (owner.guitar.gateOn.load() ? owner.guitar.gateGainDisplay.load() < .05f ? "Gate closed" : "Gate open" : "Gate");
    pickup.setSelectedId (owner.guitar.pickupConfiguration.load() + 1, juce::dontSendNotification); pickup.setEnabled (! owner.guitar.isRecording()); recalibrate.setEnabled (! owner.guitar.isRecording() && ! owner.guitar.calibrating.load() && owner.guitar.pickupConfiguration.load() != 0);
    input.setSelectedId (owner.guitar.inputChannel.load() + 1, juce::dontSendNotification);
    inputGain.setValue (owner.guitar.inputGain.load(), juce::dontSendNotification); outputGain.setValue (owner.guitar.outputGain.load(), juce::dontSendNotification);
    monitor.setToggleState (owner.guitar.monitor.load(), juce::dontSendNotification);
    if (lastPresetRevision != owner.presetRevision) { lastPresetRevision = owner.presetRevision; recordMode.setSelectedId (owner.guitarRecordMode.load(), juce::dontSendNotification); preRoll.setSelectedId (owner.guitarCountIn.load() + 1, juce::dontSendNotification); }
    const std::array<std::atomic<float>*, 7> ampValues { &owner.guitar.ampDriveDb, &owner.guitar.bassDb, &owner.guitar.midDb, &owner.guitar.trebleDb, &owner.guitar.ampOutputDb, &owner.guitar.gateThresholdDb, &owner.guitar.gateReleaseMs };
    for (size_t n = 0; n < ampControls.size(); ++n) ampControls[n].setValue (ampValues[n]->load(), juce::dontSendNotification);
    gate.setToggleState (owner.guitar.gateOn.load(), juce::dontSendNotification);
    owner.serviceRecording();
    playTake.setEnabled (owner.recorder.getPreview() != nullptr && ! owner.guitar.isRecording());
    playTake.setButtonText (owner.recorder.previewPlaying.load() ? "Stop take playback" : "Play take / Space"); sensitivity.setValue (owner.detectionStrength.load(), juce::dontSendNotification);
    metro.setToggleState (owner.metronome.load(), juce::dontSendNotification); follow.setToggleState (owner.liveRhythm.enabled.load(), juce::dontSendNotification);
    followInfo.setText (owner.liveStatus(), juce::dontSendNotification);
    backing.setToggleState (owner.parameters.getRawParameterValue ("run")->load() > 0.5f, juce::dontSendNotification);
    elapsed.setText (juce::String (owner.guitar.isRecording() ? "Recording " : "Take length ") + juce::String (owner.guitar.recordedSeconds(), 1) + " / 60.0 seconds", juce::dontSendNotification);
    if (owner.guitar.preRollSeconds() > 0) elapsed.setText ("COUNT-IN  " + juce::String (owner.guitar.preRollSeconds(), 1) + " seconds remaining - recording starts after pre-roll", juce::dontSendNotification);
    status.setText (owner.guitar.status(), juce::dontSendNotification); status.setTooltip (owner.guitar.status());
    record.setEnabled (! owner.guitar.isRecording() && ! owner.recorder.loading.load()); recordMode.setEnabled (! owner.guitar.isRecording());
    preRoll.setEnabled (! owner.guitar.isRecording()); stop.setEnabled (owner.guitar.isRecording());
    bypass.setEnabled (! owner.guitar.isRecording());
    namControls.setEnabled (! owner.guitar.isRecording());
    bypass.setToggleState (owner.guitar.bypassNam.load(), juce::dontSendNotification);
    namChoose.setEnabled (! owner.guitar.isRecording()); input.setEnabled (! owner.guitar.isRecording());
    showInspector();
}

void GuitarPanel::openNamControls()
{
    if (! owner.guitar.getNam())
        if (! owner.guitar.loadNamPlugin (juce::File ("C:/Program Files/Common Files/VST3/NeuralAmpModeler.vst3"))) return;
    owner.guitarRecordMode.store (2); recordMode.setSelectedId (2, juce::dontSendNotification);
    if (! namWindow) namWindow = std::make_unique<NamWindow> (*owner.guitar.getNam());
    namWindow->setVisible (true); namWindow->toFront (true);
}
void GuitarPanel::chooseNamPlugin()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose installed NAM VST3", juce::File ("C:/Program Files/Common Files/VST3"), "*.vst3");
    juce::Component::SafePointer<GuitarPanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
        [safe] (const juce::FileChooser& dialog)
        {
            if (safe && dialog.getResult().exists()) { safe->namWindow.reset(); if (safe->owner.guitar.loadNamPlugin (dialog.getResult())) safe->openNamControls(); }
        });
}
void GuitarPanel::startTake()
{
    if (owner.guitar.isRecording()) return;
    const auto file = owner.recordingLocation.nextTake ("Guitar");
    if (file == juce::File()) { owner.guitar.reportStatus ("Recording folder could not be created. Choose another folder in Settings."); return; }
    if (! owner.startGuitarTake (file, recordMode.getSelectedId() == 2, preRoll.getSelectedId() - 1))
        owner.recorder.setStatus (owner.guitar.status());
}

void GuitarPanel::chooseCapture()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a Neural Amp Modeler capture", owner.guitar.captureFile(), "*.nam");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<GuitarPanel> (this)] (const juce::FileChooser& dialog)
        { if (safe && dialog.getResult().existsAsFile()) { if (safe->owner.guitar.loadCapture (dialog.getResult())) { safe->owner.guitarRecordMode.store (2); safe->recordMode.setSelectedId (2, juce::dontSendNotification); } safe->refresh(); } });
}
}
