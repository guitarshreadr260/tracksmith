#include "NativeNam.h"
#include <NAM/get_dsp.h>

struct NativeNam::Impl
{
    std::unique_ptr<nam::DSP> model;
    std::vector<double> input, output;
    juce::File path;
    bool usable = false;
    void prepare (double rate, int block)
    {
        usable = false;
        if (! model) return;
        const double expected = model->GetExpectedSampleRate();
        const double required = expected > 0 ? expected : 48000;
        if (! model->SupportsArbitrarySampleRate() && std::abs (required - rate) > .5)
            throw std::runtime_error (("Set the audio device to " + juce::String (required, 0) + " Hz for this capture. Current device: " + juce::String (rate, 0) + " Hz").toStdString());
        input.resize (static_cast<size_t> (block)); output.resize (static_cast<size_t> (block));
        model->Reset (rate, block); usable = true;
    }
};
NativeNam::NativeNam() : impl (std::make_unique<Impl>()) {}
NativeNam::~NativeNam() = default;
bool NativeNam::load (const juce::File& file, double rate, int block, juce::String& error, bool allowRatePending)
{
    try
    {
        if (! file.hasFileExtension ("nam") || ! file.existsAsFile() || file.getSize() > 32 * 1024 * 1024) throw std::runtime_error ("Use a .nam capture up to 32 MB");
        auto next = std::make_unique<Impl>();
        next->model = nam::get_dsp (std::filesystem::path (file.getFullPathName().toWideCharPointer()));
        if (! next->model || next->model->NumInputChannels() != 1 || next->model->NumOutputChannels() != 1) throw std::runtime_error ("Use a mono-input, mono-output guitar capture");
        next->path = file;
        const double expected = next->model->GetExpectedSampleRate(); const double required = expected > 0 ? expected : 48000;
        if (allowRatePending && ! next->model->SupportsArbitrarySampleRate() && std::abs (required - rate) > .5)
        {
            next->prepare (required, block); next->usable = false; error = "Capture selected: " + file.getFileName() + " | Set device to " + juce::String (required, 0) + " Hz in Settings to enable NAM";
        }
        else next->prepare (rate, block);
        impl = std::move (next); return true;
    }
    catch (const std::exception& e) { error = e.what(); return false; }
}
bool NativeNam::prepare (double rate, int block, juce::String& error)
{
    try { impl->prepare (rate, block); return true; }
    catch (const std::exception& e) { error = e.what(); return false; }
}
bool NativeNam::ready() const { return impl->usable; }
juce::File NativeNam::file() const { return impl->path; }
void NativeNam::clear() { impl = std::make_unique<Impl>(); }
void NativeNam::process (juce::AudioBuffer<float>& audio, int frames)
{
    if (! impl->usable || frames > static_cast<int> (impl->input.size())) { audio.clear (0, 0, frames); return; }
    for (int n = 0; n < frames; ++n) impl->input[static_cast<size_t> (n)] = audio.getSample (0, n);
    auto* input = impl->input.data(); auto* output = impl->output.data(); impl->model->process (&input, &output, frames);
    for (int n = 0; n < frames; ++n) { const double value = impl->output[static_cast<size_t> (n)]; audio.setSample (0, n, std::isfinite (value) ? static_cast<float> (juce::jlimit (-64.0, 64.0, value)) : 0); }
}
