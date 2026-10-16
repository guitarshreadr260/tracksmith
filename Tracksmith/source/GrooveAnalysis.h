#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>
#include <functional>

struct GrooveAnalysis
{
    double bpm = 110.0, firstHitSeconds = 0.0;
    float confidence = 0.0f;
    int onsetCount = 0;
    bool valid = false;
    juce::String error;
    std::vector<double> onsets;
    std::vector<float> hitStrengths;
    std::vector<double> timingOffsetsMs;
    std::array<std::array<float, 16>, 3> pattern {};

    // RMS-rise and spectral-flux analysis, not source separation. Fold detected transients into
    // one 4/4 bar, using low/high band energy to suggest kick/snare/hat roles.
    static GrooveAnalysis analyse (const juce::AudioBuffer<float>& audio, double rate, float detectionStrength = 1.0f, const std::function<bool()>& cancelled = {})
    {
        detectionStrength = juce::jlimit (0.5f, 3.0f, detectionStrength);
        GrooveAnalysis result;
        if (rate <= 0 || audio.getNumChannels() == 0 || audio.getNumSamples() / rate < 4.0)
        { result.error = "Use at least 4 seconds of audio with a steady beat."; return result; }
        const int hop = juce::jmax (1, static_cast<int> (rate * 0.01));
        const double frameSeconds = hop / rate;
        const int frames = audio.getNumSamples() / hop;
        std::vector<double> energy (static_cast<size_t> (frames)), low (energy.size()), high (energy.size());
        std::vector<double> filtered (static_cast<size_t> (audio.getNumChannels())), previous (filtered.size());
        double peak = 0.0;
        const double alpha = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * 180.0 / rate);
        for (int f = 0; f < frames; ++f)
        {
            if (cancelled && cancelled()) { result.error = "Analysis cancelled."; return result; }
            double total = 0, bass = 0, treble = 0;
            for (int n = 0; n < hop; ++n)
            {
                // Mean channel energies avoid cancellation in out-of-phase stereo.
                for (int c = 0; c < audio.getNumChannels(); ++c)
                {
                    const auto channel = static_cast<size_t> (c);
                    const double value = audio.getSample (c, f * hop + n);
                    total += value * value / audio.getNumChannels();
                    filtered[channel] += alpha * (value - filtered[channel]);
                    bass += filtered[channel] * filtered[channel] / audio.getNumChannels();
                    const double difference = value - previous[channel];
                    treble += difference * difference / audio.getNumChannels();
                    previous[channel] = value;
                }
            }
            const auto index = static_cast<size_t> (f);
            energy[index] = std::sqrt (total / hop);
            low[index] = std::sqrt (bass / hop);
            high[index] = std::sqrt (treble / hop);
            peak = juce::jmax (peak, energy[index]);
        }
        if (peak < 0.0001) { result.error = "Audio is silent or too quiet to detect a groove."; return result; }
        // Imported-file analysis only: spectral flux across the supplied low/mid/high
        // ranges supports the RMS detector. This is evidence, not source separation.
        const int fftSize = juce::jlimit (512, 8192, juce::nextPowerOfTwo (static_cast<int> (rate * 0.04)));
        int order = 0; while ((1 << order) < fftSize) ++order;
        juce::dsp::FFT fft (order);
        std::vector<float> work (static_cast<size_t> (fftSize * 2)), hann (static_cast<size_t> (fftSize));
        std::vector<std::vector<float>> previousSpectrum (static_cast<size_t> (audio.getNumChannels()), std::vector<float> (static_cast<size_t> (fftSize / 2 + 1)));
        std::vector<double> flux (energy.size()); double fluxPeak = 0;
        for (int n = 0; n < fftSize; ++n) hann[static_cast<size_t> (n)] = static_cast<float> (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * n / (fftSize - 1)));
        for (int f = 0; f < frames; ++f)
        {
            if (cancelled && cancelled()) { result.error = "Analysis cancelled."; return result; }
            std::array<double, 3> bandFlux {};
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            {
                std::fill (work.begin(), work.end(), 0.0f);
                for (int n = 0; n < fftSize; ++n)
                {
                    const int source = f * hop + hop / 2 + n - fftSize / 2;
                    if (source >= 0 && source < audio.getNumSamples()) work[static_cast<size_t> (n)] = audio.getSample (channel, source) * hann[static_cast<size_t> (n)];
                }
                fft.performFrequencyOnlyForwardTransform (work.data());
                for (int bin = 1; bin <= fftSize / 2; ++bin)
                {
                    const double hz = bin * rate / fftSize;
                    if (hz < 20 || hz > 20000) continue;
                    const int band = hz < 150 ? 0 : hz < 4000 ? 1 : 2;
                    const auto index = static_cast<size_t> (bin);
                    const float magnitude = std::log1p (1000 * work[index] / fftSize);
                    auto& prior = previousSpectrum[static_cast<size_t> (channel)][index];
                    bandFlux[static_cast<size_t> (band)] += juce::jmax (0.0f, magnitude - prior); prior = magnitude;
                }
            }
            flux[static_cast<size_t> (f)] = (bandFlux[0] + bandFlux[1] + bandFlux[2]) / audio.getNumChannels();
            fluxPeak = juce::jmax (fluxPeak, flux[static_cast<size_t> (f)]);
        }
        std::vector<double> onset (energy.size());
        double onsetPeak = 0.0;
        for (int f = 1; f < frames; ++f)
        {
            const auto i = static_cast<size_t> (f);
            onset[i] = juce::jmax (0.0, energy[i] - energy[i - 1]);
            onsetPeak = juce::jmax (onsetPeak, onset[i]);
        }
        auto tempoOnset = onset;
        const double tempoPeak = onsetPeak;
        if (fluxPeak > 0 && onsetPeak > 0)
        {
            const double rmsPeak = onsetPeak;
            for (size_t n = 0; n < onset.size(); ++n) { onset[n] += 0.25 * rmsPeak * flux[n] / fluxPeak; onsetPeak = juce::jmax (onsetPeak, onset[n]); }
        }
        if (onsetPeak < peak * 0.015)
        { result.error = "No clear transients found. Try a drum loop or rhythmic track."; return result; }
        // Suppress weak rises and compare normalized onset autocorrelation.
        for (auto& value : onset) if (value < onsetPeak * 0.08 / detectionStrength) value = 0.0;
        for (auto& value : tempoOnset) if (value < tempoPeak * 0.08 / detectionStrength) value = 0.0;
        auto correlation = [&] (double lag)
        {
            const int offset = static_cast<int> (std::round (lag));
            if (offset < 1 || offset >= frames) return 0.0;
            double dot = 0, a = 0, b = 0;
            for (int f = offset; f < frames; ++f)
            {
                const double x = tempoOnset[static_cast<size_t> (f)];
                const auto j = static_cast<size_t> (f - offset);
                const double y = juce::jmax (tempoOnset[j], juce::jmax (j > 0 ? tempoOnset[j - 1] : 0.0, j + 1 < tempoOnset.size() ? tempoOnset[j + 1] : 0.0));
                dot += x * y; a += x * x; b += y * y;
            }
            return dot / (std::sqrt (a * b) + 1.0e-12);
        };
        double best = -1.0;
        for (int bpm = 60; bpm <= 180; ++bpm)
        {
            if (cancelled && cancelled()) { result.error = "Analysis cancelled."; return result; }
            const double lag = 60.0 / bpm / frameSeconds;
            const double score = correlation (lag) * 0.75 + correlation (lag * 2.0) * 0.25;
            // Tiny central-tempo preference breaks exact half/double-time ties.
            const double ranked = score - std::abs (bpm - 120) * 0.00005;
            if (ranked > best) { best = ranked; result.bpm = bpm; result.confidence = static_cast<float> (score); }
        }
        struct Hit { int frame; double strength; };
        std::vector<Hit> hits;
        for (int f = 1; f < frames - 1; ++f)
        {
            const auto i = static_cast<size_t> (f);
            if (onset[i] < onsetPeak * 0.15 / detectionStrength || onset[i] < onset[i - 1] || onset[i] < onset[i + 1]) continue;
            if (! hits.empty() && f - hits.back().frame < 5)
            { if (onset[i] > hits.back().strength) hits.back() = { f, onset[i] }; }
            else hits.push_back ({ f, onset[i] });
        }
        // Refine centered spectral candidates against raw energy rises to avoid
        // carrying the analysis-window delay into the displayed attack timestamps.
        for (auto& hit : hits)
        {
            int refined = hit.frame; double strongest = -1;
            for (int f = juce::jmax (1, hit.frame - 3); f <= juce::jmin (frames - 1, hit.frame + 3); ++f)
            {
                const double rise = energy[static_cast<size_t> (f)] - energy[static_cast<size_t> (f - 1)];
                if (rise > strongest) { strongest = rise; refined = f; }
            }
            hit.frame = refined;
        }
        std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.frame < b.frame; });
        hits.erase (std::unique (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.frame == b.frame; }), hits.end());
        for (const auto& hit : hits) { result.onsets.push_back (hit.frame * frameSeconds); result.hitStrengths.push_back (static_cast<float> (hit.strength / onsetPeak)); }
        result.onsetCount = static_cast<int> (hits.size());
        if (hits.size() < 4 || result.confidence < 0.08f)
        { result.error = "No reliable repeating beat found. Try a longer, steady drum loop."; return result; }
        result.firstHitSeconds = hits.front().frame * frameSeconds;
        const double stepSeconds = 60.0 / result.bpm / 4.0;
        std::array<std::array<double, 16>, 3> accumulated {};
        for (const auto& hit : hits)
        {
            const double position = (hit.frame * frameSeconds - result.firstHitSeconds) / stepSeconds;
            const auto grid = std::llround (position);
            result.timingOffsetsMs.push_back ((position - grid) * stepSeconds * 1000);
            const int cell = static_cast<int> (grid) % 16;
            const auto f = static_cast<size_t> (hit.frame);
            const double strength = hit.strength / onsetPeak;
            const bool bassDominant = low[f] > energy[f] * 0.55 && high[f] < energy[f] * 0.6;
            const int instrument = bassDominant ? 0 : (strength > 0.35 ? 1 : 2);
            accumulated[static_cast<size_t> (instrument)][static_cast<size_t> (cell)] += strength;
        }
        const double bars = juce::jmax (1.0, (audio.getNumSamples() / rate - result.firstHitSeconds) / (stepSeconds * 16.0));
        for (size_t drum = 0; drum < 3; ++drum)
            for (size_t cell = 0; cell < 16; ++cell)
            {
                const double strength = accumulated[drum][cell] / bars;
                if (strength > 0.06) result.pattern[drum][cell] = static_cast<float> (juce::jlimit (0.25, 0.95, strength));
            }
        // Retain a quiet timekeeping hat on eighth notes. Other hits come from analysis.
        for (size_t cell = 0; cell < 16; cell += 2)
            result.pattern[2][cell] = juce::jmax (result.pattern[2][cell], 0.22f);
        result.valid = true;
        return result;
    }
};
