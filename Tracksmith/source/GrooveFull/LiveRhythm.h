#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
namespace TracksmithGrooveFull {

class LiveRhythm
{
public:
    std::atomic<bool> enabled { false };
    std::atomic<float> sensitivity { 1.0f };
    std::atomic<int> hits { 0 }, bars { 0 };
    void prepare (double rate) { hop = juce::jmax (1, static_cast<int> (rate * 0.01)); frame = cooldown = 0; sum = previous = slow = 0; wasEnabled = false; }
    void process (float dry, int cell, bool newBar)
    {
        if (! enabled.load()) { wasEnabled = false; return; }
        if (! wasEnabled)
        {
            for (auto& v : accents) v.store (0); hits.store (0); bars.store (0);
            frame = cooldown = 0; sum = previous = slow = 0; wasEnabled = true;
        }
        if (newBar) bars.fetch_add (1);
        sum += dry * dry;
        if (++frame < hop) return;
        const float rms = static_cast<float> (std::sqrt (sum / hop)); sum = 0; frame = 0;
        if (cooldown > 0) --cooldown;
        const float rise = rms - previous;
        const float gate = juce::jmax (0.002f, slow * 0.35f) / sensitivity.load();
        if (cooldown == 0 && rms > 0.008f / sensitivity.load() && rise > gate && rms > previous * 1.25f)
        {
            if (cell >= 0 && cell < 1024)
            {
                auto& accent = accents[static_cast<size_t> (cell)];
                const float strength = juce::jlimit (0.3f, 1.0f, rms * 4);
                float old = accent.load();
                while (old < strength && ! accent.compare_exchange_weak (old, strength)) {}
                hits.fetch_add (1); cooldown = 8;
            }
        }
        previous = rms; slow += 0.05f * (rms - slow);
    }
    std::array<float, 1024> consume()
    {
        std::array<float, 1024> result {};
        for (size_t n = 0; n < result.size(); ++n) result[n] = accents[n].exchange (0);
        return result;
    }
private:
    std::array<std::atomic<float>, 1024> accents {};
    bool wasEnabled = false;
    int hop = 480, frame = 0, cooldown = 0;
    double sum = 0;
    float previous = 0, slow = 0;
};

class ClickTrack
{
public:
    void reset() { remaining = 0; beat = 0; envelope = phase = 0; }
    float render (bool enabled, double rate, double tempo, int numerator, int denominator, float level)
    {
        if (! enabled) { active = false; envelope = 0; return 0; }
        if (! active) { reset(); active = true; }
        if (remaining <= 0)
        {
            frequency = beat == 0 ? 1800.0 : 1100.0; envelope = 1; phase = 0;
            remaining += rate * 60 / tempo * 4 / denominator; beat = (beat + 1) % numerator;
        }
        --remaining;
        const float out = static_cast<float> (std::sin (phase) * envelope) * level;
        phase += juce::MathConstants<double>::twoPi * frequency / rate;
        envelope *= std::exp (-1 / (rate * 0.012)); return out;
    }
private:
    bool active = false;
    int beat = 0;
    double remaining = 0, envelope = 0, phase = 0, frequency = 1100;
};
}
