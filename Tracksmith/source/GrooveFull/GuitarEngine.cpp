#include "GuitarEngine.h"
#include <cmath>
namespace TracksmithGrooveFull {

GuitarEngine::GuitarEngine()
{
    formats.addFormat (new juce::VST3PluginFormat());
    writerThread.startThread();
}
GuitarEngine::~GuitarEngine() { stopRecording(); writerThread.stopThread (5000); }

void GuitarEngine::prepare (double rate, int block)
{
    stopRecording();
    deviceRate.store (rate); liveWave.reset (rate); takeWave.reset (rate); preRollRemaining.store (0);
    maximumBlock = juce::jmax (32768, block);
    gateCurve.setSize (1, maximumBlock);
    dry.setSize (1, maximumBlock); wet.setSize (1, maximumBlock); namBuffer.setSize (2, maximumBlock);
    dry.clear(); wet.clear(); namBuffer.clear();
    calibrating.store (false); calibrationFrames = 0; gateOpen = false;
    for (auto& effect : rigEffects) effect.prepare (rate); effectRevisions.fill (0);
    toneStrip.prepare (rate); gateEnvelope = 0; gateGain = driveGain = ampGain = 1; gateHold = 0;
    const juce::ScopedLock lock (namLock);
    juce::String error; nativeReady.store (native->prepare (rate, maximumBlock, error) && native->ready());
    if (backend.load() == 2) { if (error.isNotEmpty()) reportStatus (error); else if (nativeReady.load()) reportStatus ("Direct NAM: " + native->file().getFileName() + " | Ready at " + juce::String (rate, 0) + " Hz"); }
    if (nam) { nam->setRateAndBufferSizeDetails (rate, maximumBlock); nam->prepareToPlay (rate, maximumBlock); }
}

bool GuitarEngine::loadNamPlugin (const juce::File& file)
{
    if (isRecording()) return false;
    juce::String error;
    try
    {
        juce::OwnedArray<juce::PluginDescription> descriptions;
        auto* format = formats.getFormat (0);
        format->findAllTypesForFile (descriptions, file.getFullPathName());
        if (descriptions.isEmpty()) error = "No VST3 found at " + file.getFullPathName();
        else
        {
            auto instance = formats.createPluginInstance (*descriptions[0], deviceRate.load(), maximumBlock, error);
            if (instance && instance->getTotalNumInputChannels() >= 1 && instance->getTotalNumInputChannels() <= 2
                && instance->getTotalNumOutputChannels() >= 1 && instance->getTotalNumOutputChannels() <= 2)
            {
                instance->prepareToPlay (deviceRate.load(), maximumBlock);
                const juce::ScopedLock lock (namLock);
                if (nam) nam->releaseResources();
                nam = std::move (instance); backend.store (1);
                const juce::ScopedLock statusGuard (statusLock);
                message = "NAM loaded. Open NAM controls to choose your .nam capture (and cabinet IR if needed).";
                return true;
            }
            if (error.isEmpty()) error = "This plugin needs an unsupported channel layout.";
        }
    }
    catch (const std::exception& exception) { error = exception.what(); }
    const juce::ScopedLock lock (statusLock); message = "NAM load failed: " + error; return false;
}

void GuitarEngine::processInput (const juce::AudioBuffer<float>& input, int inputChannels)
{
    const int count = input.getNumSamples();
    if (count > maximumBlock) { wet.clear(); return; }
    const int selected = inputChannel.load();
    const bool available = selected < inputChannels && selected < input.getNumChannels();
    const unsigned request = calibrationRequest.load();
    if (request != lastCalibrationRequest) { lastCalibrationRequest = request; calibrationFrames = 0; calibrationSquares = 0; calibrationPeak = 0; calibrationProgress.store (0); }
    const float gain = inputGain.load();
    const bool shape = ! bypassNam.load();
    const auto layout = getRig();
    const bool gating = shape && gateOn.load();
    const double rate = deviceRate.load();
    const float threshold = juce::Decibels::decibelsToGain (gateThresholdDb.load());
    const float gateAttack = static_cast<float> (1 - std::exp (-1 / (rate * .001)));
    const float gateRelease = static_cast<float> (1 - std::exp (-1 / (rate * gateReleaseMs.load() * .001)));
    const float detectorRelease = static_cast<float> (std::exp (-1 / (rate * .01)));
    const float smoothing = static_cast<float> (1 - std::exp (-1 / (rate * .005)));
    const float targetDrive = juce::Decibels::decibelsToGain (shape ? ampDriveDb.load() : 0);
    const float targetOutput = juce::Decibels::decibelsToGain (shape ? ampOutputDb.load() : 0);
    DrumShaping tone; tone.eqOn = shape; tone.frequency = { 90, 1000, 2500, 6500 }; tone.gainDb = { bassDb.load(), midDb.load(), 0, trebleDb.load() }; toneStrip.update (tone);
    float peak = 0;
    for (int sample = 0; sample < count; ++sample)
    {
        const float original = available ? input.getSample (selected, sample) : 0;
        if (calibrating.load() && std::isfinite (original))
        {
            calibrationSquares += static_cast<double> (original) * original; calibrationPeak = juce::jmax (calibrationPeak, std::abs (original)); ++calibrationFrames;
            const int targetFrames = static_cast<int> (rate * 5);
            if (calibrationFrames >= targetFrames)
            {
                const float rms = static_cast<float> (std::sqrt (calibrationSquares / calibrationFrames));
                const int profile = pickupConfiguration.load();
                if (rms >= .003f && calibrationPeak >= .02f && profile != 0)
                {
                    const float limit = profile == 1 ? 4.0f : profile == 2 ? 3.0f : 2.0f;
                    const float adjusted = juce::jlimit (.1f, limit, juce::jmin (.0630957f / rms, .8f / calibrationPeak));
                    inputGain.store (adjusted); calibrationResult.store (1);
                }
                else calibrationResult.store (2);
                calibrating.store (false); calibrationProgress.store (1);
            }
        }
        const float raw = original * gain;
        const float value = std::isfinite (raw) ? juce::jlimit (-1.0f, 1.0f, raw) : 0;
        liveWave.push (value); dry.setSample (0, sample, value);
        wet.setSample (0, sample, value);
        peak = juce::jmax (peak, std::abs (value));
    }
    if (calibrating.load()) calibrationProgress.store (juce::jlimit (0.0f, 1.0f, calibrationFrames / static_cast<float> (rate * 5)));
    inputPeak.store (peak);
    if (shape)
    for (const auto& b : layout->blocks)
    {
        auto& effect = rigEffects[static_cast<size_t> (b.id)];
        if (effectRevisions[static_cast<size_t> (b.id)] != b.revision) { effect.reset(); effectRevisions[static_cast<size_t> (b.id)] = b.revision; }
        if (! b.enabled || b.type == RigType::empty) continue;
        if (b.type == RigType::capture)
        {
            const juce::ScopedLock lock (namLock);
            if (monitor.load() || (recording.load() && recordProcessed.load()))
            {
                if (backend.load() == 2) { if (nativeReady.load()) native->process (wet, count); else wet.clear(); }
                else if (backend.load() == 1 && nam)
                {
                    for (int ch = 0; ch < 2; ++ch) namBuffer.copyFrom (ch, 0, wet, 0, 0, count);
                    const int channels = juce::jmax (nam->getTotalNumInputChannels(), nam->getTotalNumOutputChannels());
                    juce::AudioBuffer<float> block (namBuffer.getArrayOfWritePointers(), channels, count);
                    namMidi.clear(); nam->processBlock (block, namMidi); wet.copyFrom (0, 0, block, 0, 0, count);
                }
            }
            // A hosted model must not feed non-finite samples into stateful effects.
            for (int sample = 0; sample < count; ++sample) { const float x = wet.getSample (0, sample); wet.setSample (0, sample, std::isfinite (x) ? juce::jlimit (-64.0f, 64.0f, x) : 0); }
            for (int sample = 0; sample < count; ++sample) wet.setSample (0, sample, effect.makeup (wet.getSample (0, sample), b));
            continue;
        }
        for (int sample = 0; sample < count; ++sample)
        {
            float value = wet.getSample (0, sample);
            if (b.type == RigType::gate)
            {
                gateEnvelope = juce::jmax (std::abs (value), gateEnvelope * detectorRelease);
                if (gating && gateEnvelope >= threshold) { gateOpen = true; gateHold = static_cast<int> (rate * .03); }
                else if (gateHold > 0) --gateHold;
                if (gating && gateEnvelope < threshold * .5f && gateHold == 0) gateOpen = false;
                const float target = ! gating || gateOpen ? 1.0f : 0.0f;
                gateGain += (target > gateGain ? gateAttack : gateRelease) * (target - gateGain);
                value *= gating ? gateGain : 1;
            }
            else if (b.type == RigType::drive) { driveGain += smoothing * (targetDrive - driveGain); value *= driveGain; }
            else if (b.type == RigType::tone) value = toneStrip.render ({ value, value })[0];
            else if (b.type == RigType::output) { ampGain += smoothing * (targetOutput - ampGain); value *= ampGain; }
            else value = effect.process (value, b);
            wet.setSample (0, sample, std::isfinite (value) ? effect.makeup (value, b) : 0);
        }
    }
    gateGainDisplay.store (gating ? gateGain : 1);
    for (int sample = 0; sample < count; ++sample) { const float x = wet.getSample (0, sample); wet.setSample (0, sample, std::isfinite (x) ? juce::jlimit (-1.0f, 1.0f, x) : 0); }
    outputPeak.store (wet.getMagnitude (0, 0, count));
    if (recording.load())
    {
        const juce::ScopedLock lock (recordingLock);
        if (writer && recording.load())
        {
            const auto limit = static_cast<juce::int64> (std::llround (deviceRate.load() * 60));
            const int skip = static_cast<int> (juce::jmin<juce::int64> (count, preRollRemaining.load()));
            preRollRemaining.fetch_sub (skip);
            const int amount = static_cast<int> (juce::jmin<juce::int64> (count - skip, limit - framesWritten.load()));
            const float* channels[] { recordProcessed.load() ? wet.getReadPointer (0) + skip : dry.getReadPointer (0) + skip };
            if (amount > 0 && ! writer->write (channels, amount)) { droppedAudio.store (true); recording.store (false); }
            else { for (int n = 0; n < amount; ++n) takeWave.push (channels[0][n]); framesWritten.fetch_add (amount); }
            if (framesWritten.load() >= limit) recording.store (false);
        }
    }
}

bool GuitarEngine::startRecording (const juce::File& requested, bool processed, double preRollSeconds)
{
    stopRecording();
    calibrating.store (false);
    const auto recordingRig = getRig();
    const bool captureActive = std::any_of (recordingRig->blocks.begin(), recordingRig->blocks.end(), [] (const RigBlock& b) { return b.type == RigType::capture && b.enabled; });
    if (processed && (! hasAmp() || bypassNam.load() || ! captureActive))
    { const juce::ScopedLock lock (statusLock); message = "Load NAM and disable bypass before recording processed guitar, or select Dry DI."; return false; }
    const auto file = requested.getNonexistentSibling();
    const double rate = deviceRate.load();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (! stream) { const juce::ScopedLock lock (statusLock); message = "Could not create guitar recording."; return false; }
    juce::WavAudioFormat wav;
#if JUCE_MAJOR_VERSION >= 9
    auto audioWriter = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (1).withBitsPerSample (24));
#else
    std::unique_ptr<juce::AudioFormatWriter> audioWriter (wav.createWriterFor (stream.release(), rate, 1, 24, {}, 0));
#endif
    if (! audioWriter) return false;
    auto threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (audioWriter.release(), writerThread, static_cast<int> (rate * 61));
    { const juce::ScopedLock lock (recordingLock); writer = std::move (threaded); takeWave.reset (rate); preRollRemaining.store (static_cast<juce::int64> (std::llround (rate * juce::jlimit (0.0, 60.0, preRollSeconds)))); framesWritten.store (0); droppedAudio.store (false); recordProcessed.store (processed); recording.store (true); }
    { const juce::ScopedLock lock (statusLock); takePath = file.getFullPathName(); message = "Recording " + juce::String (processed ? "processed guitar" : "dry DI") + "; automatic stop at 60 seconds."; }
    return true;
}

void GuitarEngine::stopRecording()
{
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> finished;
    { const juce::ScopedLock lock (recordingLock); recording.store (false); preRollRemaining.store (0); finished = std::move (writer); }
    if (finished)
    {
        finished.reset();
        const juce::ScopedLock lock (statusLock);
        message = droppedAudio.load() ? "Recording stopped: audio buffer overflow. Check disk/device settings. " + takePath : juce::String (recordProcessed.load() ? "NAM sound saved: " : "Dry DI saved: ") + takePath;
    }
}
void GuitarEngine::finishRecordingIfNeeded() { if (! recording.load()) stopRecording(); }
juce::String GuitarEngine::status() const { const juce::ScopedLock lock (statusLock); return message; }
juce::String GuitarEngine::recordingPath() const { const juce::ScopedLock lock (statusLock); return takePath; }

bool GuitarEngine::loadCapture (const juce::File& file, bool allowRatePending)
{
    if (isRecording()) { reportStatus ("Stop recording before changing the capture"); return false; }
    auto next = std::make_unique<NativeNam>(); juce::String error;
    if (! next->load (file, deviceRate.load(), maximumBlock, error, allowRatePending)) { reportStatus ("Capture load failed: " + error + " | Previous amp retained"); return false; }
    { const juce::ScopedLock lock (namLock); native = std::move (next); nativeReady.store (native->ready()); backend.store (2); }
    bypassNam.store (false); if (error.isNotEmpty()) { reportStatus (error); return true; } reportStatus ("Direct NAM: " + file.getFileName() + " | Amp sound available for monitoring and NAM recording"); return true;
}
juce::File GuitarEngine::captureFile() const
{ const juce::ScopedLock lock (namLock); return backend.load() == 2 ? native->file() : juce::File(); }
void GuitarEngine::clearCapture()
{
    if (isRecording()) return;
    const juce::ScopedLock lock (namLock); native->clear(); nativeReady.store (false); if (backend.load() == 2) backend.store (0);
}

void GuitarEngine::setPickupConfiguration (int profile)
{
    if (isRecording()) return;
    pickupConfiguration.store (juce::jlimit (0, 3, profile));
    if (profile == 0) { calibrating.store (false); return; }
    calibrateInput();
}
void GuitarEngine::calibrateInput()
{
    if (isRecording() || pickupConfiguration.load() == 0) return;
    calibrationResult.store (0); calibrationProgress.store (0); calibrationRequest.fetch_add (1); calibrating.store (true);
    reportStatus ("Auto input: play normal chords / picking for 5 seconds. Manual trim cancels calibration.");
}

void GuitarEngine::serviceInputCalibration()
{
    const int result = calibrationResult.exchange (0);
    if (result == 1) reportStatus ("Input calibrated: " + juce::String (juce::Decibels::gainToDecibels (inputGain.load()), 1) + " dB trim | Manual trim remains available");
    else if (result == 2) reportStatus ("Not enough guitar signal to calibrate. Play normally and click Recalibrate.");
}


bool GuitarEngine::setRig (const RigLayout& layout)
{
    if (! layout.valid() || isRecording()) return false;
    const juce::ScopedLock lock (rigEditLock);
    auto next = std::make_shared<RigLayout> (layout);
    const auto old = getRig();
    for (auto& b : next->blocks) for (const auto& previous : old->blocks) if (b.id == previous.id) b.revision = previous.revision + 1;
    std::atomic_store (&rig, std::shared_ptr<const RigLayout> (next)); return true;
}
bool GuitarEngine::editBlock (int position, RigType type)
{
    if (position < 0 || position >= RigLayout::slots || isRecording()) return false;
    const juce::ScopedLock lock (rigEditLock); auto next = std::make_shared<RigLayout> (*getRig());
    auto& b = next->blocks[static_cast<size_t> (position)]; b.type = type; b.enabled = true; ++b.revision;
    b.params = rigDefaults (type);
    if (! next->valid()) return false;
    std::atomic_store (&rig, std::shared_ptr<const RigLayout> (next)); return true;
}
bool GuitarEngine::moveBlock (int position, int destination)
{
    if (position < 0 || position >= RigLayout::slots || destination < 0 || destination >= RigLayout::slots || isRecording()) return false;
    const juce::ScopedLock lock (rigEditLock); auto next = std::make_shared<RigLayout> (*getRig());
    std::swap (next->blocks[static_cast<size_t> (position)], next->blocks[static_cast<size_t> (destination)]);
    std::atomic_store (&rig, std::shared_ptr<const RigLayout> (next)); return true;
}
bool GuitarEngine::duplicateBlock (int position)
{
    if (position < 0 || position >= RigLayout::slots || isRecording()) return false;
    const juce::ScopedLock lock (rigEditLock); auto next = std::make_shared<RigLayout> (*getRig()); const auto source = next->blocks[static_cast<size_t> (position)];
    if (! rigIsEffect (source.type)) return false;
    for (auto& b : next->blocks) if (b.type == RigType::empty) { const int id = b.id; const unsigned revision = b.revision + 1; b = source; b.id = id; b.revision = revision; std::atomic_store (&rig, std::shared_ptr<const RigLayout> (next)); return true; }
    return false;
}
void GuitarEngine::enableBlock (int position, bool enabled)
{
    if (position < 0 || position >= RigLayout::slots || isRecording()) return;
    const juce::ScopedLock lock (rigEditLock); auto next = std::make_shared<RigLayout> (*getRig()); auto& b = next->blocks[static_cast<size_t> (position)]; if (b.enabled != enabled) { b.enabled = enabled; ++b.revision; std::atomic_store (&rig, std::shared_ptr<const RigLayout> (next)); }
}
void GuitarEngine::setBlockParameter (int position, int parameter, float value)
{
    if (position < 0 || position >= RigLayout::slots || parameter < 0 || parameter >= 6 || ! std::isfinite (value)) return;
    const juce::ScopedLock lock (rigEditLock); auto next = std::make_shared<RigLayout> (*getRig()); next->blocks[static_cast<size_t> (position)].params[static_cast<size_t> (parameter)] = value;
    if (next->valid()) std::atomic_store (&rig, std::shared_ptr<const RigLayout> (next));
}
}

