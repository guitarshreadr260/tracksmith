#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>
namespace TracksmithGrooveFull {

// A repeatable musical heuristic, not a fitted model of a particular drummer.
// Shared bar energy correlates hits; approximately normal per-hit deviations
// avoid uniform randomness. Positive timing offsets keep the transport exact.
struct HumanFeel
{
    struct Hit { float velocity; double delaySeconds; };
    static float noise (uint32_t seed)
    {
        float total = 0;
        for (int n = 0; n < 12; ++n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; total += static_cast<float> (seed & 0xffffffu) / 16777216.0f; }
        return juce::jlimit (-2.5f, 2.5f, total - 6.0f);
    }
    static Hit hit (float velocity, int drum, int cell, int stepsPerBar, int division, float amount, float timingMs, uint32_t seed, double bpm, double swing)
    {
        if (amount <= 0) return { velocity, 0 };
        const auto bar = static_cast<uint32_t> (cell / stepsPerBar);
        const auto key = seed ^ (static_cast<uint32_t> (cell + 1) * 0x9e3779b9u) ^ (static_cast<uint32_t> (drum + 1) * 0x85ebca6bu);
        const float barEnergy = noise (seed ^ ((bar + 1) * 0xc2b2ae35u));
        const bool onBeat = (cell % stepsPerBar) % division == 0;
        const float accent = drum == 2 || drum == 3 || drum == 8 ? (onBeat ? .025f : -.04f) : 0;
        const float strength = juce::jlimit (.02f, 1.0f, velocity + amount * (accent + .025f * barEnergy + .045f * noise (key)));
        const float role = drum == 1 ? .55f : drum == 2 || drum == 3 ? .25f : .35f;
        const double delay = amount * timingMs * .001 * juce::jlimit (0.0f, 1.0f, role + .1f * barEnergy + .15f * noise (key ^ 0x27d4eb2du));
        const double shortestStep = 60.0 / bpm / division * (1 - swing);
        return { strength, juce::jmin (delay, shortestStep * .3) };
    }
};
}
