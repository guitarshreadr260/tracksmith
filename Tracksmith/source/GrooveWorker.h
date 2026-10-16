#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "GrooveAnalysis.h"

// Reuses AVRX GROOVE's file analyzer. No captured audio crosses the web bridge.
class GrooveWorker final : private juce::Thread, private juce::AsyncUpdater
{
public:
    std::function<void(GrooveAnalysis, double)> onFinished;
    GrooveWorker() : Thread("AVRX GROOVE analysis") {}
    ~GrooveWorker() override { signalThreadShouldExit(); stopThread(-1); cancelPendingUpdate(); }
    bool analyse(const juce::File& file)
    {
        if (isThreadRunning()) return false;
        cancelPendingUpdate(); requested = file; return startThread();
    }
    static juce::Result exportMidi(const GrooveAnalysis& analysis, const juce::File& destination)
    {
        if (!analysis.valid) return juce::Result::fail("Analyze a drum take first.");
        juce::MidiMessageSequence notes, tempo;
        auto bpm = juce::MidiMessage::tempoMetaEvent(static_cast<int>(std::round(60000000.0 / analysis.bpm)));
        tempo.addEvent(bpm, 0); tempo.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0);
        constexpr int drumNotes[] { 36, 38, 42 };
        for (size_t drum = 0; drum < 3; ++drum) for (size_t step = 0; step < 16; ++step)
        {
            const float velocity = analysis.pattern[drum][step]; if (velocity <= 0) continue;
            notes.addEvent(juce::MidiMessage::noteOn(10, drumNotes[drum], velocity), static_cast<double>(step * 240));
            notes.addEvent(juce::MidiMessage::noteOff(10, drumNotes[drum]), static_cast<double>(step * 240 + 120));
        }
        notes.updateMatchedPairs(); notes.addEvent(juce::MidiMessage::endOfTrack(), 3840);
        juce::MidiFile midi; midi.setTicksPerQuarterNote(960); midi.addTrack(tempo); midi.addTrack(notes);
        juce::TemporaryFile temporary(destination);
        { juce::FileOutputStream stream(temporary.getFile());
          if (!stream.openedOk() || !midi.writeTo(stream)) return juce::Result::fail("Could not write MIDI.");
          stream.flush(); if (stream.getStatus().failed()) return stream.getStatus(); }
        return temporary.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Could not replace the MIDI file.");
    }
private:
    void run() override
    {
        GrooveAnalysis result; double duration = 0;
        try
        {
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(requested));
            if (!reader || reader->numChannels < 1 || reader->numChannels > 2 || reader->sampleRate < 8000 || reader->sampleRate > 192000)
                result.error = "Use a mono or stereo WAV between 8 and 192 kHz.";
            else
            {
                const int count = static_cast<int>(std::min<int64_t>(reader->lengthInSamples, static_cast<int64_t>(reader->sampleRate * 60)));
                juce::AudioBuffer<float> audio(static_cast<int>(reader->numChannels), count); duration = count / reader->sampleRate;
                if (!reader->read(&audio, 0, count, 0, true, true)) result.error = "Could not read the drum take.";
                else if (!threadShouldExit())
                {
                    for (int ch = 0; ch < audio.getNumChannels(); ++ch) for (int n = 0; n < count; ++n)
                    {
                        const float sample = audio.getSample(ch, n);
                        audio.setSample(ch, n, std::isfinite(sample) ? juce::jlimit(-1.0f, 1.0f, sample) : 0.0f);
                    }
                    result = GrooveAnalysis::analyse(audio, reader->sampleRate, 1.0f, [this] { return threadShouldExit(); });
                }
            }
        }
        catch (const std::exception&) { result.error = "Drum analysis failed; try a shorter WAV."; }
        if (threadShouldExit()) return;
        { const juce::ScopedLock lock(results); pending = std::move(result); analysedSeconds = duration; }
        triggerAsyncUpdate();
    }
    void handleAsyncUpdate() override
    {
        GrooveAnalysis result; double duration;
        { const juce::ScopedLock lock(results); result = std::move(pending); duration = analysedSeconds; }
        if (onFinished) onFinished(std::move(result), duration);
    }
    juce::File requested; juce::CriticalSection results; GrooveAnalysis pending; double analysedSeconds = 0;
};
