#pragma once
#include <array>
#include <algorithm>
#include <cmath>

struct InputRouting
{
    static constexpr int maximumInputs = 32;
    static constexpr int maximumOutputs = 16;
    template<class Enabled> static bool available(int first, int channels, int count, Enabled enabled) noexcept
    {
        if (first < 0 || channels < 1 || channels > 2 || first + channels > std::min(count, maximumInputs)) return false;
        for (int ch = first; ch < first + channels; ++ch) if (!enabled(ch)) return false;
        return true;
    }
    // Old sessions used compact enabled-channel indices. Resolve once at load;
    // never substitute another physical input if a legacy path is unavailable.
    template<class Enabled> static int legacyPhysical(int index, int count, Enabled enabled) noexcept
    {
        if (index < 0) return -1;
        for (int ch = 0; ch < std::min(count, maximumInputs); ++ch)
            if (enabled(ch) && index-- == 0) return ch;
        return -1;
    }
    static std::array<const float*, 2> select(const float* const* inputs, int count, int first, int channels) noexcept
    {
        std::array<const float*, 2> result {};
        if (!inputs || first < 0 || channels < 1 || channels > 2) return result;
        // All-or-nothing stereo: a missing side must not become a different input.
        if (!available(first, channels, count, [&](int ch) { return inputs[ch] != nullptr; })) return result;
        for (int ch = 0; ch < channels; ++ch) result[static_cast<size_t>(ch)] = inputs[first + ch];
        return result;
    }
    static float sample(const float* input, int frame) noexcept
    { return input && std::isfinite(input[frame]) ? std::clamp(input[frame], -1.0f, 1.0f) : 0.0f; }
};
