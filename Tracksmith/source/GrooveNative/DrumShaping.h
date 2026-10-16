#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
namespace TracksmithNativeGroove {

struct DrumShaping
{
    bool eqOn = false, highPassOn = false, compressorOn = false, lowBell = false, highBell = false;
    std::array<float, 4> frequency { 80, 400, 2500, 10000 }, gainDb {}, q { 0.707f, 1, 1, 0.707f };
    float highPassHz = 40, eqMix = 1, inputDb = 0, outputDb = 0, thresholdDb = -18, ratio = 4, attackMs = 0.2f, releaseMs = 100, compressorMix = 0.5f;
    void sanitise()
    {
        for (size_t n = 0; n < 4; ++n) { frequency[n] = finiteClamp (frequency[n], 20, 20000, 1000); gainDb[n] = finiteClamp (gainDb[n], -15, 15, 0); q[n] = finiteClamp (q[n], 0.3f, 4, 1); }
        highPassHz = finiteClamp (highPassHz, 20, 300, 40); eqMix = finiteClamp (eqMix, 0, 1, 1);
        inputDb = finiteClamp (inputDb, -12, 24, 0); outputDb = finiteClamp (outputDb, -24, 24, 0);
        thresholdDb = finiteClamp (thresholdDb, -48, 0, -18); ratio = finiteClamp (ratio, 4, 20, 4);
        attackMs = finiteClamp (attackMs, 0.02f, 0.8f, 0.2f); releaseMs = finiteClamp (releaseMs, 50, 1100, 100); compressorMix = finiteClamp (compressorMix, 0, 1, 0.5f);
    }
    bool operator== (const DrumShaping& o) const
    {
        return eqOn == o.eqOn && highPassOn == o.highPassOn && compressorOn == o.compressorOn && lowBell == o.lowBell && highBell == o.highBell
            && frequency == o.frequency && gainDb == o.gainDb && q == o.q && highPassHz == o.highPassHz && eqMix == o.eqMix
            && inputDb == o.inputDb && outputDb == o.outputDb && thresholdDb == o.thresholdDb && ratio == o.ratio
            && attackMs == o.attackMs && releaseMs == o.releaseMs && compressorMix == o.compressorMix;
    }
private:
    static float finiteClamp (float v, float lo, float hi, float fallback) { return std::isfinite (v) ? juce::jlimit (lo, hi, v) : fallback; }
};

// RBJ biquads and a stereo-linked peak compressor. All state/storage is prepared;
// coefficient creation uses value arrays, never heap allocation in processBlock.
class DrumChannelStrip
{
    struct Biquad
    {
        std::array<double, 5> coefficient { 1, 0, 0, 0, 0 };
        std::array<std::array<double, 2>, 2> state {};
        void reset() { state = {}; }
        float process (float input, size_t channel)
        {
            auto& z = state[channel]; const auto& c = coefficient;
            const double out = c[0] * input + z[0]; z[0] = c[1] * input - c[3] * out + z[1]; z[1] = c[2] * input - c[4] * out;
            if (! std::isfinite (out)) { z = {}; return 0; }
            return static_cast<float> (out);
        }
        void set (double b0, double b1, double b2, double a0, double a1, double a2) { coefficient = { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 }; }
        void design (double rate, double frequency, double gain, double q, int type)
        {
            const double w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, rate * 0.45, frequency) / rate;
            const double c = std::cos (w), s = std::sin (w), A = std::pow (10.0, gain / 40.0), alpha = s / (2 * q);
            if (type == 0) set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha); // HP
            else if (type == 1) set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A); // Bell
            else
            {
                const double shelfAlpha = s / 2 * std::sqrt (2.0), beta = 2 * std::sqrt (A) * shelfAlpha;
                if (type == 2) set (A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - beta),
                                     (A + 1) + (A - 1) * c + beta, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
                else set (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
                          (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
            }
        }
    };
public:
    void prepare (double sampleRate)
    {
        rate = sampleRate; configured = false; reductionDb = 0; for (auto& f : filters) f.reset();
        eqBlend = compressorBlend = 0; drive = makeup = 1; update ({});
    }
    void update (DrumShaping next)
    {
        next.sanitise(); if (configured && next == settings) return;
        if (! configured || next.eqOn != settings.eqOn || next.highPassOn != settings.highPassOn)
            for (auto& f : filters) f.reset();
        if (! configured || next.compressorOn != settings.compressorOn) reductionDb = 0;
        settings = next; configured = true;
        filters[0].design (rate, next.highPassHz, 0, 0.707, 0);
        for (size_t n = 0; n < 4; ++n) filters[n + 1].design (rate, next.frequency[n], next.gainDb[n], next.q[n], n == 0 && ! next.lowBell ? 2 : n == 3 && ! next.highBell ? 3 : 1);
        attack = std::exp (-1 / (rate * next.attackMs * 0.001)); release = std::exp (-1 / (rate * next.releaseMs * 0.001));
        targetDrive = std::pow (10.0, next.inputDb / 20.0); targetMakeup = std::pow (10.0, next.outputDb / 20.0);
        smoothing = 1 - std::exp (-1 / (rate * 0.005));
    }
    std::array<float, 2> render (std::array<float, 2> input)
    {
        eqBlend += smoothing * ((settings.eqOn ? settings.eqMix : 0) - eqBlend);
        compressorBlend += smoothing * ((settings.compressorOn ? settings.compressorMix : 0) - compressorBlend);
        drive += smoothing * (targetDrive - drive); makeup += smoothing * (targetMakeup - makeup);
        if (! settings.eqOn && ! settings.compressorOn && eqBlend < 1.0e-8 && compressorBlend < 1.0e-8) return input;
        std::array<float, 2> shaped = input;
        for (size_t channel = 0; channel < 2; ++channel)
        {
            float wet = input[channel];
            if (settings.highPassOn) wet = filters[0].process (wet, channel);
            for (size_t n = 1; n < filters.size(); ++n) wet = filters[n].process (wet, channel);
            shaped[channel] = static_cast<float> (input[channel] + eqBlend * (wet - input[channel]));
        }
        const double peak = juce::jmax (std::abs (shaped[0]), std::abs (shaped[1])) * drive;
        const double db = 20 * std::log10 (juce::jmax (1.0e-9, peak));
        const double over = db - settings.thresholdDb, knee = 3;
        const double target = over <= -knee / 2 ? 0 : over >= knee / 2 ? (1 - 1 / settings.ratio) * over : (1 - 1 / settings.ratio) * (over + knee / 2) * (over + knee / 2) / (2 * knee);
        const double coefficient = target > reductionDb ? attack : release;
        reductionDb = coefficient * reductionDb + (1 - coefficient) * target;
        const double wetGain = drive * makeup * std::pow (10.0, -reductionDb / 20);
        for (auto& value : shaped) value = static_cast<float> (value + compressorBlend * (value * wetGain - value));
        return shaped;
    }
    float gainReduction() const { return settings.compressorOn ? static_cast<float> (reductionDb) : 0; }
private:
    std::array<Biquad, 5> filters;
    DrumShaping settings;
    bool configured = false;
    double rate = 48000, reductionDb = 0, attack = 0, release = 0, smoothing = 0.001;
    double eqBlend = 0, compressorBlend = 0, drive = 1, makeup = 1, targetDrive = 1, targetMakeup = 1;
};
}
