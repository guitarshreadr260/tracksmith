#pragma once
#include "InputRouting.h"
#include <atomic>

class InputMonitor
{
public:
    std::atomic<bool> enabled { false };
    std::atomic<float> gain { 0.25f };
    std::atomic<int> track { 0 };
    void reset() noexcept { smoothGain = 0.0f; smoothLeft = smoothRight = 0.0f; }
    void mix(const std::array<const float*, 2>& inputs, int inputChannels, float* const* outputs,
             int outputChannels, int frames, double rate, float master, float& peak, bool& clipped,
             float trackLeft = 1.0f, float trackRight = 1.0f, bool processed = false, bool deferCeiling = false) noexcept
    {
        const float target = enabled.load(std::memory_order_relaxed) && inputs[0] && (inputChannels == 1 || inputs[1])
            ? gain.load(std::memory_order_relaxed) * master : 0.0f;
        const float smoothing = rate > 0 ? static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.005))) : 1.0f;
        for (int frame = 0; frame < frames; ++frame)
        {
            smoothGain += smoothing * (target - smoothGain);
            smoothLeft += smoothing * (trackLeft - smoothLeft); smoothRight += smoothing * (trackRight - smoothRight);
            const auto sample = [processed, frame](const float* data)
            { return processed ? (data && std::isfinite(data[frame]) ? data[frame] : 0.0f) : InputRouting::sample(data, frame); };
            const float left = sample(inputs[0]) * smoothGain * smoothLeft;
            const float right = sample(inputChannels == 2 ? inputs[1] : inputs[0]) * smoothGain * smoothRight;
            for (int ch = 0; ch < std::min(2, outputChannels); ++ch)
                if (outputs[ch])
                {
                    const float mixed = outputs[ch][frame] + (outputChannels == 1 ? (left + right) * 0.5f : (ch == 0 ? left : right));
                    clipped = clipped || std::abs(mixed) > 1.0f;
                    outputs[ch][frame] = deferCeiling ? mixed : std::clamp(mixed, -1.0f, 1.0f); peak = std::max(peak, std::abs(outputs[ch][frame]));
                }
        }
    }
private:
    float smoothGain = 0.0f; // Audio-callback owned.
    float smoothLeft = 0.0f, smoothRight = 0.0f;
};
