#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
// Audio-thread clock; configured only while the device callback is detached.
struct CountIn
{
    int64_t total = 0, position = 0;
    double rate = 48000, tempo = 120;
    void start(int bars, double sampleRate, double bpm)
    { rate = sampleRate; tempo = bpm; position = 0; total = static_cast<int64_t>(std::llround(bars * 4.0 * 60.0 * rate / tempo)); }
    int remaining(int block) const { return static_cast<int>(std::min<int64_t>(block, total - position)); }
    int beatsLeft() const { return static_cast<int>(std::ceil((total-position) * tempo / (60.0 * rate))); }
    float next()
    {
        const double beatFrames = 60.0 * rate / tempo;
        const auto beat = static_cast<int64_t>(std::floor(position / beatFrames));
        const auto start = static_cast<int64_t>(std::ceil(beat * beatFrames));
        const double elapsed = position++ - start, length = rate * .012;
        if (elapsed >= length) return 0;
        return static_cast<float>((beat % 4 == 0 ? .18 : .12) * (1-elapsed/length)
            * std::sin(6.283185307179586 * (beat % 4 == 0 ? 1800 : 1200) * elapsed / rate));
    }
};
