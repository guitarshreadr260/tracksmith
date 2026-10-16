#pragma once
#include "TransportSelection.h"
#include <JuceHeader.h>
#include "PlaybackCore.h"
#include "SessionFile.h"
#include "RecordGroup.h"
#include "RecordingAlignment.h"
#include "PluginRack.h"
#include "InputMonitor.h"
#include "IOSettings.h"
#include "CountIn.h"

struct LoadedWav
{
    juce::File file;
    juce::String name, error;
    double rate = 48000.0;
    std::vector<float> left, right;
    juce::Array<juce::var> waveform;
};

inline LoadedWav decodeWav(const juce::File& file, int64_t maximumFrames, const std::function<bool()>& cancelled)
{
        auto result = std::make_unique<LoadedWav>();
        result->file = file;
        result->name = file.getFileName();
        juce::AudioFormatManager formats; formats.registerFormat(new juce::WavAudioFormat(), true);
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (! reader) result->error = "Could not read this WAV file.";
        else if (reader->numChannels < 1 || reader->numChannels > 2 || !std::isfinite(reader->sampleRate) || reader->sampleRate < 8000.0 || reader->sampleRate > 192000.0 || reader->lengthInSamples <= 0)
            result->error = "Use a mono or stereo WAV between 8 and 192 kHz.";
        else if (reader->lengthInSamples > maximumFrames)
            result->error = "Import exceeds the 128 MiB per-file or 512 MiB session limit. Use a shorter WAV or clear another track.";
        else
        {
            result->rate = reader->sampleRate;
            const int length = static_cast<int>(reader->lengthInSamples);
            result->left.resize(static_cast<size_t>(length)); result->right.resize(static_cast<size_t>(length));
            for (int offset = 0; offset < length && ! cancelled(); offset += 65536)
            {
                const int count = std::min(65536, length - offset);
                float* destinations[] = { result->left.data() + offset, result->right.data() + offset };
                if (! reader->read(destinations, 2, offset, count)) { result->error = "WAV decoding failed."; break; }
                if (reader->numChannels == 1) std::copy_n(destinations[0], count, destinations[1]);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < count; ++i)
                        destinations[ch][i] = std::isfinite(destinations[ch][i]) ? juce::jlimit(-1.0f, 1.0f, destinations[ch][i]) : 0.0f;
            }
            // Versioned per-channel envelope. Negative header cannot be confused with peaks.
            const int channels=reader->numChannels==1?1:2,bins=std::min(length,8192);
            result->waveform.add(-2);result->waveform.add(-channels);
            for(int bin=0;bin<bins&&!cancelled();++bin){
                const int start=static_cast<int>(static_cast<int64_t>(length)*bin/bins),end=static_cast<int>(static_cast<int64_t>(length)*(bin+1)/bins);
                for(int channel=0;channel<channels;++channel){float low=0,high=0;const auto& data=channel?result->right:result->left;
                    for(int i=start;i<end;++i){low=std::min(low,data[i]);high=std::max(high,data[i]);}result->waveform.add(low);result->waveform.add(high);
                }
            }
        }
        return std::move(*result);
}

// File decoding, validation and waveform extraction happen on a worker thread.
class WavLoader final : private juce::Thread, private juce::AsyncUpdater
{
public:
    std::function<void(LoadedWav)> onLoaded;
    WavLoader() : Thread("WAV import") {}
    ~WavLoader() override { signalThreadShouldExit(); stopThread(-1); cancelPendingUpdate(); }
    bool load(const juce::File& file, int64_t frameBudget)
    {
        if (isThreadRunning()) return false;
        cancelPendingUpdate(); pending.reset(); requestedFile = file; maximumFrames = std::min<int64_t>(16 * 1024 * 1024, frameBudget);
        return startThread();
    }
private:
    void run() override
    {
        auto result = std::make_unique<LoadedWav>();
        try { *result = decodeWav(requestedFile, maximumFrames, [this] { return threadShouldExit(); }); }
        catch (const std::exception&) { result->error = "Not enough memory to import this WAV."; }
        if (threadShouldExit()) return;
        { const juce::ScopedLock lock(resultLock); pending = std::move(result); }
        triggerAsyncUpdate();
    }
    void handleAsyncUpdate() override
    {
        std::unique_ptr<LoadedWav> result;
        { const juce::ScopedLock lock(resultLock); result = std::move(pending); }
        // Completion may be dispatched just before run() returns; join before a queued next import.
        waitForThreadToExit(1000);
        if (result && onLoaded) onLoaded(std::move(*result));
    }
    juce::File requestedFile;
    int64_t maximumFrames = 16 * 1024 * 1024;
    juce::CriticalSection resultLock; // Worker/message thread only; never audio.
    std::unique_ptr<LoadedWav> pending;
};

struct SessionJobResult
{
    juce::File file;
    SavedSession session;
    std::array<std::unique_ptr<LoadedWav>, 128> audio;
    std::array<std::vector<std::unique_ptr<LoadedWav>>, 128> extraAudio;
    juce::String error;
    juce::StringArray missing;
    bool saving = false;
};
class SessionWorker final : private juce::Thread, private juce::AsyncUpdater
{
public:
    std::function<void(SessionJobResult)> onFinished;
    SessionWorker() : Thread("Session files") {}
    ~SessionWorker() override { signalThreadShouldExit(); stopThread(-1); cancelPendingUpdate(); }
    bool isBusy()const{return busy.load();}
    bool open(const juce::File& file) { return begin(file, false, {}); }
    bool save(const juce::File& file, juce::var document) { return begin(file, true, std::move(document)); }
private:
    bool begin(const juce::File& file, bool write, juce::var document)
    {
        if (busy.exchange(true)) return false;
        cancelPendingUpdate(); pending.reset(); requested = file; saving = write; json = std::move(document);
        if(startThread())return true;busy=false;return false;
    }
    void run() override
    {
        auto result = std::make_unique<SessionJobResult>(); result->file = requested; result->saving = saving;
        try
        {
            if (saving)
            {
                const auto written = SessionFile::write(requested, json);
                if (written.failed()) result->error = written.getErrorMessage();
            }
            else
            {
                const auto read = SessionFile::read(requested, result->session);
                if (read.failed()) result->error = read.getErrorMessage();
                int64_t frames = 0;
                for (size_t id = 0; id < result->session.tracks.size() && result->error.isEmpty() && !threadShouldExit(); ++id)
                {
                    const auto& track=result->session.tracks[id];
                    const auto files=track.sources.empty()?std::vector<juce::File>{track.audioFile}:track.sources;
                    result->extraAudio[id].resize(files.size()-1);
                    for(size_t source=0;source<files.size() && result->error.isEmpty();++source)
                    {
                        const auto& file=files[source];if(file==juce::File())continue;
                        if(!file.existsAsFile()){result->missing.add(track.name+": "+file.getFileName());continue;}
                        auto wav=std::make_unique<LoadedWav>(decodeWav(file,std::min<int64_t>(16*1024*1024,64*1024*1024-frames),[this]{return threadShouldExit();}));
                        if(wav->error.isNotEmpty()){result->error=file.getFileName()+": "+wav->error;break;}
                        frames+=static_cast<int64_t>(wav->left.size());
                        for(const auto& clip:track.clips)if(clip.source==static_cast<int>(source) && clip.offset+clip.length>static_cast<double>(wav->left.size())/wav->rate+1.e-6){result->error="Clip exceeds source WAV: "+file.getFileName();break;}
                        for(const auto& playlist:track.playlists)for(const auto& clip:playlist.clips)if(clip.source==static_cast<int>(source)&&clip.offset+clip.length>static_cast<double>(wav->left.size())/wav->rate+1.e-6)result->error="Playlist clip exceeds source WAV: "+file.getFileName();
                        if(source==0)result->audio[id]=std::move(wav);else result->extraAudio[id][source-1]=std::move(wav);
                    }
                }
            }
        }
        catch (const std::exception&) { result->error = "Session operation failed: not enough memory or an unreadable file."; }
        if (threadShouldExit()) return;
        { const juce::ScopedLock lock(resultLock); pending = std::move(result); }
        triggerAsyncUpdate();
    }
    void handleAsyncUpdate() override
    {
        std::unique_ptr<SessionJobResult> result;
        { const juce::ScopedLock lock(resultLock); result = std::move(pending); }
        busy=false;
        if (result && onFinished) onFinished(std::move(*result));
    }
    juce::File requested;
    juce::var json;
    bool saving = false;std::atomic<bool> busy{false};
    juce::CriticalSection resultLock;
    std::unique_ptr<SessionJobResult> pending;
};

class AudioEngine final : private juce::AudioIODeviceCallback
{
public:
    PlaybackCore playback;
    CountIn countIn;
    std::atomic<bool> dynamicTransport {true}, punchEnabled {false};
    std::atomic<double> playStart {0}, selectionStart {0}, selectionEnd {0};
    double takeStart=0; bool recordPlanActive=false; int planTrack=-1;
    std::atomic<int64_t> planPosition{0};int64_t punchBegin=0,punchEnd=0;
    std::atomic<bool> alignRecording{true},stopAfterLatency{false};
    std::atomic<int> recordingCorrection{0};
    std::atomic<bool> logicalRecordGate{false};
    bool takeWasPunch=false; double takeEnd=0; bool continueAfterPunch=false;
    std::atomic<int> preRollBars { 0 }, countInBeats { 0 };
    bool countInCancelled = false;
    RecordGroup recorder;
    std::vector<RecordRoute> recordingRoutes;
    std::array<std::atomic<int>,128> trackInputs{},trackInputChannels{};
    std::atomic<bool> monitorArmed{false};std::array<std::atomic<bool>,128> armedMonitorMask{};
    std::array<std::atomic<int>,128> monitorOverrides{}; // 0 auto, 1 explicit on, -1 explicit off
    std::array<std::atomic<float>,128> monitorLevels{};
    bool wantsMonitor(size_t id)const noexcept {const int choice=monitorOverrides[id].load();return choice>0||(choice==0&&(((monitorArmed.load()||PluginInput::encoded(trackInputs[id].load()))&&armedMonitorMask[id].load())||(monitor.enabled.load()&&monitor.track.load()==(int)id)));}
    void clearTrackMonitoring(){for(auto& choice:monitorOverrides)choice=0;for(auto& gain:monitorLevels)gain=1;}
    std::array<std::array<const float*,2>,128> liveTrackSources{};
    std::array<int,128> liveTrackChannels{};
    std::array<float,128> liveTrackSmooth{};
    const float* const* captureInputs=nullptr;int captureInputCount=0;bool captureThisBlock=false;
    float monitorInputSmooth=0;
    std::array<const float*,2> liveSources {};int liveChannels=1;
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeakLeft {0},outputPeakRight {0};
    std::atomic<int> inputMeterChannels { 1 };
    std::atomic<int> inputSource { 0 };
    std::array<std::atomic<float>, InputRouting::maximumInputs> inputChannelPeaks {};
    std::atomic<int> mainOutputPair {0};
    std::atomic<int> deviceOutputCount {0};
    juce::String sessionDeviceKey;
    std::atomic<bool> ioDeviceMatches {true};
    InputMonitor monitor;
    std::atomic<float> monitorPeak { 0.0f };
    std::atomic<bool> monitorActive { false };
    juce::AudioDeviceManager devices;
    juce::AudioDeviceManager::LevelMeter::Ptr deviceOutputMeter = devices.getOutputLevelGetter();
    std::atomic<uint64_t> callbackCount { 0 };
    std::atomic<int> callbackOutputs { 0 }, writableOutputs { 0 };
    juce::String deviceError() const { const juce::ScopedLock lock(deviceErrorLock); return lastDeviceError; }
    PluginRack plugins;
    void detach() { devices.removeAudioCallback(this); }
    void attach()
    {
        for(size_t i=0;i<128;++i){playback.pluginInputTracks[i]=PluginInput::encoded(trackInputs[i].load());plugins.pluginInputSources[i]=playback.pluginInputTracks[i]?PluginInput::track(trackInputs[i].load()):-1;}
        plugins.rebuildCompensation(playback); playback.playbackLatencyFrames = plugins.totalLatency;
        devices.addAudioCallback(this);
    }
    void resetPluginGraph() { detach(); plugins.resetGraph(); attach(); }
    void refreshPluginLatency()
    {
        if (!plugins.latencyChanged()) return;
        const auto position = playback.seconds.load();
        detach(); plugins.rebuildCompensation(playback); plugins.resetGraph(); playback.seekSeconds = position; attach();
    }
    juce::String initialise()
    {
        playback.process = PluginRack::process; playback.processContext = &plugins;
        const auto settings = deviceSettingsFile();
        auto state = settings.existsAsFile() && settings.getSize() < 65536 ? juce::parseXML(settings) : nullptr;
        auto error = devices.initialise(0, 2, state.get(), true);
        attach(); return error;
    }
    static juce::File deviceSettingsFile()
    { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Tracksmith/audio-device.xml"); }
    juce::Result ensureMonitorInputs(int first,int width){
        if(PluginInput::encoded(first))return plugins.inputAvailable(first,width)?juce::Result::ok():juce::Result::fail("The assigned instrument output is unavailable.");
        auto* device=devices.getCurrentAudioDevice();if(!device)return juce::Result::fail("Choose an audio input device in Audio settings.");
        if(!ioDeviceMatches.load())return juce::Result::fail("Review the interface change in Setup > I/O before monitoring.");
        if(first<0||first+width>InputRouting::maximumInputs||width<1||width>2)return juce::Result::fail("Choose a valid input pair.");
        const auto names=device->getInputChannelNames();const auto active=device->getActiveInputChannels();
        if(first+width>names.size())return juce::Result::fail("The assigned physical input is not present on this device.");
        if(InputRouting::available(first,width,names.size(),[&](int ch){return active[ch];}))return juce::Result::ok();
        if(playback.recording.load())return juce::Result::fail("Stop recording before enabling additional input channels.");
        auto setup=devices.getAudioDeviceSetup();setup.useDefaultInputChannels=false;
        // Enable exactly the assigned physical channel(s); other assignments do not move.
        for(int physical=first;physical<first+width;++physical)setup.inputChannels.setBit(physical);
        const auto error=devices.setAudioDeviceSetup(setup,true);return error.isEmpty()?saveDeviceSettings():juce::Result::fail(error);
    }
    juce::Result saveDeviceSettings()
    {
        auto state = devices.createStateXml(); if (!state) return juce::Result::fail("Audio settings could not be saved.");
        const auto file = deviceSettingsFile(); const auto made = file.getParentDirectory().createDirectory(); if (made.failed()) return made;
        juce::TemporaryFile temporary(file);
        return temporary.getFile().replaceWithText(state->toString()) && temporary.overwriteTargetFileWithTemporary()
            ? juce::Result::ok() : juce::Result::fail("Audio settings could not be saved. Check the Tracksmith application-data folder.");
    }
    AudioEngine(){for(auto& channels:trackInputChannels)channels=1;playback.process=PluginRack::process;playback.processContext=&plugins;}
    ~AudioEngine() override { devices.removeAudioCallback(this); devices.closeAudioDevice(); }
    juce::Result startRecording(size_t id, const juce::File& file, int channels)
    {
        auto* device = devices.getCurrentAudioDevice();
        if (device == nullptr || !device->isPlaying()) return juce::Result::fail("Choose an active input/output device in Audio settings.");
        if (!ioDeviceMatches.load()) return juce::Result::fail("Review the interface change in Setup > I/O before recording.");
        const auto activeInputs=device->getActiveInputChannels();const int physicalInputs=device->getInputChannelNames().size();
        if (recordingRoutes.empty() && !InputRouting::available(inputSource.load(),channels,physicalInputs,[&](int ch){return activeInputs[ch];})) return juce::Result::fail("The selected recording input is unavailable. Choose an enabled physical input in Setup > I/O.");
        for(const auto& route:recordingRoutes)
            if(!(PluginInput::encoded(route.input)?plugins.inputAvailable(route.input,route.channels):InputRouting::available(route.input,route.channels,physicalInputs,[&](int ch){return activeInputs[ch];})))return juce::Result::fail("An armed track has an unavailable input or stereo pair.");
        int pluginSource=-1;bool hardware=false;for(const auto& route:recordingRoutes){if(PluginInput::encoded(route.input)){if(pluginSource>=0&&pluginSource!=PluginInput::track(route.input))return juce::Result::fail("Record outputs from one instrument at a time.");pluginSource=PluginInput::track(route.input);}else hardware=true;}if(pluginSource>=0&&hardware)return juce::Result::fail("Record plugin outputs and hardware inputs in separate passes.");
        const auto rate = device->getCurrentSampleRate();
        const auto selectedStart=selectionStart.load(),selectedEnd=selectionEnd.load();
        const auto pendingSeek=playback.seekSeconds.load();const auto insertion=pendingSeek>=0?pendingSeek:playback.seconds.load();
        const auto plan=TransportSelection::plan(dynamicTransport.load(),punchEnabled.load(),playStart.load(),selectedStart,selectedEnd,insertion,rate);
        if(plan.error)return juce::Result::fail(plan.error);
        const bool punching=plan.punch;
        if(!monitor.enabled.load())inputMeterChannels = channels;
        const auto limit = static_cast<uint64_t>(std::max<int64_t>(0,std::min<int64_t>(16 * 1024 * 1024, importBudget(id,true)/static_cast<int64_t>(std::max<size_t>(1,recordingRoutes.size())))));
        devices.removeAudioCallback(this);
        const auto started = recordingRoutes.empty()?recorder.start(file, rate, channels, limit):recorder.startGroup(recordingRoutes,rate,limit);
        playback.recordTracks.fill(false);for(const auto& route:recordingRoutes)playback.recordTracks[route.track]=true;
        if (started.wasOk()) { playback.reset(); plugins.resetGraph(); playback.recordTrack = static_cast<int>(id); playback.recording = true; countInCancelled = false;
            playback.suppressRecordedPlayback=true;continueAfterPunch=false;takeWasPunch=punching;takeStart=plan.start;takeEnd=plan.end;logicalRecordGate=false;
            planTrack=static_cast<int>(id);planPosition=0;recordPlanActive=true;
            const auto origin=plan.origin;
            punchBegin=punching?static_cast<int64_t>(std::llround((selectedStart-origin)*rate)):0;
            punchEnd=punching?static_cast<int64_t>(std::llround((selectedEnd-origin)*rate)):std::numeric_limits<int64_t>::max();
            plugins.rebuildCompensation(playback);
            recordingCorrection=RecordingAlignment::correction(device->getInputLatencyInSamples(),device->getOutputLatencyInSamples(),plugins.totalLatency,alignRecording.load());if(pluginSource>=0)recordingCorrection=alignRecording.load()?std::max(0,plugins.channels[pluginSource][0]->instance->getLatencySamples()):0;stopAfterLatency=false;
            playback.seekSeconds=origin;
            countIn.start(preRollBars.load(), rate, playback.bpm.load()); countInBeats = countIn.beatsLeft(); playback.playing = countIn.total == 0; }
        attach(); return started;
    }
    void stopRecording(bool cancelled = false, bool keepPlaying = false, bool drain = false)
    {
        if(drain&&recorder.isActive()&&countInBeats.load()==0){stopAfterLatency=true;return;}
        stopAfterLatency=false;
        const bool wasPlaying=playback.playing.load();
        recorder.requestStop(); // An intentional stop must not be reported as an input-device failure.
        devices.removeAudioCallback(this);
        if(keepPlaying)playback.playing=wasPlaying;
        if (cancelled && (countIn.remaining(1) > 0 || (recordPlanActive && planPosition<=punchBegin))) countInCancelled = true;
        continueAfterPunch=keepPlaying && playback.playing.load();
        countIn.total = countIn.position = 0; countInBeats = 0; recordPlanActive=false; recorder.requestStop();
        playback.recording = false; playback.recordTrack = -1;
        if(!keepPlaying){playback.playing=false;playback.seekSeconds=dynamicTransport.load()?playStart.load():0.0;}
        if(!keepPlaying)plugins.resetGraph(); attach();
    }
    int64_t importBudget(size_t replacingTrack, bool append = false) const
    {
        int64_t frames = 0;
        for (size_t id = 0; id < playback.tracks.size(); ++id)
            if (append || id != replacingTrack) frames += playback.tracks[id].residentFrames();
        return std::max<int64_t>(0, 64 * 1024 * 1024 - frames);
    }
    void install(size_t id, LoadedWav& wav, bool append = false, double insertion = -1, bool punch = false, double end = 0, bool keepPlaying = false)
    {
        devices.removeAudioCallback(this); // Waits for in-flight callback to finish.
        const auto playing=keepPlaying && playback.playing.load();const auto position=playback.seconds.load();
        auto& track = playback.tracks[id];
        const double start=insertion>=0?insertion:append?track.duration():0;
        const double length=static_cast<double>(wav.left.size())/wav.rate;
        int source=0;
        if(append) {track.extra.push_back({std::move(wav.left),std::move(wav.right),wav.rate});source=static_cast<int>(track.extra.size());}
        else {track.left=std::move(wav.left);track.right=std::move(wav.right);track.sourceRate=wav.rate;track.extra.clear();track.clips.clear();}
        track.timelineEnabled=true;int next=1;for(const auto& c:track.clips)next=std::max(next,c.id+1);
        if(punch)
        {
            const double stop=std::min(start+length,end>start?end:start+length);
            ClipLayout::replaceRange(track.clips,start,stop,{});
        }
        next=1;for(const auto& c:track.clips)next=std::max(next,c.id+1);
        track.clips.push_back({next,source,start,0,length}); playback.refreshDuration(); playback.reset(); plugins.resetGraph();
        if(keepPlaying){playback.seekSeconds=position;playback.playing=playing;}
        attach();
    }
    void clear(size_t id)
    {
        devices.removeAudioCallback(this);
        playback.tracks[id].extra.clear();playback.tracks[id].clips.clear();playback.tracks[id].timelineEnabled=true;
        std::vector<float>().swap(playback.tracks[id].left);
        std::vector<float>().swap(playback.tracks[id].right);
        playback.refreshDuration(); playback.reset(); plugins.resetGraph();
        attach();
    }
    void installSession(SessionJobResult& session)
    {
        devices.removeAudioCallback(this);
        mainOutputPair=session.session.mainOutputPair;sessionDeviceKey=session.session.ioDeviceKey;
        clearTrackMonitoring();monitor.enabled = false; monitor.track = 0; monitorPeak = 0.0f;
        for (size_t id = 0; id < playback.tracks.size(); ++id)
        {
            auto& track = playback.tracks[id];
            if (session.audio[id])
            {
                track.left = std::move(session.audio[id]->left); track.right = std::move(session.audio[id]->right);
                track.sourceRate = session.audio[id]->rate;
            }
            else { std::vector<float>().swap(track.left); std::vector<float>().swap(track.right); }
            track.volumeAutomation.clear();track.panAutomation.clear();track.automationMode=0;track.stereoLeft=-1;track.stereoRight=1;
            track.routing={};for(auto& level:track.sendLevels)level=0;track.extra.clear();track.clips.clear();track.timelineEnabled=true;track.midiTrack=false;track.midiSeconds=0;plugins.midiLanes[id].enabled=false;plugins.midiLanes[id].notes.clear();plugins.midiLanes[id].reset();
            for(auto& audio:session.extraAudio[id])
            { PlaybackCore::Track::Source source;if(audio){source.left=std::move(audio->left);source.right=std::move(audio->right);source.rate=audio->rate;}track.extra.push_back(std::move(source)); }
            if (id < session.session.tracks.size())
            {
                const auto& saved = session.session.tracks[id];
                playback.setRouting(id,saved.routing);track.midiTrack=saved.midiTrack;plugins.midiLanes[id].enabled=saved.midiTrack;plugins.midiLanes[id].notes=saved.notes;if(saved.midiTrack)track.midiSeconds=plugins.midiLanes[id].endBeat()*60/session.session.bpm+1;
                track.stereoLeft=saved.stereoLeft;track.stereoRight=saved.stereoRight;track.clips=saved.clips;if(!saved.timelineEnabled && !track.left.empty())track.clips.push_back({1,0,0,0,track.sourceDuration(0)});
                track.volumeAutomation=saved.volumeAutomation;track.panAutomation=saved.panAutomation;track.automationMode=saved.automationMode;
                track.gain = saved.gain; track.pan = saved.pan; track.muted = saved.muted; track.solo = saved.solo;
            }
            else { track.gain = 0.75f; track.pan = 0.0f; track.muted = false; track.solo = false; }
        }
        playback.bpm = session.session.bpm; playback.loop = session.session.loop;
        playback.masterGain = session.session.master; playback.metronome = session.session.metronome;
        playback.metronomeGain = session.session.metronomeGain; preRollBars = session.session.preRollBars;
        playback.refreshDuration(); playback.reset(); plugins.resetGraph(); attach();
    }
    // Also used by offline integration tests; caller owns the audio thread.
    void renderAudio(const float* const* inputs, int inputChannels, float* const* outputs, int channels, int count)
    {
        // JUCE indexes these arrays by physical device channel, including null holes.
        // Never compact them: saved routes and meters share the physical indices.
        const int mainPair=mainOutputPair.load();const bool matched=ioDeviceMatches.load();
        std::array<const float*,InputRouting::maximumInputs> inputView{};
        std::array<float*,InputRouting::maximumOutputs> outputView{};
        for(int ch=0;ch<channels;++ch)if(outputs[ch])std::fill_n(outputs[ch],count,0.f);
        for(int ch=0;ch<std::min(inputChannels,InputRouting::maximumInputs);++ch)inputView[ch]=matched?inputs[ch]:nullptr;
        for(int ch=0;ch<std::min(channels,InputRouting::maximumOutputs);++ch)outputView[ch]=matched?outputs[ch]:nullptr;
        if(mainPair==0&&!(channels==1&&deviceOutputCount.load()<=1)&&
            (channels<2||!outputView[0]||!outputView[1]))outputView[0]=outputView[1]=nullptr;
        if(mainPair!=0){
            outputView[0]=outputView[1]=nullptr;
            if(mainPair>0&&mainPair<8){const int left=mainPair*2,right=left+1;
                if(right<channels&&matched&&outputs[left]&&outputs[right]){outputView[0]=outputs[left];outputView[1]=outputs[right];}
                outputView[left]=outputView[right]=nullptr; // Reserve the main pair from aux writes.
            }
        }
        inputs=inputView.data();inputChannels=std::min(inputChannels,InputRouting::maximumInputs);
        outputs=outputView.data();channels=std::min(channels,InputRouting::maximumOutputs);
        for(int channel=0;channel<channels;++channel)if(outputs[channel])std::fill_n(outputs[channel],count,0.f);
        const int prefix = countIn.remaining(count);
        if (prefix > 0)
        {
            renderNormal(inputs, inputChannels, outputs, channels, prefix, false, true);
            countInBeats = countIn.beatsLeft();
            if (countIn.remaining(1) == 0) playback.playing = true;
            if (prefix == count) return;
            // Fixed stack views preserve physical channel indices across count-in and punch boundaries.
            std::array<const float*, InputRouting::maximumInputs> tailInput {}; std::array<float*, 16> tailOutput {};
            for (int ch = 0; ch < std::min(inputChannels, InputRouting::maximumInputs); ++ch) tailInput[ch] = inputs[ch] ? inputs[ch]+prefix : nullptr;
            for (int ch = 0; ch < std::min(channels, 16); ++ch) tailOutput[ch] = outputs[ch] ? outputs[ch]+prefix : nullptr;
            renderTimeline(tailInput.data(), std::min(inputChannels,InputRouting::maximumInputs), tailOutput.data(), std::min(channels,16), count-prefix);
            return;
        }
        renderTimeline(inputs, inputChannels, outputs, channels, count);
    }
    void renderTimeline(const float* const* inputs,int inputChannels,float* const* outputs,int channels,int count)
    {
        if(!recordPlanActive){renderNormal(inputs,inputChannels,outputs,channels,count,true);return;}
        int offset=0;
        if(stopAfterLatency.exchange(false)){
            if(planPosition.load()<punchBegin){countInCancelled=true;recorder.requestStop();recordPlanActive=false;playback.recording=false;playback.recordTrack=-1;playback.playing=false;renderNormal(inputs,inputChannels,outputs,channels,count,false);return;}
            punchEnd=planPosition.load();takeWasPunch=false;
        }
        const int delay=recordingCorrection.load();
        const auto captureBegin=RecordingAlignment::delayed(punchBegin,delay),captureEnd=RecordingAlignment::delayed(punchEnd,delay);
        const auto finishCapture=[&]{recorder.requestStop();recordPlanActive=false;logicalRecordGate=false;playback.recording=false;playback.recordTrack=-1;if(!takeWasPunch)playback.playing=false;};
        while(offset<count)
        {
            const auto position=planPosition.load();
            // Zero-latency Stop can land exactly on the boundary before any frames remain.
            if(position>=captureEnd){finishCapture();break;}
            const bool capture=position>=captureBegin&&position<captureEnd;
            logicalRecordGate=position>=punchBegin&&position<punchEnd;
            int64_t boundary=captureEnd;for(const auto edge:{punchBegin,punchEnd,captureBegin,captureEnd})if(edge>position)boundary=std::min(boundary,edge);
            const int frames=static_cast<int>(std::min<int64_t>(count-offset,boundary-position));if(frames<=0)break;
            std::array<const float*,InputRouting::maximumInputs> in {};std::array<float*,16> out {};
            for(int ch=0;ch<std::min(inputChannels,InputRouting::maximumInputs);++ch)in[ch]=inputs[ch]?inputs[ch]+offset:nullptr;
            for(int ch=0;ch<std::min(channels,16);++ch)out[ch]=outputs[ch]?outputs[ch]+offset:nullptr;
            playback.recordTrack=logicalRecordGate?planTrack:-1;
            renderNormal(in.data(),std::min(inputChannels,InputRouting::maximumInputs),out.data(),std::min(channels,16),frames,capture);
            offset+=frames;planPosition+=frames;
            if(planPosition.load()>=captureEnd){finishCapture();break;}
        }
        if(offset<count)
        {
            std::array<const float*,InputRouting::maximumInputs> in {};std::array<float*,16> out {};
            for(int ch=0;ch<std::min(inputChannels,InputRouting::maximumInputs);++ch)in[ch]=inputs[ch]?inputs[ch]+offset:nullptr;
            for(int ch=0;ch<std::min(channels,16);++ch)out[ch]=outputs[ch]?outputs[ch]+offset:nullptr;
            renderNormal(in.data(),std::min(inputChannels,InputRouting::maximumInputs),out.data(),std::min(channels,16),count-offset,false);
        }
    }
    void renderNormal(const float* const* inputs, int inputChannels, float* const* outputs, int channels, int count, bool capture, bool countClick = false)
    {
        callbackCount.fetch_add(1, std::memory_order_relaxed); callbackOutputs = channels;
        int writable = 0; for (int channel = 0; channel < channels; ++channel) if (outputs[channel]) ++writable;
        writableOutputs = writable;
        const int captureChannels = inputMeterChannels.load(std::memory_order_relaxed);
        const int first = inputSource.load(std::memory_order_relaxed);
        const auto selected = InputRouting::select(inputs, inputChannels, first, captureChannels);
        float peak = 0.0f;
        for (int channel = 0; channel < InputRouting::maximumInputs; ++channel)
        {
            const auto source = InputRouting::select(inputs, inputChannels, channel, 1);
            float channelPeak = 0;
            for (int frame = 0; frame < count; ++frame) channelPeak = std::max(channelPeak, std::abs(InputRouting::sample(source[0], frame)));
            inputChannelPeaks[static_cast<size_t>(channel)].store(channelPeak, std::memory_order_relaxed);
        }
        for (int channel = 0; channel < captureChannels; ++channel)
            for (int frame = 0; frame < count; ++frame) peak = std::max(peak, std::abs(InputRouting::sample(selected[static_cast<size_t>(channel)], frame)));
        inputPeak.store(peak, std::memory_order_relaxed);
        if(capture&&recordingRoutes.empty())recorder.capture(inputs,inputChannels,count,first);captureInputs=inputs;captureInputCount=inputChannels;captureThisBlock=capture&&!recordingRoutes.empty();
        playback.sourceStageContext=this;playback.sourceStage=[](void* context,int frames,int offset){auto& e=*static_cast<AudioEngine*>(context);if(e.captureThisBlock&&e.playback.offlineSourceEnd<0&&e.recorder.isActive())e.recorder.captureResolved(&e,[](void* ctx,const RecordRoute& route,int off){auto& engine=*static_cast<AudioEngine*>(ctx);if(PluginInput::encoded(route.input))return engine.plugins.inputPointers(route.input,route.channels);auto pointers=InputRouting::select(engine.captureInputs,engine.captureInputCount,route.input,route.channels);for(auto& pointer:pointers)if(pointer)pointer+=off;return pointers;},frames,offset);};
        const int liveTrack = monitor.track.load(std::memory_order_relaxed);
        const bool live = monitor.enabled.load(std::memory_order_relaxed)
            && (!(punchEnabled.load()||takeWasPunch) || !playback.playing.load() || ((recordPlanActive?logicalRecordGate.load():capture) && recorder.isActive())) && liveTrack >= 0 && liveTrack < static_cast<int>(PlaybackCore::maxTracks)
            && selected[0] && (captureChannels == 1 || selected[1]);
        monitorActive=live;liveSources=selected;liveChannels=captureChannels;
        playback.inputMonitorTrack=-1;playback.deferMasterProcessing=false;plugins.monitorChannel=-1;
        playback.liveContext=this;playback.liveInput=[](void* context,float* left,float* right,int frames,int offset)
        {
            auto& e=*static_cast<AudioEngine*>(context);const float smooth=static_cast<float>(1-std::exp(-1.0/(e.playback.deviceRate*.005)));
            for(int n=0;n<frames;++n){e.monitorInputSmooth+=smooth*(e.monitor.gain.load()-e.monitorInputSmooth);left[n]+=InputRouting::sample(e.liveSources[0],offset+n)*e.monitorInputSmooth;right[n]+=InputRouting::sample(e.liveSources[e.liveChannels==2?1:0],offset+n)*e.monitorInputSmooth;}
        };
        for(size_t id=0;id<PlaybackCore::maxTracks;++id){
            const bool plugin=PluginInput::encoded(trackInputs[id].load());const bool enabled=wantsMonitor(id)||(plugin&&armedMonitorMask[id].load()&&monitorOverrides[id].load()>=0);
            const bool gate=!(punchEnabled.load()||takeWasPunch)||!playback.playing.load()||((recordPlanActive?logicalRecordGate.load():capture)&&recorder.isActive());
            const bool legacy=live&&static_cast<int>(id)==liveTrack&&monitorOverrides[id].load()==0;const int width=legacy?captureChannels:trackInputChannels[id].load();const auto source=legacy?selected:InputRouting::select(inputs,inputChannels,trackInputs[id].load(),width);
            const bool validInput=plugin?plugins.inputAvailable(trackInputs[id].load(),width):source[0]&&(width==1||source[1]);
            const bool active=enabled&&gate&&!playback.tracks[id].midiTrack&&!playback.tracks[id].routing.aux&&validInput;
            playback.pluginInputTracks[id]=plugin;playback.inputMonitorTracks[id]=active;plugins.monitorChannels[id]=active&&!plugin;liveTrackSources[id]=source;liveTrackChannels[id]=width;if(!active)liveTrackSmooth[id]=0;
        }
        playback.trackLiveInput=[](void* context,size_t id,float* left,float* right,int frames,int offset){auto& e=*static_cast<AudioEngine*>(context);const bool plugin=PluginInput::encoded(e.trackInputs[id].load());const auto source=plugin?e.plugins.inputPointers(e.trackInputs[id].load(),e.trackInputChannels[id].load()):e.liveTrackSources[id];const int at=plugin?0:offset;const float smooth=static_cast<float>(1-std::exp(-1.0/(e.playback.deviceRate*.005)));for(int n=0;n<frames;++n){e.liveTrackSmooth[id]+=smooth*((e.monitorOverrides[id].load()>0?e.monitorLevels[id].load():e.monitor.gain.load())-e.liveTrackSmooth[id]);left[n]+=InputRouting::sample(source[0],at+n)*e.liveTrackSmooth[id];right[n]+=InputRouting::sample(source[e.liveTrackChannels[id]==2?1:0],at+n)*e.liveTrackSmooth[id];}};
        playback.clickContext=this;playback.extraClick=countClick?+[](void* context)->float{auto& e=*static_cast<AudioEngine*>(context);return e.countIn.next()*e.playback.metronomeGain.load();}:nullptr;
        if(!live)monitorInputSmooth=0;
        playback.render(outputs,channels,count);
        bool anyLive=false;for(bool active:playback.inputMonitorTracks)anyLive=anyLive||active;monitorActive=anyLive;
        float livePeak=0;for(size_t id=0;id<playback.tracks.size();++id)if(playback.inputMonitorTracks[id])livePeak=std::max(livePeak,playback.tracks[id].peak.load());monitorPeak=livePeak;
    }

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputs, int inputChannels, float* const* outputs, int channels, int count, const juce::AudioIODeviceCallbackContext&) override
    { renderAudio(inputs, inputChannels, outputs, channels, count);
        float l=0,r=0;for(int n=0;n<count;++n){const int pair=mainOutputPair.load(),left=pair*2,right=left+1;if(pair>=0&&left<channels&&outputs[left])l=std::max(l,std::abs(outputs[left][n]));if(pair>=0&&right<channels&&outputs[right])r=std::max(r,std::abs(outputs[right][n]));}
        outputPeakLeft=l;outputPeakRight=r;
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        { const juce::ScopedLock lock(deviceErrorLock); lastDeviceError.clear(); }
        playback.deviceRate = device->getCurrentSampleRate();
        deviceOutputCount=device->getOutputChannelNames().size();
        ioDeviceMatches=sessionDeviceKey.isEmpty()||sessionDeviceKey==IOSettings::deviceKey(devices);
        monitor.reset();liveTrackSmooth.fill(0);
        if (plugins.clock.rate.load() != playback.deviceRate) plugins.prepare(playback.deviceRate);
        plugins.rebuildCompensation(playback); playback.playbackLatencyFrames = plugins.totalLatency;
    }
    void audioDeviceStopped() override { playback.playing.store(false, std::memory_order_relaxed); recorder.inputStopped(); outputPeakLeft=0;outputPeakRight=0; inputPeak = 0.0f; for (auto& peak : inputChannelPeaks) peak = 0.0f; }
    void audioDeviceError(const juce::String& message) override
    {
        { const juce::ScopedLock lock(deviceErrorLock); lastDeviceError = message; }
        playback.playing = false; recorder.inputStopped(); outputPeakLeft=0;outputPeakRight=0; inputPeak = 0.0f; for (auto& peak : inputChannelPeaks) peak = 0.0f;
    }
    mutable juce::CriticalSection deviceErrorLock;
    juce::String lastDeviceError;
};

