#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "GrooveGenerator.h"
#include "GrooveAnalysis.h"
#include "NeuralGeneration.h"
#include "MidiDrag.h"
#include "GuitarEngine.h"
#include "Tone3000.h"
#include "LiveRhythm.h"
#include "MultitrackEngine.h"
#include "RecordingLocation.h"
#include "MidiGroove.h"
#include "HumanFeel.h"
#include <thread>
namespace TracksmithGrooveFull {

class AvrxGrooveProcessor : public juce::AudioProcessor
{
public:
    std::function<void()> sendGrooveToSong;
    std::atomic<bool> nativeStateChanged{false};
    double referenceOffset=0,referenceLength=180;
    AvrxGrooveProcessor();
    ~AvrxGrooveProcessor() override;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void reset() override;
    std::atomic<double> hostTempo {0};
    std::atomic<bool> hostAvailable {false};
    std::unique_ptr<juce::XmlElement> makePresetState();
    bool restorePresetState(std::unique_ptr<juce::XmlElement>,const juce::File&,bool restoreRun=false);
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Tracksmith Groove"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    RecordingLocation recordingLocation;
    void waitForBackgroundWork() { if (sampleThread.joinable()) sampleThread.join(); if (analysisThread.joinable()) analysisThread.join(); recorder.waitForLoad(); }
    juce::File packFolder, captureFolder, grooveFolder;
    juce::String grooveTitle { "Current groove" };
    bool loadDrumPack (const juce::File&);
    bool loadDefaultDrumKit();
    void useBuiltInKit();
    void setHumanFeel (float amount, float timingMs);
    bool savePreset (const juce::File&);
    bool loadPreset (const juce::File&);
    std::atomic<int> guitarRecordMode { 2 }, guitarCountIn { 1 }, recorderRecordMode { 1 }, recorderCountIn { 1 }, armedTrack { 0 };
    int presetRevision = 0;
    juce::String presetStatus { "Last session restores automatically" };
    std::function<void()> recordingLocationChanged;
    bool setRecordingFolder (const juce::File& folder)
    { if (guitar.isRecording() || ! recordingLocation.set (folder)) return false; if (recordingLocationChanged) recordingLocationChanged(); return true; }
    GuitarEngine guitar;
    Tone3000 tone3000;
    void serviceTone3000();
    MultitrackEngine recorder;
    bool applyGroove (const MidiGroove&);
    std::atomic<bool> previewingGroove{false};
    void stopGroovePreview(){previewingGroove.store(false);parameters.getParameter("run")->setValueNotifyingHost(0);}
    void playGroove();
    bool startMultitrackTake (int track, const juce::File&, bool processed, int preRollBars);
    void serviceRecording();
    void stopRecording();
    void toggleMultitrack();
    void seekMultitrack (double seconds) { recorder.seek (seconds); transportSeek.store (juce::jlimit (0.0, 3599.0, seconds)); }
    void toggleTransport (int page);

    LiveRhythm liveRhythm;
    std::atomic<bool> metronome { false };
    std::atomic<float> detectionStrength { 1.0f };
    void setDetectionStrength (float value) { const float strength = juce::jlimit (0.5f, 3.0f, value); detectionStrength.store (strength); liveRhythm.sensitivity.store (strength); }
    void reanalyseTrack();
    std::atomic<float> clickLevel { 0.18f };
    void setLiveFollow (bool);
    void applyLiveGroove(); // UI thread, once per completed bar
    juce::String liveStatus() const;
    bool startGuitarTake (const juce::File&, bool processed, int preRollBars);
    std::shared_ptr<const WaveformData> getWaveform() const;
    void seekTrack (double seconds) { requestedStart.store (juce::jmax (0.0, seconds)); rewindRequest.fetch_add (1); }

    void setDeviceManager (juce::AudioDeviceManager* manager) { devices = manager; }
    juce::AudioDeviceManager* getDeviceManager() const { return devices; }
    std::atomic<double> requestedSongTempo{0};
    std::atomic<bool> requestedNeural{false};std::atomic<float> referenceInfluence{.3f};
    std::shared_ptr<const std::vector<NeuralPerformanceNote>> neuralPerformance;
    void clearNeuralPerformance(){std::atomic_store(&neuralPerformance,std::shared_ptr<const std::vector<NeuralPerformanceNote>>{});}
    void loadAndAnalyse (const juce::File&, bool generateGroove = true, double sourceOffset = 0, double sourceLength = 180);
    bool isAnalysing() const noexcept { return analysing.load(); }
    juce::String getAnalysisStatus() const;
    void applyAnalysisTempo(); // message thread only
    void setTrackPlaying (bool value);
    bool isTrackPlaying() const { return trackPlaying.load(); }
    void rewindTrack() { requestedStart.store (0); rewindRequest.fetch_add (1); }
    void playTogether();
    void stopTogether();
    void useDefaultGroove();
    bool hasTrack() const { return std::atomic_load (&loadedClip) != nullptr; }
    double trackPositionSeconds() const { return displayedPosition.load(); }
    void setTrackLevel (float value) { trackLevel.store (value); }
    float getTrackLevel() const { return trackLevel.load(); }
    std::shared_ptr<const GrooveAnalysis> getAnalysis() const;
    bool isUsingAnalysis() const { return useAnalysed.load(); }
    std::shared_ptr<const DrumKit> getKit() const { return std::atomic_load (&loadedKit); }
    void setStepVelocity (int drum, int cell, float velocity);
    void setDrumShaping (int drum, DrumShaping settings);
    float getDrumReduction (int drum) const { return drumReduction[static_cast<size_t> (drum)].load(); }
    void setPadGain (int drum, float gain);
    void setPadPan (int drum, float pan);
    void setPadSolo (int drum, bool solo);
    float getPadPeak (int drum) const { return drumPeak[static_cast<size_t> (drum)].load(); }
    void setPadMuted (int drum, bool muted);
    void setMidiNote (int drum, int note);
    void setMidiChannel (int channel);
    void resetMidiMap (bool logic);
    void setAmbience (float room, float overhead);
    void auditionPad (int drum) { pendingAudition.store (drum); }
    void loadSample (int drum, const juce::File& file, int layer = 0);
    void clearSample (int drum, int layer = 0);
    void setLayerGain (int drum, int layer, float gain);
    void setTuning (int drum, float semitones, float cents);
    void setRootNote (int drum, int pitchClass);
    void matchSongKey (int drum, int pitchClass);
    bool setSequence (int bars, int numerator, int denominator, int division);
    juce::File createMidiDragFile();
    bool isLoadingSample() const { return loadingSample.load(); }
    juce::String getKitStatus() const;
    int getPlayingStep() const { return displayedStep.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void trigger (int, float, const DrumKit&);
    void editKit (const std::function<void (DrumKit&)>&);
    juce::String importedTake;
    int recordingDestination = -1;
    bool recordedNam = false;
    double previousRecordingOffset = 0;
    std::atomic<bool> recordingIntoTracks { false };
    std::atomic<double> transportSeek { -1 };
    ClickTrack click;
    std::atomic<bool> resetClick { false };
    int learnedBar = 0;
    double sampleRate = 44100.0, untilNext = 0.0;
    int step = 0;
    bool wasRunning = false;
    double expectedHostPpq=std::numeric_limits<double>::quiet_NaN();
    std::array<LayeredDrumVoice, GrooveGenerator::drumCount> voices;
    DrumAmbience ambience;
    std::array<DrumChannelStrip, GrooveGenerator::drumCount> channelStrips;
    std::array<std::atomic<float>, GrooveGenerator::drumCount> drumReduction {}, drumPeak {};
    std::array<int, GrooveGenerator::drumCount> feelDelay {};
    std::array<float, GrooveGenerator::drumCount> feelVelocity {};
    std::array<int, GrooveGenerator::drumCount> noteOffCountdown {}, activeMidiNotes {}, activeMidiChannels {};
    std::shared_ptr<const DrumKit> loadedKit { std::make_shared<DrumKit>() };
    juce::CriticalSection kitEditLock;
    std::atomic<int> pendingAudition { -1 }, displayedStep { -1 };
    std::thread sampleThread;
    std::atomic<bool> loadingSample { false };
    juce::String kitStatus { "Select a pad and a layer to stack WAV samples." };
    juce::AudioDeviceManager* devices = nullptr;
    int activeDivision = 4, activeStepCount = 64;
    struct Clip { juce::AudioBuffer<float> audio; double rate = 44100; GrooveAnalysis analysis; WaveformData waveform; juce::File sourceFile; };
    std::shared_ptr<const Clip> loadedClip;
    std::thread analysisThread;
    std::atomic<bool> analysing { false }, trackPlaying { false }, tempoPending { false }, useAnalysed { true }, restartGroove { false }, cancelAnalysis { false };
    std::atomic<float> trackLevel { 0.5f };
    std::atomic<unsigned> rewindRequest { 0 };
    unsigned lastRewindRequest = 0;
    double trackCursor = 0;
    const Clip* activeClip = nullptr;
    std::atomic<double> displayedPosition { 0 };
    std::atomic<double> requestedStart { 0 };
    mutable juce::CriticalSection statusLock;
    juce::String analysisStatus { "Load a rhythmic audio file to create a groove." };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvrxGrooveProcessor)
};
}

