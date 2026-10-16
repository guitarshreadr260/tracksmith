#pragma once
#include "Waveform.h"
#include <thread>
namespace TracksmithGrooveFull {

struct TrackAudio
{
    juce::AudioBuffer<float> audio;
    double rate = 48000;
    WaveformData waveform;
    juce::File file;
    bool namPrinted = false;
};

class MultitrackEngine
{
public:
    static constexpr int tracks = 4;
    struct Region { std::shared_ptr<const TrackAudio> clip; double offset = 0; };
    using Regions = std::vector<Region>;
    struct Strip
    {
        std::shared_ptr<const TrackAudio> clip;
        std::shared_ptr<const Regions> extra;
        juce::File sourceFile;
        std::atomic<float> gain { 0.7f }, pan { 0 };
        std::atomic<bool> mute { false }, solo { false };
        std::atomic<double> start { 0 };
    };
    std::array<Strip, tracks> strips;
    std::atomic<bool> playing { false }, previewPlaying { false }, loading { false };
    std::atomic<int> recordingLane { -1 };
    std::atomic<double> recordingStart { 0 };
    std::atomic<double> position { 0 }, previewPosition { 0 };
    ~MultitrackEngine() { if (loader.joinable()) loader.join(); }
    void prepare (double rate) { deviceRate = rate; cursor = 0; previewCursor = 0; playing.store (false); previewPlaying.store (false); position.store (0); }
    std::shared_ptr<const TrackAudio> getClip (int track) const { return std::atomic_load (&strips[static_cast<size_t> (track)].clip); }
    std::shared_ptr<const TrackAudio> getPreview() const { return std::atomic_load (&preview); }
    std::shared_ptr<const Regions> getExtra (int track) const { return std::atomic_load (&strips[static_cast<size_t> (track)].extra); }
    int getPreviewLane() const { return previewLane.load(); }
    void setPreviewLane (int lane) { previewLane.store (juce::jlimit (-1, tracks - 1, lane)); }
    bool loadFile (int track, const juce::File& file, bool namPrinted = false, bool alsoPreview = false, bool append = false, double at = 0)
    {
        if (track < -1 || track >= tracks || loading.exchange (true)) return false;
        if (loader.joinable()) loader.join();
        if (track >= 0) strips[static_cast<size_t> (track)].sourceFile = file;
        setStatus ("Loading " + file.getFileName());
        loader = std::thread ([this, track, file, namPrinted, alsoPreview, append, at]
        {
            try
            {
                juce::AudioFormatManager formats; formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
                if (! reader || reader->lengthInSamples <= 0 || reader->numChannels < 1 || reader->numChannels > 2 || reader->sampleRate <= 0 || reader->sampleRate > 192000)
                    setStatus ("Use mono/stereo audio up to 192 kHz. Previous clip retained.");
                else
                {
                    auto result = std::make_shared<TrackAudio>(); result->file = file; result->rate = reader->sampleRate; result->namPrinted = namPrinted;
                    const int length = static_cast<int> (juce::jmin<juce::int64> (reader->lengthInSamples, static_cast<juce::int64> (reader->sampleRate * 180)));
                    result->audio.setSize (static_cast<int> (reader->numChannels), length);
                    if (! reader->read (&result->audio, 0, length, 0, true, true)) setStatus ("Could not decode audio. Previous clip retained.");
                    else
                    {
                        for (int c = 0; c < result->audio.getNumChannels(); ++c)
                            for (int n = 0; n < length; ++n)
                                if (! std::isfinite (result->audio.getSample (c, n))) result->audio.setSample (c, n, 0);
                        result->waveform = WaveformData::make (result->audio, result->rate, file.getFileName());
                        std::shared_ptr<const TrackAudio> ready = result;
                        if (track >= 0)
                        {
                            auto& strip = strips[static_cast<size_t> (track)];
                            if (append && getClip (track))
                            {
                                auto regions = std::make_shared<Regions>(); if (auto old = getExtra (track)) *regions = *old;
                                regions->push_back ({ ready, at - strip.start.load() });
                                std::atomic_store (&strip.extra, std::shared_ptr<const Regions> (regions));
                            }
                            else { if (append) strip.start.store (at); std::atomic_store (&strip.clip, ready); std::atomic_store (&strip.extra, std::shared_ptr<const Regions>()); }
                        }
                        if (track < 0 || alsoPreview) { previewPlaying.store (false); std::atomic_store (&preview, ready); previewLane.store (track); previewSeek.store (0); previewRevision.fetch_add (1); }
                        setStatus (file.getFileName() + (namPrinted ? " | NAM sound recorded" : " | Audio clip") + (reader->lengthInSamples > length ? " | First 3 minutes" : ""));
                    }
                }
            }
            catch (const std::exception&) { setStatus ("Audio loading failed. Previous clip retained."); }
            loading.store (false);
        });
        return true;
    }
    void waitForLoad() { if (loader.joinable()) loader.join(); }
    void clearPreview() { previewPlaying.store (false); previewLane.store (-1); std::atomic_store (&preview, std::shared_ptr<const TrackAudio>()); }
    void clear (int track) { auto& s = strips[static_cast<size_t> (track)]; s.sourceFile = {}; std::atomic_store (&s.clip, std::shared_ptr<const TrackAudio>()); std::atomic_store (&s.extra, std::shared_ptr<const Regions>()); if (previewLane.load() == track) clearPreview(); }
    void seek (double seconds) { requestedSeek.store (juce::jlimit (0.0, 3599.0, seconds)); revision.fetch_add (1); position.store (requestedSeek.load()); }
    void rewind() { seek (0); }
    void togglePreview()
    {
        if (! getPreview()) return;
        const bool start = ! previewPlaying.load();
        if (start) { previewSeek.store (0); previewRevision.fetch_add (1); }
        previewPlaying.store (start);
    }
    juce::String status() const { const juce::ScopedLock lock (statusLock); return message; }
    void setStatus (juce::String value) { const juce::ScopedLock lock (statusLock); message = value; }
    void beginBlock()
    {
        const auto nextRevision = revision.load(), nextPreviewRevision = previewRevision.load();
        if (lastRevision != nextRevision) { cursor = requestedSeek.load(); lastRevision = nextRevision; }
        if (lastPreviewRevision != nextPreviewRevision) { previewCursor = previewSeek.load(); lastPreviewRevision = nextPreviewRevision; }
        anySolo = false;
        for (size_t n = 0; n < blockClips.size(); ++n)
        {
            blockClips[n] = getClip (static_cast<int> (n)); blockGain[n] = strips[n].gain.load(); blockPan[n] = strips[n].pan.load();
            blockExtra[n] = getExtra (static_cast<int> (n));
            blockMute[n] = strips[n].mute.load(); blockSolo[n] = strips[n].solo.load(); blockStart[n] = strips[n].start.load(); anySolo |= blockSolo[n];
        }
        blockPreview = getPreview();
    }
    std::array<float, 2> render (bool countIn)
    {
        std::array<float, 2> result {};
        if (playing.load() && ! countIn)
        {
            for (size_t n = 0; n < blockClips.size(); ++n)
                if (blockClips[n] && ! blockMute[n] && (! anySolo || blockSolo[n]))
                    for (int c = 0; c < 2; ++c)
                    {
                        const float panGain = c == 0 ? juce::jmin (1.0f, 1.0f - blockPan[n]) : juce::jmin (1.0f, 1.0f + blockPan[n]);
                        result[static_cast<size_t> (c)] += read (*blockClips[n], cursor - blockStart[n], c) * blockGain[n] * panGain;
                        if (blockExtra[n]) for (const auto& r : *blockExtra[n]) result[static_cast<size_t> (c)] += read (*r.clip, cursor - blockStart[n] - r.offset, c) * blockGain[n] * panGain;
                    }
            cursor += 1 / deviceRate;
            if (cursor >= 3600) playing.store (false);
        }
        if (previewPlaying.load() && blockPreview)
        {
            const int lane = previewLane.load();
            for (int c = 0; c < 2; ++c)
            {
                const auto i = static_cast<size_t> (juce::jmax (0, lane));
                const float panGain = lane < 0 ? 1 : c == 0 ? juce::jmin (1.0f, 1.0f - blockPan[i]) : juce::jmin (1.0f, 1.0f + blockPan[i]);
                const float level = lane < 0 ? .7f : blockMute[i] || (anySolo && ! blockSolo[i]) ? 0 : blockGain[i];
                if (! playing.load()) result[static_cast<size_t> (c)] += read (*blockPreview, previewCursor, c) * level * panGain;
            }
            previewCursor += 1 / deviceRate;
            if (previewCursor >= blockPreview->waveform.seconds) previewPlaying.store (false);
        }
        return result;
    }
    void endBlock() { position.store (cursor); previewPosition.store (previewCursor); }
    std::array<float, 2> monitor (float x) const
    {
        const int lane = recordingLane.load(); if (lane < 0) return { x, x };
        const auto i = static_cast<size_t> (lane); if (blockMute[i] || (anySolo && ! blockSolo[i])) return {};
        return { x * blockGain[i] * juce::jmin (1.0f, 1.0f - blockPan[i]), x * blockGain[i] * juce::jmin (1.0f, 1.0f + blockPan[i]) };
    }
private:
    static float read (const TrackAudio& clip, double seconds, int channel)
    {
        const double rawSample = seconds * clip.rate;
        const double sample = rawSample > -0.0001 ? juce::jmax (0.0, rawSample) : rawSample;
        if (sample < 0 || sample >= clip.audio.getNumSamples()) return 0;
        const int a = static_cast<int> (sample), b = juce::jmin (a + 1, clip.audio.getNumSamples() - 1);
        const float fraction = static_cast<float> (sample - a);
        const int c = juce::jmin (channel, clip.audio.getNumChannels() - 1);
        return clip.audio.getSample (c, a) * (1 - fraction) + clip.audio.getSample (c, b) * fraction;
    }
    std::shared_ptr<const TrackAudio> preview, blockPreview;
    std::array<std::shared_ptr<const TrackAudio>, tracks> blockClips;
    std::array<std::shared_ptr<const Regions>, tracks> blockExtra;
    std::array<float, tracks> blockGain {}, blockPan {};
    std::array<double, tracks> blockStart {};
    std::array<bool, tracks> blockMute {}, blockSolo {};
    bool anySolo = false;
    std::atomic<int> previewLane { -1 };
    std::thread loader;
    mutable juce::CriticalSection statusLock;
    juce::String message { "Four audio lanes: select a lane, import audio or record an overdub." };
    std::atomic<double> requestedSeek { 0 }, previewSeek { 0 };
    std::atomic<unsigned> revision { 0 }, previewRevision { 0 };
    unsigned lastRevision = 0, lastPreviewRevision = 0;
    double cursor = 0, previewCursor = 0, deviceRate = 48000;
};
}
