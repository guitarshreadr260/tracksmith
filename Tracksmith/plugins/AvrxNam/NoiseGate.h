#pragma once
#include <algorithm>
#include <cmath>
// Raw input detector controls a post-model gate, suppressing model idle output too.
struct NoiseGate
{
    float envelope = 0, gain = 1;
    int hold = 0;
    void reset() { envelope = 0; gain = 1; hold = 0; }
    float next(float input, bool enabled, float thresholdDb, float releaseMs, double rate)
    {
        const float magnitude = std::abs(input);
        envelope = std::max(magnitude, envelope * static_cast<float>(std::exp(-1 / (rate * .01))));
        const float threshold = std::pow(10.0f, thresholdDb / 20);
        if (envelope >= threshold) hold = static_cast<int>(rate * .02);
        else if (hold > 0) --hold;
        const bool open = !enabled || hold > 0 || envelope >= threshold;
        const float coefficient = static_cast<float>(1-std::exp(-1 / (rate * (open ? .001 : releaseMs*.001))));
        gain += coefficient * ((open ? 1.0f : 0.0f)-gain);
        if (gain < 1.e-7f) gain = 0;
        return gain;
    }
};
