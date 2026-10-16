#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "GrooveGenerator.h"
#include "Waveform.h"
#include "DrumShaping.h"
#include <array>
#include <cmath>

struct DrumSample
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100;
    juce::String name;
    juce::File sourceFile;
    WaveformData waveform;
};

struct VelocityGroup
{
    float upper = 1;
    std::vector<std::shared_ptr<const DrumSample>> takes;
};

struct DrumKit
{
    static constexpr int maxSteps = 1024, maxLayers = 4;
    std::array<std::array<float, maxSteps>, GrooveGenerator::drumCount> pattern {};
    std::array<std::shared_ptr<const DrumSample>, GrooveGenerator::drumCount> samples {};
    std::array<std::array<std::shared_ptr<const DrumSample>, 3>, GrooveGenerator::drumCount> extraSamples {};
    std::array<std::array<float, maxLayers>, GrooveGenerator::drumCount> layerGains {};
    std::array<float, GrooveGenerator::drumCount> tuning {}, fineTuning {};
    std::array<int, GrooveGenerator::drumCount> rootNotes { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<float, GrooveGenerator::drumCount> gains { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    std::array<bool, GrooveGenerator::drumCount> muted {}, solo {};
    std::array<float, GrooveGenerator::drumCount> pan {};
    bool anySolo() const { return std::any_of (solo.begin(), solo.end(), [] (bool value) { return value; }); }
    bool audible (size_t drum) const { return ! muted[drum] && (! anySolo() || solo[drum]); }
    std::array<int, GrooveGenerator::drumCount> notes = GrooveGenerator::notes;
    int midiChannel = 1; // Logic's GM drum instruments; choose 10 for external GM modules.
    std::array<DrumShaping, GrooveGenerator::drumCount> shaping {};
    std::array<std::array<juce::File, maxLayers>, GrooveGenerator::drumCount> layerFiles {};
    std::array<std::vector<VelocityGroup>, GrooveGenerator::drumCount> velocityGroups;
    juce::String packName;
    float humanAmount = 0, humanTimingMs = 12;
    int humanSeed = 137;
    float roomLevel = 0.0f, overheadLevel = 0.0f;
    int bars = 4, numerator = 4, denominator = 4, division = 4;
    int stepsPerBar() const { return numerator * 4 * division / denominator; }
    int stepCount() const { return bars * stepsPerBar(); }
    std::shared_ptr<const DrumSample> layer (int drum, int index) const
    { return index == 0 ? samples[static_cast<size_t> (drum)] : extraSamples[static_cast<size_t> (drum)][static_cast<size_t> (index - 1)]; }
    void setLayer (int drum, int index, std::shared_ptr<const DrumSample> sound)
    { layerFiles[static_cast<size_t> (drum)][static_cast<size_t> (index)] = sound ? sound->sourceFile : juce::File(); if (index == 0) samples[static_cast<size_t> (drum)] = std::move (sound); else extraSamples[static_cast<size_t> (drum)][static_cast<size_t> (index - 1)] = std::move (sound); }
    DrumKit() { for (auto& row : layerGains) row.fill (1); resetPattern(); }
    void resetPattern()
    {
        for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
        {
            pattern[static_cast<size_t> (drum)].fill (0);
            for (int cell = 0; cell < stepCount(); ++cell)
            {
                const int original = static_cast<int> (std::round ((cell % stepsPerBar()) * 4.0 / division));
                const bool onGrid = std::abs ((cell % stepsPerBar()) * 4.0 / division - original) < 0.001;
                if (onGrid && original < 16 && GrooveGenerator::plays (drum, original))
                    pattern[static_cast<size_t> (drum)][static_cast<size_t> (cell)] = GrooveGenerator::velocity (drum, original);
            }
        }
    }
};

// Shared by the live engine and offline stem renderer so WAV samples and
// synthesis have the same timing, velocity response and channel behavior.
struct DrumVoice
{
    std::shared_ptr<const DrumSample> sample;
    double cursor = 0, phase = 0;
    float envelope = 0, velocity = 0, previousNoise = 0;
    juce::Random noise { 12345 };
    void start (std::shared_ptr<const DrumSample> sound, float strength)
    { sample = std::move (sound); cursor = phase = 0; envelope = velocity = strength; previousNoise = 0; }
    void stop() { envelope = 0; velocity = 0; sample.reset(); }
    std::array<float, 2> render (int drum, double outputRate, double pitchRatio = 1.0)
    {
        std::array<float, 2> result {};
        if (sample)
        {
            if (cursor >= sample->audio.getNumSamples()) { velocity = 0; return result; }
            const int position = static_cast<int> (cursor);
            const int next = juce::jmin (position + 1, sample->audio.getNumSamples() - 1);
            const float fraction = static_cast<float> (cursor - position);
            for (int channel = 0; channel < 2; ++channel)
            {
                const int source = juce::jmin (channel, sample->audio.getNumChannels() - 1);
                result[static_cast<size_t> (channel)] = velocity * (sample->audio.getSample (source, position) * (1 - fraction)
                    + sample->audio.getSample (source, next) * fraction);
            }
            cursor += sample->sampleRate / outputRate * pitchRatio;
            return result;
        }
        if (envelope == 0) return result;
        const float white = noise.nextFloat() * 2 - 1;
        const float high = (white - previousNoise) * 0.5f;
        previousNoise = white;
        const double frequency = drum >= 4 ? (drum == 4 ? 180.0 : drum == 5 ? 135.0 : 85.0) + 35.0 * envelope : 48.0 + 110.0 * envelope;
        phase += juce::MathConstants<double>::twoPi * frequency * pitchRatio / outputRate;
        if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        float sound = 0;
        if (drum == 0) sound = 0.55f * envelope * static_cast<float> (std::sin (phase));
        else if (drum == 1) sound = 0.28f * envelope * white;
        else if (drum == 2 || drum == 3) sound = 0.18f * envelope * high;
        else if (drum == 7) sound = 0.24f * envelope * (high + 0.3f * white);
        else if (drum == 8) sound = 0.20f * envelope * (high * 0.65f + static_cast<float> (std::sin (phase * 17.0) * std::sin (phase * 23.7)) * 0.35f);
        else sound = 0.38f * envelope * static_cast<float> (std::sin (phase));
        const std::array<double, 9> times { 0.11, 0.075, 0.018, 0.16, 0.12, 0.15, 0.2, 0.95, 0.55 };
        envelope *= static_cast<float> (std::exp (-1.0 / (outputRate * times[static_cast<size_t> (drum)])));
        if (envelope < 0.00001f) envelope = 0;
        result.fill (sound);
        return result;
    }
};

struct LayeredDrumVoice
{
    std::array<DrumVoice, DrumKit::maxLayers> layers;
    std::array<float, DrumKit::maxLayers> gains { 1, 1, 1, 1 };
    std::array<bool, DrumKit::maxLayers> enabled {};
    unsigned roundRobin = 0;
    void start (int drum, float velocity, const DrumKit& kit)
    {
        bool hasSample = false;
        for (int layer = 0; layer < DrumKit::maxLayers; ++layer) if (kit.layer (drum, layer)) hasSample = true;
        for (int layer = 0; layer < DrumKit::maxLayers; ++layer)
        {
            auto sound = kit.layer (drum, layer);
            if (layer == 0 && ! kit.velocityGroups[static_cast<size_t> (drum)].empty())
            {
                const auto& groups = kit.velocityGroups[static_cast<size_t> (drum)];
                const VelocityGroup* chosen = &groups.back();
                for (const auto& group : groups) if (velocity <= group.upper) { chosen = &group; break; }
                if (! chosen->takes.empty()) sound = chosen->takes[roundRobin++ % chosen->takes.size()];
            }
            enabled[static_cast<size_t> (layer)] = sound != nullptr || (! hasSample && layer == 0);
            gains[static_cast<size_t> (layer)] = kit.layerGains[static_cast<size_t> (drum)][static_cast<size_t> (layer)];
            if (enabled[static_cast<size_t> (layer)]) layers[static_cast<size_t> (layer)].start (std::move (sound), velocity);
            else layers[static_cast<size_t> (layer)].stop();
        }
    }
    void stop() { for (auto& layer : layers) layer.stop(); }
    std::array<float, 2> render (int drum, double rate, double ratio)
    {
        std::array<float, 2> result {};
        for (size_t index = 0; index < layers.size(); ++index)
            if (enabled[index])
            {
                const auto sound = layers[index].render (drum, rate, ratio);
                for (size_t channel = 0; channel < 2; ++channel) result[channel] += sound[channel] * gains[index];
            }
        return result;
    }
};

struct DrumAmbience
{
    juce::Reverb room;
    std::array<float, 2> low {};
    float lowAlpha = 0;
    void prepare (double rate)
    {
        room.setSampleRate (rate);
        juce::Reverb::Parameters settings;
        settings.roomSize = 0.65f; settings.damping = 0.4f;
        settings.wetLevel = 1; settings.dryLevel = 0; settings.width = 1;
        room.setParameters (settings);
        room.reset(); low.fill (0);
        lowAlpha = static_cast<float> (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 150.0 / rate));
    }
    // First pair is a wet room effect, second pair is a cymbal-focused high-pass bus.
    std::array<float, 4> render (std::array<float, 2> dry, std::array<float, 2> cymbalSend,
                               float roomLevel, float overheadLevel)
    {
        room.processStereo (&dry[0], &dry[1], 1);
        std::array<float, 4> result { dry[0] * roomLevel, dry[1] * roomLevel, 0, 0 };
        for (size_t channel = 0; channel < 2; ++channel)
        {
            low[channel] += lowAlpha * (cymbalSend[channel] - low[channel]);
            result[channel + 2] = (cymbalSend[channel] - low[channel]) * overheadLevel * 0.6f;
        }
        return result;
    }
};
