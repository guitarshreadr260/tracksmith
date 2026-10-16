#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cmath>
namespace TracksmithGrooveFull {

enum class RigType { empty, gate, drive, capture, tone, delay, plate, output, scream808, shimmer, bloomDelay };
inline bool rigIsEffect (RigType t) { return t == RigType::delay || t == RigType::plate || t == RigType::scream808 || t == RigType::shimmer || t == RigType::bloomDelay; }
inline std::array<float, 6> rigDefaults (RigType t)
{
    if (t == RigType::plate || t == RigType::shimmer) return { 2, .5f, .2f, 0, .5f, 0 };
    if (t == RigType::scream808) return { .4f, .5f, 1, 0, 0, 0 };
    if (t == RigType::bloomDelay) return { 450, .55f, .35f, 0, 600, .7f };
    return { 320, .35f, .25f, 0, 0, 0 };
}
inline const char* rigName (RigType t)
{
    return std::array<const char*, 11> { "Empty", "Gate", "Drive", "NAM Capture", "Tone EQ", "Digital Delay", "Plate Reverb", "Output", "808 Overdrive", "Shimmer Reverb", "Bloom Ambient Delay" }[static_cast<size_t> (t)];
}
inline const char* rigDescription (RigType t)
{
    switch (t)
    {
        case RigType::gate: return "Suppresses noise below the threshold. Place before the capture for cleaner input.";
        case RigType::drive: return "Adjusts the level feeding the next block. Before NAM, more level drives the capture harder.";
        case RigType::capture: return "Plays your .nam model. The bundled Mesa full-rig capture already contains amp and cabinet.";
        case RigType::tone: return "Three-band tone shaping: bass shelf, mid bell and treble shelf.";
        case RigType::delay: return "Clean digital echoes. Time sets spacing; feedback sets repeats; mix blends dry and echoes.";
        case RigType::plate: return "Stock plate-style diffusion and damped reverb tail. Decay sets length; damping softens the tail.";
        case RigType::output: return "Final rig gain. Move it before effects to change how hard you feed their input.";
        case RigType::scream808: return "808-inspired stock overdrive: bass cut, mid focus, soft clipping and tone. Make-up sets output level.";
        case RigType::shimmer: return "Diffused reverb with an octave-up tail. Shimmer blends the pitched ambience; make-up balances output.";
        case RigType::bloomDelay: return "Stock ambient delay: diffused echoes swell after your attack. Bloom-inspired texture, not hardware modelling.";
        default: return "Add a block here. The two rows form one serial chain, running left to right on each row.";
    }
}
struct RigBlock
{
    int id = 0;
    RigType type = RigType::empty;
    bool enabled = true;
    unsigned revision = 1;
    std::array<float, 6> params { 320, .35f, .25f, 0, 0, 0 };
};
struct RigLayout
{
    static constexpr int slots = 16;
    std::array<RigBlock, slots> blocks;
    RigLayout()
    {
        const std::array<RigType, 8> types { RigType::gate, RigType::drive, RigType::capture, RigType::tone, RigType::delay, RigType::plate, RigType::empty, RigType::output };
        for (int i = 0; i < slots; ++i) { auto& b = blocks[static_cast<size_t> (i)]; b.id = i; b.type = i < 8 ? types[static_cast<size_t> (i)] : RigType::empty; if (rigIsEffect (b.type)) b.enabled = false; b.params = rigDefaults (b.type); }
    }
    bool valid() const
    {
        std::array<bool, slots> ids {}; std::array<int, 11> types {};
        for (const auto& b : blocks)
        {
            const int type = static_cast<int> (b.type);
            if (b.id < 0 || b.id >= slots || ids[static_cast<size_t> (b.id)] || type < 0 || type > 10) return false;
            ids[static_cast<size_t> (b.id)] = true;
            if (b.type != RigType::empty && ! rigIsEffect (b.type) && ++types[static_cast<size_t> (type)] > 1) return false;
            for (auto x : b.params) if (! std::isfinite (x)) return false;
            if (b.params[3] < -24 || b.params[3] > 24) return false;
            if ((b.type == RigType::delay || b.type == RigType::bloomDelay) && (b.params[0] < 10 || b.params[0] > 2000 || b.params[1] < 0 || b.params[1] > .85f || b.params[2] < 0 || b.params[2] > 1)) return false;
            if ((b.type == RigType::plate || b.type == RigType::shimmer) && (b.params[0] < .2f || b.params[0] > 8 || b.params[1] < 0 || b.params[1] > 1 || b.params[2] < 0 || b.params[2] > 1)) return false;
            if (b.type == RigType::shimmer && (b.params[4] < 0 || b.params[4] > 1)) return false;
            if (b.type == RigType::bloomDelay && (b.params[4] < 10 || b.params[4] > 1500 || b.params[5] < 0 || b.params[5] > 1)) return false;
            if (b.type == RigType::scream808) for (int p = 0; p < 3; ++p) if (b.params[static_cast<size_t> (p)] < 0 || b.params[static_cast<size_t> (p)] > 1) return false;
        }
        return true;
    }
};
// Generation tags reset delay memory in O(1), without allocating or clearing large buffers in the callback.
struct RigRing
{
    std::vector<float> data;
    std::vector<unsigned> tags;
    unsigned generation = 1;
    int position = 0;
    void prepare (int length) { data.assign (static_cast<size_t> (length), 0); tags.assign (data.size(), 0); generation = 1; position = 0; }
    void reset() { ++generation; position = 0; }
    float read (int offset) const { const auto i = static_cast<size_t> ((position + static_cast<int> (data.size()) - offset) % static_cast<int> (data.size())); return tags[i] == generation ? data[i] : 0; }
    float fractional (float offset) const { const int whole = static_cast<int> (offset); const float f = offset - whole; return read (whole) * (1 - f) + read (whole + 1) * f; }
    void push (float x) { const auto i = static_cast<size_t> (position); data[i] = x; tags[i] = generation; position = (position + 1) % static_cast<int> (data.size()); }
};
// Two overlapping read heads at twice normal speed produce an octave-up tail.
// Hann windows hide the read-head reset; no allocations occur while processing.
class RigOctave
{
public:
    void prepare (double sr) { window = static_cast<float> (sr * .05); ring.prepare (static_cast<int> (window) + 32); reset(); }
    void reset() { ring.reset(); phase = 0; }
    float process (float x)
    {
        ring.push (x);
        const float other = phase < .5f ? phase + .5f : phase - .5f;
        const float weight = .5f - .5f * std::cos (juce::MathConstants<float>::twoPi * phase);
        const float y = ring.fractional (8 + window * (1 - phase)) * weight + ring.fractional (8 + window * (1 - other)) * (1 - weight);
        phase += 1 / window; if (phase >= 1) phase -= 1;
        return y;
    }
private:
    RigRing ring; float window = 2400, phase = 0;
};
class RigEffect
{
public:
    void prepare (double sr)
    {
        rate = sr; delay.prepare (static_cast<int> (sr * 2) + 4); octave.prepare (sr);
        gainSmoothing = static_cast<float> (1 - std::exp (-1 / (rate * .005)));
        driveHP = static_cast<float> (1 - std::exp (-juce::MathConstants<double>::twoPi * 180 / (rate * 4)));
        for (size_t n = 0; n < tank.size(); ++n) { lengths[n] = static_cast<int> (sr * std::array<double, 4> { .0297, .0371, .0411, .0437 }[n]); tank[n].prepare (lengths[n] + 1); }
        for (size_t n = 0; n < diffuser.size(); ++n) { diffusionLengths[n] = static_cast<int> (sr * std::array<double, 2> { .0047, .0031 }[n]); diffuser[n].prepare (diffusionLengths[n] + 1); }
        reset();
    }
    void reset() { delay.reset(); octave.reset(); for (auto& r : tank) r.reset(); for (auto& r : diffuser) r.reset(); damping.fill (0); time = 0; cachedDecay = 0; cachedTone = cachedSwell = -1; gain = targetGain = 1; gainDb = 0; low = driveTone = previousInput = bloomEnvelope = echoDamping = 0; }
    float makeup (float x, const RigBlock& b)
    {
        if (gainDb != b.params[3]) { gainDb = b.params[3]; targetGain = juce::Decibels::decibelsToGain (gainDb); }
        gain += gainSmoothing * (targetGain - gain);
        return x * gain;
    }
    float process (float input, const RigBlock& b)
    {
        if (b.type == RigType::scream808)
        {
            // Stock approximation: mid-focused bass cut and four interpolated
            // soft-clipping stages per sample, followed by a tone low-pass.
            if (cachedTone != b.params[1]) { cachedTone = b.params[1]; driveLP = static_cast<float> (1 - std::exp (-juce::MathConstants<double>::twoPi * (900 + cachedTone * 6500) / (rate * 4))); }
            const float drive = 1 + b.params[0] * 35;
            float sum = 0;
            for (int n = 1; n <= 4; ++n) { const float v = previousInput + (input - previousInput) * (n * .25f); low += driveHP * (v - low); const float clipped = std::tanh ((v - low) * drive); driveTone += driveLP * (clipped - driveTone); sum += driveTone; }
            previousInput = input;
            return input * (1 - b.params[2]) + sum * .25f * b.params[2];
        }
        if (b.type == RigType::delay || b.type == RigType::bloomDelay)
        {
            const float target = b.params[0] * static_cast<float> (rate) * .001f;
            if (time == 0) time = target;
            time += .001f * (target - time);
            float echo = delay.fractional (time);
            delay.push (input + echo * b.params[1]);
            if (b.type == RigType::bloomDelay)
            {
                if (cachedSwell != b.params[4]) { cachedSwell = b.params[4]; bloomRelease = static_cast<float> (std::exp (-1 / (rate * cachedSwell * .001))); }
                bloomEnvelope = juce::jmax (std::abs (input), bloomEnvelope * bloomRelease);
                echoDamping += .2f * (echo - echoDamping);
                const float cloud = reverbSample (echoDamping, 3.5f, .55f);
                echo = (echoDamping * (1 - b.params[5]) + cloud * b.params[5]) / (1 + bloomEnvelope * 20);
            }
            return input * (1 - b.params[2]) + echo * b.params[2];
        }
        float reverb = reverbSample (input, b.params[0], b.params[1]);
        if (b.type == RigType::shimmer) reverb = reverb * (1 - .5f * b.params[4]) + octave.process (reverb) * b.params[4];
        return input * (1 - b.params[2]) + reverb * b.params[2];
    }
private:
    float reverbSample (float input, float decay, float damp)
    {
        if (cachedDecay != decay) { cachedDecay = decay; for (size_t n = 0; n < tank.size(); ++n) decayGain[n] = std::pow (.001f, static_cast<float> (lengths[n] / rate) / cachedDecay); }
        float diffused = input;
        for (size_t n = 0; n < diffuser.size(); ++n) { const float past = diffuser[n].read (diffusionLengths[n]); const float next = diffused + past * .65f; diffuser[n].push (next); diffused = past - next * .65f; }
        std::array<float, 4> d;
        for (size_t n = 0; n < d.size(); ++n) { const float raw = tank[n].read (lengths[n]); damping[n] += (1 - .95f * damp) * (raw - damping[n]); d[n] = damping[n]; }
        const std::array<float, 4> feedback { (d[0]+d[1]+d[2]+d[3])*.5f, (d[0]-d[1]+d[2]-d[3])*.5f, (d[0]+d[1]-d[2]-d[3])*.5f, (d[0]-d[1]-d[2]+d[3])*.5f };
        for (size_t n = 0; n < tank.size(); ++n) tank[n].push (diffused * .25f + feedback[n] * decayGain[n]);
        return (d[0] + d[1] - d[2] + d[3]) * .5f;
    }
private:
    double rate = 48000;
    RigRing delay;
    RigOctave octave;
    std::array<RigRing, 4> tank;
    std::array<RigRing, 2> diffuser;
    std::array<int, 4> lengths {};
    std::array<int, 2> diffusionLengths {};
    std::array<float, 4> damping {};
    std::array<float, 4> decayGain {};
    float time = 0, cachedDecay = 0;
    float gain = 1, gainDb = 0, targetGain = 1, low = 0, driveTone = 0, previousInput = 0, bloomEnvelope = 0, echoDamping = 0;
    float gainSmoothing = .004f, driveHP = .005f, driveLP = .1f, cachedTone = -1, cachedSwell = -1, bloomRelease = .999f;
};
}
