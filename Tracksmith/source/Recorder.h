#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "RecordQueue.h"
#include "InputRouting.h"

struct RecordedTake
{
    juce::File file;
    uint64_t frames = 0;
    double rate = 48000.0;
    juce::String error, warning;
};

class Recorder final : private juce::Thread
{
public:
    enum class Fault { none, overflow, inputStopped, inputMissing, disk, limit };
    Recorder() : Thread("Recording disk writer") {}
    ~Recorder() override { requestStop(); stopThread(-1); }
    juce::Result start(const juce::File& destination, double rate, int channels, uint64_t frameLimit)
    {
        if (isThreadRunning()) return juce::Result::fail("A take is still being finalized.");
        if (!std::isfinite(rate) || rate < 8000 || rate > 192000 || channels < 1 || channels > 2 || frameLimit == 0)
            return juce::Result::fail("Choose a valid mono/stereo input and free session audio space.");
        if (destination.exists()) return juce::Result::fail("Recording filename already exists; choose a new take.");
        const auto directory = destination.getParentDirectory().createDirectory();
        if (directory.failed()) return directory;
        auto stream = std::make_unique<juce::FileOutputStream>(destination);
        if (!stream->openedOk()) return stream->getStatus();
        outputStatus = stream.get();
        std::unique_ptr<juce::OutputStream> output = std::move(stream);
        juce::WavAudioFormat format;
        writer = format.createWriterFor(output, juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(channels).withBitsPerSample(24));
        if (!writer) { outputStatus = nullptr; return juce::Result::fail("Could not create the recording WAV."); }
        previewCount=0; previewFrames=0; previewLow=0; previewHigh=0; previewBinFrames=std::max(1,static_cast<int>(rate/50));
        queue.reset(); result = {}; result.file = destination; result.rate = rate;
        channelCount = channels; maximumFrames = frameLimit; captured = 0; fault = Fault::none; finished = false; producerBusy = false; active = true;
        if (!startThread()) { active = false; writer.reset(); outputStatus = nullptr; return juce::Result::fail("Could not start the recording writer."); }
        return juce::Result::ok();
    }
    void capture(const float* const* inputs, int inputChannels, int frames, int firstInput = 0) noexcept
    {
        if (!active.load()) return;
        producerBusy.store(true);
        if (!active.load()) { producerBusy = false; return; }
        const auto sources = InputRouting::select(inputs, inputChannels, firstInput, channelCount);
        if (!sources[0] || (channelCount == 2 && !sources[1])) { stopFor(Fault::inputMissing); producerBusy = false; return; }
        const auto already = captured.load(std::memory_order_relaxed);
        const int count = static_cast<int>(std::min<uint64_t>(static_cast<uint64_t>(std::max(0, frames)), maximumFrames - already));
        if (!queue.push(sources.data(), channelCount, count)) stopFor(Fault::overflow);
        else
        {
            captured.store(already + static_cast<uint64_t>(count), std::memory_order_release);
            if (already + static_cast<uint64_t>(count) >= maximumFrames) stopFor(Fault::limit);
        }
        producerBusy.store(false);
    }
    void requestStop() noexcept { active.store(false); }
    void inputStopped() noexcept { if (active.load()) stopFor(Fault::inputStopped); }
    bool isActive() const noexcept { return active.load(std::memory_order_acquire); }
    bool isFinished() const noexcept { return finished.load(std::memory_order_acquire) && !isThreadRunning(); }
    double elapsed() const noexcept { return static_cast<double>(captured.load()) / result.rate; }
    juce::Array<juce::var> waveformPreview() const
    {
        juce::Array<juce::var> values;
        const auto count=previewCount.load(std::memory_order_acquire);
        const size_t stride=std::max<size_t>(1,(count+1199)/1200);
        for(size_t i=0;i<count;i+=stride){float lo=0,hi=0;for(size_t j=i;j<std::min(count,i+stride);++j){lo=std::min(lo,previewMin[j].load());hi=std::max(hi,previewMax[j].load());}values.add(lo);values.add(hi);}
        return values;
    }
    RecordedTake takeResult() { finished = false; return result; }
    bool waitForFinish(int milliseconds) { return waitForThreadToExit(milliseconds); }
private:
    void stopFor(Fault reason) noexcept
    {
        auto expected = Fault::none; fault.compare_exchange_strong(expected, reason);
        active.store(false);
    }
    void run() override
    {
        uint64_t lastFlush = 0;
        while (active.load() || producerBusy.load() || queue.available() > 0)
        {
            std::array<const float*, 2> samples {}; const int count = queue.readable(samples);
            if (count == 0) { wait(2); continue; }
            if (fault.load() != Fault::disk)
            {
                if (!writer->writeFromFloatArrays(samples.data(), channelCount, count)) { fault = Fault::disk; requestStop(); }
                else result.frames += static_cast<uint64_t>(count);
                if (result.frames - lastFlush >= static_cast<uint64_t>(result.rate))
                { if (!writer->flush()) { fault = Fault::disk; requestStop(); } lastFlush = result.frames; }
            }
            // Envelope work stays on the disk thread; the audio callback is unchanged.
            for(int frame=0;frame<count;++frame){for(int ch=0;ch<channelCount;++ch){const auto v=samples[ch][frame];if(std::isfinite(v)){previewLow=std::min(previewLow,v);previewHigh=std::max(previewHigh,v);}}
                const auto index=previewFrames/static_cast<uint64_t>(previewBinFrames);
                if(index<previewCapacity){previewMin[index].store(previewLow);previewMax[index].store(previewHigh);previewCount.store(static_cast<size_t>(index+1),std::memory_order_release);}
                ++previewFrames;if(previewFrames%previewBinFrames==0){previewLow=0;previewHigh=0;}}
            queue.finishRead(count);
        }
        if (!writer->flush()) fault = Fault::disk;
        if (outputStatus->getStatus().failed()) fault = Fault::disk;
        writer.reset(); // Final WAV header and stream close happen on this worker.
        outputStatus = nullptr;
        switch (fault.load())
        {
            case Fault::disk: result.error = "Recording disk write failed. The armed track was kept unchanged."; break;
            case Fault::overflow: result.warning = "Recording stopped because the disk buffer filled; the captured partial take was kept."; break;
            case Fault::inputStopped: case Fault::inputMissing: result.warning = "Input stopped; the captured partial take was kept."; break;
            case Fault::limit: result.warning = "Recording reached the per-take/session audio limit."; break;
            case Fault::none: break;
        }
        if (result.frames == 0 && result.error.isEmpty()) result.error = "No input audio frames were captured. Choose an input in Audio settings.";
        finished.store(true, std::memory_order_release);
    }
    static constexpr size_t previewCapacity=131072;
    std::unique_ptr<std::atomic<float>[]> previewMin=std::make_unique<std::atomic<float>[]>(previewCapacity),previewMax=std::make_unique<std::atomic<float>[]>(previewCapacity);
    std::atomic<size_t> previewCount{0};
    uint64_t previewFrames=0;int previewBinFrames=960;float previewLow=0,previewHigh=0;
    RecordQueue queue;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::FileOutputStream* outputStatus = nullptr; // Non-owning; writer owns the stream.
    RecordedTake result;
    int channelCount = 1;
    uint64_t maximumFrames = 0;
    std::atomic<uint64_t> captured { 0 };
    std::atomic<bool> active { false }, producerBusy { false }, finished { false };
    std::atomic<Fault> fault { Fault::none };
};

