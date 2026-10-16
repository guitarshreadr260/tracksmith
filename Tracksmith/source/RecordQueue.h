#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// A single audio producer and single disk consumer. Storage is allocated before
// recording; producer/consumer positions publish samples with acquire/release.
class RecordQueue
{
public:
    static constexpr uint64_t capacity = 65536;
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    RecordQueue() { for (auto& channel : samples) channel.resize(capacity); }
    void reset() noexcept { written = 0; consumed = 0; }
    bool push(const float* const* input, int channels, int frames) noexcept
    {
        if (input == nullptr || channels < 1 || channels > 2) return false;
        for (int ch = 0; ch < channels; ++ch) if (input[ch] == nullptr) return false;
        const auto write = written.load(std::memory_order_relaxed);
        const auto read = consumed.load(std::memory_order_acquire);
        if (frames < 0 || static_cast<uint64_t>(frames) > capacity - (write - read)) return false;
        for (int ch = 0; ch < channels; ++ch)
            for (int frame = 0; frame < frames; ++frame)
            {
                const float sample = input[ch][frame];
                samples[static_cast<size_t>(ch)][(write + static_cast<uint64_t>(frame)) & (capacity - 1)] = std::isfinite(sample) ? std::clamp(sample, -1.0f, 1.0f) : 0.0f;
            }
        written.store(write + static_cast<uint64_t>(frames), std::memory_order_release); return true;
    }
    int readable(std::array<const float*, 2>& pointers, int maximum = 4096) const noexcept
    {
        if (maximum <= 0) return 0;
        const auto read = consumed.load(std::memory_order_relaxed);
        const auto write = written.load(std::memory_order_acquire);
        const auto offset = read & (capacity - 1);
        const auto count = std::min({write - read, capacity - offset, static_cast<uint64_t>(maximum)});
        for (size_t ch = 0; ch < 2; ++ch) pointers[ch] = samples[ch].data() + offset;
        return static_cast<int>(count);
    }
    void finishRead(int frames) noexcept { consumed.fetch_add(static_cast<uint64_t>(frames), std::memory_order_release); }
    uint64_t available() const noexcept { return written.load(std::memory_order_acquire) - consumed.load(std::memory_order_acquire); }
private:
    std::array<std::vector<float>, 2> samples;
    std::atomic<uint64_t> written { 0 }, consumed { 0 };
};
