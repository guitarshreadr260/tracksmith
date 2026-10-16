#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Waveform.h"
#include "NativeNam.h"
#include "DrumShaping.h"
#include "GuitarRig.h"
namespace TracksmithGrooveFull {

class GuitarEngine
{
public:
    GuitarEngine();
    ~GuitarEngine();
    void prepare (double rate, int block);
    void processInput (const juce::AudioBuffer<float>& input, int inputChannels);
    float outputAt (int sample) const { return monitor.load() ? wet.getSample (0, sample) * outputGain.load() : 0; }
    float dryAt (int sample) const { return dry.getSample (0, sample); }
    std::shared_ptr<const RigLayout> getRig() const { return std::atomic_load (&rig); }
    bool setRig (const RigLayout& layout);
    bool editBlock (int position, RigType type);
    bool moveBlock (int position, int destination);
    bool duplicateBlock (int position);
    void enableBlock (int position, bool enabled);
    void setBlockParameter (int position, int parameter, float value);
    WaveHistory liveWave, takeWave;
    std::atomic<juce::int64> preRollRemaining { 0 };
    double preRollSeconds() const { return preRollRemaining.load() / deviceRate.load(); }
    bool loadNamPlugin (const juce::File&);
    bool loadCapture (const juce::File&, bool allowRatePending = false);
    void clearCapture();
    juce::File captureFile() const;
    bool hasAmp() const { return backend.load() == 2 ? nativeReady.load() : backend.load() == 1 && nam != nullptr; }
    bool usesNative() const { return backend.load() == 2; }
    juce::AudioPluginInstance* getNam() const { return nam.get(); }
    bool startRecording (const juce::File&, bool processed, double preRollSeconds = 0);
    void stopRecording();
    void finishRecordingIfNeeded();
    bool isRecording() const { return recording.load(); }
    double recordedSeconds() const { return framesWritten.load() / deviceRate.load(); }
    juce::String status() const;
    void reportStatus (const juce::String& text) { const juce::ScopedLock lock (statusLock); message = text; }
    juce::String recordingPath() const;
    std::atomic<bool> monitor { false }, bypassNam { false };
    void setPickupConfiguration (int);
    void setManualInputGain (float gain) { inputGain.store (juce::jlimit (0.0f, 4.0f, gain)); pickupConfiguration.store (0); calibrating.store (false); calibrationResult.store (0); }
    void calibrateInput();
    void serviceInputCalibration();
    std::atomic<int> pickupConfiguration { 0 };
    std::atomic<bool> calibrating { false };
    std::atomic<float> calibrationProgress { 0 };
    std::atomic<bool> gateOn { false };
    std::atomic<float> ampDriveDb { 0 }, ampOutputDb { 0 }, bassDb { 0 }, midDb { 0 }, trebleDb { 0 }, gateThresholdDb { -60 }, gateReleaseMs { 100 };
    std::atomic<float> gateGainDisplay { 1 };
    std::atomic<int> inputChannel { 0 };
    std::atomic<float> inputGain { 1 }, outputGain { 0.5f }, inputPeak { 0 }, outputPeak { 0 };
private:
    juce::AudioPluginFormatManager formats;
    std::unique_ptr<juce::AudioPluginInstance> nam;
    mutable juce::CriticalSection namLock;
    juce::CriticalSection recordingLock;
    std::unique_ptr<NativeNam> native { std::make_unique<NativeNam>() };
    std::atomic<int> backend { 0 };
    std::atomic<bool> nativeReady { false };
    mutable juce::CriticalSection statusLock;
    juce::String message { "Load a .nam capture directly, or choose an installed NAM VST3." }, takePath;
    juce::TimeSliceThread writerThread { "Guitar recording writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::AudioBuffer<float> dry, wet, namBuffer, gateCurve;
    juce::MidiBuffer namMidi;
    std::atomic<double> deviceRate { 48000 };
    std::atomic<juce::int64> framesWritten { 0 };
    std::atomic<bool> recording { false }, recordProcessed { false }, droppedAudio { false };
    std::shared_ptr<const RigLayout> rig { std::make_shared<RigLayout>() };
    std::array<RigEffect, RigLayout::slots> rigEffects;
    std::array<unsigned, RigLayout::slots> effectRevisions {};
    juce::CriticalSection rigEditLock;
    DrumChannelStrip toneStrip;
    float gateEnvelope = 0, gateGain = 1, driveGain = 1, ampGain = 1;
    bool gateOpen = false;
    int gateHold = 0;
    std::atomic<unsigned> calibrationRequest { 0 };
    unsigned lastCalibrationRequest = 0;
    double calibrationSquares = 0;
    float calibrationPeak = 0;
    int calibrationFrames = 0;
    std::atomic<int> calibrationResult { 0 };
    int maximumBlock = 32768;
};
}
