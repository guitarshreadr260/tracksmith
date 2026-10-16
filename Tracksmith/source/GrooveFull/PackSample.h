#pragma once
#include "DrumKit.h"
#include <stdexcept>
namespace TracksmithGrooveFull {

inline std::shared_ptr<const DrumSample> readPackSample (const juce::File& file)
{
    juce::WavAudioFormat wav; auto stream = file.createInputStream();
    std::unique_ptr<juce::AudioFormatReader> reader (stream ? wav.createReaderFor (stream.release(), true) : nullptr);
    if (! reader || reader->numChannels < 1 || reader->numChannels > 2 || reader->sampleRate <= 0 || reader->sampleRate > 192000 || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * 10)
        throw std::runtime_error (("Invalid or missing WAV: " + file.getFileName()).toStdString());
    auto sample = std::make_shared<DrumSample>(); sample->sourceFile = file; sample->name = file.getFileName(); sample->sampleRate = reader->sampleRate;
    sample->audio.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    if (! reader->read (&sample->audio, 0, sample->audio.getNumSamples(), 0, true, true)) throw std::runtime_error ("WAV decode failed");
    for (int c = 0; c < sample->audio.getNumChannels(); ++c) for (int n = 0; n < sample->audio.getNumSamples(); ++n) if (! std::isfinite (sample->audio.getSample (c, n))) sample->audio.setSample (c, n, 0);
    sample->waveform = WaveformData::make (sample->audio, sample->sampleRate, sample->name);
    return sample;
}
}
