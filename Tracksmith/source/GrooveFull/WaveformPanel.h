#pragma once
#include "PluginProcessor.h"
namespace TracksmithGrooveFull {

class WaveformPanel final : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    WaveformPanel (AvrxGrooveProcessor& processor, bool sampleMode) : owner (processor), samples (sampleMode) { setName (samples ? "Sample waveform" : "Reference waveform"); }
    void selectLayer (int drum, int layer) { selectedDrum = drum; selectedLayer = layer; repaint(); }
    std::shared_ptr<const WaveformData> waveform() const
    {
        if (! samples) return owner.getWaveform();
        auto sound = owner.getKit()->layer (selectedDrum, selectedLayer);
        return sound ? std::shared_ptr<const WaveformData> (sound, &sound->waveform) : nullptr;
    }
    void paint (juce::Graphics& g) override
    {
        const auto data = waveform();
        const auto bounds = getLocalBounds();
        g.setColour (juce::Colour (hover ? 0xff294b54 : 0xff111315)); g.fillRoundedRectangle (bounds.toFloat(), 8);
        g.setColour (hover ? juce::Colour (0xff83c9bb) : juce::Colour (0xff34373d)); g.drawRoundedRectangle (bounds.toFloat().reduced (1), 8, hover ? 2.0f : 1.0f);
        g.setFont (14); g.setColour (juce::Colours::white);
        const bool loading = samples ? owner.isLoadingSample() : owner.isAnalysing();
        if (loading) g.drawText ("Loading audio...", 16, 8, getWidth() - 32, 24, juce::Justification::centredLeft);
        else g.drawText (data ? data->name + "  /  " + juce::String (data->seconds, 2) + " s" : (samples ? "Drop a WAV onto this layer" : "Drop a WAV or audio file here"), 16, 8, getWidth() - 32, 24, juce::Justification::centredLeft);
        const auto plot = plotBounds();
        if (data)
        {
            const auto analysis = ! samples ? owner.getAnalysis() : nullptr;
            drawWaveform (g, plot, *data, data->seconds, samples ? 0 : owner.trackPositionSeconds(), juce::Colour (samples ? 0xffd6b27b : 0xff83c9bb), analysis ? analysis->onsets : std::vector<double>());
        }
        else
        {
            g.setFont (20); g.setColour (juce::Colour (0xff83c9bb));
            g.drawText (samples ? "WAV SAMPLE" : "DRAG & DROP AUDIO", plot, juce::Justification::centred);
        }
        g.setFont (12); g.setColour (juce::Colours::lightgrey);
        g.drawText (samples ? "Selected drum / selected layer. Click the waveform to audition." : "Click to seek. Play source / together to listen.", 16, getHeight() - 28, getWidth() - 32, 22, juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! plotBounds().contains (e.getPosition())) return;
        if (samples) { owner.auditionPad (selectedDrum); return; }
        if (auto data = waveform())
        {
            const auto plot = plotBounds().reduced (12, 20);
            owner.seekTrack (juce::jlimit (0.0, 1.0, (e.x - static_cast<double> (plot.getX())) / juce::jmax (1, plot.getWidth())) * data->seconds);
        }
    }
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    { return files.size() == 1 && (! samples || juce::File (files[0]).hasFileExtension ("wav")); }
    void fileDragEnter (const juce::StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { hover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        hover = false;
        if (isInterestedInFileDrag (files))
        {
            if (samples) owner.loadSample (selectedDrum, juce::File (files[0]), selectedLayer);
            else owner.loadAndAnalyse (juce::File (files[0]));
        }
        repaint();
    }
private:
    juce::Rectangle<int> plotBounds() const { return { 8, 38, getWidth() - 16, getHeight() - 72 }; }
    AvrxGrooveProcessor& owner;
    bool samples = false, hover = false;
    int selectedDrum = 0, selectedLayer = 0;
};
}
