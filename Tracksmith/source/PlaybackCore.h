#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>
#include <limits>
#include "ClipLayout.h"
#include "MixerRouting.h"
#include "Automation.h"
#include <memory>

// Samples and duration change only while the device callback is detached.
// The callback owns the shared clock; all UI controls use lock-free atomics.
class PlaybackCore
{
public:
    static constexpr size_t maxTracks = 128;
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    struct Track
    {
        std::vector<float> left, right;
        double sourceRate = 48000.0;
        struct Source { std::vector<float> left,right; double rate=48000; };
        std::vector<Source> extra;
        std::vector<TimelineClip> clips;
        bool timelineEnabled = false;
        Automation::Lane volumeAutomation,panAutomation;std::atomic<int> automationMode {0};
        std::atomic<bool> volumeTouched{false},panTouched{false},volumeLatched{false},panLatched{false};
        MixerRoute routing;std::array<std::atomic<float>,2> sendLevels {};std::array<float,2> smoothSends {};
        bool midiTrack=false;double midiSeconds=0;
        std::atomic<float> stereoLeft {-1},stereoRight {1};
        int64_t residentFrames() const { int64_t result=static_cast<int64_t>(left.size());for(const auto& s:extra)result+=static_cast<int64_t>(s.left.size());return result; }
        double sourceDuration(int id) const { return id==0?static_cast<double>(left.size())/sourceRate:id>0&&id<=static_cast<int>(extra.size())?static_cast<double>(extra[id-1].left.size())/extra[id-1].rate:0; }
        float sample(double time,bool rightChannel) const noexcept
        {
            if(midiTrack||routing.aux)return 0;
            const auto read=[&](int source,double seconds)
            {
                const auto* data=rightChannel?&right:&left;double rate=sourceRate;
                if(source>0){if(source>static_cast<int>(extra.size()))return 0.0f;const auto& s=extra[source-1];data=rightChannel?&s.right:&s.left;rate=s.rate;}
                const double position=seconds*rate;
                if(position<0 || position>=static_cast<double>(data->size()) || data->empty())return 0.0f;
                const auto first=static_cast<size_t>(position),next=std::min(first+1,data->size()-1);const auto fraction=static_cast<float>(position-first);
                return (*data)[first]+((*data)[next]-(*data)[first])*fraction;
            };
            if(!timelineEnabled)return read(0,time);
            for(auto clip=clips.rbegin();clip!=clips.rend();++clip)if(time+1.e-10>=clip->start && time<clip->start+clip->length-1.e-10)
            {
                float value=read(clip->source,clip->offset+std::max(0.0,time-clip->start))*ClipLayout::envelope(*clip,time);
                if(clip->blend && time-clip->start+clip->fadeOffset<clip->fadeIn)
                    for(auto older=std::next(clip);older!=clips.rend();++older)if(time+1.e-10>=older->start&&time<older->start+older->length-1.e-10){value+=read(older->source,older->offset+std::max(0.0,time-older->start))*ClipLayout::envelope(*older,time);break;}
                return value;
            }
            return 0;
        }
        std::atomic<float> gain { 0.75f }, pan { 0.0f }, peak { 0.0f }, peakLeft { 0.0f }, peakRight { 0.0f };
        std::atomic<bool> muted { false }, solo { false };
        float smoothLeft = 0.0f, smoothRight = 0.0f; // Callback-owned.
        bool loaded() const noexcept { return routing.aux || midiTrack || (timelineEnabled ? !clips.empty() : !left.empty()); }
        double duration() const noexcept { if(routing.aux)return 0;if(midiTrack)return midiSeconds;if(!timelineEnabled)return static_cast<double>(left.size()) / sourceRate;double end=0;for(const auto& c:clips)end=std::max(end,c.start+c.length);return end; }
    };
    std::array<Track, maxTracks> tracks;
    MixerRoute routingSnapshot(size_t id) const { auto route=tracks[id].routing;for(size_t n=0;n<2;++n)route.sends[n].gain=tracks[id].sendLevels[n].load();return route; }
    void setRouting(size_t id,const MixerRoute& route) { tracks[id].routing=route;for(size_t n=0;n<2;++n)tracks[id].sendLevels[n]=route.sends[n].gain; }
    using Input = void(*)(void*,float*,float*,int,int);
    using TrackInput=void(*)(void*,size_t,float*,float*,int,int);
    using SourceStage=void(*)(void*,int,int);SourceStage sourceStage=nullptr;void* sourceStageContext=nullptr;
    TrackInput trackLiveInput=nullptr;
    std::array<bool,maxTracks> inputMonitorTracks{},pluginInputTracks{};
    bool monitored(size_t id)const noexcept{return (offlineSourceEnd<0||pluginInputTracks[id])&&(inputMonitorTracks[id]||static_cast<int>(id)==inputMonitorTrack.load());}
    Input liveInput=nullptr;void* liveContext=nullptr;
    using Click = float(*)(void*);
    Click extraClick=nullptr;void* clickContext=nullptr;
    static constexpr int processBlockSize = 256;
    // Installed while the device callback is detached. Native processors only.
    using Process = void (*)(void*, size_t, float*, float*, int, double, double, bool);
    Process process = nullptr;
    void* processContext = nullptr;
    // Offline-only limits, configured with the live callback detached.
    std::atomic<bool> automationBypass{false};
    double offlineSourceEnd = -1, offlineRenderEnd = -1;
    double deviceRate = 48000.0;
    int automationSourceLatencyFrames=0,automationAuxLatencyFrames=0;
    int playbackLatencyFrames = 0; // Updated only while the callback is detached.
    std::atomic<int> inputMonitorTrack { -1 };
    bool deferMasterProcessing = false; // Callback-owned for live-input mixing.
    std::atomic<bool> playing { false }, loop { false }, clipping { false };
    std::atomic<bool> recording { false };
    std::atomic<int> recordTrack { -1 };
    std::array<bool,maxTracks> recordTracks {};
    bool suppressRecordedPlayback=true;
    bool recordingTrack(size_t id) const noexcept { return recordTrack.load() >= 0 && (recordTracks[id] || static_cast<int>(id)==recordTrack.load()); }
    double blockStartSeconds=0; bool blockWasPlaying=false;
    double sourcePosition(int frame) const noexcept
    {
        double time=blockStartSeconds+static_cast<double>(frame)/deviceRate;
        if(loop.load()&&!recording.load())
        {
            const auto length=duration(),begin=std::clamp(loopRangeStart.load(),0.0,length);
            const auto end=loopRangeEnd.load()>begin&&begin<length?std::min(length,loopRangeEnd.load()):length;
            const auto start=end>begin?begin:0.0;
            if(end>start && time>=end)time=start+std::fmod(time-end,end-start);
        }
        return time;
    }
    std::atomic<bool> nativeClickTrackPresent{false};
    std::atomic<bool> metronome { false };
    std::atomic<float> metronomeGain { 1.0f };
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> loopRangeStart {0}, loopRangeEnd {0};
    std::atomic<float> masterGain { 1.0f }, peak { 0.0f };
    std::atomic<double> seconds { 0.0 }, seekSeconds { -1.0 };

    double automationTime(double time,int latency) const noexcept
    {
        time-=deviceRate>0?static_cast<double>(latency)/deviceRate:0;
        if(loop.load()&&!recording.load()&&loopHasWrapped){const double length=duration(),begin=std::clamp(loopRangeStart.load(),0.0,length),end=loopRangeEnd.load()>begin?std::min(length,loopRangeEnd.load()):length;const double start=end>begin?begin:0;if(end>start&&time<start)time=start+std::fmod(std::fmod(time-start,end-start)+(end-start),end-start);}
        return time;
    }
    void primeMixLevels() noexcept
    {
        for(auto& pair:buses->hardwareLeft)pair.fill(0);for(auto& pair:buses->hardwareRight)pair.fill(0);
        bool sourceSolo=false,auxSolo=false;for(const auto& t:tracks)if(t.loaded()&&t.solo.load()){if(t.routing.aux)auxSolo=true;else sourceSolo=true;}
        for(auto& t:tracks){const bool read=Automation::reads(t.automationMode.load(),t.volumeTouched.load(),t.volumeLatched.load(),automationBypass.load(),offlineSourceEnd>=0),readPan=Automation::reads(t.automationMode.load(),t.panTouched.load(),t.panLatched.load(),automationBypass.load(),offlineSourceEnd>=0);const double time=offlineSourceEnd>=0?0:std::max(0.0,seekSeconds.load()>=0?seekSeconds.load():seconds.load());const float gain=!t.muted.load()&&(t.routing.aux?(!auxSolo||t.solo.load()):(!sourceSolo||t.solo.load()))?(read?Automation::value(t.volumeAutomation,time,t.gain.load()):t.gain.load()):0,pan=readPan?Automation::value(t.panAutomation,time,t.pan.load()):t.pan.load();t.smoothLeft=gain*(pan>0?1-pan:1);t.smoothRight=gain*(pan<0?1+pan:1);for(size_t n=0;n<2;++n)t.smoothSends[n]=t.sendLevels[n].load();}
    }
    void refreshDuration() noexcept
    {
        lengthSeconds = 0.0;
        for (const auto& track : tracks) lengthSeconds = std::max(lengthSeconds, track.duration());
    }
    void reset() noexcept
    {
        cursorSeconds = 0.0; startSeconds = 0.0; seconds = 0.0; seekSeconds = -1.0; playing = false; peak = 0.0f; clipping = false;
        framesSinceSeek = 0; loopHasWrapped = false;
        for (auto& track : tracks) { track.smoothLeft = track.smoothRight = 0.0f; track.peak = 0.0f; track.smoothSends.fill(0); }
        smoothMaster = masterGain.load(std::memory_order_relaxed);
        lastBeat = -1; clickRemaining = 0;
    }
    double duration() const noexcept { const double clickSpan=metronome.load()?64.0*60.0/bpm.load():0.;return nativeClickTrackPresent.load()?std::max(lengthSeconds,clickSpan):(lengthSeconds>0?lengthSeconds:clickSpan); }
    void render(float* const* outputs, int channels, int count) noexcept
    {
        meterLeft.fill(0); meterRight.fill(0);
        for(auto& t:tracks){t.peakLeft=0; t.peakRight=0;}
        for (int ch = 0; ch < channels; ++ch)
            if (outputs[ch]) std::fill_n(outputs[ch], count, 0.0f);
        const auto seek = seekSeconds.exchange(-1.0, std::memory_order_relaxed);
        const bool capturing = recording.load(std::memory_order_relaxed);
        const bool repeating = loop.load(std::memory_order_relaxed) && !capturing;
        const double sourceLength = duration();
        const double latencySeconds = deviceRate > 0 ? playbackLatencyFrames / deviceRate : 0;
        const double loopBegin=std::clamp(loopRangeStart.load(),0.0,sourceLength);
        const double rangeEnd=loopRangeEnd.load();const double loopLimit=rangeEnd>loopBegin && loopBegin<sourceLength?std::min(sourceLength,rangeEnd):sourceLength;
        const double loopStart=loopLimit>loopBegin?loopBegin:0.0;
        const auto sessionLength = offlineRenderEnd >= 0 ? offlineRenderEnd + latencySeconds : (capturing || (!repeating && nativeClickTrackPresent.load() && metronome.load())) ? std::numeric_limits<double>::max() : (repeating?loopLimit:sourceLength) + (sourceLength > 0 && !repeating ? latencySeconds : 0);
        const auto tempo = bpm.load(std::memory_order_relaxed);
        const bool clickEnabled = metronome.load(std::memory_order_relaxed) && !nativeClickTrackPresent.load(std::memory_order_relaxed);
        if (!clickEnabled) clickRemaining = 0;
        if (seek >= 0.0)
        {
            cursorSeconds = std::clamp(seek, 0.0, capturing ? sessionLength : sourceLength); clickRemaining = 0;
            startSeconds = cursorSeconds;
            framesSinceSeek = 0; loopHasWrapped = false;
            const double beats = cursorSeconds * tempo / 60.0;
            lastBeat = static_cast<int64_t>(std::floor(beats + 1.0e-9));
            if (std::abs(beats - std::round(beats)) < 1.0e-8) --lastBeat;
        }
        blockStartSeconds=cursorSeconds;blockWasPlaying=playing.load();
        if (!playing.load(std::memory_order_relaxed) || sessionLength <= 0.0 || deviceRate <= 0.0)
        {
            if (sessionLength <= 0.0) { playing = false; cursorSeconds = 0.0; }
            if (seek >= 0.0 || sessionLength <= 0.0) seconds.store(cursorSeconds, std::memory_order_relaxed);
            peak = 0.0f; clipping = false;
            for (auto& track : tracks) track.peak.store(0.0f, std::memory_order_relaxed);
            renderStoppedGraph(outputs,channels,count);return;
        }

        std::array<float,maxTracks> trackPeaks {};
        const float smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (deviceRate * 0.005)));
        const float masterTarget = masterGain.load(std::memory_order_relaxed);
        const double step = 1.0 / deviceRate;
        float blockPeak = 0.0f;
        bool didClip = false;
        std::array<double, processBlockSize> positions {};
        std::array<float, processBlockSize> mixedLeft {}, mixedRight {};
        for (int offset = 0; offset < count;)
        {
            int requested = std::min(processBlockSize, count - offset);
            if (offlineSourceEnd >= 0 && cursorSeconds < offlineSourceEnd - 0.5 / deviceRate)
                requested = std::min(requested, std::max(1, static_cast<int>(std::llround((offlineSourceEnd - cursorSeconds) * deviceRate))));
            std::fill(mixedLeft.begin(), mixedLeft.end(), 0.0f);
            std::fill(mixedRight.begin(), mixedRight.end(), 0.0f);
            int frames = 0;
            for (; frames < requested; ++frames)
            {
                if (cursorSeconds + 1.0e-9 >= sessionLength)
                {
                    if (repeating) { if(frames>0)break;cursorSeconds = loopStart + std::fmod(std::max(0.0,cursorSeconds-sessionLength),sessionLength-loopStart); startSeconds = loopStart; loopHasWrapped = true; lastBeat = -1; clickRemaining = 0; }
                    else { playing.store(false, std::memory_order_relaxed); break; }
                }
                positions[static_cast<size_t>(frames)] = cursorSeconds;
                const auto beat = static_cast<int64_t>(std::floor(cursorSeconds * tempo / 60.0 + 1.0e-9));
                if (clickEnabled && (capturing || cursorSeconds < sourceLength) && beat != lastBeat)
                {
                    clickLength = std::max(1, static_cast<int>(deviceRate * 0.012)); clickRemaining = clickLength;
                    clickFrequency = beat % 4 == 0 ? 1800.0 : 1200.0; clickLevel = beat % 4 == 0 ? 0.18f : 0.12f;
                }
                lastBeat = beat;
                if (clickEnabled && clickRemaining > 0 && (capturing || cursorSeconds < sourceLength))
                {
                    const auto elapsed = clickLength - clickRemaining;
                    const float click = metronomeGain.load(std::memory_order_relaxed) * clickLevel * static_cast<float>(clickRemaining) / static_cast<float>(clickLength)
                        * static_cast<float>(std::sin(6.283185307179586 * clickFrequency * static_cast<double>(elapsed) / deviceRate));
                    mixedLeft[static_cast<size_t>(frames)] = mixedRight[static_cast<size_t>(frames)] = click; --clickRemaining;
                }
                ++framesSinceSeek;
                if (offlineRenderEnd >= 0) cursorSeconds = startSeconds + static_cast<double>(framesSinceSeek) / deviceRate;
                else cursorSeconds += step;
            }
            if (frames == 0) break;
            mixGraph(mixedLeft.data(),mixedRight.data(),frames,positions,offset,true,
                     offlineSourceEnd < 0 || positions[0] < offlineSourceEnd - 0.5 / deviceRate,trackPeaks);
            for (int frame = 0; frame < frames; ++frame)
            {
                smoothMaster += smoothing * (masterTarget - smoothMaster);
                mixedLeft[static_cast<size_t>(frame)] *= smoothMaster; mixedRight[static_cast<size_t>(frame)] *= smoothMaster;
            }
            // Master inserts are after the master fader, before the output ceiling.
            if (process && !deferMasterProcessing) process(processContext, maxTracks, mixedLeft.data(), mixedRight.data(), frames, positions[0], tempo, true);
            for (int frame = 0; frame < frames; ++frame)
            {
                const auto f = static_cast<size_t>(frame);
                const float rawL = std::isfinite(mixedLeft[f]) ? mixedLeft[f] : 0.0f;
                const float rawR = std::isfinite(mixedRight[f]) ? mixedRight[f] : 0.0f;
                didClip = didClip || std::abs(rawL) > 1.0f || std::abs(rawR) > 1.0f;
                const float l = deferMasterProcessing ? rawL : std::clamp(rawL, -1.0f, 1.0f), r = deferMasterProcessing ? rawR : std::clamp(rawR, -1.0f, 1.0f);
                if (channels == 1) { if (outputs[0]) outputs[0][offset + frame] = (l + r) * 0.5f; }
                else { if (channels > 0 && outputs[0]) outputs[0][offset + frame] = l; if (channels > 1 && outputs[1]) outputs[1][offset + frame] = r; }
                blockPeak = std::max(blockPeak, std::max(std::abs(l), std::abs(r)));
            }
            writeHardware(outputs,channels,frames,offset,didClip);
            offset+=frames;
            if (frames < requested && !repeating) break;
        }
        if (!repeating && cursorSeconds + 1.0e-9 >= sessionLength)
        { cursorSeconds = sessionLength; playing.store(false, std::memory_order_relaxed); }
        for(size_t id=0;id<maxTracks;++id){tracks[id].peak.store(trackPeaks[id],std::memory_order_relaxed);tracks[id].peakLeft=meterLeft[id];tracks[id].peakRight=meterRight[id];}
        double audibleSeconds = capturing ? cursorSeconds : std::clamp(cursorSeconds - latencySeconds, startSeconds, sourceLength);
        if (repeating && loopHasWrapped && framesSinceSeek >= static_cast<uint64_t>(playbackLatencyFrames))
            audibleSeconds = loopStart + std::fmod(std::fmod(cursorSeconds - latencySeconds-loopStart,sessionLength-loopStart) + sessionLength-loopStart,sessionLength-loopStart);
        seconds.store(audibleSeconds, std::memory_order_relaxed); peak.store(blockPeak, std::memory_order_relaxed);
        clipping.store(didClip, std::memory_order_relaxed);
    }
private:
    struct GraphWorkspace
    {
        std::array<std::array<float,processBlockSize>,maxTracks> left {},right {};
        std::array<std::array<float,processBlockSize>,8> hardwareLeft{},hardwareRight{};
    };
    std::array<float,maxTracks> meterLeft {},meterRight {};
    std::unique_ptr<GraphWorkspace> buses=std::make_unique<GraphWorkspace>();
    void mixGraph(float* mixedLeft,float* mixedRight,int frames,const std::array<double,processBlockSize>& positions,
                  int offset,bool readSources,bool transport,std::array<float,maxTracks>& peaks) noexcept
    {
        for(auto& pair:buses->hardwareLeft)pair.fill(0);for(auto& pair:buses->hardwareRight)pair.fill(0);
        bool sourceSolo=false,auxSolo=false;
        for(size_t id=0;id<maxTracks;++id){const auto& t=tracks[id];if((t.loaded()||monitored(id))&&!(recording.load()&&suppressRecordedPlayback&&recordingTrack(id)&&!monitored(id))&&t.solo.load()){if(t.routing.aux)auxSolo=true;else sourceSolo=true;}}
        for(size_t id=0;id<maxTracks;++id)if(tracks[id].routing.aux){buses->left[id].fill(0);buses->right[id].fill(0);}
        if(extraClick)for(int n=0;n<frames;++n){const float click=extraClick(clickContext);mixedLeft[n]+=click;mixedRight[n]+=click;}
        if(process)process(processContext,maxTracks+1,mixedLeft,mixedRight,frames,positions[0],bpm.load(),transport);
        const float smoothing=static_cast<float>(1-std::exp(-1.0/(deviceRate*.005)));
        const auto add=[&](int target,int n,float left,float right)
        {
            if(target<0){if(!auxSolo){mixedLeft[n]+=left;mixedRight[n]+=right;}return;}
            if(target>=static_cast<int>(maxTracks)||!tracks[target].routing.aux||(auxSolo&&!tracks[target].solo.load()))return;
            buses->left[target][n]+=left;buses->right[target][n]+=right;
        };
        std::array<float,processBlockSize> left {},right {};
        for(int pass=0;pass<2;++pass){if(pass==1&&sourceStage)sourceStage(sourceStageContext,frames,offset);
        for(size_t id=0;id<maxTracks;++id)
        {
            auto& t=tracks[id];if(t.midiTrack!=(pass==0))continue;const bool live=this->monitored(id)&&(trackLiveInput||liveInput);
            if(t.routing.aux||(!t.loaded()&&!live)||(!readSources&&!t.midiTrack&&!live))continue;
            const bool suppressed=recording.load()&&suppressRecordedPlayback&&recordingTrack(id);
            if(suppressed&&!live)continue;
            for(int n=0;n<frames;++n)
            {
                const bool source=readSources&&!suppressed&&(!live||!pluginInputTracks[id])&&(offlineSourceEnd<0||positions[n]<offlineSourceEnd-.5/deviceRate);
                left[n]=source?t.sample(positions[n],false):0;right[n]=source?t.sample(positions[n],true):0;
            }
            if(live){if(trackLiveInput)trackLiveInput(liveContext,id,left.data(),right.data(),frames,offset);else liveInput(liveContext,left.data(),right.data(),frames,offset);}
            if(process)process(processContext,id,left.data(),right.data(),frames,positions[0],bpm.load(),transport);
            const bool audible=!t.muted.load()&&(!sourceSolo||t.solo.load()),automated=Automation::reads(t.automationMode.load(),t.volumeTouched.load(),t.volumeLatched.load(),automationBypass.load(),offlineSourceEnd>=0),automatedPan=Automation::reads(t.automationMode.load(),t.panTouched.load(),t.panLatched.load(),automationBypass.load(),offlineSourceEnd>=0);
            const float manualGain=t.gain.load(),manualPan=t.pan.load();
            for(int n=0;n<frames;++n)
            {
                const float gain=audible?(automated?Automation::value(t.volumeAutomation,automationTime(positions[n],automationSourceLatencyFrames),manualGain):manualGain):0;
                const float pan=automatedPan?Automation::value(t.panAutomation,automationTime(positions[n],automationSourceLatencyFrames),manualPan):manualPan;
                const float targetL=gain*(pan>0?1-pan:1),targetR=gain*(pan<0?1+pan:1);
                t.smoothLeft+=smoothing*(targetL-t.smoothLeft);t.smoothRight+=smoothing*(targetR-t.smoothRight);
                const float sl=std::isfinite(left[n])?left[n]*t.smoothLeft:0,sr=std::isfinite(right[n])?right[n]*t.smoothRight:0;
                const float pl=t.stereoLeft.load(),pr=t.stereoRight.load();
                const float l=sl*(pl>0?1-pl:1)+sr*(pr>0?1-pr:1),r=sl*(pl<0?1+pl:1)+sr*(pr<0?1+pr:1);
                meterLeft[id]=std::max(meterLeft[id],std::abs(l));meterRight[id]=std::max(meterRight[id],std::abs(r));
                peaks[id]=std::max(peaks[id],std::max(std::abs(l),std::abs(r)));add(t.routing.output,n,l,r);
                for(size_t send=0;send<2;++send)
                {
                    const float target=t.routing.sends[send].target>=0?t.sendLevels[send].load():0;
                    t.smoothSends[send]+=smoothing*(target-t.smoothSends[send]);
                    if(t.routing.sends[send].target>=0)add(t.routing.sends[send].target,n,(t.routing.sends[send].pre?(audible?left[n]:0):l)*t.smoothSends[send],(t.routing.sends[send].pre?(audible?right[n]:0):r)*t.smoothSends[send]);
                }
            }
        }
        }
        // Align direct output with the extra latency introduced by the aux stage.
        if(process)process(processContext,maxTracks+2,mixedLeft,mixedRight,frames,positions[0],bpm.load(),transport);
        for(size_t id=0;id<maxTracks;++id)if(tracks[id].routing.aux)
        {
            auto& t=tracks[id];auto* l=buses->left[id].data();auto* r=buses->right[id].data();
            if(process)process(processContext,id,l,r,frames,positions[0],bpm.load(),transport);
            const bool audible=!t.muted.load()&&(!auxSolo||t.solo.load()),automated=Automation::reads(t.automationMode.load(),t.volumeTouched.load(),t.volumeLatched.load(),automationBypass.load(),offlineSourceEnd>=0),automatedPan=Automation::reads(t.automationMode.load(),t.panTouched.load(),t.panLatched.load(),automationBypass.load(),offlineSourceEnd>=0);
            const float manualGain=t.gain.load(),manualPan=t.pan.load();
            for(int n=0;n<frames;++n)
            {
                const float gain=audible?(automated?Automation::value(t.volumeAutomation,automationTime(positions[n],automationAuxLatencyFrames),manualGain):manualGain):0;
                const float pan=automatedPan?Automation::value(t.panAutomation,automationTime(positions[n],automationAuxLatencyFrames),manualPan):manualPan;
                const float targetL=gain*(pan>0?1-pan:1),targetR=gain*(pan<0?1+pan:1);
                t.smoothLeft+=smoothing*(targetL-t.smoothLeft);t.smoothRight+=smoothing*(targetR-t.smoothRight);
                const float sl=std::isfinite(l[n])?l[n]*t.smoothLeft:0,sr=std::isfinite(r[n])?r[n]*t.smoothRight:0;
                const float pl=t.stereoLeft.load(),pr=t.stereoRight.load();
                const float a=sl*(pl>0?1-pl:1)+sr*(pr>0?1-pr:1),b=sl*(pl<0?1+pl:1)+sr*(pr<0?1+pr:1);
                if(t.routing.hardwarePair>0){buses->hardwareLeft[static_cast<size_t>(t.routing.hardwarePair)][n]+=a;buses->hardwareRight[static_cast<size_t>(t.routing.hardwarePair)][n]+=b;}else{mixedLeft[n]+=a;mixedRight[n]+=b;}meterLeft[id]=std::max(meterLeft[id],std::abs(a));meterRight[id]=std::max(meterRight[id],std::abs(b));peaks[id]=std::max(peaks[id],std::max(std::abs(a),std::abs(b)));
            }
        }
    }
    void writeHardware(float* const* outputs,int channels,int frames,int offset,bool& clipped)noexcept{
        for(int pair=1;pair<8;++pair)for(int side=0;side<2;++side){const int channel=pair*2+side;if(channel>=channels||!outputs[channel])continue;const auto& data=side?buses->hardwareRight[static_cast<size_t>(pair)]:buses->hardwareLeft[static_cast<size_t>(pair)];for(int n=0;n<frames;++n){const float value=std::isfinite(data[n])?data[n]:0;clipped=clipped||std::abs(value)>1;outputs[channel][offset+n]=std::clamp(value,-1.f,1.f);}}
    }
    void renderStoppedGraph(float* const* outputs,int channels,int count) noexcept
    {
        bool active=inputMonitorTrack.load()>=0||extraClick;for(bool live:inputMonitorTracks)active=active||live;for(const auto& t:tracks)active=active||t.midiTrack;
        if(!active||deviceRate<=0)return;
        std::array<float,maxTracks> peaks {};std::array<double,processBlockSize> positions {};
        std::array<float,processBlockSize> l {},r {};bool clipped=false;float level=0;
        for(int offset=0;offset<count;offset+=processBlockSize)
        {
            const int frames=std::min(processBlockSize,count-offset);l.fill(0);r.fill(0);positions.fill(seconds.load());
            mixGraph(l.data(),r.data(),frames,positions,offset,false,false,peaks);
            const float smoothing=static_cast<float>(1-std::exp(-1.0/(deviceRate*.005)));
            for(int n=0;n<frames;++n){smoothMaster+=smoothing*(masterGain.load()-smoothMaster);l[n]*=smoothMaster;r[n]*=smoothMaster;}
            if(process&&!deferMasterProcessing)process(processContext,maxTracks,l.data(),r.data(),frames,seconds.load(),bpm.load(),false);
            for(int n=0;n<frames;++n)
            {
                const float a=std::isfinite(l[n])?l[n]:0,b=std::isfinite(r[n])?r[n]:0;
                clipped=clipped||std::abs(a)>1||std::abs(b)>1;
                const float left=deferMasterProcessing?a:std::clamp(a,-1.0f,1.0f),right=deferMasterProcessing?b:std::clamp(b,-1.0f,1.0f);
                if(channels==1){if(outputs[0])outputs[0][offset+n]=(left+right)*.5f;}
                else{if(channels>0&&outputs[0])outputs[0][offset+n]=left;if(channels>1&&outputs[1])outputs[1][offset+n]=right;}
                level=std::max(level,std::max(std::abs(left),std::abs(right)));
            }
            writeHardware(outputs,channels,frames,offset,clipped);
        }
        for(size_t id=0;id<maxTracks;++id){tracks[id].peak=peaks[id];tracks[id].peakLeft=meterLeft[id];tracks[id].peakRight=meterRight[id];}peak=level;clipping=clipped;
    }
    double cursorSeconds = 0.0, lengthSeconds = 0.0;
    double startSeconds = 0.0;
    uint64_t framesSinceSeek = 0;
    bool loopHasWrapped = false;
    float smoothMaster = 1.0f;
    int64_t lastBeat = -1;
    int clickRemaining = 0, clickLength = 1;
    double clickFrequency = 1200.0;
    float clickLevel = 0.12f;
};
