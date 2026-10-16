#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
namespace TracksmithGrooveFull {

struct WaveformData
{
    juce::String name;
    double seconds = 0;
    std::vector<std::array<float, 2>> peaks;
    static WaveformData make (const juce::AudioBuffer<float>& audio, double rate, juce::String name)
    {
        WaveformData result; result.name = name; result.seconds = audio.getNumSamples() / rate;
        const int bins = juce::jmin (2400, audio.getNumSamples());
        result.peaks.resize (static_cast<size_t> (bins));
        for (int b = 0; b < bins; ++b)
        {
            float lo = 0, hi = 0;
            const int begin = static_cast<int> (static_cast<juce::int64> (b) * audio.getNumSamples() / bins);
            const int end = static_cast<int> (static_cast<juce::int64> (b + 1) * audio.getNumSamples() / bins);
            for (int c = 0; c < audio.getNumChannels(); ++c)
                for (int n = begin; n < end; ++n) { const auto v = audio.getSample (c, n); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
            result.peaks[static_cast<size_t> (b)] = { lo, hi };
        }
        return result;
    }
};

// Audio-thread producer, UI reader. A published bin covers 10 ms; no audio allocation.
class WaveHistory
{
public:
    static constexpr int capacity = 6000;
    WaveHistory() { reset (48000); }
    void reset (double rate) { hop = juce::jmax (1, static_cast<int> (std::round (rate / 100))); samples = 0; lo = hi = 0; published.store (0); }
    void push (float v)
    {
        lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
        if (++samples == hop)
        {
            const auto n = published.load (std::memory_order_relaxed);
            lows[static_cast<size_t> (n % capacity)].store (lo, std::memory_order_relaxed);
            highs[static_cast<size_t> (n % capacity)].store (hi, std::memory_order_relaxed);
            published.store (n + 1, std::memory_order_release); samples = 0; lo = hi = 0;
        }
    }
    WaveformData snapshot (bool scrolling) const
    {
        WaveformData result;
        const auto end = published.load (std::memory_order_acquire);
        const auto begin = scrolling ? juce::jmax (0, end - 800) : juce::jmax (0, end - capacity);
        result.seconds = (end - begin) / 100.0;
        for (int n = begin; n < end; ++n) result.peaks.push_back ({ lows[static_cast<size_t> (n % capacity)].load(), highs[static_cast<size_t> (n % capacity)].load() });
        return result;
    }
private:
    std::array<std::atomic<float>, capacity> lows {}, highs {};
    std::atomic<int> published { 0 };
    int hop = 480, samples = 0;
    float lo = 0, hi = 0;
};

inline void drawWaveform (juce::Graphics& g, juce::Rectangle<int> area, const WaveformData& data,
                          double span, double cursor, juce::Colour colour, const std::vector<double>& markers = {})
{
    g.setColour (juce::Colour (0xff101214)); g.fillRoundedRectangle (area.toFloat(), 8);
    const auto plot = area.reduced (12, 20); const float middle = static_cast<float> (plot.getCentreY());
    g.setColour (juce::Colour (0xff35383e)); g.drawHorizontalLine (plot.getCentreY(), static_cast<float> (plot.getX()), static_cast<float> (plot.getRight()));
    if (data.peaks.empty()) { g.setColour (juce::Colours::lightgrey); g.drawText ("Audio waveform appears here", plot, juce::Justification::centred); return; }
    span = juce::jmax (0.01, span);
    g.setColour (colour);
    const int width = juce::jmax (1, static_cast<int> (plot.getWidth() * data.seconds / span));
    for (int x = 0; x < juce::jmin (width, plot.getWidth()); ++x)
    {
        const size_t start = static_cast<size_t> (static_cast<double> (x) / width * data.peaks.size());
        const size_t end = juce::jmin (data.peaks.size(), juce::jmax (start + 1, static_cast<size_t> (static_cast<double> (x + 1) / width * data.peaks.size())));
        float lo = 0, hi = 0;
        for (size_t i = start; i < end; ++i) { lo = juce::jmin (lo, data.peaks[i][0]); hi = juce::jmax (hi, data.peaks[i][1]); }
        g.drawVerticalLine (plot.getX() + x, middle - hi * plot.getHeight() * 0.46f, middle - lo * plot.getHeight() * 0.46f + 1);
    }
    g.setColour (juce::Colour (0x5570a8ff));
    for (auto t : markers) if (t <= span) g.drawVerticalLine (plot.getX() + static_cast<int> (t / span * plot.getWidth()), static_cast<float> (plot.getY()), static_cast<float> (plot.getBottom()));
    g.setColour (juce::Colours::white);
    const auto x = plot.getX() + static_cast<int> (juce::jlimit (0.0, 1.0, cursor / span) * plot.getWidth());
    g.drawVerticalLine (x, static_cast<float> (plot.getY()), static_cast<float> (plot.getBottom()));
}
}
