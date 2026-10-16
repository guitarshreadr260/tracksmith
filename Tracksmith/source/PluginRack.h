#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PlaybackCore.h"
#include "CompensationDelay.h"
#include "MidiLane.h"
#include "NativeClick.h"
#include "PluginAutomation.h"

// Ownership and editor operations run on the message thread with audio detached.
// Only process() runs in the device callback. Instances are never replaced there.
class PluginRack
{
public:
    static juce::String parameterId(const juce::AudioProcessorParameter* p){if(auto* native=dynamic_cast<const juce::AudioProcessorParameterWithID*>(p))return native->paramID;if(auto* hosted=dynamic_cast<const juce::HostedAudioProcessorParameter*>(p))return hosted->getParameterID();return {};}
    static constexpr int slotsPerChannel = 5;
    static constexpr int maximumPathLatency = 65536;
    static constexpr size_t clickChannel = PlaybackCore::maxTracks + 1;
    std::array<CompensationDelay, PlaybackCore::maxTracks + 2> compensation;
    std::array<int, PlaybackCore::maxTracks + 1> pathLatencies {};
    int mixLatency = 0, auxLatency = 0, totalLatency = 0;
    CompensationDelay directCompensation;
    std::array<MidiLane,PlaybackCore::maxTracks> midiLanes;
    PlaybackCore* playbackClock=nullptr;
    std::array<bool,PlaybackCore::maxTracks> monitorChannels{};
    int monitorChannel = -1; // Callback-owned; live input skips added playback delay.
    struct Clock final : juce::AudioPlayHead
    {
        std::atomic<double> seconds { 0.0 }, bpm { 120.0 }, rate { 48000.0 };
        std::atomic<bool> playing { false };
        juce::Optional<PositionInfo> getPosition() const override
        {
            const double time = seconds.load(std::memory_order_relaxed), tempo = bpm.load(std::memory_order_relaxed), sampleRate = rate.load(std::memory_order_relaxed);
            PositionInfo p; p.setTimeInSeconds(time); p.setTimeInSamples(static_cast<int64_t>(time * sampleRate));
            p.setBpm(tempo); p.setTimeSignature(TimeSignature { 4, 4 }); p.setPpqPosition(time * tempo / 60.0);
            p.setPpqPositionOfLastBarStart(std::floor(time * tempo / 240.0) * 4.0); p.setIsPlaying(playing.load(std::memory_order_relaxed));
            return p;
        }
    };
    class EditorWindow final : public juce::DocumentWindow
    {
    public:
        EditorWindow(juce::AudioProcessor& instance)
            : DocumentWindow(instance.getName(), juce::Colour(0xff191c24), closeButton)
        {
            setUsingNativeTitleBar(true);
            auto* editor = instance.createEditorIfNeeded();
            if (!editor) editor = new juce::GenericAudioProcessorEditor(instance);
            setContentOwned(editor, true); setResizable(editor->isResizable(), false);
            centreWithSize(editor->getWidth(), editor->getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { setVisible(false); }
    };
    struct Slot : juce::AudioProcessorListener
    {
        juce::PluginDescription description;
        juce::MemoryBlock retainedState;
        std::unique_ptr<juce::AudioProcessor> instance;
        std::unique_ptr<EditorWindow> window;
        juce::AudioBuffer<float> buffer { 2, PlaybackCore::processBlockSize };
        juce::MidiBuffer midi;
        struct Activity {std::atomic<bool> touched{false},latched{false},pending{false};std::atomic<float> value{0};};
        std::unique_ptr<Activity[]> activity;int parameterCount=0;
        struct Binding {juce::AudioProcessorParameter* parameter=nullptr;size_t lane=0;int index=0;};
        std::vector<PluginAutomation::Lane> automation;
        std::vector<Binding> bindings;
        std::vector<Automation::Writer> writers;
        std::vector<std::vector<PluginAutomation::Lane>> automationUndo,automationRedo;
        void rememberAutomation(){automationRedo.clear();automationUndo.push_back(automation);size_t points=0;for(const auto& state:automationUndo)for(const auto& lane:state)points+=lane.points.size();while(automationUndo.size()>1&&(automationUndo.size()>16||points>262144)){for(const auto& lane:automationUndo.front())points-=lane.points.size();automationUndo.erase(automationUndo.begin());}}
        uint64_t token=0;
        void bindAutomation(){writers.clear();writers.resize(automation.size());bindings.clear();if(!instance)return;for(size_t lane=0;lane<automation.size();++lane)for(int p=0;p<instance->getParameters().size();++p){auto* parameter=instance->getParameters()[p];if(parameter&&parameter->isAutomatable()&&parameterId(parameter)==automation[lane].id){bindings.push_back({parameter,lane,p});break;}}}
        Slot(){midi.ensureSize(65536);}
        std::atomic<bool> bypass { false };
        int channels = 2;
        bool occupied = false;
        std::atomic<bool> changed { false };
        void audioProcessorParameterChanged(juce::AudioProcessor*, int index, float value) override { changed = true;if(index>=0&&index<parameterCount&&activity[index].latched.load())activity[index].value=value; }
        void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int index)override {if(index>=0&&index<parameterCount){activity[index].value=instance->getParameters()[index]->getValue();activity[index].pending=true;activity[index].touched=true;activity[index].latched=true;}}
        void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int index)override {if(index>=0&&index<parameterCount)activity[index].touched=false;}
        void resetGestures(){for(int p=0;p<parameterCount;++p){activity[p].touched=false;activity[p].latched=false;activity[p].pending=false;}}
        void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override { changed = true; }
        ~Slot() { window.reset(); if (instance) { instance->removeListener(this); instance->releaseResources(); } }
    };
    std::array<std::array<std::unique_ptr<Slot>, slotsPerChannel>, PlaybackCore::maxTracks + 1> channels;
    juce::AudioPluginFormatManager formats;
    Clock clock;
    uint64_t nextToken=1;
    PluginRack() { formats.addFormat(new juce::VST3PluginFormat()); }
    ~PluginRack() { clear(); }
    void clear() { for (auto& channel : channels) for (auto& slot : channel) slot.reset(); }
    std::array<int, PlaybackCore::maxTracks + 1> reportedLatencies() const
    {
        std::array<int, PlaybackCore::maxTracks + 1> result {};
        for (size_t channel = 0; channel < channels.size(); ++channel) for (const auto& slot : channels[channel])
            if (slot && slot->instance && !slot->bypass.load())
                result[channel] = static_cast<int>(std::min<int64_t>(static_cast<int64_t>(result[channel]) + std::max(0, slot->instance->getLatencySamples()), INT_MAX));
        return result;
    }
    bool latencyChanged() const { return pathLatencies != reportedLatencies(); }
    void rebuildCompensation(PlaybackCore& playback)
    {
        playbackClock=&playback;
        bool hasClick=false;for(const auto& channel:channels)for(const auto& slot:channel)if(slot && slot->description.fileOrIdentifier==NativeClickProcessor::identifier)hasClick=true;playback.nativeClickTrackPresent.store(hasClick);
        for (auto& channel : channels)
        {
            int64_t sum = 0;
            for (auto& slot : channel)
                if (slot && slot->instance && !slot->bypass.load())
                {
                    const auto latency = std::max(0, slot->instance->getLatencySamples());
                    if (sum + latency > maximumPathLatency) { slot->bypass = true; slot->changed = true; }
                    else sum += latency;
                }
        }
        pathLatencies=reportedLatencies();mixLatency=auxLatency=0;
        const auto excluded=[&](size_t id){return playback.recording.load()&&playback.suppressRecordedPlayback&&playback.recordingTrack(id);};
        for(size_t id=0;id<PlaybackCore::maxTracks;++id)
        {
            if(playback.tracks[id].routing.aux)auxLatency=std::max(auxLatency,pathLatencies[id]);
            else if(playback.tracks[id].loaded()&&!excluded(id))mixLatency=std::max(mixLatency,pathLatencies[id]);
        }
        for(size_t id=0;id<PlaybackCore::maxTracks;++id)
            compensation[id].configure(playback.tracks[id].routing.aux?auxLatency-pathLatencies[id]:playback.tracks[id].loaded()&&!excluded(id)?mixLatency-pathLatencies[id]:0);
        compensation[clickChannel].configure(mixLatency);directCompensation.configure(auxLatency);
        totalLatency=mixLatency+auxLatency+pathLatencies[PlaybackCore::maxTracks];
        playback.automationSourceLatencyFrames=mixLatency;playback.automationAuxLatencyFrames=mixLatency+auxLatency;
    }
    void resetGraph()
    {
        for (auto& delay : compensation) delay.reset();directCompensation.reset();
        for(auto& lane:midiLanes)lane.reset();
        for (auto& channel : channels) for (auto& slot : channel) if (slot && slot->instance)
        {
            if(slot->description.isInstrument){slot->buffer.clear();slot->midi.clear();for(int ch=1;ch<=16;++ch)slot->midi.addEvent(juce::MidiMessage::allSoundOff(ch),0);float* data[]{slot->buffer.getWritePointer(0),slot->buffer.getWritePointer(1)};juce::AudioBuffer<float> view(data,slot->channels,PlaybackCore::processBlockSize);slot->instance->processBlock(view,slot->midi);}
            slot->instance->reset();
        }
    }
    static bool configure(juce::AudioProcessor& instance, int& count, bool instrument = false)
    {

        auto layout = instance.getBusesLayout();
        if ((!instrument && layout.inputBuses.isEmpty()) || layout.outputBuses.isEmpty()) return false;
        for (int ch : { 2, 1 })
        {
            for (auto& bus : layout.inputBuses) bus = juce::AudioChannelSet::disabled();
            for (auto& bus : layout.outputBuses) bus = juce::AudioChannelSet::disabled();
            if(!instrument)layout.inputBuses.getReference(0)=juce::AudioChannelSet::canonicalChannelSet(ch);
            layout.outputBuses.getReference(0)=juce::AudioChannelSet::canonicalChannelSet(ch);
            if (instance.setBusesLayout(layout)) { count = ch; return true; }
        }
        return false;
    }
    void prepare(double rate)
    {
        clock.rate = rate;
        for (auto& channel : channels) for (auto& slot : channel)
            if (slot && slot->instance)
            {
                slot->instance->releaseResources(); slot->instance->setRateAndBufferSizeDetails(rate, PlaybackCore::processBlockSize);
                slot->instance->prepareToPlay(rate, PlaybackCore::processBlockSize);
            }
    }
    juce::Result install(size_t channel, int index, const juce::PluginDescription& description,
                         std::unique_ptr<juce::AudioProcessor> instance, const juce::MemoryBlock& state, bool bypassed, double rate)
    {
        if(channel>=channels.size()||index<0||index>=slotsPerChannel)return juce::Result::fail("Invalid insert.");
        if(description.isInstrument && (channel>=PlaybackCore::maxTracks || !midiLanes[channel].enabled || index!=0))return juce::Result::fail("Load instruments in insert A of a MIDI track.");
        auto slot = std::make_unique<Slot>(); slot->description = description; slot->retainedState = state;
        slot->token=nextToken++;slot->occupied = true; slot->bypass = bypassed;
        if (instance)
        {
            int64_t path = bypassed ? 0 : std::max(0, instance->getLatencySamples());
            for (int other = 0; other < slotsPerChannel; ++other)
                if (other != index)
                {
                    const auto& previous = channels[channel][static_cast<size_t>(other)];
                    if (previous && previous->instance && !previous->bypass.load()) path += std::max(0, previous->instance->getLatencySamples());
                }
            if (path > maximumPathLatency) return juce::Result::fail("Insert chain exceeds 65,536 samples of supported playback latency.");
            if (!configure(*instance, slot->channels,description.isInstrument)) return juce::Result::fail("Use a mono/stereo effect or an instrument with a disabled input and mono/stereo output.");
            instance->setPlayHead(&clock);
            instance->setRateAndBufferSizeDetails(rate, PlaybackCore::processBlockSize);
            if (state.getSize()) instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            instance->prepareToPlay(rate, PlaybackCore::processBlockSize);
            slot->instance = std::move(instance);
            slot->parameterCount=slot->instance->getParameters().size();slot->activity=std::make_unique<Slot::Activity[]>(static_cast<size_t>(slot->parameterCount));
            slot->instance->addListener(slot.get());
        }
        channels[channel][static_cast<size_t>(index)] = std::move(slot);
        return juce::Result::ok();
    }
    void restoreAutomation(size_t channel,int index,const juce::var& saved){auto& slot=channels[channel][static_cast<size_t>(index)];if(slot&&PluginAutomation::decode(saved,slot->automation))slot->bindAutomation();}
    juce::var automationSnapshot(int channel,int index,const juce::String& selected) const {
        auto* item=new juce::DynamicObject();item->setProperty("channel",channel);item->setProperty("slot",index);
        if(channel<0||channel>=static_cast<int>(channels.size())||index<0||index>=slotsPerChannel)return juce::var(item);
        const auto& slot=channels[static_cast<size_t>(channel)][static_cast<size_t>(index)];if(!slot)return juce::var(item);
        item->setProperty("canUndo",!slot->automationUndo.empty());item->setProperty("canRedo",!slot->automationRedo.empty());item->setProperty("plugin",slot->description.name);juce::Array<juce::var> parameters;
        if(slot->instance)for(int p=0;p<slot->instance->getParameters().size();++p){const auto* parameter=slot->instance->getParameters()[p];if(!parameter||!parameter->isAutomatable()||parameterId(parameter).isEmpty())continue;auto* param=new juce::DynamicObject();param->setProperty("id",parameterId(parameter));param->setProperty("name",parameter->getName(512));param->setProperty("value",parameter->getValue());param->setProperty("text",parameter->getText(parameter->getValue(),80));parameters.add(juce::var(param));}
        item->setProperty("parameters",parameters);item->setProperty("selected",selected);
        juce::Array<juce::var> selectedLanes;for(const auto& lane:slot->automation)if(lane.id==selected){selectedLanes=PluginAutomation::encode({lane});selectedLanes.getReference(0).getDynamicObject()->setProperty("mode",lane.mode);}
        item->setProperty("lanes",selectedLanes);return juce::var(item);
    }
    void editor(size_t channel, int index)
    {
        auto& slot = channels[channel][static_cast<size_t>(index)];
        if (!slot || !slot->instance) return;
        if (!slot->window) slot->window = std::make_unique<EditorWindow>(*slot->instance);
        slot->window->setVisible(true); slot->window->toFront(true);
    }
    bool consumeChanges()
    {
        bool changed = false;
        for (auto& channel : channels) for (auto& slot : channel)
            if (slot) changed = slot->changed.exchange(false) || changed;
        return changed;
    }
    juce::Array<juce::var> snapshot(int trackCount) const
    {
        juce::Array<juce::var> result;
        for (size_t channel = 0; channel < channels.size(); ++channel)
        {
            if (channel >= static_cast<size_t>(trackCount) && channel != PlaybackCore::maxTracks) continue;
            for (int index = 0; index < slotsPerChannel; ++index)
            {
                const auto& slot = channels[channel][static_cast<size_t>(index)]; if (!slot) continue;
                auto* item = new juce::DynamicObject(); item->setProperty("channel", static_cast<int>(channel)); item->setProperty("slot", index);
                item->setProperty("token",static_cast<juce::int64>(slot->token));item->setProperty("name", slot->description.name); item->setProperty("bypass", slot->bypass.load()); item->setProperty("missing", slot->instance == nullptr);
                item->setProperty("latency", slot->instance ? slot->instance->getLatencySamples() : 0);
                item->setProperty("compensation", compensation[channel].samples());
                result.add(juce::var(item));
            }
        }
        return result;
    }
    juce::Array<juce::var> save()
    {
        juce::Array<juce::var> result;
        for (size_t channel = 0; channel < channels.size(); ++channel) for (int index = 0; index < slotsPerChannel; ++index)
        {
            auto& slot = channels[channel][static_cast<size_t>(index)]; if (!slot) continue;
            if (slot->instance) slot->instance->getStateInformation(slot->retainedState);
            auto* item = new juce::DynamicObject(); item->setProperty("channel", static_cast<int>(channel)); item->setProperty("slot", index);
            item->setProperty("description", slot->description.createXml()->toString()); item->setProperty("state", slot->retainedState.toBase64Encoding());
            item->setProperty("automation",PluginAutomation::encode(slot->automation));
            item->setProperty("bypass", slot->bypass.load()); result.add(juce::var(item));
        }
        return result;
    }
    static void process(void* context, size_t channel, float* left, float* right, int frames, double seconds, double bpm, bool playing)
    {
        auto& rack = *static_cast<PluginRack*>(context); rack.clock.seconds = seconds; rack.clock.bpm = bpm; rack.clock.playing = playing;
        if(channel==PlaybackCore::maxTracks+2){rack.directCompensation.process(left,right,frames);return;}
        if (channel == clickChannel) { rack.compensation[channel].process(left, right, frames); return; }
        int upstream=channel==PlaybackCore::maxTracks?rack.mixLatency+rack.auxLatency:(rack.playbackClock&&rack.playbackClock->tracks[channel].routing.aux?rack.mixLatency:0);
        for (auto& slot : rack.channels[channel])
        {
            if (!slot || !slot->instance || slot->bypass.load(std::memory_order_relaxed)) continue;
            const double time=rack.playbackClock?rack.playbackClock->automationTime(seconds,upstream):std::max(0.0,seconds-upstream/rack.clock.rate.load());
            for(const auto& binding:slot->bindings){const auto& lane=slot->automation[binding.lane];if(Automation::reads(lane.read?lane.mode:0,slot->activity[binding.index].touched.load(),slot->activity[binding.index].latched.load(),rack.playbackClock&&rack.playbackClock->automationBypass.load(),rack.playbackClock&&rack.playbackClock->offlineSourceEnd>=0)&&!lane.points.empty())binding.parameter->setValue(Automation::value(lane.points,time,binding.parameter->getValue()));}
            upstream+=std::max(0,slot->instance->getLatencySamples());
            float* data[] { slot->buffer.getWritePointer(0), slot->buffer.getWritePointer(1) };
            if (slot->channels == 1)
                for (int n = 0; n < frames; ++n) data[0][n] = (left[n] + right[n]) * 0.5f;
            else { std::copy_n(left, frames, data[0]); std::copy_n(right, frames, data[1]); }
            // This wrapper points to preallocated storage; MIDI output is discarded.
            juce::AudioBuffer<float> view(data, slot->channels, frames);
            slot->midi.clear();
            if(slot->description.isInstrument && channel<PlaybackCore::maxTracks)rack.midiLanes[channel].emit(slot->midi,seconds,bpm,rack.clock.rate.load(),frames,playing);
            if(auto* click=dynamic_cast<NativeClickProcessor*>(slot->instance.get())){const bool exporting=rack.playbackClock&&rack.playbackClock->offlineSourceEnd>=0;click->transportEnabled.store(exporting||!rack.playbackClock||rack.playbackClock->metronome.load());click->recording.store(rack.playbackClock&&rack.playbackClock->recording.load());click->offline.store(exporting);}
            slot->instance->processBlock(view, slot->midi);
            for (int n = 0; n < frames; ++n)
            {
                left[n] = std::isfinite(data[0][n]) ? data[0][n] : 0.0f;
                right[n] = std::isfinite(data[slot->channels - 1][n]) ? data[slot->channels - 1][n] : 0.0f;
            }
        }
        if (channel < PlaybackCore::maxTracks && !((!rack.playbackClock||rack.playbackClock->offlineSourceEnd<0)&&(static_cast<int>(channel)==rack.monitorChannel||rack.monitorChannels[channel]))) rack.compensation[channel].process(left, right, frames);
    }
};
