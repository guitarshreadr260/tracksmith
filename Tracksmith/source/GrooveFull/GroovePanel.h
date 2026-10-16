#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class GroovePanel final : public juce::Component, private juce::ListBoxModel, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit GroovePanel (AvrxGrooveProcessor& p) : owner (p)
    {
        for (auto* b : { &load, &folder, &bundled, &play, &stop }) addAndMakeVisible (*b);
        load.onClick = [this] { choose (false); }; folder.onClick = [this] { choose (true); };
        bundled.onClick = [this] { loadFolder (owner.grooveFolder); };
        song.setName ("Library songs"); addAndMakeVisible (song); song.onChange = [this] { filterSongs(); };
        sourceMap.addItem ("Auto source map", 1); sourceMap.addItem ("GGD groove format", 2); sourceMap.addItem ("General MIDI", 3); sourceMap.setSelectedId (1); addAndMakeVisible (sourceMap);
        sourceMap.addItem("Current custom kit map",4);
        sourceMap.setTooltip ("Auto reads library mapping metadata. Choose General MIDI or GGD for imported grooves, or Current custom kit map to reload your remapped MIDI exports.");
        addAndMakeVisible(autoPreview);autoPreview.setToggleState(true,juce::dontSendNotification);
        play.onClick = [this] { owner.playGroove(); }; stop.onClick = [this] { owner.stopGroovePreview(); };
        list.setModel (this); list.setRowHeight (36); list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141618)); list.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff363a3e)); addAndMakeVisible (list);
        bar.onChange = [this] { repaint(); }; addAndMakeVisible (bar); updateBars();
        tempo.setSliderStyle (juce::Slider::LinearHorizontal); tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24); addAndMakeVisible (tempo);
        tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (owner.parameters, "tempo", tempo);
        info.setFont (juce::FontOptions (12)); info.setColour (juce::Label::textColourId, juce::Colour (0xffc3c6cb)); addAndMakeVisible (info);
        info.setText ("Choose a starter groove, load a MIDI file, or browse your groove library.", juce::dontSendNotification);
        for (auto* slider : { &human, &timing }) { slider->setSliderStyle (juce::Slider::LinearHorizontal); slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 24); addAndMakeVisible (*slider); }
        human.setRange (0, 1, .01); timing.setRange (0, 25, .1); timing.setTextValueSuffix (" ms");
        human.setTooltip ("0 = your exact velocities and grid. Increase for beat accents, shared bar energy and bounded hit variation."); timing.setTooltip ("Maximum timing looseness; snare leans later. The clock and bar length stay exact.");
        human.onValueChange = [this] { owner.setHumanFeel (static_cast<float> (human.getValue()), owner.getKit()->humanTimingMs); };
        timing.onValueChange = [this] { owner.setHumanFeel (owner.getKit()->humanAmount, static_cast<float> (timing.getValue())); };
        currentName = owner.grooveTitle;
        if (owner.grooveFolder.isDirectory()) loadFolder (owner.grooveFolder);
        startTimerHz (20);
    }
    void loadFile (const juce::File& file)
    {
        if (owner.guitar.isRecording() || owner.isAnalysing()) { info.setText ("Finish recording or audio analysis before changing the groove.", juce::dontSendNotification); return; }
        auto options = MidiGroove::libraryOptions (file);
        if (sourceMap.getSelectedId() != 1) options.ggd = sourceMap.getSelectedId() == 2;
        options.kitMapping=sourceMap.getSelectedId()==4;
        const auto data = MidiGroove::read (file, *owner.getKit(), owner.parameters.getRawParameterValue ("tempo")->load(), options);
        if (! data.valid) { info.setText (data.error, juce::dontSendNotification); return; }
        if (! owner.applyGroove (data)) return;
        currentName = data.name;if(autoPreview.getToggleState())owner.playGroove();
        info.setText (data.name + " | " + juce::String (data.bpm, 0) + " BPM | " + juce::String (data.bars) + " bars | " + juce::String (data.numerator) + "/" + juce::String (data.denominator) + " | " + (options.ggd ? "GGD source map | " : "Kit / GM | ") + juce::String (data.mapped) + " hits, " + juce::String (data.ignored) + " ignored, " + juce::String (data.merged) + " merged. Timing follows the grid.", juce::dontSendNotification);
        if (std::find (files.begin(), files.end(), file) == files.end()) { files.push_back (file); list.updateContent(); }
        const int row = 5 + static_cast<int> (std::find (files.begin(), files.end(), file) - files.begin());
        selecting = true; list.selectRow (row); selecting = false; updateBars(); repaint();
    }
    void loadFolder (const juce::File& directory)
    {
        if (owner.guitar.isRecording()) { info.setText ("Stop recording before changing the groove library.", juce::dontSendNotification); return; }
        files.clear(); libraryFiles.clear(); libraryDirectory = directory;owner.grooveFolder=directory;
        for (const auto& entry : juce::RangedDirectoryIterator (directory, true, "*", juce::File::findFiles))
        {
            if (entry.getFile().hasFileExtension ("mid;midi")) libraryFiles.push_back (entry.getFile());
            if (libraryFiles.size() >= 2000) break;
        }
        std::sort (libraryFiles.begin(), libraryFiles.end(), [] (const juce::File& a, const juce::File& b) { return a.getFullPathName().compareNatural (b.getFullPathName()) < 0; });
        songDirectories.clear();
        for (const auto& file : libraryFiles) { const auto parent = file.getParentDirectory(); if (std::find (songDirectories.begin(), songDirectories.end(), parent) == songDirectories.end()) songDirectories.push_back (parent); }
        song.clear (juce::dontSendNotification); song.addItem ("All songs / " + juce::String (static_cast<int> (libraryFiles.size())) + " grooves", 1);
        for (size_t n = 0; n < songDirectories.size(); ++n) song.addItem (songDirectories[n].getFileName(), static_cast<int> (n) + 2);
        song.setSelectedId (1, juce::dontSendNotification); files = libraryFiles;
        selecting = true; list.deselectAllRows(); list.updateContent(); selecting = false;
        info.setText ("Library: " + directory.getFileName() + " | " + juce::String (static_cast<int> (files.size())) + " MIDI files" + (files.size() == 2000 ? " (first 2000)" : "") + ". Select a groove to load it; enable Auto-preview to hear selections.", juce::dontSendNotification);
    }
    void resized() override
    {
        load.setBounds (20, 96, 160, 32); folder.setBounds (190, 96, 160, 32); bundled.setBounds (360, 96, 120, 32);
        human.setBounds (535, 96, 173, 32); timing.setBounds (790, 96, 230, 32);
        song.setBounds (20, 146, 316, 26); list.setBounds (20, 178, 316, 490); sourceMap.setBounds (364, 146, 190, 26); bar.setBounds (570, 146, 180, 26);
        autoPreview.setBounds(20,710,316,34);
        play.setBounds (364, 710, 180, 34); stop.setBounds (556, 710, 140, 34); tempo.setBounds (740, 710, getWidth() - 760, 34);
        info.setBounds (20, 754, getWidth() - 40, 44);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xffece8e0)); g.setFont (juce::FontOptions (24).withStyle ("Bold")); g.drawText ("GROOVE PLAYER", 20, 12, 500, 36, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffa2a6ac)); g.setFont (12); g.drawText ("Load MIDI drum grooves, audition them through your kit, and edit them in Drum Engine.", 20, 56, getWidth() - 40, 28, juce::Justification::centredLeft);
        g.setFont (11); g.drawText ("FEEL", 493, 96, 40, 32, juce::Justification::centredLeft); g.drawText ("TIMING", 726, 96, 62, 32, juce::Justification::centredLeft);
        g.drawText ("TEMPO", 740, 683, 200, 22, juce::Justification::centredLeft);
        const auto kit = owner.getKit(); const auto selectedBar = juce::jlimit (0, kit->bars - 1, bar.getSelectedId() - 1);
        g.setColour (juce::Colour (0xff17191c)); g.fillRoundedRectangle (356, 178, getWidth() - 376.0f, 490, 6);
        g.setColour (juce::Colour (0xffd6b27b)); g.setFont (14); g.drawText (currentName, 372, 188, getWidth() - 400, 32, juce::Justification::centredLeft);
        const int steps = kit->stepsPerBar(); const float left = 486, width = getWidth() - left - 38;
        for (int drum = 0; drum < 9; ++drum)
        {
            const int y = 239 + drum * 42; g.setColour (juce::Colour (drum % 2 == 0 ? 0xff212426 : 0xff1a1d20)); g.fillRect (364, y, getWidth() - 394, 40);
            g.setColour (juce::Colour (0xffc3c6cb)); g.setFont (11); g.drawText (GrooveGenerator::names[static_cast<size_t> (drum)], 376, y, 102, 40, juce::Justification::centredLeft);
            for (int cell = 0; cell < steps; ++cell)
            {
                const float x = left + width * cell / steps;
                g.setColour (juce::Colour (cell % kit->division == 0 ? 0xff454b50 : 0xff2e3438)); g.drawVerticalLine (static_cast<int> (x), static_cast<float> (y), y + 40.0f);
                const auto velocity = kit->pattern[static_cast<size_t> (drum)][static_cast<size_t> (selectedBar * steps + cell)];
                if (velocity > 0) { g.setColour (juce::Colour (drum < 2 ? 0xffd6b27b : 0xff83c9bb).withAlpha (0.35f + velocity * .65f)); g.fillRoundedRectangle (x + 1, y + 7.0f, juce::jmax (2.0f, width / steps - 2), 26, 2); }
            }
        }
        const int playing = owner.getPlayingStep();
        if (playing >= 0 && playing / steps == selectedBar) { const int x = static_cast<int> (left + width * (playing % steps) / steps); g.setColour (juce::Colours::white); g.drawVerticalLine (x, 237, 618); }
        g.setColour (juce::Colour (0xffa2a6ac)); g.setFont (11); g.drawFittedText ("Loops your current kit. GGD imports use the groove-file map; extra articulations use the nearest AVRX pad. MIDI drag-out uses your destination map.", 364, 632, getWidth() - 404, 32, juce::Justification::centredLeft, 2);
    }
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() == 1 && (juce::File (f[0]).isDirectory() || juce::File (f[0]).hasFileExtension ("mid;midi")); }
    void filesDropped (const juce::StringArray& f, int, int) override { if (isInterestedInFileDrag (f)) { const juce::File file (f[0]); if (file.isDirectory()) loadFolder (file); else loadFile (file); } }
private:
    void filterSongs()
    {
        files.clear(); const int choice = song.getSelectedId() - 2;
        for (const auto& file : libraryFiles) if (choice < 0 || (choice < static_cast<int> (songDirectories.size()) && file.getParentDirectory() == songDirectories[static_cast<size_t> (choice)])) files.push_back (file);
        selecting = true; list.deselectAllRows(); list.updateContent(); selecting = false; repaint();
    }
    int getNumRows() override { return 5 + static_cast<int> (files.size()); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= getNumRows()) return;
        if (selected) { g.setColour (juce::Colour (0xff343c39)); g.fillRect (0, 0, width, height); }
        g.setColour (row < 5 ? juce::Colour (0xffd6b27b) : juce::Colour (0xffdce0e3)); g.setFont (12);
        const auto text = row < 5 ? "STARTER  /  " + MidiGroove::starter (row).name : files[static_cast<size_t> (row - 5)].getFileNameWithoutExtension();
        g.drawFittedText (text, 12, 0, width - 24, height, juce::Justification::centredLeft, 1);
    }
    void selectedRowsChanged (int row) override
    {
        if (selecting || row < 0 || row >= getNumRows()) return;
        if (owner.guitar.isRecording() || owner.isAnalysing()) { info.setText ("Finish recording or audio analysis before loading another groove.", juce::dontSendNotification); return; }
        if (row >= 5) { loadFile (files[static_cast<size_t> (row - 5)]); return; }
        const auto data = MidiGroove::starter (row); if (owner.applyGroove (data)) { currentName = data.name;if(autoPreview.getToggleState())owner.playGroove(); info.setText ("Loaded " + data.name + ". Press Play groove or Space to loop it.", juce::dontSendNotification); updateBars(); repaint(); }
    }
    void updateBars() { const int selected = juce::jmax (1, bar.getSelectedId()); bar.clear (juce::dontSendNotification); for (int n = 1; n <= owner.getKit()->bars; ++n) bar.addItem ("Preview bar " + juce::String (n), n); bar.setSelectedId (juce::jmin (selected, owner.getKit()->bars), juce::dontSendNotification); }
    void timerCallback() override { currentName = owner.grooveTitle; human.setValue (owner.getKit()->humanAmount, juce::dontSendNotification); timing.setValue (owner.getKit()->humanTimingMs, juce::dontSendNotification); if (bar.getNumItems() != owner.getKit()->bars) updateBars(); load.setEnabled (! owner.guitar.isRecording() && ! owner.isAnalysing()); folder.setEnabled (! owner.guitar.isRecording()); bundled.setEnabled (! owner.guitar.isRecording() && owner.grooveFolder.isDirectory()); play.setEnabled (! owner.guitar.isRecording()); repaint(); }
    void choose (bool directory)
    {
        chooser = std::make_unique<juce::FileChooser> (directory ? "Browse your MIDI groove library" : "Load a drum MIDI groove", juce::File(), directory ? "" : "*.mid;*.midi");
        juce::Component::SafePointer<GroovePanel> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | (directory ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),
            [safe, directory] (const juce::FileChooser& dialog) { if (safe && dialog.getResult().exists()) { if (directory) safe->loadFolder (dialog.getResult()); else safe->loadFile (dialog.getResult()); } });
    }
    AvrxGrooveProcessor& owner;
    juce::TextButton load { "Load MIDI groove" }, folder { "Browse groove folder" }, bundled { "GGD library" }, play { "Preview groove / Space" }, stop { "Stop groove" };
    juce::ToggleButton autoPreview{"Auto-preview selected groove"};
    juce::ListBox list;
    juce::ComboBox bar, song, sourceMap;
    juce::Slider tempo, human, timing;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;
    juce::Label info;
    std::vector<juce::File> files;
    std::vector<juce::File> libraryFiles, songDirectories;
    juce::File libraryDirectory;
    bool selecting = false;
    juce::String currentName { "Current groove" };
    std::unique_ptr<juce::FileChooser> chooser;
};
}
