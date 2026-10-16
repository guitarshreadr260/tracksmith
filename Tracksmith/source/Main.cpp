#include "MidiBatchEditing.h"
#include "TrackEditing.h"
#include "MidiGrooveImport.h"
#include "RecoveryStore.h"
#include <optional>
#include "PlaylistEditing.h"
#include "GroupEditing.h"
#include <JuceHeader.h>
#include <BinaryData.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "AudioEngine.h"
#include "GrooveWorker.h"
#include "MixExport.h"
#include "PluginDiscovery.h"
#include "StartupScan.h"
#include "PluginCatalog.h"
#include "NativeInstruments.h"

static uint64_t nextTimelineEdit() { static uint64_t serial=0; return ++serial; }

// Message-thread controls are validated here, then published as atomic audio parameters.
struct SessionControls
{
    bool playing = false;
    double bpm = 120.0, beat = 0.0;
    struct Channel { float gain = 0.75f, pan = 0.0f; bool muted = false, solo = false, armed = false; };
    std::array<Channel, 128> channels {};

    void accept(const juce::var& message)
    {
        if (! message.isObject()) return;
        const auto type = message.getProperty("type", {}).toString();
        const auto number = [&message](const char* key, double fallback, double low, double high)
        {
            const auto value = message.getProperty(key, fallback);
            if (! (value.isDouble() || value.isInt() || value.isInt64())) return fallback;
            const auto n = static_cast<double>(value);
            return std::isfinite(n) ? juce::jlimit(low, high, n) : fallback;
        };
        const auto flag = [&message](const char* key)
        {
            const auto value = message.getProperty(key, false);
            return value.isBool() && static_cast<bool>(value);
        };
        if (type == "transport")
        {
            playing = flag("playing");
            bpm = number("bpm", 120.0, 20.0, 300.0);
        }
        else if (type == "seek") beat = number("beat", 0.0, 0.0, 65536.0);
        else if (type == "track")
        {
            const auto id = message.getProperty("id", -1);
            if (! id.isInt()) return;
            const int index = static_cast<int>(id);
            if (index < 0 || index >= static_cast<int>(channels.size())) return;
            auto& c = channels[static_cast<size_t>(index)];
            c.gain = static_cast<float>(number("gain", 0.75, 0.0, 1.0));
            c.pan = static_cast<float>(number("pan", 0.0, -1.0, 1.0));
            c.muted = flag("muted"); c.solo = flag("solo"); c.armed = flag("armed");
        }
    }
};

class AudioSettingsWindow final : public juce::DialogWindow
{
public:
    AudioSettingsWindow(juce::AudioDeviceManager& devices, std::function<void()> closed)
        : DialogWindow("Tracksmith audio input and output", juce::Colour(0xff191c24), true), onClosed(std::move(closed))
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new juce::AudioDeviceSelectorComponent(devices, 0, InputRouting::maximumInputs, 1, InputRouting::maximumOutputs, false, false, false, false), true);
        centreWithSize(500, 430); setVisible(true);
    }
    void closeButtonPressed() override { setVisible(false); if (onClosed) onClosed(); }
private:
    std::function<void()> onClosed;
};

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent(bool diagnostic=false) : browser(makeOptions())
    {
        setSize(1280, 820);
        if(diagnostic)return;
        if (! juce::WebBrowserComponent::areOptionsSupported(makeOptions()))
        {
            error.setText("Tracksmith needs the Microsoft Edge WebView2 Runtime. Install the Evergreen Runtime, then reopen Tracksmith.", juce::dontSendNotification);
            error.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(error);
            return;
        }
        addAndMakeVisible(browser);
        for (size_t id = 0; id < trackNames.size(); ++id) trackNames[id] = "Audio " + juce::String(static_cast<int>(id) + 1);
        audioError = engine.initialise();
        grooveWorker.onFinished = [this](GrooveAnalysis result, double analysedSeconds)
        {
            loading = false; groove = std::move(result); grooveSeconds = analysedSeconds;
            status = groove.valid ? "AVRX GROOVE analyzed the first " + juce::String(analysedSeconds, 1) + " seconds. Review the suggested pattern before exporting." : groove.error;
            publish();
        };
        loader.onLoaded = [this](LoadedWav wav)
        {
            loading = false;
            if (wav.error.isNotEmpty())
            {
                status = wav.error;
                if (loadingTake) status += " The recorded WAV remains at " + lastTakeFile.getFullPathName();
                if(loadingTake){recordingIssues.add(status);loadNextRecordedTake();}else {loadingTake=false;recordingBusy=false;} publish(); return;
            }
            if (loadingTake && static_cast<uint64_t>(wav.left.size()) != expectedTakeFrames)
            {
                status = "The recorded WAV could not be read completely. The previous track was kept. Take file: " + lastTakeFile.getFullPathName();
                if(loadingTake){recordingIssues.add(status);loadNextRecordedTake();}else {loadingTake=false;recordingBusy=false;} publish(); return;
            }
            clearBatchHistory();const auto id = static_cast<size_t>(importingTrack);
            float recordedPeak = 0.0f;
            for (const auto& value : wav.waveform) recordedPeak = std::max(recordedPeak, static_cast<float>(static_cast<double>(value)));
            if (static_cast<int>(id) == grooveTrack) { groove = {}; grooveTrack = -1; }
            const bool append=trackInfo[id].file!=juce::File()||!trackInfo[id].extra.empty()||!engine.playback.tracks[id].clips.empty()||engine.playback.tracks[id].residentFrames()>0;
            auto before=engine.playback.tracks[id].clips;
            syncPlaylist(static_cast<int>(id));
            const double recordedEnd=engine.takeStart+static_cast<double>(wav.left.size())/wav.rate;
            if(loadingTake&&std::any_of(before.begin(),before.end(),[&](const auto& c){return c.start<recordedEnd&&c.start+c.length>engine.takeStart;}))
            {
                auto& list=trackPlaylists[id];if(list.size()<maximumPlaylists){int next=1;for(const auto& p:list)next=std::max(next,p.id+1);list.push_back({next,"Before take "+juce::String(next),before,{}});}else recordingIssues.add("Playlist backup limit reached; Undo still restores this replacement.");
            }
            engine.install(id,wav,append,loadingTake?engine.takeStart:-1,loadingTake,recordedEnd,loadingTake&&engine.continueAfterPunch);
            MediaInfo info {wav.name,wav.file,std::move(wav.waveform)};
            if(append)trackInfo[id].extra.push_back(std::move(info));
            else {trackInfo[id].fileName=info.fileName;trackInfo[id].file=info.file;trackInfo[id].waveform=std::move(info.waveform);trackInfo[id].extra.clear();}
            if(!loadingTake){undoClips.clear();redoClips.clear();midiRedo.clear();midiUndo.clear();midiRedo.clear();}
            else{clearBatchHistory();undoClips.push_back({static_cast<int>(id),std::move(before),engine.playback.tracks[id].clips,activePlaylists[id]});if(undoClips.size()>64)undoClips.erase(undoClips.begin());redoClips.clear();midiRedo.clear();}
            syncPlaylist(static_cast<int>(id));
            markDirty();
            status = "Loaded " + wav.name + " on " + trackNames[id];
            if (loadingTake)
            {
                status = "Recorded " + wav.name + " on " + trackNames[id];
                if (takeWarning.isNotEmpty()) status += ". " + takeWarning;
                if (recordedPeak < 0.001f) status += ". Input was below -60 dBFS. Compare the input meters and choose the channel receiving your signal.";
                if(takeWarning.isNotEmpty())recordingIssues.add(trackNames[id]+": "+takeWarning);
                loadNextRecordedTake();
            }
            applyChannels(); publish(true);
        };
        sessionWorker.onFinished = [this](SessionJobResult result)
        {
            sessionBusy = false;
            if (result.error.isNotEmpty()) { pendingRecovery.reset();status = result.error; publish(true); return; }
            const bool recovered=!result.saving&&pendingRecovery&&pendingRecovery->snapshot==result.file;
            if (!result.saving)
            {
                ++sessionRevision;newSessionName.clear();nativeAnalysisPending=false;nativeAnalysisTrack=-1;nativeAnalysisToken=0;
                cleanRecovery();recoveryRun=RecoveryStore::newRun();recoverySourceRun={};recoverySuggestedFile={};recoveredName.clear();editRevision=0;lastAutosaveRevision=-1;savedRevision=0;recoveryGeneration=0;lastAutosaveTime=0;autosaveNotice="Autosave ready";lastAutosaveAttempt=juce::Time::getMillisecondCounterHiRes();
                if(recovered){recoverySourceRun=pendingRecovery->run;recoveredName="Recovered "+pendingRecovery->name;const auto original=pendingRecovery->original;if(original!=juce::File())recoverySuggestedFile=original.getSiblingFile(original.getFileNameWithoutExtension()+" Recovered.tracksmith");const auto id=pendingRecovery->id;recoveries.erase(std::remove_if(recoveries.begin(),recoveries.end(),[&](const auto& e){return e.id==id;}),recoveries.end());}

                clearBatchHistory();undoClips.clear();redoClips.clear();midiRedo.clear();midiUndo.clear();midiRedo.clear();
                groove = {}; grooveTrack = -1;
                engine.monitorArmed=false;engine.playback.automationBypass=false;engine.dynamicTransport=true;engine.punchEnabled=false;engine.playStart=0;engine.selectionStart=0;engine.selectionEnd=0;engine.playback.loopRangeStart=0;engine.playback.loopRangeEnd=0;
                engine.detach(); engine.plugins.clear(); engine.attach();
                automationCurvesChanged=true;automationPassActive=false;for(auto& pass:automationPasses)pass={};automationUndo.clear();automationRedo.clear();
                if(result.session.legacyInputRouting){
                    auto* device=engine.devices.getCurrentAudioDevice();const auto active=device?device->getActiveInputChannels():juce::BigInteger();
                    const int count=device?device->getInputChannelNames().size():0;
                    for(auto& track:result.session.tracks){const int old=track.inputSource;
                        track.inputSource=InputRouting::legacyPhysical(old,count,[&](int ch){return active[ch];});
                        if(track.inputChannels==2&&InputRouting::legacyPhysical(old+1,count,[&](int ch){return active[ch];})!=track.inputSource+1)track.inputSource=-1;
                    }
                }
                engine.installSession(result); controls = {}; controls.bpm = result.session.bpm;
                trackCount = static_cast<int>(result.session.tracks.size());
                for (size_t id = 0; id < trackInfo.size(); ++id)
                {
                    trackPlaylists[id].clear();activePlaylists[id]=1;
                    engine.trackInputs[id]=0;engine.trackInputChannels[id]=1;
                    trackInfo[id] = {}; trackNames[id] = "Audio " + juce::String(static_cast<int>(id) + 1);
                    if (id >= result.session.tracks.size()) continue;
                    const auto& track = result.session.tracks[id];trackPlaylists[id]=track.playlists;activePlaylists[id]=track.activePlaylist;syncPlaylist(static_cast<int>(id)); trackNames[id] = track.name;
                    auto& channel = controls.channels[id]; channel.gain = track.gain; channel.pan = track.pan;
                    channel.muted = track.muted; channel.solo = track.solo; channel.armed = track.armed;
                    engine.trackInputs[id]=track.inputSource;engine.trackInputChannels[id]=track.inputChannels;
                    trackInfo[id].file = track.audioFile;
                    if (result.audio[id]) { trackInfo[id].fileName = result.audio[id]->name; trackInfo[id].waveform = std::move(result.audio[id]->waveform); }
                    for(size_t source=0;source<result.extraAudio[id].size();++source)
                    {
                        MediaInfo info;info.file=track.sources[source+1];info.fileName=info.file.getFileName();
                        if(result.extraAudio[id][source])info.waveform=std::move(result.extraAudio[id][source]->waveform);
                        trackInfo[id].extra.push_back(std::move(info));
                    }
                }
            }
            if (engine.plugins.consumeChanges()){++pluginChangeSerial;++editRevision;}
            sessionFile = recovered?juce::File():result.file; dirty = recovered||(result.saving && pluginChangeSerial != savingPluginSerial);
            if(recovered){++editRevision;lastAutosaveAttempt-=110000;}
            if(result.saving&&!dirty){savedRevision=editRevision;cleanRecovery();recoveredName.clear();recoverySuggestedFile={};}
            if(!result.saving)pendingRecovery.reset();
            status = result.saving ? "Saved " + sessionFile.getFileName() : "Opened " + sessionFile.getFileName();
            if(recovered)status="Recovered an unsaved copy. Save it under a new filename.";
            if (!result.missing.isEmpty()) status += ". Missing WAVs: " + result.missing.joinIntoString(", ") + ". Re-import them onto their tracks.";
            if (!result.saving && !result.session.inserts.isEmpty())
            {
                restoringInserts = result.session.inserts; restoreIndex = 0; pluginBusy = true; restoreNext();
            }
            publish(true);
        };
        mixExporter.validate = [this] { return engine.plugins.latencyChanged()
            ? juce::String("Plugin latency changed during export. Wait for compensation to rebuild, then export again.") : juce::String(); };
        mixExporter.onFinished = [this](MixExportResult result)
        {
            exportBusy = false;
            engine.plugins.resetGraph(); engine.attach();
            status = result.cancelled ? "Export cancelled. The previous destination file was kept."
                : result.error.isNotEmpty() ? result.error
                : "Exported " + exportFile.getFileName() + (result.clipped ? ". Mix reached the output ceiling; lower levels and export again if needed." : ".");
            publish();
        };
        recoveryWorker.onFinished=[this](RecoveryResult result){
            if(result.scanning){recoveries=std::move(result.entries);if(result.error.isNotEmpty())autosaveNotice=result.error;}
            else if(result.id==recoveryRun){if(result.error.isNotEmpty())autosaveNotice="Autosave failed: "+result.error;else{lastAutosaveTime=result.time;lastAutosaveRevision=result.revision;autosaveNotice="Recovery snapshot saved";if(!dirty||result.revision<=savedRevision)RecoveryStore::markClean(recoveryRoot.getChildFile(recoveryRun));}}publish();
        };
        recoveryWorker.scan(recoveryRoot,recoveryRun);
        browser.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
        startTimerHz(20);
        startupScan.onProgress=[safe=juce::Component::SafePointer<MainComponent>(this)](StartupScan::Progress p){if(!safe)return;safe->startupProgress=p;safe->pluginBusy=p.active;if(!p.active)safe->status=p.failed?"Studio ready. "+juce::String(p.failed)+" plugin(s) skipped after failed discovery; manual rescan is available in the insert menu.":"Studio ready. VST3 library checked.";safe->publish();};
        pluginBusy=true;startupScan.begin();
    }
    ~MainComponent() override { startupScan.shutdown();stopTimer();recoveryWorker.onFinished={};recoveryWorker.shutdown(); }
    void resized() override { browser.setBounds(getLocalBounds()); error.setBounds(getLocalBounds().reduced(30)); }
    void requestClose()
    {
        if(recoveryWorker.isBusy()){status="Wait for the recovery snapshot to finish before closing.";publish();return;}
        if (exportBusy) { status = "Cancel export or wait for it to finish before closing."; publish(); return; }
        if (recordingBusy) { status = "Stop recording and wait for the take to finish before closing."; publish(); return; }
        if (sessionBusy) { status = "Wait for the session operation to finish before closing."; publish(); return; }
        if (pluginBusy) { status = "Wait for the plugin operation to finish before closing."; publish(); return; }
        if(automationPassActive){automationTick();engine.playback.playing=false;finishAutomation();}
        confirmDiscard("Close Tracksmith?", [safe=juce::Component::SafePointer<MainComponent>(this)] {if(safe){safe->cleanRecovery();juce::JUCEApplication::getInstance()->quit();}});
    }
    bool editingRegression(const juce::File& reference,const juce::File& report)
    {
        juce::String failure;auto check=[&](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
        auto send=[&](const juce::String& json){command(juce::JSON::parse(json));};
        try{
            trackCount=2;trackNames[0]="Reference";trackNames[1]="MIDI";
            auto wav=decodeWav(reference,48000*180,[]{return false;});check(wav.error.isEmpty()&&!wav.left.empty(),"decode reference");
            engine.detach();auto& t=engine.playback.tracks[0];t.left=std::move(wav.left);t.right=std::move(wav.right);t.sourceRate=wav.rate;t.timelineEnabled=true;
            t.clips.push_back({1,0,0,0,8});trackInfo[0].file=reference;trackInfo[0].fileName=reference.getFileName();trackInfo[0].waveform=wav.waveform;syncPlaylist(0);
            engine.playback.tracks[1].midiTrack=true;engine.plugins.midiLanes[1].enabled=true;syncPlaylist(1);engine.attach();
            send(R"({"type":"clipEdit","track":0,"id":1,"action":"delete","position":0})");check(t.clips.empty(),"audio delete");
            send(R"({"type":"undo","track":1})");check(t.clips.size()==1,"undo audio after selecting MIDI");
            send(R"({"type":"redo","track":1})");check(t.clips.empty(),"redo audio");send(R"({"type":"undo"})");
            send(R"({"type":"midiEdit","track":1,"id":1,"action":"add","pitch":36,"channel":10,"start":2,"length":1,"velocity":0.8})");
            send(R"({"type":"midiEdit","track":1,"id":1,"action":"delete"})");check(engine.plugins.midiLanes[1].notes.empty(),"MIDI delete");
            send(R"({"type":"undo","track":0})");check(engine.plugins.midiLanes[1].notes.size()==1,"undo MIDI after selecting audio");
            send(R"({"type":"editRange","start":1,"end":2})");send(R"({"type":"groupEdit","action":"delete","tracks":[0]})");check(t.clips.size()==2&&engine.plugins.midiLanes[1].notes.size()==1,"range edits only target track");send(R"({"type":"undo"})");check(t.clips.size()==1,"range delete undo");
            send(R"({"type":"ezAnalysePart","id":0,"clip":1,"neural":true,"referenceInfluence":0.3})");check(nativeAnalysisPending&&trackCount==3,"timeline analysis starts native instrument");check(NativeInstruments::grooveInfo(engine.plugins.channels[2][0]->instance.get()).kitName=="Chango Luke Holland","New drum tracks default to Luke Holland");
            const auto deadline=juce::Time::getMillisecondCounterHiRes()+20000;
            while(nativeAnalysisPending&&juce::Time::getMillisecondCounterHiRes()<deadline){checkNativeAnalysis();juce::Thread::sleep(10);}
            check(!nativeAnalysisPending&&!engine.plugins.midiLanes[2].notes.empty(),"timeline analysis writes drum MIDI");check(nativeAnalysisMessage.contains("GrooVAE"),"Actual pretrained neural inference committed song MIDI");
            check(wav.waveform.size()>512&&static_cast<double>(wav.waveform[0])<=0,"high resolution signed waveform");
        }catch(const std::exception& e){failure=e.what();}
        report.replaceWithText(failure.isEmpty()?"PASS: Native command audio/MIDI delete undo/redo across track changes, scoped range deletion/undo, signed waveform extraction, timeline analysis to drum MIDI.":"FAIL: "+failure);
        return failure.isEmpty();
    }

private:
    RecoveryWorker recoveryWorker;
    juce::File recoveryRoot=RecoveryStore::defaultRoot(),recoverySourceRun,recoverySuggestedFile;
    juce::String recoveryRun=RecoveryStore::newRun(),recoveredName,autosaveNotice="Autosave ready";
    std::vector<RecoveryStore::Entry> recoveries;
    std::optional<RecoveryStore::Entry> pendingRecovery;
    juce::int64 editRevision=0,lastAutosaveRevision=-1,savedRevision=0,recoveryGeneration=0,lastAutosaveTime=0;
    double lastAutosaveAttempt=juce::Time::getMillisecondCounterHiRes();
    bool autosaveEnabled=true;
    void markDirty(){dirty=true;++editRevision;}
    void cleanRecovery(){
        const auto run=recoveryRoot.getChildFile(recoveryRun);if(!RecoveryStore::markClean(run)||!RecoveryStore::markClean(recoverySourceRun))autosaveNotice="Recovery cleanup could not be marked; snapshots were retained.";
        recoveries.erase(std::remove_if(recoveries.begin(),recoveries.end(),[&](const auto& entry){return entry.run==run||entry.run==recoverySourceRun;}),recoveries.end());
    }
    void autosaveTick(bool force=false){
        const double now=juce::Time::getMillisecondCounterHiRes();RecoveryStore::Policy policy{autosaveEnabled,dirty,loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||nativeAnalysisPending||chooser!=nullptr,engine.playback.playing.load(),automationPassActive,recoveryWorker.isBusy(),now-lastAutosaveAttempt,editRevision,lastAutosaveRevision};if(!policy.eligible(force))return;
        lastAutosaveAttempt=now;auto snapshot=currentSession();const auto name=sessionFile==juce::File()?(recoveredName.isEmpty()?(newSessionName.isEmpty()?juce::String("Untitled session"):newSessionName):recoveredName):sessionFile.getFileNameWithoutExtension();
        if(!recoveryWorker.save(recoveryRoot,recoveryRun,recoveryGeneration++,editRevision,sessionFile,name,std::move(snapshot)))autosaveNotice="Recovery worker is busy.";else autosaveNotice="Writing recovery snapshot…";
    }
    void recoverSession(const juce::String& id){
        if(loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||recoveryWorker.isBusy()||chooser)return;
        const auto found=std::find_if(recoveries.begin(),recoveries.end(),[&](const auto& entry){return entry.id==id;});if(found==recoveries.end())return;const auto entry=*found;
        confirmDiscard("Recover this session?",[safe=juce::Component::SafePointer<MainComponent>(this),entry]{if(!safe)return;safe->engine.playback.playing=false;safe->pendingRecovery=entry;safe->sessionBusy=safe->sessionWorker.open(entry.snapshot);if(!safe->sessionBusy)safe->pendingRecovery.reset();safe->status="Opening a recovered copy…";safe->publish();});
    }
    void syncPlaylist(int track)
    {
        if(engine.playback.tracks[track].routing.aux)return;
        auto& list=trackPlaylists[track];if(list.empty()){list.push_back({1,"Main",{}, {}});activePlaylists[track]=1;}
        auto found=std::find_if(list.begin(),list.end(),[&](const auto& p){return p.id==activePlaylists[track];});
        if(found!=list.end()){found->clips=engine.playback.tracks[track].clips;found->notes=engine.plugins.midiLanes[track].notes;}
    }
    void selectPlaylist(int track,int id)
    {
        const auto found=std::find_if(trackPlaylists[track].begin(),trackPlaylists[track].end(),[&](const auto& p){return p.id==id;});if(found==trackPlaylists[track].end())return;
        activePlaylists[track]=id;engine.playback.tracks[track].clips=found->clips;engine.plugins.midiLanes[track].notes=found->notes;
        if(engine.playback.tracks[track].midiTrack)engine.playback.tracks[track].midiSeconds=engine.plugins.midiLanes[track].endBeat()*60/controls.bpm+1;
    }
    struct PlaylistChange {int track=0;std::vector<TrackPlaylist> before,after;int beforeActive=1,afterActive=1;};
    struct GroupChange {std::vector<GroupEditing::Layout> before,after;};
    std::vector<PlaylistChange> playlistUndo,playlistRedo;
    std::vector<GroupChange> groupUndo,groupRedo;
    void clearBatchHistory(){playlistUndo.clear();playlistRedo.clear();groupUndo.clear();groupRedo.clear();}
    void clearClipHistory(){undoClips.clear();redoClips.clear();midiRedo.clear();midiUndo.clear();midiRedo.clear();}
    void applyGroup(const std::vector<GroupEditing::Layout>& layouts){for(const auto& layout:layouts){selectPlaylist(layout.track,layout.playlist);if(layout.midi){engine.plugins.midiLanes[layout.track].notes=layout.notes;engine.playback.tracks[layout.track].midiSeconds=engine.plugins.midiLanes[layout.track].endBeat()*60/controls.bpm+1;}else engine.playback.tracks[layout.track].clips=layout.clips;syncPlaylist(layout.track);}}
    bool editorBatchUndo(bool redo){
        auto& groups=redo?groupRedo:groupUndo;auto& playlists=redo?playlistRedo:playlistUndo;
        if(!groups.empty()){auto change=std::move(groups.back());groups.pop_back();engine.detach();applyGroup(redo?change.after:change.before);finishClipEdit();(redo?groupUndo:groupRedo).push_back(std::move(change));markDirty();status=redo?"Group edit redone.":"Group edit undone.";publish(true);return true;}
        if(!playlists.empty()){auto change=std::move(playlists.back());playlists.pop_back();engine.detach();trackPlaylists[change.track]=redo?change.after:change.before;selectPlaylist(change.track,redo?change.afterActive:change.beforeActive);finishClipEdit();(redo?playlistUndo:playlistRedo).push_back(std::move(change));markDirty();status=redo?"Playlist change redone.":"Playlist change undone.";publish(true);return true;}return false;
    }
    void groupCommand(const juce::var& message){
        if(loading||recordingBusy||pluginBusy||sessionBusy||exportBusy)return;
        const auto action=message.getProperty("action",{}).toString();if(action=="undo"||action=="redo"){editorBatchUndo(action=="redo");return;}
        const auto targets=message.getProperty("tracks",{});const auto* ids=targets.getArray();if(!ids||ids->isEmpty()||ids->size()>128)return;
        const auto deltaValue=message.getProperty("delta",0.0);if(!(deltaValue.isInt()||deltaValue.isDouble()))return;const double delta=static_cast<double>(deltaValue);
        std::vector<GroupEditing::Layout> before;for(const auto& id:*ids){if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=trackCount)return;const int track=static_cast<int>(id);const auto& t=engine.playback.tracks[track];if(t.routing.aux)return;for(const auto& old:before)if(old.track==track)return;syncPlaylist(track);before.push_back({track,activePlaylists[track],t.midiTrack,t.clips,engine.plugins.midiLanes[track].notes});}
        auto after=before;if(!GroupEditing::edit(after,action,engine.selectionStart.load(),engine.selectionEnd.load(),delta,controls.bpm)){status="Group edit needs a valid selected range and room on every affected track. No tracks changed.";publish();return;}
        engine.detach();applyGroup(after);finishClipEdit();clearClipHistory();playlistUndo.clear();playlistRedo.clear();groupRedo.clear();groupUndo.push_back({std::move(before),std::move(after)});
        size_t stored=0;const auto count=[](const auto& group){size_t n=0;for(const auto& layout:group.before)n+=layout.clips.size()+layout.notes.size();for(const auto& layout:group.after)n+=layout.clips.size()+layout.notes.size();return n;};for(const auto& group:groupUndo)stored+=count(group);while(groupUndo.size()>1&&(groupUndo.size()>32||stored>262144)){stored-=count(groupUndo.front());groupUndo.erase(groupUndo.begin());}
        markDirty();status="Group range edit applied to active playlists. Undo restores the whole group.";publish(true);
    }
    void playlistCommand(const juce::var& message)
    {
        const auto lane=message.getProperty("track",{});if(!lane.isInt()||static_cast<int>(lane)<0||static_cast<int>(lane)>=trackCount)return;const int track=static_cast<int>(lane);
        const auto requested=message.getProperty("action",{}).toString();if(requested=="undo"||requested=="redo"){editorBatchUndo(requested=="redo");return;}
        if(engine.playback.tracks[track].routing.aux)return;syncPlaylist(track);auto& list=trackPlaylists[track];const auto action=message.getProperty("action",{}).toString();
        const auto key=message.getProperty("id",activePlaylists[track]);if(!key.isInt())return;const int id=static_cast<int>(key);
        auto found=std::find_if(list.begin(),list.end(),[&](const auto& p){return p.id==id;});if(found==list.end())return;
        if(action=="promote")
        {
            if(id==activePlaylists[track])return;const double start=engine.selectionStart.load(),end=engine.selectionEnd.load();if(end<=start){status="Highlight a time range to copy from the alternate playlist.";publish();return;}
            if(engine.playback.tracks[track].midiTrack)
            {
                const double a=start*controls.bpm/60,b=end*controls.bpm/60;if(std::none_of(found->notes.begin(),found->notes.end(),[&](const auto& n){return n.start<b&&n.start+n.length>a;})){status="The alternate playlist has no notes in this range.";publish();return;}auto notes=engine.plugins.midiLanes[track].notes;std::vector<MidiNote> result;int next=1;for(const auto& n:notes)next=std::max(next,n.id+1);
                for(const auto& n:notes){next=std::max(next,n.id+1);if(n.start>=b||n.start+n.length<=a)result.push_back(n);else{if(n.start<a){auto left=n;left.length=a-n.start;result.push_back(left);}if(n.start+n.length>b){auto right=n;right.id=next++;right.start=b;right.length=n.start+n.length-b;result.push_back(right);}}}
                for(auto n:found->notes){const double from=std::max(a,n.start),to=std::min(b,n.start+n.length);if(to>from){n.id=next++;n.start=from;n.length=to-from;result.push_back(n);}}
                applyMidiNotes(track,std::move(result));return;
            }
            auto before=engine.playback.tracks[track].clips,after=before;std::vector<TimelineClip> incoming;
            for(const auto& c:found->clips){const auto a=std::max(start,c.start),b=std::min(end,c.start+c.length);if(b>a)incoming.push_back(ClipLayout::slice(c,a,b,c.id));}
            if(incoming.empty()){status="The alternate playlist has no audio in this range.";publish();return;}
            if(!ClipLayout::replaceRange(after,start,end,incoming)){status="Playlist edit exceeds the clip limit.";publish();return;}
            clearBatchHistory();undoClips.push_back({track,before,after,activePlaylists[track]});if(undoClips.size()>64)undoClips.erase(undoClips.begin());redoClips.clear();midiRedo.clear();engine.detach();engine.playback.tracks[track].clips=std::move(after);syncPlaylist(track);finishClipEdit();markDirty();status="Selected range copied to the active playlist.";publish(true);return;
        }
        auto after=list;int active=activePlaylists[track];const auto name=message.getProperty("name",{}).toString();
        if(!PlaylistEditing::manage(after,active,id,action,name)){status=action=="delete"?"Keep at least one playlist on each track.":"Choose a valid playlist name or move direction; names must be unique.";publish();return;}
        PlaylistChange change{track,list,after,activePlaylists[track],active};
        engine.detach();list=std::move(after);selectPlaylist(track,active);finishClipEdit();clearClipHistory();groupUndo.clear();groupRedo.clear();playlistRedo.clear();playlistUndo.push_back(std::move(change));
        size_t stored=0;const auto count=[](const auto& change){size_t n=0;for(const auto& p:change.before)n+=p.clips.size()+p.notes.size();for(const auto& p:change.after)n+=p.clips.size()+p.notes.size();return n;};for(const auto& item:playlistUndo)stored+=count(item);while(playlistUndo.size()>1&&(playlistUndo.size()>32||stored>262144)){stored-=count(playlistUndo.front());playlistUndo.erase(playlistUndo.begin());}
        markDirty();status=action=="delete"?"Playlist removed. Undo restores it; source audio files are retained.":"Playlist updated. Undo restores this change.";publish(true);
    }
    void chooseMixExport(const juce::var& message)
    {
        if (loading || recordingBusy || sessionBusy || pluginBusy || chooser) return;
        if (settings && settings->isVisible()) { status = "Close Audio settings before exporting."; publish(); return; }
        MixExportOptions options;
        const auto tailValue = message.getProperty("tail", 2.0);
        const auto bitValue = message.getProperty("bits", 24);
        const auto selection = message.getProperty("selection", false);
        if (!(tailValue.isDouble() || tailValue.isInt()) || !bitValue.isInt() || !selection.isBool()) return;
        options.tail = static_cast<double>(tailValue); options.bits = static_cast<int>(bitValue);
        options.start = static_cast<bool>(selection) ? engine.selectionStart.load() : 0;
        options.end = static_cast<bool>(selection) ? engine.selectionEnd.load() : engine.playback.duration();
        if (!std::isfinite(options.tail) || options.tail < 0 || options.tail > 30 ||
            (options.bits != 16 && options.bits != 24) || options.start < 0 || options.end <= options.start ||
            options.end > engine.playback.duration() + 1.e-6 || options.end > 3600)
        { status = "Choose a nonempty session or selection within the session, up to one hour, and a 0â€“30 second tail."; publish(); return; }
        for (const auto& channel : engine.plugins.channels) for (const auto& slot : channel)
            if (slot && !slot->bypass.load() && !slot->instance)
            { status = "Restore or bypass unavailable plugins before exporting."; publish(); return; }
        chooser = std::make_unique<juce::FileChooser>("Export stereo WAV", juce::File(), "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe = juce::Component::SafePointer<MainComponent>(this), options](const juce::FileChooser& dialog)
            {
                if (!safe) return;
                const auto file = dialog.getResult(); safe->chooser.reset();
                if (file == juce::File() || safe->loading || safe->recordingBusy || safe->sessionBusy || safe->pluginBusy) return;
                // The selected filename is used exactly as confirmed by the save dialog.
                for (const auto& info : safe->trackInfo)
                {
                    if (file == info.file) { safe->status = "Choose a different filename; this WAV is used by the session."; safe->publish(); return; }
                    for (const auto& source : info.extra) if (file == source.file)
                    { safe->status = "Choose a different filename; this WAV is used by the session."; safe->publish(); return; }
                }
                safe->engine.playback.playing = false;
                safe->engine.detach(); safe->engine.plugins.rebuildCompensation(safe->engine.playback);
                safe->engine.playback.playbackLatencyFrames = safe->engine.plugins.totalLatency;
                safe->engine.plugins.resetGraph();
                safe->engine.plugins.monitorChannel = -1;
                for (auto& channel : safe->engine.plugins.channels) for (auto& slot : channel)
                    if (slot && slot->window) slot->window->setVisible(false);
                safe->exportFile = file;
                safe->publish(); safe->exportBusy = true;
                if (!safe->mixExporter.start(safe->engine.playback, file, options))
                { safe->exportBusy = false; safe->engine.attach(); safe->status = "Could not start WAV export."; }
                safe->publish();
            });
    }
    struct AutomationPass {Automation::Writer volume,pan;};
    struct AutomationChange {int track;Automation::Lane volumeBefore,panBefore,volumeAfter,panAfter;float gainBefore,panValueBefore,gainAfter,panValueAfter;int modeBefore,modeAfter;};
    std::array<int,128> automationPreviousMode {};
    std::array<AutomationPass,128> automationPasses;
    std::vector<std::vector<AutomationChange>> automationUndo,automationRedo;
    bool automationPassActive=false,automationCurvesChanged=true;
    double automationLastTime=0;
    void rememberAutomation(std::vector<AutomationChange> changes){if(changes.empty())return;automationCurvesChanged=true;automationUndo.push_back(std::move(changes));size_t stored=0;for(const auto& group:automationUndo)for(const auto& c:group)stored+=c.volumeBefore.size()+c.panBefore.size()+c.volumeAfter.size()+c.panAfter.size();while(automationUndo.size()>1&&(automationUndo.size()>32||stored>524288)){for(const auto& c:automationUndo.front())stored-=c.volumeBefore.size()+c.panBefore.size()+c.volumeAfter.size()+c.panAfter.size();automationUndo.erase(automationUndo.begin());}automationRedo.clear();}
    void finishAutomation(){
        if(!automationPassActive)return;
        const double end=std::max(automationLastTime,engine.playback.seconds.load());
        if(end-automationLastTime<.25){for(int id=0;id<trackCount;++id){auto& pass=automationPasses[id];auto& t=engine.playback.tracks[id];if(pass.volume.active)pass.volume.sample(end,t.gain.load(),true,0,1);if(pass.pan.active)pass.pan.sample(end,t.pan.load(),true,-1,1);}for(auto& channel:engine.plugins.channels)for(auto& slot:channel)if(slot)for(const auto& binding:slot->bindings){auto& writer=slot->writers[binding.lane];if(writer.active)writer.sample(end,slot->automation[binding.lane].mode==2?binding.parameter->getValue():slot->activity[binding.index].value.load(),true,0,1);}}
        engine.detach();std::vector<AutomationChange> changes;bool failed=false;
        for(int id=0;id<trackCount;++id){auto& pass=automationPasses[id];auto& t=engine.playback.tracks[id];auto volume=t.volumeAutomation,pan=t.panAutomation;
            const bool v=!pass.volume.segments.empty(),p=!pass.pan.segments.empty();
            if(v||p){if((!v||pass.volume.commit(volume,0,1))&&(!p||pass.pan.commit(pan,-1,1))){const int mode=t.automationMode.load(),after=mode==2?1:mode;changes.push_back({id,t.volumeAutomation,t.panAutomation,volume,pan,t.gain.load(),t.pan.load(),t.gain.load(),t.pan.load(),mode==2?automationPreviousMode[id]:mode,after});t.volumeAutomation=std::move(volume);t.panAutomation=std::move(pan);}else failed=true;}
            if(t.automationMode.load()==2)t.automationMode=1;t.volumeTouched=false;t.panTouched=false;t.volumeLatched=false;t.panLatched=false;pass={};
        }
        for(auto& channel:engine.plugins.channels)for(auto& slot:channel)if(slot){auto lanes=slot->automation;bool changed=false,bad=false;for(size_t n=0;n<slot->writers.size();++n){if(slot->writers[n].segments.empty())continue;if(!slot->writers[n].commit(lanes[n].points,0,1)){bad=true;break;}changed=true;}if(changed&&!bad){slot->rememberAutomation();slot->automation=std::move(lanes);}failed=failed||bad;for(auto& lane:slot->automation)if(lane.mode==2)lane.mode=1;slot->resetGestures();slot->bindAutomation();}
        engine.attach();automationPassActive=false;rememberAutomation(std::move(changes));markDirty();
        status=failed?"An automation pass exceeded its limits; its previous curves were kept.":"Automation saved. Write returned to Read; Touch/Latch are ready for the next pass.";publish(true);
    }
    void automationTick(){
        if(exportBusy||recordingBusy)return;
        if(!engine.playback.playing.load()){finishAutomation();return;}
        const double seek=engine.playback.seekSeconds.load(),time=std::clamp(seek>=0?seek:engine.playback.seconds.load(),0.0,Automation::maxTime);
        if(automationPassActive&&time+.001<automationLastTime){finishAutomation();return;}
        if(engine.playback.automationBypass.load())return;
        bool any=false;
        for(int id=0;id<trackCount;++id){auto& t=engine.playback.tracks[id];const int mode=t.automationMode.load();if(mode<2)continue;any=true;auto& pass=automationPasses[id];pass.volume.sample(time,t.gain.load(),Automation::writes(mode,t.volumeTouched.load(),t.volumeLatched.load()),0,1);pass.pan.sample(time,t.pan.load(),Automation::writes(mode,t.panTouched.load(),t.panLatched.load()),-1,1);}
        for(auto& channel:engine.plugins.channels)for(auto& slot:channel)if(slot&&!slot->bypass.load())for(const auto& binding:slot->bindings){const auto& lane=slot->automation[binding.lane];if(lane.mode<2)continue;any=true;auto& activity=slot->activity[binding.index];const bool pending=activity.pending.exchange(false),touch=activity.touched.load();slot->writers[binding.lane].sample(time,lane.mode==2?binding.parameter->getValue():activity.value.load(),Automation::writes(lane.mode,touch||pending,activity.latched.load()),0,1);if(!touch&&lane.mode==3)slot->writers[binding.lane].sample(time,activity.value.load(),false,0,1);}
        automationPassActive=automationPassActive||any;automationLastTime=time;
    }
    void automationCommand(const juce::var& message)
    {
        if(exportBusy||recordingBusy||loading||sessionBusy||pluginBusy)return;
        const auto laneId=message.getProperty("track",{});if(!laneId.isInt())return;const int id=static_cast<int>(laneId);if(id<0||id>=trackCount)return;
        const auto action=message.getProperty("action",{}).toString();auto& t=engine.playback.tracks[id];
        if(action=="mode"){
            if(engine.playback.playing.load()){status="Stop playback before changing automation modes.";publish();return;}
            const auto mode=message.getProperty("mode",{});if(!mode.isInt()||static_cast<int>(mode)<0||static_cast<int>(mode)>4)return;
            if(static_cast<int>(mode)>=2&&engine.playback.loop.load()){status="Turn Loop off before writing automation.";publish();return;}
            if(static_cast<int>(mode)==2)automationPreviousMode[id]=t.automationMode.load()==2?automationPreviousMode[id]:t.automationMode.load();
            if(static_cast<int>(mode)!=1&&t.automationMode.load()==1){const auto position=engine.playback.seconds.load();controls.channels[id].gain=Automation::value(t.volumeAutomation,position,t.gain.load());controls.channels[id].pan=Automation::value(t.panAutomation,position,t.pan.load());applyChannels();}
            t.volumeTouched=false;t.panTouched=false;t.volumeLatched=false;t.panLatched=false;t.automationMode=static_cast<int>(mode);markDirty();publish(true);return;
        }
        engine.playback.playing=false;finishAutomation();engine.detach();
        if(action=="undo"||action=="redo"){
            auto& from=action=="undo"?automationUndo:automationRedo;auto& to=action=="undo"?automationRedo:automationUndo;
            if(!from.empty()){auto changes=std::move(from.back());from.pop_back();for(const auto& c:changes){auto& track=engine.playback.tracks[c.track];track.volumeAutomation=action=="undo"?c.volumeBefore:c.volumeAfter;track.panAutomation=action=="undo"?c.panBefore:c.panAfter;track.automationMode=action=="undo"?c.modeBefore:c.modeAfter;controls.channels[c.track].gain=action=="undo"?c.gainBefore:c.gainAfter;controls.channels[c.track].pan=action=="undo"?c.panValueBefore:c.panValueAfter;}applyChannels();to.push_back(std::move(changes));markDirty();}
        }else{
            auto volume=t.volumeAutomation,pan=t.panAutomation;const bool isPan=message.getProperty("parameter",{}).toString()=="pan";auto& lane=isPan?pan:volume;bool ok=false;
            const auto seconds=message.getProperty("time",{});const auto value=message.getProperty("value",{});
            if(action=="clear"){lane.clear();ok=true;}
            else if((seconds.isDouble()||seconds.isInt()||seconds.isInt64())&&std::isfinite(static_cast<double>(seconds))){
                const double time=static_cast<double>(seconds);
                if(action=="put"&&(value.isDouble()||value.isInt()))ok=Automation::put(lane,{time,static_cast<float>(value)},isPan?-1.f:0.f,1.f);
                else if(action=="delete"){auto found=std::find_if(lane.begin(),lane.end(),[time](const auto& point){return std::abs(point.time-time)<1.e-8;});if(found!=lane.end()){lane.erase(found);ok=true;}}
            }
            if(ok){rememberAutomation({{id,t.volumeAutomation,t.panAutomation,volume,pan,t.gain.load(),t.pan.load(),t.gain.load(),t.pan.load(),t.automationMode.load(),1}});t.volumeAutomation=std::move(volume);t.panAutomation=std::move(pan);t.automationMode=1;markDirty();status="Automation updated. Press Play to hear the curve.";}else status="Choose a valid automation point within the lane limits.";
        }
        automationCurvesChanged=true;engine.attach();publish(true);
    }
    int pluginAutomationChannel=-1,pluginAutomationSlot=0;
    juce::String pluginAutomationParameter;
    void pluginAutomationCommand(const juce::var& message){
        const auto c=message.getProperty("channel",{}),i=message.getProperty("slot",{});if(!c.isInt()||!i.isInt())return;
        const int channel=static_cast<int>(c),index=static_cast<int>(i);if(channel<0||channel>128||(channel!=128&&channel>=trackCount)||index<0||index>=5)return;
        if(exportBusy||recordingBusy||loading||sessionBusy||pluginBusy)return;
        const auto action=message.getProperty("action",{}).toString(),id=message.getProperty("parameter",{}).toString();
        pluginAutomationChannel=channel;pluginAutomationSlot=index;pluginAutomationParameter=id;
        auto& slot=engine.plugins.channels[static_cast<size_t>(channel)][static_cast<size_t>(index)];
        if(action=="select"){publish(true);return;}
        if(!slot||!slot->instance||engine.playback.playing.load()){status="Stop playback before editing plugin automation.";publish();return;}
        if(action=="undo"||action=="redo"){
            auto& from=action=="undo"?slot->automationUndo:slot->automationRedo;auto& to=action=="undo"?slot->automationRedo:slot->automationUndo;
            if(!from.empty()){engine.detach();to.push_back(std::move(slot->automation));slot->automation=std::move(from.back());from.pop_back();slot->bindAutomation();engine.attach();markDirty();}publish(true);return;
        }
        juce::AudioProcessorParameter* parameter=nullptr;for(int p=0;p<slot->instance->getParameters().size();++p){auto* candidate=slot->instance->getParameters()[p];if(candidate&&candidate->isAutomatable()&&PluginRack::parameterId(candidate)==id){parameter=candidate;break;}}
        if(!parameter)return;
        auto lanes=slot->automation;auto found=std::find_if(lanes.begin(),lanes.end(),[&](const auto& lane){return lane.id==id;});
        if(found==lanes.end()){if(lanes.size()>=PluginAutomation::maxLanes){status="This insert already has 64 automation lanes.";publish();return;}lanes.push_back({id,parameter->getName(512),{},true});found=std::prev(lanes.end());}
        bool ok=false;
        if(action=="mode"&&message.getProperty("read",{}).isBool()){found->read=static_cast<bool>(message.getProperty("read",{}));found->mode=found->read?1:0;ok=true;}
        else if(action=="writeMode"){const auto mode=message.getProperty("mode",{});if(mode.isInt()&&static_cast<int>(mode)>=0&&static_cast<int>(mode)<=4){if(static_cast<int>(mode)>=2&&engine.playback.loop.load()){status="Turn Loop off before writing automation.";publish();return;}found->mode=static_cast<int>(mode);found->read=found->mode!=0;ok=true;}}
        else if(action=="clear"){lanes.erase(found);ok=true;}
        else {const auto t=message.getProperty("time",{}),v=message.getProperty("value",{});if((t.isInt()||t.isInt64()||t.isDouble())&&std::isfinite(static_cast<double>(t))){const double time=static_cast<double>(t);if(action=="put"&&(v.isInt()||v.isDouble())){ok=Automation::put(found->points,{time,static_cast<float>(v)},0,1);found->read=true;found->mode=1;}else if(action=="delete"){auto point=std::find_if(found->points.begin(),found->points.end(),[time](const auto& p){return std::abs(p.time-time)<1.e-8;});if(point!=found->points.end()){found->points.erase(point);ok=true;}}}}
        if(ok){engine.detach();slot->rememberAutomation();slot->automation=std::move(lanes);slot->bindAutomation();engine.attach();markDirty();status="Plugin automation saved. Read follows the curve; Off leaves the current control value.";}else status="Choose a valid plugin automation point.";
        publish(true);
    }
    void createInsert(size_t channel, int index, juce::PluginDescription description,
                      juce::MemoryBlock state = {}, bool bypass = false, bool restoring = false, juce::var automation = {})
    {
        if(description.pluginFormatName=="VST3"&&description.name=="AVRX Drums"&&description.manufacturerName=="AVRX")description=NativeInstruments::description(true);
        if(description.pluginFormatName=="Tracksmith Native"){
            auto instance=NativeInstruments::create(description);engine.detach();const auto result=engine.plugins.install(channel,index,description,std::move(instance),state,bypass,engine.playback.deviceRate);engine.plugins.restoreAutomation(channel,index,automation);if(auto& slot=engine.plugins.channels[channel][static_cast<size_t>(index)];slot&&slot->instance){const auto token=slot->token;NativeInstruments::bindGrooveToSong(slot->instance.get(),[safe=juce::Component::SafePointer<MainComponent>(this),token]{if(!safe)return;for(int id=0;id<safe->trackCount;++id){const auto& current=safe->engine.plugins.channels[id][0];if(current&&current->token==token){safe->copyNativeGrooveToSong(id);return;}}});}engine.plugins.resetGraph();engine.attach();
            if(restoring){if(result.failed())missingPlugins.add(description.name);++restoreIndex;restoreNext();}else{status=result.wasOk()?description.name+" is ready.":result.getErrorMessage();if(result.wasOk())markDirty();publish(true,static_cast<int>(channel));}return;
        }
        if(description.pluginFormatName=="VST3"&&(description.name=="AVRX NAM"||description.name=="Tracksmith NAM")&&(description.manufacturerName=="AVRX"||description.manufacturerName=="Tracksmith")){
            const auto bundled=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Stock Plugins/Tracksmith NAM.vst3");if(bundled.exists()){description.fileOrIdentifier=bundled.getFullPathName();description.name=description.descriptiveName="Tracksmith NAM";description.manufacturerName="Tracksmith";}
        }
        if(restoring && PluginDiscovery::blocked(description.createIdentifierString())){
            engine.detach();engine.plugins.install(channel,index,description,nullptr,state,bypass,engine.playback.deviceRate);engine.plugins.restoreAutomation(channel,index,automation);engine.attach();missingPlugins.add(description.name+" (previous load interrupted)");++restoreIndex;restoreNext();return;
        }
        PluginDiscovery::loadMarker().replaceWithText(description.createIdentifierString());
        pluginBusy = true; engine.playback.playing = false;
        const double rate = engine.playback.deviceRate;
        engine.plugins.formats.createPluginInstanceAsync(description, rate, PlaybackCore::processBlockSize,
            [safe = juce::Component::SafePointer<MainComponent>(this), channel, index, description, state, bypass, restoring, rate, automation]
            (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
            {
                if (!safe) return;
                safe->engine.detach();
                auto result = instance ? safe->engine.plugins.install(channel, index, description, std::move(instance), state, bypass, rate)
                                       : juce::Result::fail(error.isEmpty() ? "Plugin could not be loaded." : error);
                if (result.failed() && restoring)
                {
                    safe->engine.plugins.install(channel, index, description, nullptr, state, bypass, rate);
                    safe->missingPlugins.add(description.name);
                }
                if(result.wasOk()||restoring)safe->engine.plugins.restoreAutomation(channel,index,automation);
                safe->engine.plugins.resetGraph();
                PluginDiscovery::loadMarker().deleteFile();
                if(result.wasOk())PluginDiscovery::allow(description.createIdentifierString());
                safe->engine.playback.seekSeconds = safe->engine.playback.seconds.load();
                safe->engine.attach();
                if (restoring) { ++safe->restoreIndex; safe->restoreNext(); }
                else
                {
                    safe->pluginBusy = false;
                    safe->status = result.wasOk() ? "Loaded " + description.name + ". Click its insert to open controls." : result.getErrorMessage();
                    if (result.wasOk()) safe->markDirty();
                    safe->publish();
                }
            });
    }
    void restoreNext()
    {
        if (restoreIndex >= restoringInserts.size())
        {
            pluginBusy = false; restoringInserts.clear();
            if (!missingPlugins.isEmpty()) status += ". Unavailable inserts preserved: " + missingPlugins.joinIntoString(", ");
            missingPlugins.clear(); publish(); return;
        }
        const auto item = restoringInserts[restoreIndex]; juce::PluginDescription description;
        auto xml = juce::parseXML(item.getProperty("description", {}).toString());
        if (!xml || !description.loadFromXml(*xml) || (description.pluginFormatName != "VST3"&&description.pluginFormatName!="Tracksmith Native"))
        { missingPlugins.add("Invalid plugin description"); ++restoreIndex; restoreNext(); return; }
        juce::MemoryBlock state; state.fromBase64Encoding(item.getProperty("state", {}).toString());
        createInsert(static_cast<size_t>(static_cast<int>(item.getProperty("channel", {}))),
                     static_cast<int>(item.getProperty("slot", {})), description, state,
                     static_cast<bool>(item.getProperty("bypass", false)), true, item.getProperty("automation",juce::Array<juce::var>()));
    }
    void choosePlugin(size_t channel, int index)
    {
        if (chooser) return;
        const auto stock = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Stock Plugins");
        chooser = std::make_unique<juce::FileChooser>("Choose a Windows x64 VST3 effect or instrument",
            stock.isDirectory() ? stock : juce::File("C:/Program Files/Common Files/VST3"), "*.vst3");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
            [safe = juce::Component::SafePointer<MainComponent>(this), channel, index](const juce::FileChooser& selection)
            {
                if (!safe) return;
                const auto file = selection.getResult();
                juce::MessageManager::callAsync([safe, channel, index, file]
                {
                    if (!safe) return; safe->chooser.reset();
                    if (file == juce::File()) return;
                    if (!file.hasFileExtension("vst3")) { safe->status = "Choose a .vst3 file or bundle."; safe->publish(); return; }
                    safe->pluginBusy = true; safe->engine.playback.playing = false; safe->status = "Inspecting " + file.getFileName() + "..."; safe->publish();
                    safe->discoverPlugin(file,channel,index);

                });
            });
    }
    void discoverPlugin(const juce::File& file,size_t channel,int index)
    {
        if(discovery.isThreadRunning())return;
        pluginBusy=true;engine.playback.playing=false;status="Scanning "+file.getFileName()+"...";publish();
        discovery.begin(file,[safe=juce::Component::SafePointer<MainComponent>(this),channel,index](juce::String xml)
        {
            if(!safe)return;
            juce::KnownPluginList list;auto tree=juce::parseXML(xml);if(tree)list.recreateFromXml(*tree);
            safe->pluginBusy=false;
            if(list.getNumTypes()==0){safe->status="Plugin scan failed or timed out. Tracksmith kept running; choose another plugin or retry.";safe->publish();return;}
            auto cache=safe->pluginLibrary();for(const auto& d:list.getTypes())cache->addType(d);
            if(auto saved=cache->createXml())saved->writeTo(PluginDiscovery::cacheFile());
            safe->status="Plugin added to the browser. Choose it from the insert menu.";safe->publish();safe->insertMenu(channel,index);
        });
    }
    std::unique_ptr<juce::KnownPluginList> pluginLibrary()
    {
        auto list=std::make_unique<juce::KnownPluginList>();auto xml=juce::XmlDocument::parse(PluginDiscovery::cacheFile());if(xml)list->recreateFromXml(*xml);PluginCatalog::deduplicate(*list,juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Stock Plugins"));return list;
    }
    void insertMenu(size_t channel, int index, juce::String query = {})
    {
        const auto stock=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Stock Plugins/Tracksmith NAM.vst3");
        const auto plate=stock.getParentDirectory().getChildFile("Tracksmith Plate.vst3");
        const auto delay=stock.getParentDirectory().getChildFile("Tracksmith Digital Delay.vst3");
        const auto limiter=stock.getParentDirectory().getChildFile("Tracksmith Limiter.vst3");
        const auto deesser=stock.getParentDirectory().getChildFile("Tracksmith De-Esser.vst3");
        const auto fet=stock.getParentDirectory().getChildFile("Tracksmith FET Compressor.vst3");
        const auto busComp=stock.getParentDirectory().getChildFile("Tracksmith Bus Compressor.vst3");
        const auto gate=stock.getParentDirectory().getChildFile("Tracksmith Gate.vst3");
        const auto eq=stock.getParentDirectory().getChildFile("Tracksmith EQ 7.vst3");
        auto descriptions=std::make_shared<juce::Array<juce::PluginDescription>>(pluginLibrary()->getTypes());
        juce::PopupMenu menu;menu.addSectionHeader("Plugin browser · saved VST3s");
        for(int i=0;i<descriptions->size();++i){const auto& d=descriptions->getReference(i);if(query.isNotEmpty()&&!(d.name+" "+d.manufacturerName).containsIgnoreCase(query))continue;const bool allowed=!d.isInstrument||(channel<128&&engine.plugins.midiLanes[channel].enabled&&index==0);menu.addItem(100+i,d.name+" — "+d.manufacturerName+(d.isInstrument?" (Instrument)":""),allowed);}
        const auto& unavailable=engine.plugins.channels[channel][static_cast<size_t>(index)];menu.addItem(3,"Retry unavailable insert with saved settings",unavailable&&!unavailable->instance);
        menu.addItem(4,"Search plugins...");menu.addSeparator();
        auto stockFallback=[&](int id,const juce::File& file){bool listed=false;for(const auto& d:*descriptions)if(juce::File(d.fileOrIdentifier).getFileName()==file.getFileName()){listed=true;break;}if(file.exists()&&!listed)menu.addItem(id,file.getFileNameWithoutExtension());};
        stockFallback(11,plate);stockFallback(12,delay);stockFallback(9,limiter);stockFallback(10,deesser);stockFallback(8,fet);stockFallback(7,busComp);stockFallback(6,gate);stockFallback(5,eq);stockFallback(1,stock);menu.addItem(2,"Add / rescan VST3...");
        menu.showMenuAsync(juce::PopupMenu::Options(),[safe=juce::Component::SafePointer<MainComponent>(this),channel,index,stock,eq,gate,busComp,fet,limiter,deesser,plate,delay,descriptions](int result)
        {
            if(!safe||safe->loading||safe->sessionBusy||safe->recordingBusy||safe->pluginBusy)return;
            if(result==4){
                if(safe->pluginSearch)return;
                safe->pluginSearch=std::make_unique<juce::AlertWindow>("Search plugins","Search by plugin name or manufacturer. Leave blank to show all.",juce::MessageBoxIconType::NoIcon);
                safe->pluginSearch->addTextEditor("query", "", "Search");safe->pluginSearch->addButton("Search",1,juce::KeyPress(juce::KeyPress::returnKey));safe->pluginSearch->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
                safe->pluginSearch->enterModalState(true,juce::ModalCallbackFunction::create([safe,channel,index](int answer){if(!safe)return;auto query=safe->pluginSearch->getTextEditorContents("query").trim();juce::MessageManager::callAsync([safe,channel,index,answer,query]{if(!safe)return;safe->pluginSearch.reset();if(answer==1)safe->insertMenu(channel,index,query);});}),false);
            }
            else if(result==3){const auto& old=safe->engine.plugins.channels[channel][static_cast<size_t>(index)];if(old&&!old->instance)safe->createInsert(channel,index,old->description,old->retainedState,old->bypass.load(),false,PluginAutomation::encode(old->automation));}
            else if(result==11)safe->discoverPlugin(plate,channel,index);
            else if(result==12)safe->discoverPlugin(delay,channel,index);
            else if(result==9)safe->discoverPlugin(limiter,channel,index);
            else if(result==10)safe->discoverPlugin(deesser,channel,index);
            else if(result==8)safe->discoverPlugin(fet,channel,index);
            else if(result==7)safe->discoverPlugin(busComp,channel,index);
            else if(result==6)safe->discoverPlugin(gate,channel,index);
            else if(result==5)safe->discoverPlugin(eq,channel,index);
            else if(result==1)safe->discoverPlugin(stock,channel,index);
            else if(result==2)safe->choosePlugin(channel,index);
            else if(result>=100&&result-100<descriptions->size()){
                const auto d=descriptions->getReference(result-100);
                const auto& old=safe->engine.plugins.channels[channel][static_cast<size_t>(index)];
                const bool retry=old&&!old->instance&&old->description.isDuplicateOf(d);
                safe->createInsert(channel,index,d,retry?old->retainedState:juce::MemoryBlock(),retry?old->bypass.load():false,false,retry?juce::var(PluginAutomation::encode(old->automation)):juce::var());
            }
        });
    }
    void newTrackDialog()
    {
        if(trackDialog)return;
        trackDialog=std::make_unique<juce::AlertWindow>("New Tracks","Choose tracks to create. Aux / Bus tracks are stereo returns.",juce::MessageBoxIconType::NoIcon);
        trackDialog->addComboBox("kind",{"Audio","Instrument","Aux / Bus"},"Track type");
        trackDialog->addComboBox("format",{"Mono","Stereo"},"Audio input format");
        trackDialog->addTextEditor("count","1","Number of tracks");trackDialog->addTextEditor("name","","Name (optional)");
        trackDialog->addButton("Create",1,juce::KeyPress(juce::KeyPress::returnKey));trackDialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
        trackDialog->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<MainComponent>(this)](int result){
            if(!safe)return;
            if(result==1&&!safe->loading&&!safe->sessionBusy&&!safe->recordingBusy&&!safe->pluginBusy){
                const int kind=safe->trackDialog->getComboBoxComponent("kind")->getSelectedItemIndex();
                const int count=safe->trackDialog->getTextEditorContents("count").getIntValue();
                const auto name=safe->trackDialog->getTextEditorContents("name").trim().substring(0,70);
                int auxCount=0;for(int i=0;i<safe->trackCount;++i)if(safe->engine.playback.tracks[i].routing.aux)++auxCount;
                if(count<1||count>128-safe->trackCount||(kind==2&&count>16-auxCount)){safe->status="Choose a count within the 128-track and 16-aux limits.";safe->publish();}
                else for(int i=0;i<count;++i){auto* m=new juce::DynamicObject();m->setProperty("type","addTrack");m->setProperty("kind",kind==2?"AUX":kind==1?"INSTRUMENT":"AUDIO");m->setProperty("inputChannels",safe->trackDialog->getComboBoxComponent("format")->getSelectedItemIndex()+1);if(name.isNotEmpty())m->setProperty("name",name+(count>1?" "+juce::String(i+1):juce::String()));safe->command(juce::var(m));}
            }
            juce::MessageManager::callAsync([safe]{if(safe)safe->trackDialog.reset();});
        }),false);
    }
    void confirmDiscard(const juce::String& title, std::function<void()> proceed)
    {
        if (!dirty) { proceed(); return; }
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle(title).withMessage("This session has unsaved changes. Save first to keep them, or continue without saving.")
            .withButton("Continue without saving").withButton("Cancel"),
            [safe = juce::Component::SafePointer<MainComponent>(this), proceed = std::move(proceed)](int result)
            { if (safe != nullptr && result == 1) proceed(); });
    }
    SavedSession currentSession()
    {
        if(automationPassActive){engine.playback.playing=false;finishAutomation();}
        SavedSession saved; saved.mainOutputPair=engine.mainOutputPair.load();saved.ioDeviceKey=engine.sessionDeviceKey; saved.bpm = controls.bpm; saved.master = engine.playback.masterGain.load();
        saved.loop = engine.playback.loop.load(); saved.metronome = engine.playback.metronome.load();
        saved.preRollBars = engine.preRollBars.load(); saved.metronomeGain = engine.playback.metronomeGain.load();
        for (int id = 0; id < trackCount; ++id)
        {
            const auto index = static_cast<size_t>(id); const auto& c = controls.channels[index];
            SessionTrack track {trackNames[index],trackInfo[index].file,c.gain,c.pan,c.muted,c.solo,c.armed};
            syncPlaylist(id);track.playlists=trackPlaylists[index];track.activePlaylist=activePlaylists[index];
            track.volumeAutomation=engine.playback.tracks[index].volumeAutomation;track.panAutomation=engine.playback.tracks[index].panAutomation;track.automationMode=engine.playback.tracks[index].automationMode.load()==0?0:1;
            track.routing=engine.playback.routingSnapshot(index);
            track.stereoLeft=engine.playback.tracks[index].stereoLeft.load();track.stereoRight=engine.playback.tracks[index].stereoRight.load();
            track.inputSource=engine.trackInputs[index].load();track.inputChannels=engine.trackInputChannels[index].load();
            track.midiTrack=engine.playback.tracks[index].midiTrack;track.notes=engine.plugins.midiLanes[index].notes;
            track.timelineEnabled=true;track.clips=engine.playback.tracks[index].clips;track.sources.push_back(trackInfo[index].file);
            for(const auto& source:trackInfo[index].extra)track.sources.push_back(source.file);
            saved.tracks.push_back(std::move(track));
        }
        const bool wasPlaying = engine.playback.playing.load();
        engine.detach(); saved.inserts = engine.plugins.save(); engine.attach(); engine.playback.playing = wasPlaying;
        return saved;
    }
    void saveTo(const juce::File& file)
    {
        if (engine.plugins.consumeChanges()) ++pluginChangeSerial;
        savingPluginSerial = pluginChangeSerial;
        sessionBusy = sessionWorker.save(file, SessionFile::encode(currentSession(), file));
        status = sessionBusy ? "Saving session..." : "Another session operation is in progress."; publish();
    }
    void chooseSession(bool saving)
    {
        if(nativeAnalysisPending){status="Wait for Groove to finish analyzing before opening or saving a session.";publish();return;}
        if(recoveryWorker.isBusy()){status="Wait for the recovery snapshot before opening or saving a session.";publish();return;}
        if (chooser || loading || sessionBusy) return;
        if (saving && sessionFile != juce::File()) { saveTo(sessionFile); return; }
        chooser = std::make_unique<juce::FileChooser>(saving ? "Save Tracksmith session" : "Open Tracksmith session",
            saving ? (recoverySuggestedFile!=juce::File()?recoverySuggestedFile:juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile((newSessionName.isEmpty()?juce::String("Untitled"):juce::File::createLegalFileName(newSessionName))+".tracksmith")) : juce::File(), "*.tracksmith");
        const int chooserFlags = saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting
                                 : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(chooserFlags, [safe = juce::Component::SafePointer<MainComponent>(this), saving](const juce::FileChooser& selection)
        {
            if (safe == nullptr) return;
            auto file = selection.getResult();
            if (file != juce::File())
            {
                if (saving)
                {
                    if (!file.hasFileExtension("tracksmith")) { safe->status = "Use a filename ending in .tracksmith."; safe->publish(); }
                    else safe->saveTo(file);
                }
                else { safe->sessionBusy = safe->sessionWorker.open(file); safe->status = safe->sessionBusy ? "Opening session..." : "Another session operation is in progress."; safe->publish(); }
            }
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->chooser.reset(); });
        });
    }
    void startRecording(int channels)
    {
        if (chooser || loading || sessionBusy || recordingBusy || pluginBusy)
        {
            status = chooser ? "Close the file dialog before recording."
                   : recordingBusy ? "Wait for the previous take to finish."
                   : pluginBusy ? "Wait for the plugin to finish loading before recording."
                   : "Wait for the current file operation to finish before recording.";
            publish(); return;
        }
        juce::ignoreUnused(channels);
        engine.recordingRoutes.clear();
        const auto folder=sessionFile==juce::File()?juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Tracksmith Recordings"):sessionFile.getParentDirectory().getChildFile("Recordings");
        for(int id=0;id<trackCount;++id)if(controls.channels[id].armed)
        {
            const auto& lane=engine.playback.tracks[id];
            if(lane.midiTrack||lane.routing.aux){status="Disarm MIDI and aux lanes; this build records audio inputs only.";publish();return;}
            if(lane.extra.size()+1>=ClipLayout::maximumSources || lane.clips.size()*2+1>ClipLayout::maximumClips){status="An armed track has reached its source/clip limit.";publish();return;}
            const auto name="Take-"+juce::String(id+1)+"-"+juce::Uuid().toString()+".wav";
            engine.recordingRoutes.push_back({id,engine.trackInputs[id].load(),engine.trackInputChannels[id].load(),folder.getChildFile(name)});
        }
        if(engine.recordingRoutes.empty()||engine.recordingRoutes.size()>RecordGroup::maximumTracks){status="Arm between one and 32 audio tracks, then press Record.";publish();return;}
        const int armed=engine.recordingRoutes.front().track;
        lastTakeFile=engine.recordingRoutes.front().file;
        if(engine.monitor.enabled.load())
        {
            const int monitored=engine.monitor.track.load();
            engine.inputSource=engine.trackInputs[monitored].load();engine.inputMeterChannels=engine.trackInputChannels[monitored].load();
        }
        else {engine.inputSource=engine.trackInputs[armed].load();engine.inputMeterChannels=engine.trackInputChannels[armed].load();}
        const auto started = engine.startRecording(static_cast<size_t>(armed), lastTakeFile, engine.trackInputChannels[armed].load());
        if (started.failed()) { status = started.getErrorMessage(); publish(); return; }
        if(engine.monitor.enabled.load()){const int id=engine.monitor.track.load();engine.inputSource=engine.trackInputs[id].load();engine.inputMeterChannels=engine.trackInputChannels[id].load();}
        recordingBusy = true; recordingTrack = armed; takeWarning.clear();recordingIssues.clear();pendingRecorded.clear();
        status = engine.takeWasPunch?"Punch armed: playback begins before the highlighted range, capture ends at selection end.":"Recording "+juce::String(static_cast<int>(engine.recordingRoutes.size()))+" armed audio track(s). Press Stop to finish.";
        publish(true);
    }
    void loadNextRecordedTake()
    {
        loadingTake=false;loading=false;
        while(!pendingRecorded.empty())
        {
            auto item=std::move(pendingRecorded.front());pendingRecorded.erase(pendingRecorded.begin());
            const auto& take=item.take;
            if(engine.countInCancelled&&take.frames==0)continue;
            if(take.error.isNotEmpty()){recordingIssues.add(take.error+" File: "+take.file.getFullPathName());continue;}
            lastTakeFile=take.file;takeWarning=take.warning;expectedTakeFrames=take.frames;importingTrack=item.track;
            loadingTake=loader.load(take.file,engine.importBudget(static_cast<size_t>(item.track),true));loading=loadingTake;
            if(loadingTake){recordingBusy=true;status="Preparing recorded audio for "+trackNames[item.track]+"...";return;}
            recordingIssues.add("Take saved at "+take.file.getFullPathName()+"; import it manually.");
        }
        recordingBusy=false;
        status=engine.countInCancelled?"Recording cancelled before capture.":"Recorded takes are ready.";
        if(!recordingIssues.isEmpty()){status+=" "+recordingIssues.joinIntoString(" ");recordingIssues.clear();}
    }
    bool copyNativeGrooveToSong(int track)
    {
        if(track<0||track>=trackCount||recordingBusy||loading||sessionBusy||exportBusy)return false;auto& slot=engine.plugins.channels[track][0];if(!slot||!slot->instance||!engine.plugins.midiLanes[track].enabled)return false;
        if(NativeInstruments::grooveInfo(slot->instance.get()).busy){status="Wait for Groove to finish analysis or loading.";publish();return false;}
        auto notes=NativeInstruments::grooveNotes(slot->instance.get());if(notes.empty()){status="No drum hits detected. Try a clearer rhythmic recording.";publish();return false;}if(notes.size()>MidiLane::maximumNotes){status="This groove exceeds the song MIDI note limit. Shorten it in Groove first.";publish();return false;}
        NativeInstruments::useSongMidi(slot->instance.get());applyMidiNotes(track,std::move(notes));status="Groove is in your song. Open Groove to shape the sounds.";publish(true,track);return true;
    }
    void checkNativeAnalysis()
    {
        if(!nativeAnalysisPending)return;int found=-1;for(int id=0;id<trackCount;++id){const auto& slot=engine.plugins.channels[id][0];if(slot&&slot->token==nativeAnalysisToken){found=id;break;}}
        if(found<0){nativeAnalysisPending=false;nativeAnalysisTrack=-1;return;}nativeAnalysisTrack=found;auto& slot=engine.plugins.channels[found][0];const auto result=NativeInstruments::grooveInfo(slot->instance.get());if(result.busy)return;
        if(recordingBusy||loading||sessionBusy||exportBusy)return;nativeAnalysisPending=false;if(result.valid){nativeAnalysisMessage=copyNativeGrooveToSong(found)?result.status:status;}else{nativeAnalysisMessage=result.status;status=result.status;}
    }
    void timerCallback() override
    {
        automationTick();checkNativeAnalysis();bool nativeChanged=false;for(auto& channel:engine.plugins.channels)for(auto& slot:channel)if(slot&&slot->instance&&NativeInstruments::consumeGrooveChanges(slot->instance.get())){slot->changed=true;nativeChanged=true;}if(nativeChanged)publish(true);
        if (exportBusy) { publish(); return; }
        if (engine.plugins.consumeChanges()) { markDirty(); ++pluginChangeSerial; }
        if (engine.plugins.latencyChanged())
        {
            if (recordingBusy && engine.recorder.isActive())
            { engine.stopRecording(); status = "Plugin latency changed; finishing the take and rebuilding compensation."; }
            else if (!recordingBusy)
            { engine.refreshPluginLatency(); status = "Plugin latency changed. Playback stopped and compensation rebuilt; paths over the limit are bypassed."; }
        }
        if (recordingBusy && !loadingTake && engine.recorder.isFinished())
        {
            engine.stopRecording(false,engine.takeWasPunch && !engine.countInCancelled);
            pendingRecorded=engine.recorder.takeResults();
            loadNextRecordedTake();
        }
        autosaveTick();publish();
    }
    void setSessionTempo(double tempo){
        const bool playing=engine.playback.playing.load();engine.detach();controls.bpm=tempo;engine.playback.bpm=tempo;
        for(int id=0;id<trackCount;++id)if(engine.playback.tracks[id].midiTrack)engine.playback.tracks[id].midiSeconds=engine.plugins.midiLanes[id].endBeat()*60/tempo+1;
        engine.playback.refreshDuration();engine.plugins.resetGraph();engine.attach();engine.playback.playing=playing;markDirty();
    }
    void stopRemovedClick(bool removed){
        if(!removed)return;for(const auto& channel:engine.plugins.channels)for(const auto& slot:channel)if(slot&&slot->description.fileOrIdentifier=="tracksmith-native-click")return;
        engine.playback.metronome=false;
    }
    void applyChannels()
    {
        for (size_t id = 0; id < controls.channels.size(); ++id)
        {
            const auto& channel = controls.channels[id];
            auto& track = engine.playback.tracks[id];
            track.gain.store(channel.gain); track.pan.store(channel.pan);
            track.muted.store(channel.muted); track.solo.store(channel.solo);engine.armedMonitorMask[id]=channel.armed&&!track.midiTrack&&!track.routing.aux;
        }
    }
    void publish(bool includeWaveform = false, int selectTrack = -1)
    {
        if (exportBusy)
        {
            auto progressState = exportSnapshot.clone();
            auto* o = progressState.getDynamicObject();
            if (o) { o->setProperty("exportBusy", true); o->setProperty("loading", true);
                o->setProperty("playing", false); o->setProperty("exportProgress", mixExporter.progress.load());
                o->setProperty("status", "Exporting WAV — " + juce::String(static_cast<int>(mixExporter.progress.load() * 100)) + "%"); }
            browser.emitEventIfBrowserIsVisible("audioState", progressState); return;
        }
        auto* object = new juce::DynamicObject();
        juce::var snapshot(object);
        auto* device = engine.devices.getCurrentAudioDevice();
        object->setProperty("autosaveEnabled",autosaveEnabled);object->setProperty("autosaveBusy",recoveryWorker.isBusy());object->setProperty("autosaveTime",static_cast<double>(lastAutosaveTime));object->setProperty("autosaveNotice",autosaveNotice);
        juce::Array<juce::var> recoveryItems;for(const auto& entry:recoveries){auto* item=new juce::DynamicObject();item->setProperty("id",entry.id);item->setProperty("name",entry.name);item->setProperty("time",static_cast<double>(entry.time));recoveryItems.add(juce::var(item));}object->setProperty("recoveries",recoveryItems);
        object->setProperty("exportBusy", false);
        object->setProperty("exportProgress", 0.0);
        object->setProperty("playing", engine.playback.playing.load());
        object->setProperty("seconds", engine.playback.seconds.load());
        object->setProperty("duration", engine.playback.duration());
        object->setProperty("peak", engine.playback.peak.load());
        object->setProperty("peakLeft",engine.outputPeakLeft.load());object->setProperty("peakRight",engine.outputPeakRight.load());
        object->setProperty("clipping", engine.playback.clipping.load());
        object->setProperty("sampleRate", device ? device->getCurrentSampleRate() : 0.0);
        object->setProperty("driver", engine.devices.getCurrentAudioDeviceType());
        object->setProperty("deviceName", device ? device->getName() : juce::String());
        object->setProperty("bufferSize", device ? device->getCurrentBufferSizeSamples() : 0);
        object->setProperty("cpuLoad", engine.devices.getCpuUsage()); object->setProperty("xruns", engine.devices.getXRunCount());
       #if JUCE_DEBUG
        object->setProperty("debugBuild", true);
       #else
        object->setProperty("debugBuild", false);
       #endif
        object->setProperty("inputLatency", device ? device->getInputLatencyInSamples() : 0);
        object->setProperty("outputLatency", device ? device->getOutputLatencyInSamples() : 0);
        object->setProperty("deviceOutputPeak", engine.deviceOutputMeter->getCurrentLevel());
        object->setProperty("callbackOutputs", engine.callbackOutputs.load()); object->setProperty("writableOutputs", engine.writableOutputs.load());
        const auto driverError = engine.deviceError(); object->setProperty("driverError", driverError);
        publishIO(*object);
        juce::StringArray outputNames;
        if (device)
        {
            const auto names = device->getOutputChannelNames(); const auto active = device->getActiveOutputChannels();
            for (int index = 0; index < names.size(); ++index) if (active[index]) outputNames.add(names[index]);
        }
        object->setProperty("outputNames", outputNames.joinIntoString(", "));
        juce::Array<juce::var> outputPairs;if(device){const auto names=device->getOutputChannelNames();const auto active=device->getActiveOutputChannels();for(int pair=1;pair<8&&pair*2+1<names.size();++pair)if(pair!=engine.mainOutputPair.load()&&active[pair*2]&&active[pair*2+1]){auto* p=new juce::DynamicObject();p->setProperty("id",pair);p->setProperty("name",(ioSettings.outputs[pair*2].isEmpty()?names[pair*2]:ioSettings.outputs[pair*2])+" / "+(ioSettings.outputs[pair*2+1].isEmpty()?names[pair*2+1]:ioSettings.outputs[pair*2+1]));outputPairs.add(juce::var(p));}}object->setProperty("outputPairs",outputPairs);object->setProperty("monitorArmed",engine.monitorArmed.load());
        juce::Array<juce::var> liveTracks;for(int id=0;id<trackCount;++id)if(engine.wantsMonitor((size_t)id))liveTracks.add(id);object->setProperty("monitorTracks",liveTracks);
        auto* scan=new juce::DynamicObject();scan->setProperty("active",startupProgress.active);scan->setProperty("done",startupProgress.done);scan->setProperty("total",startupProgress.total);scan->setProperty("failed",startupProgress.failed);scan->setProperty("name",startupProgress.name);object->setProperty("startupScan",juce::var(scan));
        if(nativeAnalysisTrack>=0&&nativeAnalysisTrack<trackCount){const auto& slot=engine.plugins.channels[nativeAnalysisTrack][0];if(slot&&slot->instance){const auto info=NativeInstruments::grooveInfo(slot->instance.get());auto* result=new juce::DynamicObject();result->setProperty("track",nativeAnalysisTrack);result->setProperty("source",nativeAnalysisSource);result->setProperty("busy",nativeAnalysisPending);result->setProperty("valid",info.valid);result->setProperty("bpm",info.bpm);result->setProperty("confidence",info.confidence);result->setProperty("hits",info.hits);result->setProperty("status",nativeAnalysisPending?info.status:nativeAnalysisMessage);object->setProperty("nativeGrooveAnalysis",juce::var(result));}}
        object->setProperty("recording", engine.recorder.isActive());object->setProperty("captureActive",engine.recorder.isActive()&&engine.playback.recording.load()&&engine.logicalRecordGate.load());object->setProperty("recordingStart",engine.takeStart);
        object->setProperty("recordingBusy", recordingBusy);
        object->setProperty("countInBeats", engine.countInBeats.load());
        object->setProperty("dynamicTransport",engine.dynamicTransport.load());object->setProperty("punchEnabled",engine.punchEnabled.load());
        object->setProperty("playStart",engine.playStart.load());object->setProperty("selectionStart",engine.selectionStart.load());object->setProperty("selectionEnd",engine.selectionEnd.load());
        object->setProperty("bpm",controls.bpm);
        object->setProperty("preRollBars", engine.preRollBars.load()); object->setProperty("metronomeGain", engine.playback.metronomeGain.load());
        object->setProperty("recordingTrack", recordingTrack);
        juce::Array<juce::var> captureTracks;for(const auto& route:engine.recordingRoutes)captureTracks.add(route.track);object->setProperty("recordingTracks",captureTracks);
        object->setProperty("recordSeconds", engine.recorder.elapsed());
        if(engine.recorder.isActive())object->setProperty("recordingWaveforms",engine.recorder.waveformPreviews());
        object->setProperty("inputPeak", engine.inputPeak.load());
        object->setProperty("inputChannels", device ? device->getActiveInputChannels().countNumberOfSetBits() : 0);
        object->setProperty("monitorActive",engine.monitorActive.load());object->setProperty("monitorEnabled", engine.monitor.enabled.load()); object->setProperty("monitorGain", engine.monitor.gain.load());
        object->setProperty("monitorTrack", engine.monitor.track.load()); object->setProperty("monitorPeak", engine.monitorPeak.load());
        object->setProperty("inputSource", engine.inputSource.load());
        juce::Array<juce::var> inputNames, inputPeaks;
        if (device)
        {
            const auto names = device->getInputChannelNames(); const auto active = device->getActiveInputChannels();
            for (int index = 0; index < std::min(names.size(),InputRouting::maximumInputs); ++index) inputNames.add((ioSettings.inputs[index].isEmpty()?names[index]:ioSettings.inputs[index])+(active[index]?"":" (disabled)"));
        }
        for (auto& peak : engine.inputChannelPeaks) inputPeaks.add(peak.load());
        object->setProperty("generationLibrary",generationGrooveFolder.isDirectory()?generationGrooveFolder.getFileName():juce::String("Bundled grooves"));object->setProperty("inputNames", inputNames); object->setProperty("inputPeaks", inputPeaks);
        juce::Array<juce::var> automation;
        for(int id=0;id<trackCount;++id){const auto& t=engine.playback.tracks[id];auto* lane=new juce::DynamicObject();lane->setProperty("track",id);lane->setProperty("mode",t.automationMode.load());const bool read=Automation::reads(t.automationMode.load(),t.volumeTouched.load(),t.volumeLatched.load(),engine.playback.automationBypass.load());const double time=engine.playback.seconds.load();lane->setProperty("gain",read?Automation::value(t.volumeAutomation,time,t.gain.load()):t.gain.load());lane->setProperty("panValue",Automation::reads(t.automationMode.load(),t.panTouched.load(),t.panLatched.load(),engine.playback.automationBypass.load())?Automation::value(t.panAutomation,time,t.pan.load()):t.pan.load());if(includeWaveform&&automationCurvesChanged){lane->setProperty("volume",SessionFile::encodeAutomation(t.volumeAutomation));lane->setProperty("pan",SessionFile::encodeAutomation(t.panAutomation));}automation.add(juce::var(lane));}
        if(includeWaveform)automationCurvesChanged=false;
        object->setProperty("recordingAlignment",engine.alignRecording.load());object->setProperty("recordingCorrection",engine.recordingCorrection.load());
        object->setProperty("automationBypass",engine.playback.automationBypass.load());
        object->setProperty("automation",automation);object->setProperty("canAutomationUndo",!automationUndo.empty());object->setProperty("canAutomationRedo",!automationRedo.empty());
        if(includeWaveform)object->setProperty("pluginAutomation",engine.plugins.automationSnapshot(pluginAutomationChannel,pluginAutomationSlot,pluginAutomationParameter));
        object->setProperty("inserts", engine.plugins.snapshot(trackCount));
        object->setProperty("pluginBusy", pluginBusy);
        object->setProperty("midiDropResult",midiDropResult);
        object->setProperty("playbackLatency", engine.playback.playbackLatencyFrames);
        if (groove.valid)
        {
            auto* result = new juce::DynamicObject(); result->setProperty("track", grooveTrack);
            result->setProperty("bpm", groove.bpm); result->setProperty("confidence", groove.confidence);
            result->setProperty("hits", groove.onsetCount); result->setProperty("seconds", grooveSeconds);
            juce::Array<juce::var> pattern;
            for (const auto& row : groove.pattern) { juce::Array<juce::var> cells; for (auto velocity : row) cells.add(velocity); pattern.add(cells); }
            result->setProperty("pattern", pattern); object->setProperty("groove", juce::var(result));
        }
        juce::Array<juce::var> tracks;
        for (size_t id = 0; id < engine.playback.tracks.size(); ++id)
        {
            const auto& track = engine.playback.tracks[id];
            if (id>=static_cast<size_t>(trackCount)) continue;
            auto* detail = new juce::DynamicObject();
            detail->setProperty("id", static_cast<int>(id));
            detail->setProperty("fileName", trackInfo[id].fileName);
            detail->setProperty("duration", track.duration());
            detail->setProperty("peak", track.peak.load());
            detail->setProperty("peakLeft",track.peakLeft.load());detail->setProperty("peakRight",track.peakRight.load());
            if (includeWaveform) detail->setProperty("waveform", trackInfo[id].waveform);
            tracks.add(juce::var(detail));
        }
        object->setProperty("tracks", tracks);
        juce::Array<juce::var> clips;
        for(int id=0;id<trackCount;++id)for(const auto& clip:engine.playback.tracks[id].clips)
        {
            const auto& info=trackInfo[id];const MediaInfo* media=clip.source>0&&clip.source<=static_cast<int>(info.extra.size())?&info.extra[clip.source-1]:nullptr;
            auto* item=new juce::DynamicObject();item->setProperty("id",clip.id);item->setProperty("track",id);item->setProperty("source",clip.source);
            item->setProperty("fadeIn",clip.fadeIn);item->setProperty("fadeOut",clip.fadeOut);item->setProperty("fadeOffset",clip.fadeOffset);item->setProperty("fadeSpan",clip.fadeSpan);item->setProperty("blend",clip.blend);
            item->setProperty("start",clip.start);item->setProperty("offset",clip.offset);item->setProperty("length",clip.length);
            item->setProperty("sourceDuration",engine.playback.tracks[id].sourceDuration(clip.source));item->setProperty("name",media?media->fileName:info.fileName);
            if(includeWaveform)item->setProperty("waveform",media?media->waveform:info.waveform);
            clips.add(juce::var(item));
        }
        juce::Array<juce::var> midiNotes;
        for(int track=0;track<trackCount;++track)if(engine.plugins.midiLanes[track].enabled)for(const auto& n:engine.plugins.midiLanes[track].notes){auto* item=new juce::DynamicObject();item->setProperty("track",track);item->setProperty("id",n.id);item->setProperty("pitch",n.pitch);item->setProperty("channel",n.channel);item->setProperty("start",n.start);item->setProperty("length",n.length);item->setProperty("velocity",n.velocity);midiNotes.add(juce::var(item));}object->setProperty("midiNotes",midiNotes);
        object->setProperty("canBatchUndo",!groupUndo.empty()||!playlistUndo.empty());object->setProperty("canBatchRedo",!groupRedo.empty()||!playlistRedo.empty());
        object->setProperty("clips",clips);object->setProperty("canUndo",!undoClips.empty());object->setProperty("canRedo",!redoClips.empty());object->setProperty("canMidiUndo",!midiUndo.empty());object->setProperty("canMidiRedo",!midiRedo.empty());
        object->setProperty("loading", loading || sessionBusy || pluginBusy || (recordingBusy && !engine.recorder.isActive()));
        object->setProperty("sessionRevision",sessionRevision);
        object->setProperty("sessionName", sessionFile == juce::File() ? (recoveredName.isEmpty()?(newSessionName.isEmpty()?juce::String("Untitled session"):newSessionName):recoveredName) : sessionFile.getFileNameWithoutExtension());
        object->setProperty("dirty", dirty);
        if (includeWaveform)
        {
            juce::Array<juce::var> playlists;
            for(int track=0;track<trackCount;++track){syncPlaylist(track);for(const auto& p:trackPlaylists[track]){auto* item=new juce::DynamicObject();item->setProperty("track",track);item->setProperty("id",p.id);item->setProperty("name",p.name);item->setProperty("active",p.id==activePlaylists[track]);juce::Array<juce::var> c,n;for(const auto& clip:p.clips){auto value=SessionFile::encodeClip(clip);auto* detail=value.getDynamicObject();detail->setProperty("track",track);const auto& info=trackInfo[track];const auto* media=clip.source>0&&clip.source<=static_cast<int>(info.extra.size())?&info.extra[clip.source-1]:nullptr;detail->setProperty("name",media?media->fileName:info.fileName);detail->setProperty("sourceDuration",engine.playback.tracks[track].sourceDuration(clip.source));c.add(value);}for(const auto& note:p.notes)n.add(SessionFile::encodeNote(note));item->setProperty("clips",c);item->setProperty("notes",n);playlists.add(juce::var(item));}}object->setProperty("playlists",playlists);
            auto* project = new juce::DynamicObject();
            project->setProperty("bpm", controls.bpm); project->setProperty("master", engine.playback.masterGain.load());
            project->setProperty("selectTrack", selectTrack);
            project->setProperty("loop", engine.playback.loop.load()); project->setProperty("metronome", engine.playback.metronome.load());
            juce::Array<juce::var> lanes;
            for (int id = 0; id < trackCount; ++id)
            {
                const auto& c = controls.channels[static_cast<size_t>(id)]; auto* lane = new juce::DynamicObject();
                lane->setProperty("kind",engine.playback.tracks[id].routing.aux?"AUX":engine.playback.tracks[id].midiTrack?"MIDI":"AUDIO");
                const auto route=engine.playback.routingSnapshot(id);lane->setProperty("output",route.output);lane->setProperty("hardwarePair",route.hardwarePair);juce::Array<juce::var> sends;for(const auto& send:route.sends){auto* item=new juce::DynamicObject();item->setProperty("target",send.target);item->setProperty("gain",send.gain);item->setProperty("pre",send.pre);sends.add(juce::var(item));}lane->setProperty("sends",sends);lane->setProperty("id", id); lane->setProperty("name", trackNames[static_cast<size_t>(id)]);
                lane->setProperty("stereoLeft",engine.playback.tracks[id].stereoLeft.load());lane->setProperty("stereoRight",engine.playback.tracks[id].stereoRight.load());
                lane->setProperty("gain", c.gain); lane->setProperty("pan", c.pan); lane->setProperty("muted", c.muted);
                const auto& nativeSlot=engine.plugins.channels[id][0];if(nativeSlot&&nativeSlot->description.pluginFormatName=="Tracksmith Native"){if(nativeSlot->description.fileOrIdentifier=="tracksmith-native-click")lane->setProperty("clickTrack",true);else lane->setProperty("ezInstrument",nativeSlot->description.isInstrument?"drums":"guitar");if(nativeSlot->description.isInstrument)lane->setProperty("drumNoteMap",NativeInstruments::grooveNoteMap(nativeSlot->instance.get()));}
                lane->setProperty("inputSource",engine.trackInputs[id].load());lane->setProperty("inputChannels",engine.trackInputChannels[id].load());lane->setProperty("solo", c.solo); lane->setProperty("armed", c.armed); lanes.add(juce::var(lane));
            }
            project->setProperty("tracks", lanes); object->setProperty("project", juce::var(project));
        }
        object->setProperty("status", device == nullptr && audioError.isNotEmpty() ? audioError : status);
        if (driverError.isNotEmpty()) object->setProperty("status", "Audio driver error: " + driverError);
        if (++diagnosticTicks >= 40)
        {
            diagnosticTicks = 0;
            auto* diagnostic = new juce::DynamicObject(); juce::var report(diagnostic);
            for (const auto* key : { "driver", "deviceName", "sampleRate", "bufferSize", "cpuLoad", "xruns", "debugBuild", "playing", "seconds", "clipping", "outputNames", "callbackOutputs", "writableOutputs", "deviceOutputPeak", "peak", "inputPeak", "monitorEnabled", "monitorTrack", "inputSource", "inputChannels", "inserts", "driverError", "status" })
                diagnostic->setProperty(key, object->getProperty(key));
            diagnostic->setProperty("masterGain", engine.playback.masterGain.load()); diagnostic->setProperty("monitorGain", engine.monitor.gain.load());
            diagnostic->setProperty("callbackCount", static_cast<juce::int64>(engine.callbackCount.load()));
            const auto folder = engine.deviceSettingsFile().getParentDirectory();
            if (folder.createDirectory().wasOk()) folder.getChildFile("audio-diagnostics.json").replaceWithText(juce::JSON::toString(report));
        }
        exportSnapshot = snapshot;
        browser.emitEventIfBrowserIsVisible("audioState", snapshot);
    }
    IOSettings ioSettings;
    void publishIO(juce::DynamicObject& object)
    {
        ioSettings.refresh(engine.devices);
        auto* device=engine.devices.getCurrentAudioDevice();
        // Bind a fresh session once. Subsequent device changes require explicit review.
        if(engine.sessionDeviceKey.isEmpty()&&device){engine.detach();engine.sessionDeviceKey=ioSettings.key;engine.attach();}
        auto* state=new juce::DynamicObject();juce::var value(state);
        state->setProperty("deviceKey",ioSettings.key);state->setProperty("sessionDeviceKey",engine.sessionDeviceKey);
        state->setProperty("matched",engine.ioDeviceMatches.load());state->setProperty("mainOutputPair",engine.mainOutputPair.load());
        state->setProperty("maxInputs",InputRouting::maximumInputs);state->setProperty("maxOutputs",InputRouting::maximumOutputs);
        state->setProperty("maxRecordTracks",static_cast<int>(RecordGroup::maximumTracks));
        juce::Array<juce::var> inputs,outputs;
        if(device){
            const auto append=[&](bool input,juce::Array<juce::var>& result){
                const auto names=input?device->getInputChannelNames():device->getOutputChannelNames();
                const auto active=input?device->getActiveInputChannels():device->getActiveOutputChannels();
                for(int id=0;id<std::min(names.size(),input?InputRouting::maximumInputs:InputRouting::maximumOutputs);++id){
                    auto* path=new juce::DynamicObject();path->setProperty("id",id);path->setProperty("hardwareName",names[id]);
                    const auto label=input?ioSettings.inputs[id]:ioSettings.outputs[id];path->setProperty("name",label.isEmpty()?names[id]:label);
                    path->setProperty("active",active[id]);result.add(juce::var(path));
                }
            };append(true,inputs);append(false,outputs);
        }
        state->setProperty("inputs",inputs);state->setProperty("outputs",outputs);object.setProperty("io",value);
    }
    void ioCommand(const juce::var& message)
    {
        if(engine.playback.playing.load()||recordingBusy||chooser||(settings&&settings->isVisible())){status="Stop playback and close audio settings before editing I/O.";publish();return;}
        auto* device=engine.devices.getCurrentAudioDevice();if(!device){status="Choose an audio device first.";publish();return;}
        ioSettings.refresh(engine.devices);const auto type=message.getProperty("type",{}).toString();
        if(type=="ioUseDevice"){
            engine.detach();engine.sessionDeviceKey=ioSettings.key;engine.attach();markDirty();status="Session I/O assigned to this interface. Review each track input before recording.";
        }
        else if(type=="ioMainOutput"){
            const auto value=message.getProperty("pair",{});if(!value.isInt())return;const int pair=static_cast<int>(value);
            const auto active=device->getActiveOutputChannels();const auto names=device->getOutputChannelNames();
            if(pair<-1||pair>7||(pair>=0&&(!active[pair*2]||(!active[pair*2+1]&&!(pair==0&&names.size()==1))))){status="Enable both channels of that output pair in I/O setup first.";publish();return;}
            for(int id=0;id<trackCount;++id)if(pair>0&&engine.playback.tracks[id].routing.aux&&engine.playback.tracks[id].routing.hardwarePair==pair){status="That output pair is used by a bus. Route the bus to Main before selecting it for the main output.";publish();return;}
            engine.mainOutputPair=pair;markDirty();status=pair<0?"Main output disconnected.":"Main output assigned. Mixer processing stays on the Main bus.";
        }
        else if(type=="ioName"||type=="ioChannel"){
            const auto kind=message.getProperty("kind",{}).toString();const auto index=message.getProperty("index",{});
            if((kind!="input"&&kind!="output")||!index.isInt())return;const int id=static_cast<int>(index);const bool input=kind=="input";
            const auto names=input?device->getInputChannelNames():device->getOutputChannelNames();
            if(id<0||id>=names.size()||id>=(input?InputRouting::maximumInputs:InputRouting::maximumOutputs))return;
            if(type=="ioName"){
                const auto name=message.getProperty("name",{});if(!name.isString()||name.toString().length()>48)return;
                auto& label=input?ioSettings.inputs[id]:ioSettings.outputs[id];const auto old=label;label=name.toString().trim();
                const auto saved=ioSettings.save();if(saved.failed())label=old;status=saved.wasOk()?"I/O name saved for this interface.":saved.getErrorMessage();
            }else{
                const auto enabled=message.getProperty("enabled",{});if(!enabled.isBool())return;
                auto setup=engine.devices.getAudioDeviceSetup();
                if(input){setup.useDefaultInputChannels=false;setup.inputChannels.setBit(id,static_cast<bool>(enabled));}
                else{setup.useDefaultOutputChannels=false;setup.outputChannels.setBit(id,static_cast<bool>(enabled));if(setup.outputChannels.isZero()){status="Keep at least one device output enabled. Disconnect Main to silence it.";publish();return;}}
                const auto error=engine.devices.setAudioDeviceSetup(setup,true);
                if(error.isNotEmpty())status=error;else{const auto saved=engine.saveDeviceSettings();status=saved.wasOk()?"Device channels saved. Existing physical assignments were preserved.":saved.getErrorMessage();}
            }
        }
        else if(type=="ioResetNames"){
            const auto before=ioSettings;ioSettings.inputs.fill({});ioSettings.outputs.fill({});const auto saved=ioSettings.save();
            if(saved.failed())ioSettings=before;status=saved.wasOk()?"Hardware channel names restored.":saved.getErrorMessage();
        }
        else return;
        publish(true);
    }
    void command(const juce::var& message)
    {
        if (! message.isObject()) return;
        const auto type = message.getProperty("type", {}).toString();
        if(type=="midiDrop"){importDroppedMidi(message);return;}
        if(type=="autosave"){const auto enabled=message.getProperty("enabled",{});if(enabled.isBool())autosaveEnabled=static_cast<bool>(enabled);if(static_cast<bool>(message.getProperty("now",false)))autosaveTick(true);publish();return;}
        if(type=="recoveryFolder"){recoveryRoot.createDirectory();recoveryRoot.revealToUser();return;}
        if(type=="recoverSession"){if(automationPassActive&&!recordingBusy){automationTick();engine.playback.playing=false;finishAutomation();}recoverSession(message.getProperty("id",{}).toString());return;}
        if(type=="monitorArmed"){const auto enabled=message.getProperty("enabled",{});if(!enabled.isBool()||loading||sessionBusy||pluginBusy||exportBusy)return;engine.monitorArmed=static_cast<bool>(enabled);status="Armed-track monitoring updated. Each audio track uses its assigned input and shared monitor level.";publish();return;}
        if(type=="recordingAlignment"){const auto enabled=message.getProperty("enabled",{});if(enabled.isBool()&&!recordingBusy&&!loading&&!exportBusy)engine.alignRecording=static_cast<bool>(enabled);publish();return;}
        if(type=="automationBypass"){if(recordingBusy||loading||sessionBusy||pluginBusy||exportBusy||engine.playback.playing.load())return;finishAutomation();engine.playback.automationBypass=!engine.playback.automationBypass.load();publish(true);return;}
        if(type=="automationGesture"){const auto id=message.getProperty("track",{}),active=message.getProperty("active",{});if(!id.isInt()||!active.isBool()||static_cast<int>(id)<0||static_cast<int>(id)>=trackCount||recordingBusy||exportBusy)return;auto& t=engine.playback.tracks[static_cast<int>(id)];const bool pan=message.getProperty("parameter",{}).toString()=="pan";auto& touch=pan?t.panTouched:t.volumeTouched;auto& latch=pan?t.panLatched:t.volumeLatched;if(!static_cast<bool>(active))automationTick();if(static_cast<bool>(active)&&!touch.load()&&Automation::reads(t.automationMode.load(),false,latch.load(),engine.playback.automationBypass.load())){const double time=engine.playback.seconds.load();if(pan)controls.channels[static_cast<int>(id)].pan=Automation::value(t.panAutomation,time,t.pan.load());else controls.channels[static_cast<int>(id)].gain=Automation::value(t.volumeAutomation,time,t.gain.load());applyChannels();}touch=static_cast<bool>(active);if(touch.load())latch=true;automationTick();return;}
        if(type=="pluginAutomation"){pluginAutomationCommand(message);return;}
        if(type=="automation"){automationCommand(message);return;}
        if(type=="recoverSession"||type=="groupEdit"||type=="playlist"||type=="clipEdit"||type=="stop"||type=="seek"||type=="saveSession"||type=="openSession"||type=="exportMix"||type.startsWith("plugin")){
            if(automationPassActive&&!recordingBusy&&!loading&&!sessionBusy&&!pluginBusy){automationTick();engine.playback.playing=false;finishAutomation();}
        }
        if(type=="transport"&&static_cast<bool>(message.getProperty("loop",false))){for(const auto& channel:engine.plugins.channels)for(const auto& slot:channel)if(slot)for(const auto& lane:slot->automation)if(lane.mode>=2){status="Turn plugin Write/Touch/Latch off before enabling Loop.";publish(true);return;}for(int id=0;id<trackCount;++id)if(engine.playback.tracks[id].automationMode.load()>=2){status="Turn Write/Touch/Latch off before enabling Loop.";publish(true);return;}}

        if (exportBusy) { if (type == "cancelExport") mixExporter.cancel(); publish(); return; }
        if (type == "exportMix") { chooseMixExport(message); return; }
        if(type=="transport"&&static_cast<bool>(message.getProperty("playing",false))&&!engine.playback.playing.load()){for(auto& t:engine.playback.tracks){t.volumeTouched=false;t.panTouched=false;t.volumeLatched=false;t.panLatched=false;}for(auto& channel:engine.plugins.channels)for(auto& slot:channel)if(slot)slot->resetGestures();}
        if(type=="newSession"){
            if(chooser||loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||nativeAnalysisPending||recoveryWorker.isBusy())return;
            auto name=message.getProperty("name","Untitled session").toString().trim().substring(0,80);if(name.isEmpty())name="Untitled session";
            const auto tempo=message.getProperty("bpm",120);if(!(tempo.isInt()||tempo.isDouble())||!std::isfinite(static_cast<double>(tempo))||static_cast<double>(tempo)<20||static_cast<double>(tempo)>300)return;
            const auto instrument=message.getProperty("instrument",{}).toString();if(instrument.isNotEmpty()&&instrument!="guitar"&&instrument!="drums")return;
            confirmDiscard("Start a new session?",[safe=juce::Component::SafePointer<MainComponent>(this),name,tempo,instrument]{if(!safe)return;safe->engine.playback.playing=false;SessionJobResult result;result.session.bpm=static_cast<double>(tempo);SessionTrack track;track.name=instrument=="guitar"?"Guitar":instrument=="drums"?"Drums":"Audio 1";track.midiTrack=instrument=="drums";track.inputChannels=track.midiTrack?2:1;track.armed=instrument=="guitar";result.session.tracks.push_back(track);safe->sessionWorker.onFinished(std::move(result));if(instrument.isNotEmpty()){safe->createInsert(0,0,NativeInstruments::description(instrument=="drums"));if(instrument=="guitar"){safe->engine.monitor.track=0;safe->engine.monitor.enabled=true;safe->engine.monitorOverrides[0]=1;safe->engine.monitorLevels[0]=1;safe->engine.monitor.gain=1;safe->applyChannels();}}safe->newSessionName=name;safe->markDirty();safe->status="New session ready. Add tracks or record your first part.";safe->publish(true);});return;
        }
        if(type=="cancelStartupScan"){startupScan.cancel();return;}
        if (type == "ready") { automationCurvesChanged=true;publish(true); return; }
        if (type == "testOutput")
        {
            if (recordingBusy) { status = "Stop the take before testing the output."; publish(); return; }
            auto* device = engine.devices.getCurrentAudioDevice();
            if (!device || !device->isPlaying() || device->getActiveOutputChannels().isZero())
            { status = "The audio driver has no active output. Open Audio settings and enable an output pair."; publish(); return; }
            engine.playback.playing = false; engine.monitor.enabled = false;engine.clearTrackMonitoring();engine.monitorArmed=false;
            engine.devices.playTestSound(); status = "Playing the JUCE driver test sound, bypassing the mixer and plugins."; publish(); return;
        }
        if (type == "monitor"){
            if(loading||sessionBusy||pluginBusy||exportBusy)return;
            const auto id=message.getProperty("id",-1),enabled=message.getProperty("enabled",{}),gain=message.getProperty("gain",1);
            if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=trackCount||!enabled.isBool()||!(gain.isInt()||gain.isDouble())||!std::isfinite(static_cast<double>(gain)))return;
            const int track=static_cast<int>(id);if(engine.playback.tracks[track].midiTrack||engine.playback.tracks[track].routing.aux){status="Input monitoring is available on audio tracks.";publish();return;}
            if(static_cast<bool>(enabled)){const auto result=engine.ensureMonitorInputs(engine.trackInputs[track].load(),engine.trackInputChannels[track].load());if(result.failed()){status=result.getErrorMessage();publish();return;}}
            engine.monitorLevels[track]=static_cast<float>(juce::jlimit(0.,1.,static_cast<double>(gain)));engine.monitorOverrides[track]=static_cast<bool>(enabled)?1:-1;
            status="Input monitoring "+juce::String(static_cast<bool>(enabled)?"on: ":"off: ")+trackNames[track];publish();return;
        }
        if (type == "stop")
        {
            if (recordingBusy) { engine.stopRecording(true,false,true); engine.continueAfterPunch=false; status = "Finalizing recorded WAV..."; }
            else { engine.resetPluginGraph(); engine.playback.playing = false; engine.playback.seekSeconds = engine.dynamicTransport.load()?engine.playStart.load():0.0; }
            publish(); return;
        }
        if (loading || sessionBusy || pluginBusy) return;
        if (recordingBusy && type != "track" && type != "master" && type != "sendLevel") return;
        if(type.startsWith("io")){ioCommand(message);return;}
        if(type=="groupEdit"){groupCommand(message);return;}
        if(type=="playlist"){playlistCommand(message);return;}
        if(type=="routing"||type=="sendLevel")
        {
            const auto lane=message.getProperty("id",{});if(!lane.isInt()||static_cast<int>(lane)<0||static_cast<int>(lane)>=trackCount)return;
            const size_t id=static_cast<size_t>(static_cast<int>(lane));if(engine.playback.tracks[id].routing.aux&&type=="sendLevel")return;
            const auto slot=message.getProperty("send",{});
            if(type=="sendLevel")
            {
                const auto value=message.getProperty("gain",{});if(!slot.isInt()||static_cast<int>(slot)<0||static_cast<int>(slot)>1||!(value.isDouble()||value.isInt()))return;
                const double gain=static_cast<double>(value);if(!std::isfinite(gain)||gain<0||gain>1)return;
                engine.playback.tracks[id].sendLevels[static_cast<size_t>(static_cast<int>(slot))]=static_cast<float>(gain);markDirty();publish();return;
            }
            std::vector<MixerRoute> routes;for(int track=0;track<trackCount;++track)routes.push_back(engine.playback.routingSnapshot(track));
            if(message.hasProperty("hardwarePair")){const auto pair=message.getProperty("hardwarePair",{});if(!pair.isInt())return;const int target=static_cast<int>(pair);auto* device=engine.devices.getCurrentAudioDevice();if(target<0||target>7||(target>0&&target==engine.mainOutputPair.load())||!routes[id].aux||(target>0&&(!device||!device->getActiveOutputChannels()[target*2]||!device->getActiveOutputChannels()[target*2+1]))){status="Enable that stereo output pair in Audio settings first.";publish();return;}routes[id].hardwarePair=target;}
            else if(message.hasProperty("pre")){const auto pre=message.getProperty("pre",{});if(!slot.isInt()||static_cast<int>(slot)<0||static_cast<int>(slot)>1||!pre.isBool())return;routes[id].sends[static_cast<size_t>(static_cast<int>(slot))].pre=static_cast<bool>(pre);}
            else if(message.hasProperty("output")){const auto output=message.getProperty("output",{});if(!output.isInt())return;routes[id].output=static_cast<int>(output);}
            else {const auto target=message.getProperty("target",{});if(!slot.isInt()||static_cast<int>(slot)<0||static_cast<int>(slot)>1||!target.isInt())return;routes[id].sends[static_cast<size_t>(static_cast<int>(slot))].target=static_cast<int>(target);}
            if(!validMixerRoutes(routes)){status="Choose an aux return as the destination. Aux returns feed Master only.";publish();return;}
            engine.detach();engine.playback.setRouting(id,routes[id]);engine.plugins.resetGraph();engine.playback.seekSeconds=engine.playback.seconds.load();engine.attach();markDirty();status="Routing updated. Press Play to audition.";publish(true);return;
        }
        if(type=="midiBatch" || type=="midiEdit" || type=="midiImport" || type=="midiExport" || type=="grooveToMidi")
        {
            if(recordingBusy||loading||sessionBusy||pluginBusy||recoveryWorker.isBusy()||chooser||automationPassActive)return;
            const auto lane=message.getProperty("track",{});if(!lane.isInt()||static_cast<int>(lane)<0||static_cast<int>(lane)>=trackCount)return;const int track=static_cast<int>(lane);if(!engine.plugins.midiLanes[track].enabled)return;
            if(type=="midiBatch"){auto notes=engine.plugins.midiLanes[track].notes;if(editMidiBatch(notes,message))applyMidiNotes(track,std::move(notes));else{status="MIDI edit was not applied. Check note bounds, overlaps and the 2048-note limit.";publish();}return;}
            if(type=="midiImport" || type=="midiExport"){chooseMidiFile(track,type=="midiExport");return;}
            auto notes=engine.plugins.midiLanes[track].notes;
            if(type=="grooveToMidi")
            {
                if(!groove.valid)return;notes.clear();int id=1;for(int d=0;d<3;++d)for(int step=0;step<16;++step)if(groove.pattern[d][step]>0)notes.push_back({id++,juce::jlimit(0,127,std::array<int,3>{36,38,42}[d]),10,step*.25,.125,groove.pattern[d][step]});
            }
            else
            {
                const auto action=message.getProperty("action",{}).toString();const auto id=message.getProperty("id",{});if(!id.isInt())return;
                auto found=std::find_if(notes.begin(),notes.end(),[&](const auto& n){return n.id==static_cast<int>(id);});
                if(action=="delete"){if(found==notes.end())return;notes.erase(found);}
                else
                {
                    MidiNote note;note.id=static_cast<int>(id);const auto pitch=message.getProperty("pitch",{}),channel=message.getProperty("channel",{}),start=message.getProperty("start",{}),length=message.getProperty("length",{}),velocity=message.getProperty("velocity",{});
                    if(!pitch.isInt()||!channel.isInt()||!(start.isDouble()||start.isInt())||!(length.isDouble()||length.isInt())||!(velocity.isDouble()||velocity.isInt()))return;
                    note.pitch=static_cast<int>(pitch);note.channel=static_cast<int>(channel);note.start=static_cast<double>(start);note.length=static_cast<double>(length);note.velocity=static_cast<float>(velocity);if(!validMidiNote(note)){status="Use pitch 0â€“127, channel 1â€“16, a positive note length and velocity, and a non-negative start beat.";publish();return;}
                    if(action=="add"){if(notes.size()>=MidiLane::maximumNotes || found!=notes.end())return;notes.push_back(note);}else if(action=="update"&&found!=notes.end())*found=note;else return;
                }
            }
            applyMidiNotes(track,std::move(notes));return;
        }
        if(type=="clipEdit" || type=="undo" || type=="redo")
        {
            if(type=="undo" || type=="redo")
            {
                if(editorBatchUndo(type=="redo"))return;
                const bool redo=type=="redo";const auto& audioHistory=redo?redoClips:undoClips;const auto& midiHistory=redo?midiRedo:midiUndo;
                const bool midiLatest=!midiHistory.empty()&&(audioHistory.empty()||(redo?midiHistory.back().serial<audioHistory.back().serial:midiHistory.back().serial>audioHistory.back().serial));
                if(midiLatest)
                {
                    auto& from=type=="undo"?midiUndo:midiRedo;auto& to=type=="undo"?midiRedo:midiUndo;if(from.empty())return;auto change=std::move(from.back());from.pop_back();engine.detach();if(change.playlist>0)selectPlaylist(change.track,change.playlist);engine.plugins.midiLanes[change.track].notes=type=="undo"?change.before:change.after;syncPlaylist(change.track);engine.playback.tracks[change.track].midiSeconds=engine.plugins.midiLanes[change.track].endBeat()*60/controls.bpm+1;finishClipEdit();to.push_back(std::move(change));markDirty();publish(true);return;
                }
                auto& from=type=="undo"?undoClips:redoClips;auto& to=type=="undo"?redoClips:undoClips;
                if(from.empty())return;auto change=std::move(from.back());from.pop_back();
                engine.detach();if(change.playlist>0)selectPlaylist(change.track,change.playlist);engine.playback.tracks[change.track].clips=type=="undo"?change.before:change.after;syncPlaylist(change.track);finishClipEdit();to.push_back(std::move(change));
                status=type=="undo"?"Clip edit undone.":"Clip edit redone.";markDirty();publish(true);return;
            }
            const auto lane=message.getProperty("track",{}),id=message.getProperty("id",{}),position=message.getProperty("position",{});
            if(!lane.isInt() || !id.isInt() || !(position.isInt()||position.isDouble()) || static_cast<int>(lane)<0 || static_cast<int>(lane)>=trackCount)return;
            const int track=static_cast<int>(lane);const auto action=message.getProperty("action",{}).toString();
            auto before=engine.playback.tracks[track].clips,after=before;int next=1;for(const auto& c:before)next=std::max(next,c.id+1);
            if(action=="split" && engine.selectionEnd.load()>engine.selectionStart.load())
            {
                const auto selected=std::find_if(after.begin(),after.end(),[&](const auto& c){return c.id==static_cast<int>(id);});if(selected==after.end())return;const auto original=*selected;
                const auto a=engine.selectionStart.load(),b=engine.selectionEnd.load();bool edited=false;
                const int cuts=static_cast<int>(b>original.start+1.e-6 && b<original.start+original.length-1.e-6)+static_cast<int>(a>original.start+1.e-6 && a<original.start+original.length-1.e-6);
                if(after.size()+cuts>ClipLayout::maximumClips)return;
                if(b>original.start+1.e-6 && b<original.start+original.length-1.e-6)edited=ClipLayout::edit(after,original.id,"split",b,next++)||edited;
                if(a>original.start+1.e-6 && a<original.start+original.length-1.e-6)edited=ClipLayout::edit(after,original.id,"split",a,next++)||edited;
                if(!edited)return;
            }
            else if(action=="trimSelection")
            {
                auto clip=std::find_if(after.begin(),after.end(),[&](const auto& c){return c.id==static_cast<int>(id);});if(clip==after.end())return;
                const auto start=std::max(clip->start,engine.selectionStart.load()),end=std::min(clip->start+clip->length,engine.selectionEnd.load());
                if(end<=start)return;*clip=ClipLayout::slice(*clip,start,end,clip->id);
            }
            else if(action=="crossfade")
            {if(!ClipLayout::crossfade(after,static_cast<int>(id),static_cast<double>(position),[&](int source){return engine.playback.tracks[track].sourceDuration(source);})){status="Crossfade needs a touching or overlapping previous clip with enough source audio handles.";publish();return;}}
            else if(!ClipLayout::edit(after,static_cast<int>(id),action.toRawUTF8(),static_cast<double>(position),next))return;
            for(const auto& c:after){const auto duration=engine.playback.tracks[track].sourceDuration(c.source);if(duration>0 && c.offset+c.length>duration+1.e-6)return;}
            clearBatchHistory();undoClips.push_back({track,std::move(before),after,activePlaylists[track]});if(undoClips.size()>64)undoClips.erase(undoClips.begin());redoClips.clear();midiRedo.clear();
            engine.detach();engine.playback.tracks[track].clips=std::move(after);syncPlaylist(track);finishClipEdit();markDirty();status="Clip "+action+" applied. Playback stopped for editing.";publish(true);return;
        }
        if(type=="inputSource"||type=="recordMode")
        {
            const auto lane=message.getProperty("id",0),value=message.getProperty(type=="inputSource"?"index":"channels",{});
            if(!lane.isInt()||!value.isInt()||static_cast<int>(lane)<0||static_cast<int>(lane)>=trackCount)return;
            const int id=static_cast<int>(lane),v=static_cast<int>(value);
            if(type=="inputSource"){if(v<-1||v>=InputRouting::maximumInputs)return;engine.trackInputs[id]=v;}
            else {if(v<1||v>2)return;engine.trackInputChannels[id]=v;}
            if(!engine.monitor.enabled.load()||engine.monitor.track.load()==id)
            {engine.inputSource=engine.trackInputs[id].load();engine.inputMeterChannels=engine.trackInputChannels[id].load();}
            markDirty();publish(true);return;
        }
        if (type == "analyseDrums")
        { auto* request=new juce::DynamicObject();request->setProperty("type","ezAnalysePart");request->setProperty("id",message.getProperty("id",-1));request->setProperty("clip",message.getProperty("clip",-1));command(juce::var(request));return; }
        if (type == "grooveTempo" && groove.valid)
        { engine.detach();controls.bpm=groove.bpm;engine.playback.bpm=groove.bpm;for(int id=0;id<trackCount;++id)if(engine.playback.tracks[id].midiTrack)engine.playback.tracks[id].midiSeconds=engine.plugins.midiLanes[id].endBeat()*60/controls.bpm+1;engine.playback.refreshDuration();engine.playback.playing=false;engine.plugins.resetGraph();engine.attach();markDirty();status="Suggested tempo applied. MIDI notes follow the new tempo.";publish(true);return; }
        if (type == "grooveExport" && groove.valid)
        {
            if (chooser) return;
            chooser = std::make_unique<juce::FileChooser>("Export suggested one-bar GM drum MIDI", juce::File(), "*.mid");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& selection)
                {
                    if (!safe) return; auto file = selection.getResult();
                    if (file != juce::File())
                    {
                        if (!file.hasFileExtension("mid")) file = file.withFileExtension("mid");
                        const auto result = GrooveWorker::exportMidi(safe->groove, file);
                        safe->status = result.wasOk() ? "Exported one-bar drum suggestion to " + file.getFullPathName() : result.getErrorMessage(); safe->publish();
                    }
                    juce::MessageManager::callAsync([safe] { if (safe) safe->chooser.reset(); });
                }); return;
        }
        if(type=="stereoPan"){
            const int id=static_cast<int>(message.getProperty("id",-1));const auto left=message.getProperty("left",{}),right=message.getProperty("right",{});
            const auto valid=[](const juce::var& v){return (v.isDouble()||v.isInt())&&std::isfinite(static_cast<double>(v))&&static_cast<double>(v)>=-1&&static_cast<double>(v)<=1;};
            if(id<0||id>=trackCount||!valid(left)||!valid(right))return;
            engine.playback.tracks[id].stereoLeft=static_cast<float>(left);engine.playback.tracks[id].stereoRight=static_cast<float>(right);markDirty();publish(true);return;
        }
        if(type=="generationLibrary"){
            if(chooser||nativeAnalysisPending)return;chooser=std::make_unique<juce::FileChooser>("Choose your MIDI groove library",generationGrooveFolder,"*");
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe=juce::Component::SafePointer<MainComponent>(this)](const auto& dialog){if(!safe)return;const auto folder=dialog.getResult();if(folder.isDirectory()){safe->generationGrooveFolder=folder;safe->status="Generation library: "+folder.getFileName();safe->publish();}juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});});return;
        }
        if(type=="ezAnalysePart"){
            if(loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||nativeAnalysisPending)return;const int source=static_cast<int>(message.getProperty("id",-1));if(source<0||source>=trackCount)return;
            const auto& lane=engine.playback.tracks[source];if(lane.midiTrack||lane.routing.aux||lane.clips.empty()){status="Select a recorded audio part to analyze.";publish();return;}
            const auto selected=static_cast<int>(message.getProperty("clip",-1));const TimelineClip* chosen=&lane.clips.back();for(const auto& clip:lane.clips)if(clip.id==selected){chosen=&clip;break;}
            const auto clip=*chosen;juce::File file;if(clip.source==0)file=trackInfo[source].file;else if(clip.source>0&&clip.source<=static_cast<int>(trackInfo[source].extra.size()))file=trackInfo[source].extra[static_cast<size_t>(clip.source-1)].file;
            if(!file.existsAsFile()){status="The recorded source file is missing. Locate it before analysis.";publish();return;}
            const int target=trackCount;auto* add=new juce::DynamicObject();add->setProperty("type","addEZInstrument");add->setProperty("instrument","drums");command(juce::var(add));if(trackCount!=target+1)return;
            auto& slot=engine.plugins.channels[target][0];if(slot&&slot->instance){const double influence=(double)message.getProperty("referenceInfluence",.3);NativeInstruments::setNeuralOptions(slot->instance.get(),(bool)message.getProperty("neural",false),std::isfinite(influence)?(float)influence:.3f);}if(slot&&slot->instance&&generationGrooveFolder.isDirectory())NativeInstruments::setGrooveLibrary(slot->instance.get(),generationGrooveFolder);if(!slot||!slot->instance||!NativeInstruments::analyseReference(slot->instance.get(),file,clip.offset,clip.length,clip.start*controls.bpm/60,controls.bpm)){status="Groove could not start analysis.";publish();return;}
            nativeAnalysisTrack=target;nativeAnalysisSource=source;nativeAnalysisToken=slot->token;nativeAnalysisPending=true;nativeAnalysisMessage="Analyzing your performance and building drums…";status=nativeAnalysisMessage;publish(true,target);return;
        }
        if(type=="tempo"){
            if(loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||chooser)return;
            const auto value=message.getProperty("bpm",{});if(!(value.isDouble()||value.isInt()))return;const double tempo=static_cast<double>(value);if(!std::isfinite(tempo)||tempo<20||tempo>300)return;
            if(controls.bpm!=tempo)setSessionTempo(tempo);status="Session tempo: "+juce::String(tempo,1)+" BPM";publish(true);return;
        }
        if(type=="shuttle"){
            if(loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||chooser)return;
            const auto direction=message.getProperty("direction",0);if(!direction.isInt()||(static_cast<int>(direction)!=-1&&static_cast<int>(direction)!=1))return;
            const double pending=engine.playback.seekSeconds.load(),position=engine.dynamicTransport.load()?engine.playStart.load():pending>=0?pending:engine.playback.seconds.load();
            const auto target=TransportSelection::nudge(position,(int)direction,controls.bpm);
            auto* seek=new juce::DynamicObject();seek->setProperty("type","seek");seek->setProperty("beat",target*controls.bpm/60);command(juce::var(seek));return;
        }
        if(type=="createClickTrack"){
            if(loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||trackCount>=128)return;
            const int id=trackCount;auto* add=new juce::DynamicObject();add->setProperty("type","addTrack");add->setProperty("kind","MIDI");add->setProperty("name","Click");add->setProperty("inputChannels",1);command(juce::var(add));if(trackCount!=id+1)return;
            engine.playback.metronome=true;createInsert((size_t)id,0,NativeInstruments::clickDescription());
            status="Click track ready. Press Play or Record; open Click settings to choose its sound.";markDirty();publish(true,id);return;
        }
        if(type=="clickSettings"){
            for(int id=0;id<trackCount;++id){const auto& slot=engine.plugins.channels[id][0];if(slot&&slot->description.fileOrIdentifier=="tracksmith-native-click"){auto* open=new juce::DynamicObject();open->setProperty("type","pluginEditor");open->setProperty("channel",id);open->setProperty("slot",0);command(juce::var(open));return;}}
            status="Create a click track from the Track menu first.";publish();return;
        }
        if(type=="addEZInstrument"){
            if(loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||trackCount>=128)return;
            const auto instrument=message.getProperty("instrument",{}).toString();if(instrument!="guitar"&&instrument!="drums")return;const bool drums=instrument=="drums";const int id=trackCount;
            auto* add=new juce::DynamicObject();add->setProperty("type","addTrack");add->setProperty("kind",drums?"MIDI":"AUDIO");add->setProperty("name",drums?"Drums":"Guitar");add->setProperty("inputChannels",drums?2:1);command(juce::var(add));if(trackCount!=id+1)return;
            createInsert(static_cast<size_t>(id),0,NativeInstruments::description(drums));
            if(!drums){for(int other=0;other<trackCount;++other)controls.channels[other].armed=false;controls.channels[id].armed=true;engine.monitor.track=id;engine.monitor.enabled=true;engine.monitorOverrides[id]=1;engine.monitorLevels[id]=1;engine.monitor.gain=1;engine.inputSource=engine.trackInputs[id].load();engine.inputMeterChannels=1;applyChannels();}
            status=drums?"Drums ready. Tap the kit or add a starter beat.":"Guitar ready. NAM is built in; choose your input and play.";publish(true,id);return;
        }
        if(type=="ezApplyGroove"){
            if(!groove.valid||loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||trackCount>=128)return;const int id=trackCount;
            auto* add=new juce::DynamicObject();add->setProperty("type","addEZInstrument");add->setProperty("instrument","drums");command(juce::var(add));if(trackCount!=id+1)return;
            auto* apply=new juce::DynamicObject();apply->setProperty("type","grooveToMidi");apply->setProperty("track",id);command(juce::var(apply));publish(true,id);return;
        }
        if(type=="ezDrumHit"){
            const int id=static_cast<int>(message.getProperty("id",-1)),drum=static_cast<int>(message.getProperty("drum",-1));if(id<0||id>=trackCount||drum<0||drum>=9||loading||recordingBusy||sessionBusy||pluginBusy||exportBusy)return;
            auto& slot=engine.plugins.channels[id][0];if(slot&&slot->instance)NativeInstruments::audition(slot->instance.get(),drum);return;
        }
        if(type=="stockPlugin"){
            const int channel=static_cast<int>(message.getProperty("channel",-1)),index=static_cast<int>(message.getProperty("slot",-1));
            const auto name=message.getProperty("name",{}).toString();
            if(channel<0||(channel>=trackCount&&channel!=128)||index<0||index>=5||(name!="NAM"&&name!="Bus Compressor"))return;
            if(engine.plugins.channels[channel][static_cast<size_t>(index)]){status="Insert already occupied. Choose an empty slot.";publish();return;}
            const auto file=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Stock Plugins/Tracksmith "+name+".vst3");
            if(!file.exists()){status="Bundled plugin not found: "+file.getFileName();publish();return;}
            auto library=pluginLibrary();for(const auto& description:library->getTypes())if(description.fileOrIdentifier==file.getFullPathName()){
                createInsert(static_cast<size_t>(channel),index,description);return;
            }
            discoverPlugin(file,static_cast<size_t>(channel),index);return;
        }
        if (type.startsWith("plugin"))
        {
            const auto channelValue = message.getProperty("channel", {}), slotValue = message.getProperty("slot", {});
            if (!channelValue.isInt() || !slotValue.isInt()) return;
            const int channel = static_cast<int>(channelValue), index = static_cast<int>(slotValue);
            if (channel < 0 || (channel >= trackCount && channel != 128) || index < 0 || index >= 5) return;
            auto& slot = engine.plugins.channels[static_cast<size_t>(channel)][static_cast<size_t>(index)];
            if (type == "pluginLoad") { insertMenu(static_cast<size_t>(channel), index); return; }
            if (!slot) return;
            if (type == "pluginEditor") { engine.plugins.editor(static_cast<size_t>(channel), index); markDirty(); }
            if (type == "pluginBypass") { engine.detach(); slot->bypass = !slot->bypass.load(); engine.plugins.resetGraph(); engine.playback.seekSeconds = engine.playback.seconds.load(); engine.attach(); markDirty(); }
            if (type == "pluginRemove") { const bool click=slot->description.fileOrIdentifier=="tracksmith-native-click";engine.detach(); slot.reset();stopRemovedClick(click); engine.plugins.resetGraph(); engine.playback.seekSeconds = engine.playback.seconds.load(); engine.attach(); markDirty(); }
            publish(); return;
        }
        if (type == "record")
        {
            for(int id=0;id<trackCount;++id)if(engine.playback.tracks[id].automationMode.load()>=2){status="Set track automation to Read or Off before recording audio.";publish();return;}
            for(const auto& channel:engine.plugins.channels)for(const auto& slot:channel)if(slot)for(const auto& lane:slot->automation)if(lane.mode>=2){status="Set plugin automation to Read or Off before recording audio.";publish();return;}

            const auto channels = message.getProperty("channels", 1);
            if (!channels.isInt() || static_cast<int>(channels) < 1 || static_cast<int>(channels) > 2) return;
            startRecording(static_cast<int>(channels)); return;
        }
        if (type == "saveSession") { chooseSession(true); return; }
        if (type == "openSession")
        {
            if (chooser) return;
            confirmDiscard("Open another session?", [safe = juce::Component::SafePointer<MainComponent>(this)] { if (safe != nullptr) safe->chooseSession(false); }); return;
        }
        if(type=="trackMenu")
        {
            const auto id=message.getProperty("id",{});if(!id.isInt()||static_cast<int>(id)<0||static_cast<int>(id)>=trackCount||loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||chooser||recoveryWorker.isBusy())return;
            const int track=static_cast<int>(id);juce::PopupMenu menu;menu.addItem(1,"New track...");menu.addSeparator();menu.addItem(2,"Delete track (cannot undo)",trackCount>1);
            menu.showMenuAsync(juce::PopupMenu::Options(),[safe=juce::Component::SafePointer<MainComponent>(this),track](int choice){if(!safe)return;if(choice==1)safe->newTrackDialog();if(choice==2)safe->deleteTrack(track);});return;
        }
        if (type == "newTrackDialog") { newTrackDialog(); return; }
        if (type == "addTrack")
        {
            if (trackCount >= 128) return;
            const auto kind=message.getProperty("kind",{}).toString();const bool midi=kind=="MIDI"||kind=="INSTRUMENT",aux=kind=="AUX";
            int auxCount=0;for(int id=0;id<trackCount;++id)if(engine.playback.tracks[id].routing.aux)++auxCount;
            if(aux&&auxCount>=16){status="This build supports up to 16 aux returns.";publish();return;}
            const int track=trackCount++;engine.trackInputs[track]=0;engine.monitorOverrides[track]=0;engine.monitorLevels[track]=1;engine.trackInputChannels[track]=static_cast<int>(message.getProperty("inputChannels",1))==2?2:1;
            engine.detach();engine.playback.tracks[track].midiTrack=midi;engine.playback.tracks[track].routing.aux=aux;engine.plugins.midiLanes[track].enabled=midi;
            if(midi){engine.playback.tracks[track].midiSeconds=engine.plugins.midiLanes[track].endBeat()*60/controls.bpm+1;trackNames[track]="Instrument "+juce::String(track+1);}
            if(aux){trackNames[track]="Aux "+juce::String(auxCount+1);engine.playback.tracks[track].gain=1;controls.channels[track].gain=1;}
            const auto customName=message.getProperty("name",{}).toString().trim().substring(0,80);if(customName.isNotEmpty())trackNames[track]=customName;
            engine.playback.refreshDuration();engine.attach();markDirty();publish(true,track);return;
        }
        if (type == "rename")
        {
            const auto id = message.getProperty("id", -1), name = message.getProperty("name", {});
            if (!id.isInt() || static_cast<int>(id) < 0 || static_cast<int>(id) >= trackCount || !name.isString()) return;
            const auto text = name.toString().trim().substring(0, 80); if (text.isEmpty()) return;
            auto& previous = trackNames[static_cast<size_t>(static_cast<int>(id))];
            if (previous != text) { previous = text; markDirty(); } publish(true); return;
        }
        if(type=="editRange")
        {
            const auto start=message.getProperty("start",{}),end=message.getProperty("end",{});
            if(!(start.isDouble()||start.isInt()) || !(end.isDouble()||end.isInt()))return;
            const double a=static_cast<double>(start),b=static_cast<double>(end);
            if(!std::isfinite(a)||!std::isfinite(b)||a<0||b<a||b>86400)return;
            engine.selectionStart=a;engine.selectionEnd=b;engine.playback.loopRangeStart=a;engine.playback.loopRangeEnd=b;if(engine.dynamicTransport.load()&&!engine.playback.playing.load()){engine.playStart=a;engine.playback.seekSeconds=a;}publish();return;
        }
        if(type=="playStart")
        {
            const auto position=message.getProperty("position",{});if(!(position.isDouble()||position.isInt()))return;
            const double seconds=static_cast<double>(position);if(!std::isfinite(seconds)||seconds<0||seconds>86400)return;engine.playStart=seconds;engine.playback.seekSeconds=seconds;publish();return;
        }
        if(type=="dynamicTransport" || type=="punch")
        {
            const auto enabled=message.getProperty("enabled",{});if(!enabled.isBool())return;
            if(type=="dynamicTransport"){const bool on=(bool)enabled;if(on&&!engine.dynamicTransport.load()){const auto pending=engine.playback.seekSeconds.load();engine.playStart=pending>=0?pending:engine.playback.seconds.load();}engine.dynamicTransport=on;}else engine.punchEnabled=(bool)enabled;publish();return;
        }
        if (type == "preRoll")
        {
            const auto bars = message.getProperty("bars", {});
            if (!bars.isInt() || static_cast<int>(bars)<0 || static_cast<int>(bars)>4) return;
            engine.preRollBars = static_cast<int>(bars); markDirty(); publish(); return;
        }
        if (type == "clickLevel")
        {
            const auto gain = message.getProperty("gain", {});
            if (!(gain.isDouble() || gain.isInt()) || !std::isfinite(static_cast<double>(gain)) || static_cast<double>(gain)<0 || static_cast<double>(gain)>1) return;
            engine.playback.metronomeGain = static_cast<float>(gain); markDirty(); publish(); return;
        }
        if (type == "metronome")
        {
            const auto enabled = message.getProperty("enabled", {}); if (!enabled.isBool()) return;
            if (engine.playback.metronome.load() != static_cast<bool>(enabled)) markDirty();
            engine.playback.metronome = static_cast<bool>(enabled); publish(); return;
        }
        if (type == "import")
        {
            if (loading || chooser) return;
            const auto id = message.getProperty("id", -1);
            if (! id.isInt() || static_cast<int>(id) < 0 || static_cast<int>(id) >= trackCount) return;
            if(engine.playback.tracks[static_cast<int>(id)].routing.aux){status="Aux returns receive buses and sends; select an audio track for WAV import.";publish();return;}
            if(engine.playback.tracks[static_cast<int>(id)].midiTrack){status="Use Import MIDI on a MIDI track.";publish();return;}
            importingTrack = static_cast<int>(id);
            const auto& track=engine.playback.tracks[static_cast<size_t>(importingTrack)];
            if(track.extra.size()+1>=ClipLayout::maximumSources || track.clips.size()>=ClipLayout::maximumClips){status="Track limit: 32 sources / 128 clips. Clear a track to free sources.";publish();return;}
            chooser = std::make_unique<juce::FileChooser>("Import a mono or stereo WAV", juce::File(), "*.wav");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& selection)
                {
                    if (safe == nullptr) return;
                    const auto file = selection.getResult();
                    if (file.existsAsFile())
                    {
                        safe->loading = safe->loader.load(file, safe->engine.importBudget(static_cast<size_t>(safe->importingTrack),true));
                        safe->status = safe->loading ? "Loading WAV..." : "An import is already in progress.";
                        safe->publish();
                    }
                    // Release chooser after its callback has returned.
                    juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->chooser.reset(); });
                });
            return;
        }
        if (type == "devices")
        {
            if(engine.playback.playing.load()){status="Stop playback before changing the audio device.";publish();return;}
            settings = std::make_unique<AudioSettingsWindow>(engine.devices, [safe = juce::Component::SafePointer<MainComponent>(this)]
            {
                if (!safe) return; const auto saved = safe->engine.saveDeviceSettings();
                safe->status = saved.wasOk() ? "Audio driver and channel settings saved for next launch." : saved.getErrorMessage(); safe->publish(true);
            }); return;
        }
        if (type == "clear")
        {
            const auto id = message.getProperty("id", -1);
            if (!id.isInt() || static_cast<int>(id) < 0 || static_cast<int>(id) >= 128) return;
            const auto index = static_cast<size_t>(static_cast<int>(id));
            clearBatchHistory();undoClips.clear();redoClips.clear();midiRedo.clear();midiUndo.clear();midiRedo.clear();engine.detach();engine.playback.tracks[index].clips.clear();syncPlaylist(static_cast<int>(index));finishClipEdit();
            if (static_cast<int>(index) == grooveTrack) { groove = {}; grooveTrack = -1; }
            markDirty();
            status = "Cleared " + trackNames[index];
            publish(true); return;
        }
        if (type == "master")
        {
            const auto value = message.getProperty("gain", {});
            if (!(value.isDouble() || value.isInt())) return;
            const auto gain = static_cast<double>(value);
            if (!std::isfinite(gain)) return;
            const auto level = static_cast<float>(juce::jlimit(0.0, 1.0, gain));
            if (engine.playback.masterGain.load() != level) markDirty();
            engine.playback.masterGain = level;
            return;
        }
        const auto oldBpm = controls.bpm; const auto oldLoop = engine.playback.loop.load();
        const int changedTrack=static_cast<int>(message.getProperty("id",-1));
        const bool oldArmed=changedTrack>=0&&changedTrack<trackCount?controls.channels[changedTrack].armed:false;
        controls.accept(message);
        if (type == "track")
        {
            const int id = static_cast<int>(message.getProperty("id", -1));
            if (id < 0 || id >= trackCount) return;
            if(recordingBusy)controls.channels[static_cast<size_t>(id)].armed=oldArmed;
            if(engine.playback.tracks[id].routing.aux)controls.channels[id].armed=false;
        }
        if (type == "track") markDirty();
        if (type == "transport")
        {
            auto* device = engine.devices.getCurrentAudioDevice();
            if (controls.playing && (engine.playback.duration() <= 0.0 || device == nullptr || ! device->isPlaying()))
            { status = engine.playback.duration() <= 0.0 ? "Import a WAV before pressing Play." : "Choose an output in Audio settings."; engine.playback.playing = false; }
            else
            {
                if (controls.playing && engine.playback.seconds.load() >= engine.playback.duration()) { engine.resetPluginGraph(); engine.playback.seekSeconds = 0.0; }
                if(controls.playing && !engine.playback.playing.load() && engine.dynamicTransport.load())engine.playback.seekSeconds=std::min(engine.playStart.load(),engine.playback.duration());
                engine.playback.playing = controls.playing;
            }
            const auto loop = message.getProperty("loop", false);
            engine.playback.loop = loop.isBool() && static_cast<bool>(loop);
            if(oldBpm!=controls.bpm)setSessionTempo(controls.bpm);
            engine.playback.bpm = controls.bpm;
            if (oldBpm != controls.bpm || oldLoop != engine.playback.loop.load()) markDirty();
        }
        else if (type == "stop") { engine.playback.playing = false; engine.playback.seekSeconds = 0.0; }
        else if (type == "seek")
        {
            const bool wasPlaying = engine.playback.playing.load(); engine.resetPluginGraph();
            const auto target=juce::jlimit(0.0,engine.dynamicTransport.load()?86400.0:engine.playback.duration(),controls.beat*60.0/controls.bpm);engine.playback.seekSeconds=target;if(engine.dynamicTransport.load())engine.playStart=target;
            engine.playback.playing = wasPlaying;
        }
        else if (type == "track") {applyChannels();automationTick();}
        if(type=="transport")automationTick();
        publish(type == "track");
    }
    juce::WebBrowserComponent::Options makeOptions()
    {
        using Browser = juce::WebBrowserComponent;
        return Browser::Options()
            .withBackend(Browser::Options::Backend::webview2)
            .withWinWebView2Options(Browser::Options::WinWebView2()
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Tracksmith/WebView2")))
            .withNativeIntegrationEnabled()
            .withEventListener("command", [this](const juce::var& message) { command(message); })
            .withResourceProvider([](const juce::String& path) -> std::optional<Browser::Resource>
            {
                const char* bytes = nullptr;
                int size = 0;
                juce::String mime;
                if (path == "/" || path == "/index.html") { bytes = BinaryData::index_html; size = BinaryData::index_htmlSize; mime = "text/html"; }
                else if (path == "/app.js") { bytes = BinaryData::app_js; size = BinaryData::app_jsSize; mime = "text/javascript"; }
                else if (path == "/app.css") { bytes = BinaryData::app_css; size = BinaryData::app_cssSize; mime = "text/css"; }
                else return std::nullopt;
                std::vector<std::byte> data(static_cast<size_t>(size));
                std::memcpy(data.data(), bytes, static_cast<size_t>(size));
                return Browser::Resource { std::move(data), mime };
            });
    }
    void deleteTrack(int id)
    {
        if(id<0||id>=trackCount||trackCount<=1||loading||recordingBusy||sessionBusy||pluginBusy||exportBusy||chooser||recoveryWorker.isBusy())return;
        const double position=engine.playback.seconds.load();engine.playback.playing=false;finishAutomation();engine.detach();
        const auto& removedSlot=engine.plugins.channels[id][0];const bool removedClick=removedSlot&&removedSlot->description.fileOrIdentifier=="tracksmith-native-click";
        if(!TrackEditing::remove(engine,id,trackCount)){engine.attach();return;}
        stopRemovedClick(removedClick);
        const auto name=trackNames[id];for(int i=id;i<trackCount-1;++i){controls.channels[i]=controls.channels[i+1];trackInfo[i]=std::move(trackInfo[i+1]);trackNames[i]=trackNames[i+1];trackPlaylists[i]=std::move(trackPlaylists[i+1]);activePlaylists[i]=activePlaylists[i+1];automationPreviousMode[i]=automationPreviousMode[i+1];}
        --trackCount;controls.channels[trackCount]={};trackInfo[trackCount]={};trackNames[trackCount]="Audio "+juce::String(trackCount+1);trackPlaylists[trackCount].clear();activePlaylists[trackCount]=1;automationPreviousMode[trackCount]=0;
        clearBatchHistory();clearClipHistory();automationUndo.clear();automationRedo.clear();for(auto& pass:automationPasses)pass={};automationCurvesChanged=true;pluginAutomationChannel=-1;groove={};grooveTrack=-1;
        applyChannels();engine.plugins.resetGraph();engine.playback.seekSeconds=std::min(position,engine.playback.duration());engine.attach();markDirty();status="Deleted "+name+". Source audio files are retained. Track deletion cannot be undone.";publish(true,std::min(id,trackCount-1));
    }
    juce::var midiDropResult;
    void importDroppedMidi(const juce::var& message)
    {
        const auto request=message.getProperty("request",{}).toString();const auto lane=message.getProperty("track",{});const int track=lane.isInt()?static_cast<int>(lane):-1;
        const auto report=[&](bool success,const juce::String& text,const std::vector<int>& ids=std::vector<int>{}){auto* result=new juce::DynamicObject();result->setProperty("request",request.substring(0,100));result->setProperty("track",track);result->setProperty("success",success);result->setProperty("message",text);juce::Array<juce::var> added;for(int id:ids)added.add(id);result->setProperty("ids",added);midiDropResult=juce::var(result);status=text;publish(true);};
        if(track<0||track>=trackCount||!engine.plugins.midiLanes[track].enabled){report(false,"Drop the groove onto an Instrument track.");return;}
        if(recordingBusy||loading||sessionBusy||pluginBusy||exportBusy||recoveryWorker.isBusy()||chooser){report(false,"MIDI was not imported: wait for recording, loading, saving or the open dialog to finish, then drop again.");return;}
        const auto encoded=message.getProperty("data",{}).toString();const double beat=static_cast<double>(message.getProperty("beat",-1));juce::MemoryOutputStream bytes;
        if(encoded.length()>1400000||!std::isfinite(beat)||beat<0||beat>=100000||!juce::Base64::convertFromBase64(bytes,encoded)||bytes.getDataSize()>1024*1024){report(false,"MIDI was not imported: use a groove under 1 MiB at a valid timeline position.");return;}
        juce::MemoryInputStream stream(bytes.getData(),bytes.getDataSize(),false);const auto& old=engine.plugins.midiLanes[track].notes;auto notes=old;
        if(!appendMidiGroove(stream,beat,notes)){report(false,"MIDI was not imported. The groove may overlap existing notes: try an empty position or clear that range. Use PPQ MIDI with at most 2048 total notes.");return;}
        std::vector<int> ids;for(const auto& n:notes)if(std::none_of(old.begin(),old.end(),[&](const auto& previous){return n.id==previous.id;}))ids.push_back(n.id);
        if(automationPassActive){automationTick();engine.playback.playing=false;finishAutomation();}
        applyMidiNotes(track,std::move(notes));report(true,"Imported "+juce::String(static_cast<int>(ids.size()))+" MIDI notes into "+trackNames[track]+".",ids);
    }
    void applyMidiNotes(int track,std::vector<MidiNote> notes)
    {
        if(track>=0&&track<trackCount){auto& native=engine.plugins.channels[track][0];if(native&&native->instance)NativeInstruments::useSongMidi(native->instance.get());}
        std::sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.start<b.start;});
        for(size_t a=0;a<notes.size();++a)for(size_t b=a+1;b<notes.size();++b)if(notes[a].pitch==notes[b].pitch&&notes[a].channel==notes[b].channel&&notes[b].start<notes[a].start+notes[a].length-1.e-8){status="Overlapping notes of the same pitch/channel are not supported yet.";publish();return;}
        midiUndo.push_back({track,engine.plugins.midiLanes[track].notes,notes,activePlaylists[track]});if(midiUndo.size()>64)midiUndo.erase(midiUndo.begin());midiRedo.clear();redoClips.clear();
        clearBatchHistory();engine.detach();engine.plugins.midiLanes[track].notes=std::move(notes);syncPlaylist(track);engine.playback.tracks[track].midiSeconds=engine.plugins.midiLanes[track].endBeat()*60/controls.bpm+1;finishClipEdit();markDirty();status="MIDI notes updated. Playback stopped for editing.";publish(true);
    }
    void chooseMidiFile(int track,bool saving)
    {
        if(chooser)return;chooser=std::make_unique<juce::FileChooser>(saving?"Export MIDI notes":"Import MIDI notes",juce::File(),"*.mid;*.midi");
        const auto midiFlags=saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(midiFlags,[safe=juce::Component::SafePointer<MainComponent>(this),track,saving](const juce::FileChooser& selection)
        {
            if(!safe)return;const auto file=selection.getResult();
            if(file!=juce::File())
            {
                if(saving){juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(60000000/safe->controls.bpm)));for(const auto& n:safe->engine.plugins.midiLanes[track].notes){auto on=juce::MidiMessage::noteOn(n.channel,n.pitch,n.velocity),off=juce::MidiMessage::noteOff(n.channel,n.pitch);on.setTimeStamp(n.start*960);off.setTimeStamp((n.start+n.length)*960);sequence.addEvent(on);sequence.addEvent(off);}sequence.updateMatchedPairs();juce::MidiFile midi;midi.setTicksPerQuarterNote(960);midi.addTrack(sequence);juce::TemporaryFile temporary(file.withFileExtension("mid"));auto stream=temporary.getFile().createOutputStream();const bool written=stream&&midi.writeTo(*stream);if(stream)stream->flush();stream.reset();safe->status=written&&temporary.overwriteTargetFileWithTemporary()?"MIDI exported.":"MIDI export failed.";}
                else
                {
                    juce::MidiFile midi;juce::FileInputStream stream(file);std::vector<MidiNote> notes;bool ok=file.getSize()<=1024*1024&&midi.readFrom(stream)&&midi.getTimeFormat()>0;int id=1;
                    if(ok)for(int lane=0;lane<midi.getNumTracks()&&ok;++lane){juce::MidiMessageSequence sequence(*midi.getTrack(lane));sequence.updateMatchedPairs();for(int i=0;i<sequence.getNumEvents();++i){auto* event=sequence.getEventPointer(i);if(event->message.isNoteOn()&&event->noteOffObject){const auto& message=event->message;MidiNote n{id++,message.getNoteNumber(),message.getChannel(),message.getTimeStamp()/midi.getTimeFormat(),(event->noteOffObject->message.getTimeStamp()-message.getTimeStamp())/midi.getTimeFormat(),message.getFloatVelocity()};if(validMidiNote(n))notes.push_back(n);if(notes.size()>MidiLane::maximumNotes){ok=false;break;}}}}
                    if(ok)safe->applyMidiNotes(track,std::move(notes));else safe->status="Use a PPQ MIDI file under 1 MiB with at most 2048 notes. SMPTE MIDI is not supported.";
                }
            }safe->publish(true);juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});
        });
    }
    struct MidiChange { int track;std::vector<MidiNote> before,after;int playlist=0; uint64_t serial=nextTimelineEdit(); };std::vector<MidiChange> midiUndo,midiRedo;
    void finishClipEdit()
    {
        const auto position=engine.playback.seconds.load();engine.playback.refreshDuration();engine.playback.reset();engine.plugins.resetGraph();engine.playback.seekSeconds=std::min(position,engine.playback.duration());engine.attach();
    }
    struct ClipChange { int track=0;std::vector<TimelineClip> before,after;int playlist=0; uint64_t serial=nextTimelineEdit(); };
    std::vector<ClipChange> undoClips,redoClips;
    SessionControls controls; // Must outlive the browser callbacks.
    AudioEngine engine;
    bool exportBusy = false;
    juce::var exportSnapshot;
    juce::File exportFile;
    juce::String audioError, status { "Import WAVs onto separate tracks to mix them." };
    struct MediaInfo { juce::String fileName; juce::File file; juce::Array<juce::var> waveform; };
    struct TrackInfo : MediaInfo { std::vector<MediaInfo> extra; };
    std::array<TrackInfo, 128> trackInfo;
    std::array<std::vector<TrackPlaylist>,128> trackPlaylists;
    std::array<int,128> activePlaylists=[] {std::array<int,128> a;a.fill(1);return a;}();
    std::array<juce::String, 128> trackNames;
    bool nativeAnalysisPending=false;int nativeAnalysisTrack=-1,nativeAnalysisSource=-1;uint64_t nativeAnalysisToken=0;juce::String nativeAnalysisMessage;
    int sessionRevision=0;juce::String newSessionName;
    int trackCount = 1;
    bool dirty = false, sessionBusy = false;
    PluginDiscovery discovery;
    StartupScan startupScan;
    StartupScan::Progress startupProgress;
    std::unique_ptr<juce::AlertWindow> trackDialog,pluginSearch;
    bool pluginBusy = false;
    juce::Array<juce::var> restoringInserts;
    int restoreIndex = 0;
    juce::StringArray missingPlugins;
    uint64_t pluginChangeSerial = 0, savingPluginSerial = 0;
    int diagnosticTicks = 0;
    juce::File generationGrooveFolder;
    juce::File sessionFile;
    int importingTrack = 0;
    bool loading = false;
    bool recordingBusy = false, loadingTake = false;
    int recordingTrack = -1;
    std::vector<TrackTake> pendingRecorded;
    juce::StringArray recordingIssues;
    juce::File lastTakeFile;
    juce::String takeWarning;
    uint64_t expectedTakeFrames = 0;
    juce::Label error;
    juce::WebBrowserComponent browser;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<AudioSettingsWindow> settings;
    WavLoader loader;
    SessionWorker sessionWorker;
    GrooveWorker grooveWorker;
    GrooveAnalysis groove;
    int grooveTrack = -1;
    double grooveSeconds = 0;
    MixExportWorker mixExporter;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow() : DocumentWindow("Tracksmith", juce::Colour(0xff101217), allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        setResizable(true, false);
        setResizeLimits(900, 620, 3840, 2160);
        centreWithSize(1280, 820);
        setVisible(true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    void requestClose() { if (auto* content = dynamic_cast<MainComponent*>(getContentComponent())) content->requestClose(); }
};
class TracksmithApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Tracksmith"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise(const juce::String& args) override {
        juce::StringArray tokens;tokens.addTokens(args,true);tokens.removeEmptyStrings();
        if(tokens.size()==3&&tokens[0]=="--scan-vst3"){PluginDiscovery::scan(tokens[1].unquoted(),tokens[2].unquoted());quit();return;}
        if(tokens.size()==3&&tokens[0]=="--editing-regression"){auto test=std::make_unique<MainComponent>(true);const bool ok=test->editingRegression(juce::File(tokens[1].unquoted()),juce::File(tokens[2].unquoted()));test.reset();setApplicationReturnValue(ok?0:1);quit();return;}
        PluginDiscovery::recover();
        mainWindow=std::make_unique<MainWindow>();
    }
    bool moreThanOneInstanceAllowed() override { return getCommandLineParameters().contains("--scan-vst3")||getCommandLineParameters().contains("--editing-regression"); }
    void shutdown() override { mainWindow.reset(); }
    void systemRequestedQuit() override { if (mainWindow) mainWindow->requestClose(); else quit(); }
private:
    std::unique_ptr<MainWindow> mainWindow;
};
START_JUCE_APPLICATION(TracksmithApplication)




