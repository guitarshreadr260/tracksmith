#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "PlaybackCore.h"
#include <functional>

struct MixExportOptions
{
    double start = 0, end = 0, tail = 2;
    int bits = 24;
};
struct MixExportResult
{
    juce::String error;
    bool cancelled = false, clipped = false;
    int64_t frames = 0;
};

// Caller owns the graph exclusively until completion. IO runs off the audio callback.
inline MixExportResult writeMixWav(PlaybackCore& core, const juce::File& target,
                                   const MixExportOptions& options,
                                   const std::function<bool()>& cancelled,
                                   const std::function<void(double)>& progress,
                                   const std::function<juce::String()>& validate = {})
{
    MixExportResult result;
    const double rate = core.deviceRate;
    if (!std::isfinite(rate) || rate < 8000 || rate > 384000 ||
        !std::isfinite(options.start) || !std::isfinite(options.end) || !std::isfinite(options.tail) ||
        options.start < 0 || options.end <= options.start || options.end > core.duration() + 1.e-6 ||
        options.end > 3600 || options.tail < 0 || options.tail > 30 ||
        (options.bits != 16 && options.bits != 24))
    { result.error = "Choose a valid range up to one hour, a 0–30 second tail and 16/24-bit PCM."; return result; }
    const auto first = static_cast<int64_t>(std::llround(options.start * rate));
    const auto end = static_cast<int64_t>(std::llround(options.end * rate));
    const auto tail = static_cast<int64_t>(std::llround(options.tail * rate));
    const auto frames = end - first + tail;
    if (frames <= 0 || static_cast<uint64_t>(frames) * 2u * static_cast<unsigned>(options.bits / 8) > 0xffff0000ULL)
    { result.error = "Export exceeds the WAV size limit. Choose a shorter range."; return result; }
    juce::TemporaryFile temporary(target);
    auto fileStream = temporary.getFile().createOutputStream();
    if (!fileStream || !fileStream->openedOk()) { result.error = "Cannot create the export file."; return result; }
    std::unique_ptr<juce::OutputStream> stream(std::move(fileStream));
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions()
        .withSampleRate(rate).withNumChannels(2).withBitsPerSample(options.bits));
    if (!writer) { result.error = "Cannot create the WAV writer."; return result; }

    const double oldPosition = core.seconds.load();
    const bool oldLoop = core.loop.load(), oldClick = core.metronome.load();
    const int oldMonitor = core.inputMonitorTrack.load();
    const bool oldDefer = core.deferMasterProcessing;
    const auto oldClickSource=core.extraClick;
    struct Restore
    {
        PlaybackCore& c; double position; bool loop, click; int monitor; bool defer; PlaybackCore::Click clickSource;
        ~Restore() { c.offlineSourceEnd = c.offlineRenderEnd = -1; c.reset(); c.seekSeconds = position;
                     c.loop = loop; c.metronome = click; c.inputMonitorTrack = monitor; c.deferMasterProcessing = defer; c.extraClick=clickSource; }
    } restore {core, oldPosition, oldLoop, oldClick, oldMonitor, oldDefer,oldClickSource};
    core.reset(); core.loop = false; core.metronome = false; core.inputMonitorTrack = -1; core.deferMasterProcessing = false;core.extraClick=nullptr;
    core.offlineSourceEnd = static_cast<double>(end) / rate;
    core.offlineRenderEnd = static_cast<double>(end + tail) / rate;
    // Export starts at the requested mix levels, without a transport-start fader ramp.
    core.primeMixLevels();
    core.playing = true;
    std::array<float, PlaybackCore::processBlockSize> left {}, right {};
    float* output[] {left.data(), right.data()};
    const int64_t skip = first + core.playbackLatencyFrames;
    const int64_t total = end + tail + core.playbackLatencyFrames;
    for (int64_t position = 0; position < total;)
    {
        if (cancelled && cancelled()) { result.cancelled = true; return result; }
        const int count = static_cast<int>(std::min<int64_t>(PlaybackCore::processBlockSize, total - position));
        if (validate) { const auto error = validate(); if (error.isNotEmpty()) { result.error = error; return result; } }
        core.render(output, 2, count);
        if (position + count > skip)
        {
            const int offset = static_cast<int>(std::max<int64_t>(0, skip - position));
            const float* audio[] {left.data() + offset, right.data() + offset};
            if (!writer->writeFromFloatArrays(audio, 2, count - offset))
            { result.error = "WAV write failed. Check free disk space."; return result; }
            result.frames += count - offset;
            result.clipped = result.clipped || core.clipping.load();
        }
        position += count;
        if (progress) progress(static_cast<double>(position) / static_cast<double>(total));
    }
    if (validate) { const auto error = validate(); if (error.isNotEmpty()) { result.error = error; return result; } }
    if (!writer->flush()) { result.error = "Could not flush the WAV file."; return result; }
    writer.reset();
    if (cancelled && cancelled()) { result.cancelled = true; return result; }
    if (!temporary.overwriteTargetFileWithTemporary()) result.error = "Could not replace the destination WAV. The previous file was kept.";
    return result;
}

class MixExportWorker final : private juce::Thread, private juce::AsyncUpdater
{
public:
    MixExportWorker() : Thread("Tracksmith WAV export") {}
    ~MixExportWorker() override { signalThreadShouldExit(); stopThread(-1); cancelPendingUpdate(); }
    std::function<void(MixExportResult)> onFinished;
    std::function<juce::String()> validate;
    std::atomic<double> progress {0};
    void cancel() { signalThreadShouldExit(); }
    bool start(PlaybackCore& playback, const juce::File& file, MixExportOptions settings)
    {
        if (isThreadRunning()) return false;
        core = &playback; target = file; options = settings; progress = 0; result = {};
        return startThread();
    }
private:
    void run() override
    {
        try { result = writeMixWav(*core, target, options, [this] { return threadShouldExit(); },
                                   [this](double value) { progress = value; }, validate); }
        catch (...) { result.error = "Export failed. The destination was not replaced."; }
        triggerAsyncUpdate();
    }
    void handleAsyncUpdate() override { if (onFinished) onFinished(result); }
    PlaybackCore* core = nullptr;
    juce::File target;
    MixExportOptions options;
    MixExportResult result;
};

