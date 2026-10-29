#pragma once
#include <atomic>
#include <cmath>

// Accumulate callback peaks until the UI consumes them. No locks or allocation
// are used on the audio thread, and peaks in short callbacks are not lost.
inline void accumulateMeterPeak(std::atomic<float>& target, float value) noexcept {
    if (!std::isfinite(value) || value <= 0.0f) return;
    float current = target.load(std::memory_order_relaxed);
    while (current < value && !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}
