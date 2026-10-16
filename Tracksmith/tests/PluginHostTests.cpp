#include "StockTimeTests.h"
#include "StockFinalDynamicsTests.h"
#include "StockFetTests.h"
#include "StockBusTests.h"
#include "StockGateTests.h"
#include "StockEqTests.h"
#include "ReliabilitySoak.h"
#include "../source/TrackEditing.h"
#include "../source/MidiBatchEditing.h"

#include "../source/MidiGrooveImport.h"
#include "../source/RecoveryStore.h"
#include "../source/PlaylistEditing.h"
#include "../source/GroupEditing.h"
#include "../source/PluginRack.h"
#include "../source/MixExport.h"
#include "../source/GrooveWorker.h"
#include "../source/SessionFile.h"
#include "../source/AudioEngine.h"
#include <iostream>
#include "../plugins/AvrxNam/NoiseGate.h"
#include <cstdlib>
static void require(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
static bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
#define grooveCheck(condition) require((condition), "MIDI groove import")
int main(int argc, char** argv)
{
    if(argc>5&&juce::String(argv[4])=="--delay")return runDelayTests(argv[3],argv[5]);
    if(argc>5&&juce::String(argv[4])=="--plate")return runPlateTests(argv[3],argv[5]);
    if(argc>5&&juce::String(argv[4])=="--limiter")return runLimiterTests(argv[3],argv[5]);
    if(argc>5&&juce::String(argv[4])=="--deesser")return runDeesserTests(argv[3],argv[5]);
    if(argc>5&&juce::String(argv[4])=="--fet")return runFetTests(argv[3],argv[5]);
    if(argc>5&&juce::String(argv[4])=="--bus")return runBusTests(argv[3],argv[5]);
    if(argc>5&&juce::String(argv[4])=="--gate")return runGateTests(argv[3],argv[5]);
    if(argc>4&&juce::String(argv[4])=="--eq")return runStockEqTests(argv[3],argc>5?argv[5]:"eq-preview.png");
    if(argc>4&&juce::String(argv[4])=="--soak")return runReliabilitySoak(juce::String::fromUTF8(argv[2]),argc>5?juce::jlimit(60,600,juce::String(argv[5]).getIntValue()):120);

    {
        juce::MidiMessageSequence events;
        const auto add=[&](bool on,double time,float velocity){auto m=on?juce::MidiMessage::noteOn(10,38,velocity):juce::MidiMessage::noteOff(10,38);m.setTimeStamp(time);events.addEvent(m);};
        add(true,0,.4f);add(true,0,.9f);add(true,60,.6f);add(false,120,0);add(false,120,0);add(false,180,0);
        juce::MidiFile file;file.setTicksPerQuarterNote(480);file.addTrack(events);juce::MemoryOutputStream bytes;require(file.writeTo(bytes),"write duplicate and flam fixture");
        std::vector<MidiNote> notes{{19,60,1,0,1,.5f}};juce::MemoryInputStream input(bytes.getData(),bytes.getDataSize(),false);require(appendMidiGroove(input,8,notes)&&notes.size()==3,"duplicate GGD hits import without dropping distinct flam");
        require(notes[1].start==8&&notes[2].start==8.125&&notes[1].pitch==38&&notes[1].channel==10&&notes[1].length==.125&&std::abs(notes[1].velocity-.9f)<.01f,"duplicate strongest velocity retained and retrigger gate shortened");
        const auto original=notes;juce::MemoryInputStream overlap(bytes.getData(),bytes.getDataSize(),false);require(!appendMidiGroove(overlap,8,notes)&&notes.size()==original.size(),"conflicting drop leaves current notes unchanged");
        auto* erase=new juce::DynamicObject();erase->setProperty("action","delete");erase->setProperty("ids",juce::Array<juce::var>{notes[1].id,notes[2].id});require(editMidiBatch(notes,juce::var(erase)),"clear imported MIDI selection");juce::MemoryInputStream retry(bytes.getData(),bytes.getDataSize(),false);require(appendMidiGroove(retry,8,notes)&&notes.size()==3,"GGD drop works after selection deletion");
        juce::MidiMessageSequence zero;auto on=juce::MidiMessage::noteOn(1,36,.8f),off=juce::MidiMessage::noteOff(1,36);zero.addEvent(on);zero.addEvent(off);juce::MidiFile zeroFile;zeroFile.setTicksPerQuarterNote(480);zeroFile.addTrack(zero);juce::MemoryOutputStream zeroBytes;require(zeroFile.writeTo(zeroBytes),"zero gate fixture");juce::MemoryInputStream zeroInput(zeroBytes.getData(),zeroBytes.getDataSize(),false);std::vector<MidiNote> zeroNotes;require(appendMidiGroove(zeroInput,0,zeroNotes)&&zeroNotes.size()==1&&validMidiNote(zeroNotes[0]),"zero-length drum trigger gets one-tick gate");
    }

    if(argc>3){const juce::File folder(juce::String::fromUTF8(argv[3]));const auto files=folder.findChildFiles(juce::File::findFiles,true,"*.mid");require(!files.isEmpty(),"GGD fixture directory contains MIDI");int passed=0;for(const auto& file:files){juce::FileInputStream input(file);std::vector<MidiNote> notes;if(!appendMidiGroove(input,8,notes)){std::cerr<<"GGD import rejected: "<<file.getFileName()<<'\n';std::exit(1);}require(!notes.empty()&&notes.front().start>=8,"GGD notes use drop offset");++passed;}std::cout<<"GGD MIDI files imported: "<<passed<<'\n';}

    {
        std::vector<MidiNote> notes{{1,60,1,0,1,.8f},{2,64,1,0,1,.7f},{3,67,1,2,1,.6f}};
        auto* command=new juce::DynamicObject();command->setProperty("action","delete");command->setProperty("ids",juce::Array<juce::var>{1,2});juce::var message(command);
        require(editMidiBatch(notes,message)&&notes.size()==1&&notes[0].id==3,"delete highlighted chord atomically");
        command->setProperty("ids",juce::Array<juce::var>{3,999});require(!editMidiBatch(notes,message)&&notes.size()==1,"invalid batch never partly deletes");
        auto original=notes[0],a=original,b=original;a.id=4;a.pitch=72;b.id=5;b.pitch=76;command->setProperty("action","add");command->setProperty("notes",juce::Array<juce::var>{SessionFile::encodeNote(a),SessionFile::encodeNote(b)});require(editMidiBatch(notes,message)&&notes.size()==3,"paste chord as a single candidate");
        a.start=4;b.start=4;command->setProperty("action","update");command->setProperty("notes",juce::Array<juce::var>{SessionFile::encodeNote(a),SessionFile::encodeNote(b)});require(editMidiBatch(notes,message)&&notes[1].start==4&&notes[2].start==4,"move multi-note selection together");
        b.pitch=72;command->setProperty("notes",juce::Array<juce::var>{SessionFile::encodeNote(a),SessionFile::encodeNote(b)});require(!editMidiBatch(notes,message)&&notes[2].pitch==76,"reject batch overlaps without partial changes");
        b.pitch=76;b.id=4;command->setProperty("notes",juce::Array<juce::var>{SessionFile::encodeNote(a),SessionFile::encodeNote(b)});require(!editMidiBatch(notes,message),"reject repeated IDs");
    }
    {
        auto owner=std::make_unique<AudioEngine>();auto& e=*owner;
        e.playback.tracks[1].routing.aux=true;e.playback.tracks[3].routing.aux=true;
        auto& audio=e.playback.tracks[2];audio.left={.1f,.2f};audio.right=audio.left;audio.sourceRate=44100;audio.timelineEnabled=true;audio.clips={{1,0,0,0,2.0/44100}};audio.volumeAutomation={{0,.3f}};audio.automationMode=1;audio.gain=.4f;e.trackInputs[2]=2;e.trackInputChannels[2]=2;e.armedMonitorMask[2]=true;
        auto route=e.playback.routingSnapshot(0);route.output=3;route.sends[0]={1,.6f,true};e.playback.setRouting(0,route);
        e.plugins.midiLanes[4].enabled=true;e.plugins.midiLanes[4].notes={{8,36,10,1,.25,.9f}};e.playback.tracks[4].midiTrack=true;e.playback.tracks[4].midiSeconds=2;
        e.plugins.channels[4][0]=std::make_unique<PluginRack::Slot>();auto* instrument=e.plugins.channels[4][0].get();instrument->description.name="Retained instrument";
        e.plugins.channels[128][0]=std::make_unique<PluginRack::Slot>();auto* master=e.plugins.channels[128][0].get();
        e.monitor.track=4;e.monitor.enabled=true;
        require(TrackEditing::remove(e,1,5),"delete aux in middle");
        require(e.playback.tracks[1].left.size()==2&&e.playback.tracks[1].sourceRate==44100&&e.playback.tracks[1].clips.size()==1&&e.playback.tracks[1].volumeAutomation.size()==1&&e.playback.tracks[1].automationMode==1,"audio clips and automation shift intact");
        require(e.trackInputs[1]==2&&e.trackInputChannels[1]==2&&e.armedMonitorMask[1],"input assignment and armed monitor shift intact");
        const auto changed=e.playback.routingSnapshot(0);require(changed.output==2&&changed.sends[0].target==-1&&changed.sends[0].gain==0,"remaining aux remapped and deleted aux sends disconnected");
        require(e.plugins.channels[3][0].get()==instrument&&e.plugins.channels[128][0].get()==master&&e.plugins.midiLanes[3].notes[0].id==8&&e.playback.tracks[3].midiTrack,"instrument/MIDI moves while master stays fixed");
        require(e.monitor.track==3&&e.monitor.enabled&&!e.playback.tracks[4].loaded()&&!e.plugins.channels[4][0],"monitor follows track and vacated slot cleared");
        require(TrackEditing::remove(e,3,4)&&!e.monitor.enabled,"delete monitored track disables its monitor");
        require(TrackEditing::remove(e,0,3)&&e.playback.tracks[0].left.size()==2,"first track deletion retains next audio track");
        require(!TrackEditing::remove(e,0,1),"keep one track in session");
    }

    {
        juce::MidiMessageSequence sequence;auto on=juce::MidiMessage::noteOn(10,38,.75f),off=juce::MidiMessage::noteOff(10,38);on.setTimeStamp(240);off.setTimeStamp(360);sequence.addEvent(on);sequence.addEvent(off);
        juce::MidiFile midi;midi.setTicksPerQuarterNote(480);midi.addTrack(sequence);juce::MemoryOutputStream bytes;grooveCheck(midi.writeTo(bytes));
        std::vector<MidiNote> notes{{9,60,1,0,1,.5f}};
        juce::MemoryInputStream input(bytes.getData(),bytes.getDataSize(),false);grooveCheck(appendMidiGroove(input,8,notes));grooveCheck(notes.size()==2&&notes[0].id==9&&notes[1].id==10&&notes[1].pitch==38&&notes[1].channel==10&&notes[1].start==8.5&&notes[1].length==.25&&std::abs(notes[1].velocity-.75f)<.01f);
        juce::MemoryInputStream duplicate(bytes.getData(),bytes.getDataSize(),false);grooveCheck(!appendMidiGroove(duplicate,8,notes)&&notes.size()==2);
        juce::MemoryInputStream beyond(bytes.getData(),bytes.getDataSize(),false);grooveCheck(!appendMidiGroove(beyond,99999.5,notes)&&notes.size()==2);
        juce::MemoryInputStream corrupt("bad",3,false);grooveCheck(!appendMidiGroove(corrupt,0,notes)&&notes.size()==2);
        midi.setSmpteTimeFormat(25,40);juce::MemoryOutputStream smpte;grooveCheck(midi.writeTo(smpte));juce::MemoryInputStream unsupported(smpte.getData(),smpte.getDataSize(),false);grooveCheck(!appendMidiGroove(unsupported,0,notes)&&notes.size()==2);
    }

    NoiseGate gate;
    for(int n=0;n<96000;++n)gate.next(.000063f,true,-60,120,48000);
    require(gate.gain<.00001f,"gate suppresses low idle noise");
    for(int n=0;n<480;++n)gate.next(.1f,true,-60,120,48000);
    require(gate.gain>.999f,"gate opens for instrument attack");
    for(int n=0;n<96000;++n)gate.next(0,true,-60,120,48000);
    require(gate.gain<.00001f,"gate releases smoothly to silence");
    for(int n=0;n<480;++n)gate.next(0,false,-60,120,48000);
    require(gate.gain>.999f,"disabled gate passes sustained quiet input");
    juce::ScopedJuceInitialiser_GUI gui;
    require(argc > 2, "provide stock NAM and delay-test VST3 paths");
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile("work/plugin-tests");
    require(folder.createDirectory().wasOk(), "test folder");
    const auto capture = folder.getChildFile("Linear.nam");
    require(capture.replaceWithText(R"({"version":"0.5.4","architecture":"Linear","sample_rate":48000,"config":{"receptive_field":1,"bias":false},"weights":[0.5]})"), "linear model fixture");
    PluginRack rack;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    rack.formats.getFormat(0)->findAllTypesForFile(descriptions, juce::String(argv[1]));
    require(descriptions.size() == 1 && descriptions[0]->name == "Tracksmith NAM", "scan actual stock VST3");
    juce::String error;
    auto instance = rack.formats.createPluginInstance(*descriptions[0], 48000, 256, error);
    require(instance != nullptr, "instantiate actual stock VST3");
    auto state = juce::ValueTree("AVRX_NAM"); state.setProperty("modelPath", capture.getFullPathName(), nullptr);
    juce::MemoryBlock component; juce::AudioProcessor::copyXmlToBinary(*state.createXml(), component);
    juce::XmlElement hostState("VST3PluginState"); hostState.createNewChildElement("IComponent")->addTextElement(component.toBase64Encoding());
    juce::MemoryBlock bytes; juce::AudioProcessor::copyXmlToBinary(hostState, bytes);
    require(rack.install(0, 0, *descriptions[0], std::move(instance), bytes, false, 48000).wasOk(), "install stock VST3");
    float left[256], right[256];
    auto render = [&]
    {
        std::fill_n(left, 256, 0.4f); std::fill_n(right, 256, 0.4f);
        PluginRack::process(&rack, 0, left, right, 256, 1.0, 120.0, true);
    };
    bool loaded = false;
    for (int i = 0; i < 100 && !loaded; ++i)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(30); render(); loaded = near(left[255], 0.2f);
    }
    require(loaded && near(right[255], 0.2f), "worker-loaded NAM produces expected linear DSP samples");
    render(); require(near(left[0], 0.2f), "model runs at block start");
    require(near(static_cast<float>(*rack.clock.getPosition()->getTimeInSeconds()), 1.0f), "VST3 host publishes transport time");
    rack.channels[0][0]->bypass = true; render(); require(near(left[255], 0.4f), "host bypass passes stereo through"); rack.channels[0][0]->bypass = false;
    const auto saved = rack.save(); require(saved.size() == 1, "insert state saved");
    juce::MemoryBlock retained; require(retained.fromBase64Encoding(saved[0].getProperty("state", {}).toString()), "state base64 decodes");
    auto second = rack.formats.createPluginInstance(*descriptions[0], 48000, 256, error);
    require(second != nullptr && rack.install(0, 1, *descriptions[0], std::move(second), retained, false, 48000).wasOk(), "second independent instance restores state");
    loaded = false;
    for (int i = 0; i < 100 && !loaded; ++i) { juce::MessageManager::getInstance()->runDispatchLoopUntil(30); render(); loaded = near(left[255], 0.1f); }
    require(loaded, "two independent inserts process in serial order");
    rack.channels[0][1].reset();
    const auto setParameter=[&](const juce::String& id,float value)
    {
        for(auto* parameter:rack.channels[0][0]->instance->getParameters())
            if(parameter->getName(100)==(id=="gate"?"Noise gate":id=="threshold"?"Gate threshold dBFS":"Gate release ms"))
            {parameter->setValueNotifyingHost(id=="gate"?value:id=="threshold"?(value+90)/70:(value-20)/480);return;}
        require(false,"NAM gate parameter exposed through VST3");
    };
    setParameter("gate",1); setParameter("threshold",-55);setParameter("release",80);
    for(int i=0;i<400;++i){std::fill_n(left,256,.000063f);std::fill_n(right,256,.000063f);PluginRack::process(&rack,0,left,right,256,0,120,false);}
    require(std::abs(left[255])<1.e-8f,"actual NAM gate suppresses amplified idle input");
    for(int i=0;i<4;++i)render();require(near(left[255],.2f),"actual NAM gate opens for performance");
    const auto gateState=rack.save();juce::MemoryBlock gateBytes;require(gateBytes.fromBase64Encoding(gateState[0].getProperty("state",{}).toString()),"gate state serializes");
    setParameter("gate",0);rack.channels[0][0]->instance->setStateInformation(gateBytes.getData(),static_cast<int>(gateBytes.getSize()));
    for(int i=0;i<400;++i){std::fill_n(left,256,.000063f);std::fill_n(right,256,.000063f);PluginRack::process(&rack,0,left,right,256,0,120,false);}
    require(std::abs(left[255])<1.e-8f,"NAM gate settings restore with plugin state");
    setParameter("gate",0); for(int i=0;i<4;++i)render();

    const auto malformedModel = folder.getChildFile("Malformed.nam"); require(malformedModel.replaceWithText("{bad json"), "bad model fixture");
    state.setProperty("modelPath", malformedModel.getFullPathName(), nullptr);
    juce::AudioProcessor::copyXmlToBinary(*state.createXml(), component);
    juce::XmlElement malformedState("VST3PluginState"); malformedState.createNewChildElement("IComponent")->addTextElement(component.toBase64Encoding());
    juce::MemoryBlock malformedBytes; juce::AudioProcessor::copyXmlToBinary(malformedState, malformedBytes);
    rack.channels[0][0]->instance->setStateInformation(malformedBytes.getData(), static_cast<int>(malformedBytes.getSize()));
    for(int i=0;i<20;++i) { juce::MessageManager::getInstance()->runDispatchLoopUntil(30);render(); }
    require(near(left[255],0.2f),"malformed NAM load keeps previously active DSP");
    require(rack.install(128, 0, *descriptions[0], nullptr, retained, true, 48000).wasOk(), "unavailable insert placeholder retained");
    require(rack.snapshot(1).size() == 2 && static_cast<bool>(rack.snapshot(1)[1].getProperty("missing", false)), "missing insert visible in mixer");
    require(rack.save()[1].getProperty("state", {}).toString() == retained.toBase64Encoding(), "missing plugin state survives save");
    SavedSession session; session.tracks.push_back({ "Guitar", {} }); session.inserts = rack.save();
    SavedSession restored; const auto file = folder.getChildFile("Plugin.tracksmith");
    require(SessionFile::decode(SessionFile::encode(session, file), file, restored).wasOk() && restored.inserts.size() == 2, "plugin session roundtrip");
    auto invalid = SessionFile::encode(session, file); auto entries = invalid.getProperty("inserts", {});
    entries.getArray()->add(entries.getArray()->getReference(0)); require(SessionFile::decode(invalid, file, restored).failed(), "duplicate insert slots reject");
    juce::AudioBuffer<float> audio(2, 48000 * 8); audio.clear();
    for (int beat = 0; beat < 16; ++beat) for (int n = 0; n < 4000; ++n)
    {
        const float sample = static_cast<float>(0.8 * std::exp(-n / 700.0) * std::sin(6.283185307179586 * 80 * n / 48000));
        audio.setSample(0, beat * 24000 + n, sample); audio.setSample(1, beat * 24000 + n, -sample);
    }
    const auto groove = GrooveAnalysis::analyse(audio, 48000);
    require(groove.valid && groove.onsetCount >= 12 && std::abs(groove.bpm - 120) <= 2, "AVRX GROOVE detects synthetic drum tempo without stereo cancellation");
    const auto midiPath = folder.getChildFile("Suggested.mid"); require(GrooveWorker::exportMidi(groove, midiPath).wasOk(), "MIDI suggestion exports");
    juce::FileInputStream stream(midiPath); juce::MidiFile midi;
    require(midi.readFrom(stream) && midi.getNumTracks() == 2 && midi.getTimeFormat() == 960, "MIDI export reopens with tempo and notes");
    int notes = 0;
    for (int n = 0; n < midi.getTrack(1)->getNumEvents(); ++n)
    {
        const auto& message = midi.getTrack(1)->getEventPointer(n)->message;
        if (message.isNoteOn()) { ++notes; require(message.getChannel() == 10 && (message.getNoteNumber() == 36 || message.getNoteNumber() == 38 || message.getNoteNumber() == 42), "GM drum mapping"); }
    }
    require(notes > 0, "MIDI contains drum notes");
    audio.clear(); require(!GrooveAnalysis::analyse(audio, 48000).valid, "silent take rejected");
    require(!GrooveAnalysis::analyse(audio, 48000, 1.0f, [] { return true; }).valid, "analysis can cancel on shutdown");
    {
        PluginRack delays; juce::OwnedArray<juce::PluginDescription> delayDescriptions;
        delays.formats.getFormat(0)->findAllTypesForFile(delayDescriptions, juce::String(argv[2]));
        require(delayDescriptions.size()==1,"scan real 512-sample delay VST3");
        auto delayed = delays.formats.createPluginInstance(*delayDescriptions[0],48000,256,error);
        require(delayed != nullptr,"instantiate delay VST3");
        require(delays.install(1,0,*delayDescriptions[0],std::move(delayed),{},false,48000).wasOk(),"delay insert installation");
        require(delays.channels[1][0]->instance->getLatencySamples()==512,"prepared VST3 reports setLatencySamples value");
        PlaybackCore mix;mix.process=PluginRack::process;mix.processContext=&delays;
        for(size_t channel=0;channel<2;++channel)
        {
            auto& track=mix.tracks[channel];track.sourceRate=48000;track.gain=1;track.smoothLeft=track.smoothRight=1;
            track.left.assign(512,0);track.right.assign(512,0);
            track.left[0]=track.left[511]=channel==0?0.2f:0.4f;
            track.right[0]=track.right[511]=channel==0?0.3f:0.5f;
        }
        mix.refreshDuration();delays.rebuildCompensation(mix);mix.playbackLatencyFrames=delays.totalLatency;mix.playing=true;
        require(delays.totalLatency==512&&delays.compensation[0].samples()==512&&delays.compensation[1].samples()==0,"fast track receives exact latency difference");
        float l[512],r[512];float* outputs[]{l,r};mix.render(outputs,2,512);
        require(l[0]==0&&l[511]==0&&r[511]==0&&mix.playing,"latency warm-up remains silent with transport active");
        mix.render(outputs,2,512);
        require(near(l[0],0.6f)&&near(r[0],0.8f)&&near(l[511],0.6f)&&near(r[511],0.8f),"stereo impulses align and last delayed samples drain");
        require(!mix.playing&&std::abs(mix.seconds.load()-512.0/48000)<1e-9,"audible clock reaches source end after compensation flush");
        auto master=delays.formats.createPluginInstance(*delayDescriptions[0],48000,256,error);
        require(delays.install(128,0,*delayDescriptions[0],std::move(master),{},false,48000).wasOk(),"master delay install");
        for(size_t channel=0;channel<2;++channel) { std::fill(mix.tracks[channel].left.begin(),mix.tracks[channel].left.end(),0);std::fill(mix.tracks[channel].right.begin(),mix.tracks[channel].right.end(),0); }
        mix.metronome=true;mix.reset();delays.resetGraph();delays.rebuildCompensation(mix);mix.playbackLatencyFrames=delays.totalLatency;mix.playing=true;
        require(delays.totalLatency==1024&&delays.compensation[PluginRack::clickChannel].samples()==512,"metronome and shared master latency included");
        PlaybackCore reference;reference.tracks[0].left.assign(512,0);reference.tracks[0].right.assign(512,0);reference.refreshDuration();reference.metronome=true;reference.playing=true;
        float expectedL[512],expectedR[512];float* expected[]{expectedL,expectedR};reference.render(expected,2,512);
        mix.render(outputs,2,512);require(l[511]==0,"master delays compensated click further");mix.render(outputs,2,512);require(l[511]==0,"complete graph latency warm-up");mix.render(outputs,2,512);
        for(int n=0;n<512;++n)require(near(l[n],expectedL[n])&&near(r[n],expectedR[n]),"metronome samples align through compensation and master insert");
        delays.channels[1][0]->bypass=true;delays.channels[128][0]->bypass=true;delays.rebuildCompensation(mix);
        require(delays.totalLatency==0&&delays.compensation[0].samples()==0,"bypass rebuild removes latency and compensation");
        delays.channels[1][0]->bypass=false;delays.channels[1][0]->instance->setLatencySamples(1024);
        require(delays.latencyChanged(),"dynamic reported latency change is detected");delays.rebuildCompensation(mix);
        require(delays.totalLatency==1024&&delays.compensation[0].samples()==1024,"dynamic latency rebuild updates compensation buffers");
        delays.channels[1][0]->instance->setLatencySamples(PluginRack::maximumPathLatency+1);delays.rebuildCompensation(mix);
        require(delays.channels[1][0]->bypass&&delays.totalLatency==0,"excessive dynamic latency bypasses offending insert");
    }
    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;juce::String error;
        auto trackNam=engine.plugins.formats.createPluginInstance(*descriptions[0],48000,256,error);
        auto masterNam=engine.plugins.formats.createPluginInstance(*descriptions[0],48000,256,error);
        require(trackNam&&masterNam,"instantiate track and master NAM for live monitoring");
        require(engine.plugins.install(0,0,*descriptions[0],std::move(trackNam),retained,false,48000).wasOk(),"live track NAM installs");
        require(engine.plugins.install(128,0,*descriptions[0],std::move(masterNam),retained,false,48000).wasOk(),"live master NAM installs");
        float input[512],l[512],r[512];std::fill_n(input,512,0.4f);const float* inputs[]{input};float* outputs[]{l,r};
        engine.monitor.enabled=true;engine.monitor.gain=1;engine.monitor.track=0;engine.playback.tracks[0].gain=1;
        bool wet=false;
        for(int n=0;n<100&&!wet;++n){juce::MessageManager::getInstance()->runDispatchLoopUntil(30);engine.renderAudio(inputs,1,outputs,2,512);wet=near(l[511],0.1f);}
        require(wet&&near(r[511],0.1f)&&engine.playback.seconds==0,"stopped transport monitors input through both actual NAM VST3s");
        engine.playback.tracks[0].left.assign(48000,0.8f);engine.playback.tracks[0].right.assign(48000,0.8f);
        engine.playback.tracks[1].left.assign(48000,0.3f);engine.playback.tracks[1].right.assign(48000,0.3f);engine.playback.tracks[1].gain=1;
        engine.playback.refreshDuration();engine.playback.playing=true;
        for(int n=0;n<10;++n)engine.renderAudio(inputs,1,outputs,2,512);
        require(near(l[511],0.45f)&&near(r[511],0.45f),"own clip and live input share one track insert and one master pass alongside backing");
        engine.monitor.gain=0;for(int n=0;n<10;++n)engine.renderAudio(inputs,1,outputs,2,512);
        require(near(l[511],.35f),"monitor level zero silences live input while own clip and backing continue through plugins");
        engine.monitor.gain=1;
        engine.playback.tracks[0].muted=true;for(int n=0;n<10;++n)engine.renderAudio(inputs,1,outputs,2,512);
        require(near(l[511],0.15f),"monitored track mute leaves backing audio audible");
        engine.playback.tracks[0].muted=false;engine.playback.tracks[0].solo=true;for(int n=0;n<10;++n)engine.renderAudio(inputs,1,outputs,2,512);
        require(near(l[511],0.3f),"monitored track solo keeps own clip and live input but excludes backing tracks");
        engine.plugins.channels[128][0]->bypass=true; // NAM intentionally sums its input to mono; test pan without that master model.
        engine.playback.tracks[0].pan=1;for(int n=0;n<10;++n)engine.renderAudio(inputs,1,outputs,2,512);
        require(std::abs(l[511])<0.0001f&&near(r[511],0.6f),"monitored track pan follows plugin chain");
        const auto file=folder.getChildFile("DryWhileWet-"+juce::Uuid().toString()+".wav");
        require(engine.recorder.start(file,48000,1,10000).wasOk(),"dry recorder starts during wet monitoring");
        engine.renderAudio(inputs,1,outputs,2,512);engine.recorder.requestStop();require(engine.recorder.waitForFinish(10000),"dry take finalizes");
        juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::FileInputStream(file),true));
        float raw[512];float* samples[]{raw};require(reader&&reader->read(samples,1,0,512),"dry take reopens");
        require(near(raw[511],0.4f),"recorded WAV stays dry while monitor output is plugin processed");
        float quiet[512] {}; const float* fourInputs[] { quiet, quiet, quiet, input };
        engine.inputSource=3;for(int n=0;n<10;++n)engine.renderAudio(fourInputs,4,outputs,2,512);
        require(near(engine.inputChannelPeaks[3],0.4f)&&engine.inputChannelPeaks[0]==0&&near(r[511],0.6f),"fourth enabled input is metered and monitored through track plugins");
        engine.monitor.enabled=false;engine.playback.playing=false;engine.renderAudio(inputs,1,outputs,2,512);
        require(l[511]==0&&r[511]==0,"monitor off stops live plugin output");
    }
    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner; engine.playback.deviceRate=48000; engine.playback.bpm=123;
        const auto file=folder.getChildFile("CountIn-"+juce::Uuid().toString()+".wav");
        require(engine.recorder.start(file,48000,1,10000).wasOk(),"count-in recorder prepares");
        engine.countIn.start(1,48000,123); engine.playback.recording=true;
        const auto prefix=engine.countIn.total; int64_t rendered=0;
        float input[256],l[256],r[256];std::fill_n(input,256,.25f);const float* inputs[]{input};float* outputs[]{l,r};bool heard=false;
        while(rendered<prefix+256){engine.renderAudio(inputs,1,outputs,2,256);rendered+=256;for(auto sample:l)heard=heard||std::abs(sample)>.01f;}
        require(heard,"count-in sounds even when metronome is off");
        engine.recorder.requestStop();require(engine.recorder.waitForFinish(10000),"count-in take finalizes");const auto result=engine.recorder.takeResult();
        require(result.frames==static_cast<uint64_t>(rendered-prefix),"capture starts on exact sample inside callback");
        juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::FileInputStream(file),true));
        float dry[256];float* samples[]{dry};require(reader&&reader->read(samples,1,0,256)&&near(dry[0],.25f)&&near(dry[255],.25f),"count-in clicks excluded from raw recording");
        engine.countIn.start(2,48000,120);engine.stopRecording(true);require(engine.countInCancelled&&engine.countIn.remaining(1)==0,"stop cancels count-in");
    }
    {
        std::vector<TimelineClip> clips{{1,0,1,.25,1}};
        require(ClipLayout::edit(clips,1,"split",1.5,2)&&clips.size()==2&&clips[1].offset==.75,"split preserves source offset");
        require(ClipLayout::edit(clips,2,"move",3,3)&&clips[1].start==3,"clip move changes timeline only");
        require(ClipLayout::edit(clips,2,"trimLeft",3.1,3)&&std::abs(clips[1].offset-.85)<1.e-9,"left trim advances source offset");
        require(!ClipLayout::edit(clips,2,"trimLeft",4,3),"invalid trim keeps clip");
        require(ClipLayout::edit(clips,1,"delete",0,3)&&clips.size()==1,"delete preserves other clips");
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;auto& track=engine.playback.tracks[0];
        track.left.assign(1024,.1f);track.right=track.left;track.sourceRate=48000;track.gain=1;track.timelineEnabled=true;track.clips={{1,0,0,0,1024.0/48000}};
        engine.playback.deviceRate=48000;engine.playback.refreshDuration();engine.playback.reset();track.smoothLeft=track.smoothRight=1;
        engine.playback.playing=true;engine.playback.recording=true;engine.planTrack=0;engine.recordPlanActive=true;engine.punchBegin=100;engine.punchEnd=500;
        const auto file=folder.getChildFile("Punch-"+juce::Uuid().toString()+".wav");require(engine.recorder.start(file,48000,1,10000).wasOk(),"punch recorder prepares");
        float input[768],l[768],r[768];std::fill_n(input,768,.25f);const float* in[]{input};float* out[]{l,r};engine.renderAudio(in,1,out,2,768);
        require(near(l[99],.1f)&&l[100]==0&&l[499]==0&&near(l[500],.1f),"punch suppresses armed backing only inside exact selection");
        require(engine.recorder.waitForFinish(10000),"punch-out automatically finalizes WAV");const auto result=engine.recorder.takeResult();require(result.frames==400,"punch captures only selected sample range");
        auto wav=decodeWav(file,10000,[]{return false;});require(wav.error.isEmpty()&&near(wav.left[0],.25f)&&near(wav.left[399],.25f),"punch take stays dry");
        engine.install(0,wav,true,100.0/48000,true,500.0/48000,true);
        require(track.clips.size()==3&&near(track.sample(99.0/48000,false),.1f)&&near(track.sample(100.0/48000,false),.25f)&&near(track.sample(499.0/48000,false),.25f)&&near(track.sample(500.0/48000,false),.1f),"punch clip preserves audio before and after selection");
        require(engine.playback.playing.load(),"punch clip install preserves continuing playback");
        // Clips from independent files share the existing track insert/fader path.
        track.clips={{1,0,.01,.001,.003},{2,1,.02,0,.004}};engine.playback.refreshDuration();
        require(track.sample(0,false)==0&&near(track.sample(.011,false),.1f)&&near(track.sample(.021,false),.25f)&&track.sample(.025,false)==0,"multi-source clips play gaps and offsets correctly");
        engine.monitor.enabled=true;engine.monitor.gain=1;engine.punchEnabled=true;engine.playback.playing=true;engine.playback.recording=false;
        engine.renderAudio(in,1,out,2,128);require(!engine.monitorActive.load(),"punch playback uses existing clips outside capture window");
        engine.playback.playing=false;engine.renderAudio(in,1,out,2,128);require(engine.monitorActive.load(),"stopped punch transport still permits live setup monitoring");
        {
            auto second=file.getSiblingFile("Second-"+juce::Uuid().toString()+".wav");require(file.copyFileTo(second),"second source fixture");
            SavedSession session;SessionTrack lane;lane.name="Edited lane";lane.audioFile=file;lane.sources={file,second};lane.timelineEnabled=true;
            lane.clips={{1,0,0,0,100.0/48000},{2,1,200.0/48000,50.0/48000,150.0/48000}};session.tracks.push_back(lane);
            const auto project=folder.getChildFile("Timeline-"+juce::Uuid().toString()+".tracksmith");require(SessionFile::write(project,SessionFile::encode(session,project)).wasOk(),"edited session saves");
            SessionWorker worker;bool finished=false;SessionJobResult loaded;
            worker.onFinished=[&](SessionJobResult result){loaded=std::move(result);finished=true;};require(worker.open(project),"multi-source session worker starts");
            for(int n=0;n<100&&!finished;++n)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            require(finished&&loaded.error.isEmpty()&&loaded.extraAudio[0].size()==1&&loaded.extraAudio[0][0],"worker decodes every source off audio thread");
            auto restoredOwner=std::make_unique<AudioEngine>();auto& restored=*restoredOwner;restored.installSession(loaded);require(restored.playback.tracks[0].clips.size()==2&&near(restored.playback.tracks[0].sample(250.0/48000,false),.25f),"session install restores independent source offsets");
            session.tracks[0].clips[0].length=10;require(SessionFile::write(project,SessionFile::encode(session,project)).wasOk(),"out-of-file bounds fixture writes structurally");
            finished=false;require(worker.open(project),"invalid source length worker starts");for(int n=0;n<100&&!finished;++n)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            require(finished&&loaded.error.isNotEmpty(),"worker rejects clips beyond source duration before install");
        }
        auto before=track.clips;ClipLayout::edit(track.clips,1,"delete",0,3);track.clips=before;require(track.clips.size()==2,"restoring clip history leaves source samples available");
    }
    {
        MidiLane lane;lane.enabled=true;lane.notes={{1,36,10,.25,.5,.9f}};juce::MidiBuffer midi;midi.ensureSize(65536);
        lane.emit(midi,.12,120,48000,512,true);bool on=false;for(const auto event:midi)if(event.getMessage().isNoteOn()){on=true;require(event.samplePosition==240,"MIDI note starts at exact sample offset");}require(on,"scheduled MIDI note emitted");
        lane.emit(midi,.25,120,48000,256,true);on=false;for(const auto event:midi)on=on||event.getMessage().isNoteOn();require(on,"seeking into held note chases note-on");
        lane.emit(midi,.37,120,48000,512,true);bool off=false;for(const auto event:midi)if(event.getMessage().isNoteOff()){off=true;require(event.samplePosition==240,"MIDI note-off uses exact sample offset");}require(off,"scheduled MIDI note released");
        lane.emit(midi,0,120,48000,256,false);require(!midi.isEmpty(),"stop sends MIDI panic");lane.emit(midi,0,120,48000,256,false);require(midi.isEmpty(),"stopped blocks do not repeatedly cancel pad audition");

    }
    {
        // Exercise the WAV writer through an actual VST3 on the track and master.
        PluginRack exportRack; juce::OwnedArray<juce::PluginDescription> found;
        exportRack.formats.getFormat(0)->findAllTypesForFile(found,juce::String(argv[2]));
        require(found.size()==1,"scan export delay fixture");
        auto core=std::make_unique<PlaybackCore>(); core->deviceRate=48000;
        auto& track=core->tracks[0];track.left.assign(4800,.4f);track.right.assign(4800,.4f);track.gain=1;
        core->refreshDuration(); core->process=PluginRack::process;core->processContext=&exportRack;
        for(size_t channel : {size_t(0),PlaybackCore::maxTracks})
        {
            auto instance=exportRack.formats.createPluginInstance(*found[0],48000,256,error);
            require(instance!=nullptr,"create actual VST3 for export");
            require(exportRack.install(channel,0,*found[0],std::move(instance),{},false,48000).wasOk(),"install export VST3");
        }
        exportRack.rebuildCompensation(*core); core->playbackLatencyFrames=exportRack.totalLatency;exportRack.resetGraph();
        const auto output=folder.getChildFile("VST3-mix.wav");
        const auto result=writeMixWav(*core,output,{0,.1,0,24},{},{});
        require(result.error.isEmpty()&&result.frames==4800,"actual VST3 export succeeds");
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.createInputStream().release(),true));
        juce::AudioBuffer<float> audio(2,4800);require(reader&&reader->read(&audio,0,4800,0,true,true),"reopen VST3-rendered WAV");
        require(near(audio.getSample(0,0),.4f)&&near(audio.getSample(1,4799),.4f),"real track/master VST3 latency compensated across whole export");
    }
    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;engine.playback.deviceRate=48000;
        auto& a=engine.playback.tracks[0];auto& b=engine.playback.tracks[1];
        a.left.assign(1024,.1f);a.right=a.left;a.sourceRate=48000;a.timelineEnabled=true;a.clips={{1,0,0,0,1024.0/48000}};
        b.left.assign(1024,.2f);b.right=b.left;b.sourceRate=48000;b.timelineEnabled=true;b.clips={{1,0,0,0,1024.0/48000}};
        engine.playback.refreshDuration();engine.playback.reset();
        engine.playback.recordTracks[0]=engine.playback.recordTracks[1]=true;
        engine.playback.recording=true;engine.playback.playing=true;engine.planTrack=0;engine.recordPlanActive=true;engine.punchBegin=100;engine.punchEnd=500;
        auto first=folder.getChildFile("Multi-A-"+juce::Uuid().toString()+".wav"),second=folder.getChildFile("Multi-B-"+juce::Uuid().toString()+".wav");
        require(engine.recorder.startGroup({{0,0,1,first},{1,1,1,second}},48000,10000).wasOk(),"start simultaneous independently routed mono tracks");
        float one[768],two[768],l[768],r[768];for(int n=0;n<768;++n){one[n]=static_cast<float>(n)/1000;two[n]=-.25f;}
        const float* in[]{one,two};float* out[]{l,r};engine.renderAudio(in,2,out,2,768);
        require(engine.recorder.waitForFinish(10000),"simultaneous punch finalizes all writers");auto takes=engine.recorder.takeResults();
        require(takes.size()==2&&takes[0].track==0&&takes[1].track==1&&takes[0].take.frames==400&&takes[1].take.frames==400,"all tracks capture identical sample-exact punch boundaries");
        auto wavA=decodeWav(first,10000,[]{return false;}),wavB=decodeWav(second,10000,[]{return false;});
        require(wavA.error.isEmpty()&&wavB.error.isEmpty()&&near(wavA.left[0],.1f)&&near(wavA.left[399],.499f)&&near(wavB.left[0],-.25f)&&near(wavB.left[399],-.25f),"each captured file has its own dry input from the same clock");
        require(l[100]==0&&l[499]==0&&l[700]>.1f,"punch suppresses both armed clips only inside selection");
        engine.install(0,wavA,true,100.0/48000,true,500.0/48000);
        engine.install(1,wavB,true,100.0/48000,true,500.0/48000);
        require(near(a.sample(99.0/48000,false),.1f)&&near(b.sample(99.0/48000,false),.2f)&&near(b.sample(100.0/48000,false),-.25f)&&near(b.sample(500.0/48000,false),.2f),"simultaneous takes install at the same timeline position and preserve surrounding clips");
        const auto stereo=folder.getChildFile("Multi-stereo-"+juce::Uuid().toString()+".wav");
        require(engine.recorder.startGroup({{0,0,2,stereo},{1,1,1,folder.getChildFile("Multi-mono-"+juce::Uuid().toString()+".wav")}},48000,256).wasOk(),"mixed stereo and mono capture");
        engine.recorder.capture(in,2,256);require(engine.recorder.waitForFinish(10000),"shared frame limit finalizes mixed capture");takes=engine.recorder.takeResults();
        auto pair=decodeWav(stereo,10000,[]{return false;});require(pair.error.isEmpty()&&near(pair.left[200],.2f)&&near(pair.right[200],-.25f)&&takes[1].take.frames==256,"stereo pair and independent mono input remain separate");
        require(engine.recorder.startGroup({{0,0,1,first},{0,1,1,second}},48000,10000).failed(),"duplicate armed destinations reject");
        std::vector<RecordRoute> routes;for(int id=0;id<8;++id)routes.push_back({id,id%2,1,folder.getChildFile("Eight-"+juce::Uuid().toString()+".wav")});
        require(engine.recorder.startGroup(routes,48000,256).wasOk(),"eight armed tracks prepare independent writers");
        engine.recorder.capture(in,2,256);require(engine.recorder.waitForFinish(10000),"eight tracks finalize");takes=engine.recorder.takeResults();
        require(takes.size()==8,"eight takes retained");for(const auto& item:takes){auto decoded=decodeWav(item.take.file,10000,[]{return false;});require(item.take.frames==256&&decoded.error.isEmpty()&&near(decoded.left[200],item.track%2?-.25f:.2f),"eight tracks retain assigned input and sample count");}
        WavLoader sequential;int imported=0;bool queued=true;
        sequential.onLoaded=[&](LoadedWav wav){require(wav.error.isEmpty()&&wav.left.size()==256,"queued take imports decode fully");++imported;if(imported<static_cast<int>(takes.size()))queued=sequential.load(takes[imported].take.file,10000);};
        require(sequential.load(takes[0].take.file,10000),"start queued take imports");
        for(int attempt=0;attempt<200&&imported<8&&queued;++attempt)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        require(imported==8&&queued,"all eight takes import automatically in sequence");
        routes.push_back({8,0,1,folder.getChildFile("Ninth.wav")});require(engine.recorder.startGroup(routes,48000,256).failed(),"ninth armed track rejects before recording");
        require(engine.recorder.startGroup({{0,0,1,folder.getChildFile("Missing-A-"+juce::Uuid().toString()+".wav")},{1,1,1,folder.getChildFile("Missing-B-"+juce::Uuid().toString()+".wav")}},48000,10000).wasOk(),"missing-input capture prepares");
        engine.recorder.capture(in,1,128);require(!engine.recorder.isActive()&&engine.recorder.waitForFinish(10000),"missing input stops the entire recording group");takes=engine.recorder.takeResults();require(takes.size()==2&&takes[0].take.frames==128&&takes[1].take.error.isNotEmpty(),"valid partial take kept and missing channel reported");
    }
    {
        PluginRack busRack;juce::OwnedArray<juce::PluginDescription> found;busRack.formats.getFormat(0)->findAllTypesForFile(found,juce::String(argv[2]));
        require(found.size()==1,"scan aux latency VST3 fixture");auto core=std::make_unique<PlaybackCore>();core->deviceRate=48000;
        core->tracks[0].left.assign(4800,.2f);core->tracks[0].right=core->tracks[0].left;core->tracks[0].gain=1;
        MixerRoute send;send.sends[0]={2,.5f};core->setRouting(0,send);MixerRoute aux;aux.aux=true;core->setRouting(2,aux);core->tracks[2].gain=1;
        core->refreshDuration();core->process=PluginRack::process;core->processContext=&busRack;
        for(size_t channel:{size_t(2),PlaybackCore::maxTracks}){auto instance=busRack.formats.createPluginInstance(*found[0],48000,256,error);require(instance&&busRack.install(channel,0,*found[0],std::move(instance),{},false,48000).wasOk(),"install actual aux/master delay VST3s");}
        busRack.rebuildCompensation(*core);core->playbackLatencyFrames=busRack.totalLatency;busRack.resetGraph();
        require(busRack.totalLatency==1024&&busRack.directCompensation.samples()==512,"direct output aligned with bus latency plus master latency");
        const auto output=folder.getChildFile("Aux-VST3-mix.wav");const auto result=writeMixWav(*core,output,{0,.1,0,24},{},{});
        require(result.error.isEmpty()&&result.frames==4800,"aux routing exports exact duration");juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.createInputStream().release(),true));juce::AudioBuffer<float> audio(2,4800);
        require(reader&&reader->read(&audio,0,4800,0,true,true)&&near(audio.getSample(0,0),.3f)&&near(audio.getSample(1,4799),.3f),"dry and aux paths align through actual VST3s including first and last exported samples");
    }
    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;engine.playback.deviceRate=48000;MixerRoute send;send.sends[0]={2,1};engine.playback.setRouting(0,send);MixerRoute aux;aux.aux=true;engine.playback.setRouting(2,aux);engine.playback.tracks[2].gain=1;
        engine.playback.tracks[0].gain=1;engine.playback.tracks[0].left.assign(48000,.2f);engine.playback.tracks[0].right=engine.playback.tracks[0].left;
        for(size_t channel:{size_t(2),PlaybackCore::maxTracks}){auto instance=engine.plugins.formats.createPluginInstance(*descriptions[0],48000,256,error);require(instance&&engine.plugins.install(channel,0,*descriptions[0],std::move(instance),retained,false,48000).wasOk(),"shared NAM aux and master install");}
        engine.playback.refreshDuration();engine.monitor.enabled=true;engine.monitor.gain=1;engine.monitor.track=0;engine.playback.playing=true;
        float input[512],l[512],r[512];std::fill_n(input,512,.2f);const float* in[]{input};float* out[]{l,r};bool ready=false;
        for(int n=0;n<100&&!ready;++n){juce::MessageManager::getInstance()->runDispatchLoopUntil(20);engine.renderAudio(in,1,out,2,512);ready=near(l[511],.3f);}
        require(ready&&near(r[511],.3f),"clips plus live input feed shared aux and master exactly once");
        engine.monitor.gain=0;for(int n=0;n<10;++n)engine.renderAudio(in,1,out,2,512);require(near(l[511],.15f),"zero monitor gain retains dry clip plus its aux effect");
        engine.playback.playing=false;engine.monitor.gain=1;for(int n=0;n<10;++n)engine.renderAudio(in,1,out,2,512);require(near(l[511],.15f),"stopped transport monitors live input through shared bus");
    }
    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;auto& track=engine.playback.tracks[0];track.left.assign(96000,.2f);track.right=track.left;track.sourceRate=48000;track.timelineEnabled=true;track.clips={{1,0,0,0,2}};
        auto before=track.clips;LoadedWav take;take.left.assign(4800,.8f);take.right=take.left;take.rate=48000;
        engine.install(0,take,true,.5,true,.6);
        require(track.clips.size()==3&&near(track.sample(.499,false),.2f)&&near(track.sample(.55,false),.8f)&&near(track.sample(.6,false),.2f),"actual recorded-take install replaces range and preserves audio around it");
        auto after=track.clips;track.clips=before;require(near(track.sample(.55,false),.2f)&&track.extra.size()==1,"undo retains original source and old arrangement");track.clips=after;
        track.gain=1;engine.playback.refreshDuration();const auto output=folder.getChildFile("Overwrite-mix.wav");auto result=writeMixWav(engine.playback,output,{0,2,0,24},{},{});require(result.error.isEmpty(),"replacement arrangement exports");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(output.createInputStream().release(),true));juce::AudioBuffer<float> samples(2,96000);require(reader&&reader->read(&samples,0,96000,0,true,true)&&near(samples.getSample(0,24000),.8f)&&near(samples.getSample(0,28800),.2f),"export plays new take only inside recorded interval");
        track.clips={{1,0,0,0,1}};require(ClipLayout::edit(track.clips,1,"fadeIn",.1,2),"integration fade applies");engine.playback.refreshDuration();reader.reset();result=writeMixWav(engine.playback,output,{0,1,0,24},{},{});reader.reset(format.createReaderFor(output.createInputStream().release(),true));samples.setSize(2,48000);require(reader&&reader->read(&samples,0,48000,0,true,true)&&near(samples.getSample(0,0),0)&&near(samples.getSample(0,2400),.1f),"WAV export applies fade envelope to real source samples");
    }
    {
        PluginRack delayedRack;juce::OwnedArray<juce::PluginDescription> found;delayedRack.formats.getFormat(0)->findAllTypesForFile(found,juce::String(argv[2]));require(found.size()==1,"automation latency fixture found");
        auto plain=std::make_unique<PlaybackCore>(),delayed=std::make_unique<PlaybackCore>();
        for(auto* core:{plain.get(),delayed.get()}){core->deviceRate=48000;auto& t=core->tracks[0];t.sourceRate=48000;t.left.assign(4800,.5f);t.right=t.left;t.gain=1;t.automationMode=1;t.volumeAutomation={{0,.2f},{.01,.2f},{.02,.8f},{.1,.8f}};t.panAutomation={{0,-.5f},{.04,-.5f},{.05,.5f},{.1,.5f}};MixerRoute source;source.output=2;core->setRouting(0,source);MixerRoute aux;aux.aux=true;core->setRouting(2,aux);auto& bus=core->tracks[2];bus.gain=1;bus.automationMode=1;bus.volumeAutomation={{0,.5f},{.03,.5f},{.04,1},{.1,1}};core->refreshDuration();}
        for(size_t id:{size_t(0),size_t(2),size_t(128)}){auto plugin=delayedRack.formats.createPluginInstance(*found[0],48000,256,error);require(plugin&&delayedRack.install(id,0,*found[0],std::move(plugin),{},false,48000).wasOk(),"install automated source/aux/master latency plugins");}
        delayed->process=PluginRack::process;delayed->processContext=&delayedRack;delayedRack.rebuildCompensation(*delayed);delayed->playbackLatencyFrames=delayedRack.totalLatency;delayedRack.resetGraph();
        const auto baseline=folder.getChildFile("Automation-no-latency.wav"),withLatency=folder.getChildFile("Automation-latency.wav");auto a=writeMixWav(*plain,baseline,{0,.1,0,24},{},{}),b=writeMixWav(*delayed,withLatency,{0,.1,0,24},{},{});require(a.error.isEmpty()&&b.error.isEmpty(),"automation latency comparisons export");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> ra(format.createReaderFor(baseline.createInputStream().release(),true)),rb(format.createReaderFor(withLatency.createInputStream().release(),true));juce::AudioBuffer<float> sa(2,4800),sb(2,4800);require(ra&&rb&&ra->read(&sa,0,4800,0,true,true)&&rb->read(&sb,0,4800,0,true,true),"read automated reference WAVs");
        float difference=0;for(int ch=0;ch<2;++ch)for(int n=0;n<4800;++n)difference=std::max(difference,std::abs(sa.getSample(ch,n)-sb.getSample(ch,n)));require(difference<.00001f,"source and aux automation align sample-for-sample through real VST3 source/aux/master latency");
    }

    {
        PluginRack automated;juce::OwnedArray<juce::PluginDescription> found;automated.formats.getFormat(0)->findAllTypesForFile(found,juce::String(argv[2]));require(found.size()==1,"plugin automation fixture found");
        auto core=std::make_unique<PlaybackCore>();core->deviceRate=48000;auto& source=core->tracks[0];source.sourceRate=48000;source.left.assign(5120,.5f);source.right=source.left;source.gain=1;MixerRoute route;route.output=2;core->setRouting(0,route);MixerRoute aux;aux.aux=true;core->setRouting(2,aux);core->tracks[2].gain=1;core->refreshDuration();
        juce::String gainId;
        for(size_t channel:{size_t(0),size_t(2),size_t(128)}){
            auto instance=automated.formats.createPluginInstance(*found[0],48000,256,error);require(instance&&automated.install(channel,0,*found[0],std::move(instance),{},false,48000).wasOk(),"automated VST3 source aux master install");
            auto& slot=*automated.channels[channel][0];for(int p=0;p<slot.instance->getParameters().size();++p){auto* parameter=slot.instance->getHostedParameter(p);if(parameter->getName(100)=="Test gain")gainId=parameter->getParameterID();}require(gainId.isNotEmpty(),"VST3 stable gain ID exposed");
            slot.automation={{gainId,"Test gain",{{0,.25f},{.1,.75f}},true},{"missing-id","Old removed control",{{0,.9f}},true}};slot.bindAutomation();require(slot.bindings.size()==1,"unresolved IDs are retained without binding to a different control");
        }
        core->process=PluginRack::process;core->processContext=&automated;automated.rebuildCompensation(*core);core->playbackLatencyFrames=automated.totalLatency;
        const auto output=folder.getChildFile("Plugin-automation.wav");automated.resetGraph();auto result=writeMixWav(*core,output,{0,5120.0/48000,0,24},{},{});require(result.error.isEmpty(),"plugin automation WAV export");
        juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(output.createInputStream().release(),true));juce::AudioBuffer<float> audio(2,5120);require(reader&&reader->read(&audio,0,5120,0,true,true),"read plugin automation export");
        for(int n=0;n<5120;++n){const float gain=std::min(.75f,.25f+static_cast<float>((n/256)*256)/4800.f*.5f);require(near(audio.getSample(0,n),.5f*gain*gain*gain)&&near(audio.getSample(1,n),.5f*gain*gain*gain),"source aux master plugin curves follow audio through upstream latency");}
        const auto saved=automated.save();std::vector<PluginAutomation::Lane> decoded;require(PluginAutomation::decode(saved[0].getProperty("automation",{}),decoded)&&decoded.size()==2&&decoded[0].id==gainId,"stable IDs and unavailable controls save intact");
        require(automated.install(1,0,*found[0],nullptr,{},false,48000).wasOk(),"missing plugin placeholder");automated.restoreAutomation(1,0,saved[0].getProperty("automation",{}));require(automated.channels[1][0]->automation.size()==2&&automated.channels[1][0]->bindings.empty(),"missing plugin retains unbound automation");
        auto& slot=*automated.channels[0][0];slot.rememberAutomation();slot.automation[0].read=false;slot.bindAutomation();slot.bindings[0].parameter->setValue(.6f);float l[256],r[256];std::fill_n(l,256,.5f);std::fill_n(r,256,.5f);PluginRack::process(&automated,0,l,r,256,0,120,false);require(near(slot.bindings[0].parameter->getValue(),.6f),"Off leaves manual plugin value intact while stopped monitoring");
        slot.automation=std::move(slot.automationUndo.back());slot.automationUndo.pop_back();slot.bindAutomation();PluginRack::process(&automated,0,l,r,256,0,120,false);require(near(slot.bindings[0].parameter->getValue(),.25f),"restored Read curve follows stopped monitor playhead");
        slot.automation[0].mode=3;slot.audioProcessorParameterChangeGestureBegin(slot.instance.get(),slot.bindings[0].index);slot.bindings[0].parameter->setValueNotifyingHost(.7f);PluginRack::process(&automated,0,l,r,256,0,120,true);require(near(slot.bindings[0].parameter->getValue(),.7f),"native VST3 Touch gesture takes manual control");slot.audioProcessorParameterChangeGestureEnd(slot.instance.get(),slot.bindings[0].index);PluginRack::process(&automated,0,l,r,256,0,120,true);require(near(slot.bindings[0].parameter->getValue(),.25f)&&near(slot.activity[slot.bindings[0].index].value.load(),.7f),"Touch releases to Read without losing recorded gesture value");
        slot.automation[0].mode=4;slot.audioProcessorParameterChangeGestureBegin(slot.instance.get(),slot.bindings[0].index);slot.bindings[0].parameter->setValueNotifyingHost(.6f);slot.audioProcessorParameterChangeGestureEnd(slot.instance.get(),slot.bindings[0].index);PluginRack::process(&automated,0,l,r,256,0,120,true);require(near(slot.bindings[0].parameter->getValue(),.6f),"VST3 Latch holds after release");slot.resetGestures();PluginRack::process(&automated,0,l,r,256,0,120,true);require(near(slot.bindings[0].parameter->getValue(),.25f),"Latch resets to Read on next pass");slot.automation[0].mode=1;core->automationBypass=true;slot.bindings[0].parameter->setValue(.8f);PluginRack::process(&automated,0,l,r,256,0,120,true);require(near(slot.bindings[0].parameter->getValue(),.8f),"global compare bypass suspends plugin curve");core->automationBypass=false;
        SavedSession session;session.tracks.resize(3);for(auto& track:session.tracks)track.name="Automation test";session.inserts=saved;SavedSession restored;const auto path=folder.getChildFile("Plugin-automation.tracksmith");auto document=SessionFile::encode(session,path);require(SessionFile::decode(document,path,restored).wasOk(),"version8 plugin curves roundtrip");
        auto bad=document.clone();auto lanes=bad.getProperty("inserts",{}).getArray()->getReference(0).getProperty("automation",{});lanes.getArray()->add(lanes.getArray()->getReference(0).clone());require(SessionFile::decode(bad,path,restored).failed(),"duplicate parameter ID rejected");
        bad=document.clone();bad.getProperty("inserts",{}).getArray()->getReference(0).getProperty("automation",{}).getArray()->getReference(0).getProperty("points",{}).getArray()->getReference(0).getDynamicObject()->setProperty("value",2);require(SessionFile::decode(bad,path,restored).failed(),"out of range plugin curve rejected");
        auto old=document.clone();old.getDynamicObject()->setProperty("version",7);for(auto& insert:*old.getProperty("inserts",{}).getArray())insert.getDynamicObject()->removeProperty("automation");require(SessionFile::decode(old,path,restored).wasOk(),"version7 inserts without plugin curves load");
    }


    {
        require(RecordingAlignment::correction(64,128,512)==704&&RecordingAlignment::correction(64,128,512,false)==0,"record alignment sums reported round trip and plugin path, with bypass");
        auto alignedOwner=std::make_unique<AudioEngine>();auto& aligned=*alignedOwner;aligned.playback.deviceRate=48000;aligned.playback.bpm=120;aligned.playback.tracks[0].left.assign(48000,.2f);aligned.playback.tracks[0].right=aligned.playback.tracks[0].left;aligned.playback.refreshDuration();aligned.playback.recording=true;aligned.playback.playing=true;aligned.playback.recordTrack=0;aligned.recordPlanActive=true;aligned.planTrack=0;aligned.planPosition=0;aligned.punchBegin=100;aligned.punchEnd=200;aligned.recordingCorrection=40;
        const auto path=folder.getNonexistentChildFile("Aligned-punch",".wav",false);require(aligned.recorder.start(path,48000,1,1000).wasOk(),"aligned punch writer starts");float input[300],l[300],r[300];for(int n=0;n<300;++n)input[n]=n==160?.75f:0;const float* in[]{input};float* out[]{l,r};aligned.renderAudio(in,1,out,2,300);require(!aligned.recordPlanActive&&aligned.recorder.waitForFinish(3000),"punch waits through delayed capture end");auto take=aligned.recorder.takeResult();require(take.frames==100,"latency correction preserves exact punch length");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(path.createInputStream().release(),true));juce::AudioBuffer<float> recorded(1,100);require(reader&&reader->read(&recorded,0,100,0,true,false)&&near(recorded.getSample(0,20),.75f),"delayed physical impulse is placed at its intended time inside punch");
        auto stoppedOwner=std::make_unique<AudioEngine>();auto& stopped=*stoppedOwner;stopped.playback.deviceRate=48000;stopped.playback.recording=true;stopped.playback.playing=true;stopped.recordPlanActive=true;stopped.planTrack=0;stopped.punchBegin=0;stopped.punchEnd=std::numeric_limits<int64_t>::max();stopped.recordingCorrection=40;
        const auto stopPath=folder.getNonexistentChildFile("Aligned-stop",".wav",false);require(stopped.recorder.start(stopPath,48000,1,1000).wasOk(),"aligned normal writer starts");stopped.renderAudio(in,1,out,2,100);stopped.stopRecording(false,false,true);stopped.renderAudio(in,1,out,2,40);require(stopped.recorder.waitForFinish(3000)&&stopped.recorder.takeResult().frames==100,"manual Stop drains latency tail without shortening take");
    }


    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;engine.playback.deviceRate=48000;engine.monitor.gain=1;engine.monitorArmed=true;
        for(int id=0;id<2;++id){engine.armedMonitorMask[id]=true;engine.trackInputs[id]=id;engine.trackInputChannels[id]=1;engine.playback.tracks[id].gain=1;}
        float a[4096],b[4096],l[4096],r[4096];std::fill_n(a,4096,.2f);std::fill_n(b,4096,.4f);const float* in[]{a,b};float* out[]{l,r};engine.renderAudio(in,2,out,2,4096);require(near(l[4095],.6f)&&near(r[4095],.6f)&&engine.monitorActive.load(),"two armed mono inputs monitor simultaneously on their own tracks while stopped");
        engine.monitor.enabled=true;engine.monitor.track=0;engine.renderAudio(in,2,out,2,4096);require(near(l[4095],.6f),"selected and armed-group monitoring do not double the same track");engine.monitor.enabled=false;
        engine.armedMonitorMask[1]=false;engine.renderAudio(in,2,out,2,4096);require(near(l[4095],.2f),"disarming one track removes only its monitor path");engine.armedMonitorMask[1]=true;
        for(int id=0;id<2;++id){auto& track=engine.playback.tracks[id];track.sourceRate=48000;track.left.assign(48000,id==0?.02f:.03f);track.right=track.left;}engine.playback.refreshDuration();engine.playback.playing=true;engine.renderAudio(in,2,out,2,4096);require(near(l[4095],.65f),"existing files and both live inputs share the mix once");
        const auto output=folder.getChildFile("Multi-monitor-export.wav");auto exported=writeMixWav(engine.playback,output,{0,.1,0,24},{},{});require(exported.error.isEmpty(),"export with multiple monitors enabled");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(output.createInputStream().release(),true));juce::AudioBuffer<float> samples(2,4800);require(reader&&reader->read(&samples,0,4800,0,true,true)&&near(samples.getSample(0,2400),.05f),"WAV export excludes every monitored input");
        const auto fileA=folder.getNonexistentChildFile("Dry-input-A",".wav",false),fileB=folder.getNonexistentChildFile("Dry-input-B",".wav",false);require(engine.recorder.startGroup({{0,0,1,fileA},{1,1,1,fileB}},48000,10000).wasOk(),"two dry recording routes start");engine.renderAudio(in,2,out,2,1024);engine.recorder.requestStop();require(engine.recorder.waitForFinish(3000),"dry multi-track capture finalizes");auto takes=engine.recorder.takeResults();require(takes.size()==2&&takes[0].take.frames==1024&&takes[1].take.frames==1024,"both monitored routes capture the same frame count");reader.reset(format.createReaderFor(fileB.createInputStream().release(),true));samples.setSize(1,1024);require(reader&&reader->read(&samples,0,1024,0,true,false)&&near(samples.getSample(0,100),.4f),"recording retains assigned dry input rather than summed monitor mix");
        MixerRoute send;send.sends[0]={2,1,true};engine.playback.setRouting(0,send);MixerRoute aux;aux.aux=true;aux.hardwarePair=1;engine.playback.setRouting(2,aux);engine.playback.tracks[2].gain=1;engine.playback.tracks[0].gain=0;engine.playback.tracks[1].muted=true;engine.playback.playing=false;float cueL[4096],cueR[4096];float* four[]{l,r,cueL,cueR};engine.countIn.start(1,48000,120);engine.countIn.position=engine.countIn.total-64;engine.renderAudio(in,2,four,4,4096);require(near(cueL[4095],.22f)&&near(cueR[4095],.22f),"cue outputs survive count-in callback split with live input and playback");
    }
    {
        SavedSession session;SessionTrack source;source.name="Guitar";source.routing.sends[0]={1,.4f,true};session.tracks.push_back(source);SessionTrack aux;aux.name="Headphones";aux.routing.aux=true;aux.routing.hardwarePair=1;session.tracks.push_back(aux);const auto file=folder.getChildFile("Cue-routing.tracksmith");auto document=SessionFile::encode(session,file);SavedSession restored;require(SessionFile::decode(document,file,restored).wasOk()&&restored.tracks[0].routing.sends[0].pre&&restored.tracks[1].routing.hardwarePair==1,"version9 saves pre-fader sends and hardware output routes");
        auto bad=document.clone();bad.getProperty("tracks",{}).getArray()->getReference(1).getDynamicObject()->setProperty("hardwarePair",8);require(SessionFile::decode(bad,file,restored).failed(),"invalid saved hardware route rejected");bad=document.clone();bad.getProperty("tracks",{}).getArray()->getReference(0).getProperty("sends",{}).getArray()->getReference(0).getDynamicObject()->setProperty("pre","yes");require(SessionFile::decode(bad,file,restored).failed(),"invalid pre-fader mode rejected");
        document.getDynamicObject()->setProperty("version",8);require(SessionFile::decode(document,file,restored).wasOk()&&!restored.tracks[0].routing.sends[0].pre&&restored.tracks[1].routing.hardwarePair==0,"legacy version8 defaults to post-fader sends and main outputs");
    }


    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;engine.playback.deviceRate=48000;engine.monitor.gain=1;engine.monitorArmed=true;engine.armedMonitorMask[0]=true;engine.trackInputs[0]=1;engine.playback.tracks[0].gain=1;float a[4096],b[4096],l[4096],r[4096];std::fill_n(a,4096,.2f);std::fill_n(b,4096,.4f);const float* in[]{nullptr,nullptr,a,nullptr,nullptr,b};float* out[]{l,r};engine.countIn.start(1,48000,120);engine.countIn.position=engine.countIn.total-128;engine.renderAudio(in,6,out,2,4096);require(near(l[4095],.4f)&&near(r[4095],.4f),"multiple-input routing retains enabled input order across null device channels and count-in splits");
    }


    {
        auto engineOwner=std::make_unique<AudioEngine>();auto& engine=*engineOwner;engine.playback.deviceRate=48000;engine.monitor.gain=1;engine.monitorArmed=true;
        for(int id=0;id<2;++id){engine.armedMonitorMask[id]=true;engine.trackInputs[id]=id;engine.playback.tracks[id].gain=1;MixerRoute route;route.sends[0]={2,1,false};engine.playback.setRouting(id,route);}MixerRoute aux;aux.aux=true;engine.playback.setRouting(2,aux);engine.playback.tracks[2].gain=1;
        for(size_t id:{size_t(0),size_t(1),size_t(2),size_t(128)}){auto plugin=engine.plugins.formats.createPluginInstance(*descriptions[0],48000,256,error);require(plugin&&engine.plugins.install(id,0,*descriptions[0],std::move(plugin),retained,false,48000).wasOk(),"multi-monitor NAM insert install");}
        float a[512],b[512],l[512],r[512];std::fill_n(a,512,.2f);std::fill_n(b,512,.4f);const float* in[]{a,b};float* out[]{l,r};bool ready=false;for(int attempt=0;attempt<100&&!ready;++attempt){juce::MessageManager::getInstance()->runDispatchLoopUntil(20);engine.renderAudio(in,2,out,2,512);ready=near(l[511],.225f)&&near(r[511],.225f);}require(ready,"two monitored tracks, shared aux and master each process exactly once through real VST3s");
        auto route=engine.playback.routingSnapshot(0);route.sends[0].pre=true;engine.playback.setRouting(0,route);engine.playback.tracks[0].gain=0;for(int block=0;block<10;++block)engine.renderAudio(in,2,out,2,512);require(near(l[511],.175f),"pre-fader send includes source insert but is independent of the source fader");
    }


    {
        std::vector<TrackPlaylist> list{{1,"Main",{{1,0,0,0,8}},{}},{2,"Take 2",{{2,0,1,1,6}},{}},{3,"Take 3",{{3,0,2,2,4}},{}}};int active=2;
        require(PlaylistEditing::manage(list,active,3,"up")&&list[1].id==3&&active==2,"reorder keeps stable IDs and active playlist");require(PlaylistEditing::manage(list,active,3,"rename","Solo")&&list[1].name=="Solo","rename inactive playlist");require(!PlaylistEditing::manage(list,active,1,"rename","solo"),"playlist names remain unique regardless of case");
        auto before=list;const int previous=active;require(PlaylistEditing::manage(list,active,2,"delete")&&list.size()==2&&active==3,"deleting active playlist selects adjacent survivor");list=before;active=previous;require(list[2].id==2&&active==2&&list[2].clips[0].source==0,"playlist snapshot undo restores ordering, active ID and media references");
        require(PlaylistEditing::manage(list,active,2,"duplicate","Double")&&active==4&&list.back().clips[0].start==1,"duplicate retains arrangement with a new stable playlist ID");
        while(list.size()>1)require(PlaylistEditing::manage(list,active,list.front().id,"delete"),"delete playlist until one remains");require(!PlaylistEditing::manage(list,active,list.front().id,"delete")&&list.size()==1,"final playlist cannot be deleted");
        for(int n=1;n<16;++n)require(PlaylistEditing::manage(list,active,active,"new","New "+juce::String(n)),"fill playlist capacity");require(!PlaylistEditing::manage(list,active,active,"new","Overflow"),"playlist limit enforced atomically");
        SavedSession session;SessionTrack track;track.name="Playlist order";track.timelineEnabled=true;track.playlists=before;track.activePlaylist=2;track.clips=before[2].clips;track.sources={folder.getChildFile("Playlist-source.wav")};track.audioFile=track.sources[0];session.tracks.push_back(track);const auto file=folder.getChildFile("Reordered-playlists.tracksmith");SavedSession restored;require(SessionFile::decode(SessionFile::encode(session,file),file,restored).wasOk()&&restored.tracks[0].playlists[1].id==3&&restored.tracks[0].activePlaylist==2,"playlist ordering and active choice persist in session format 9");
    }
    {
        GroupEditing::Layout a{0,1,false,{{1,0,0,0,8}},{}},b{1,2,false,{{1,0,1,1,6}},{}},midi{2,1,true,{},{{1,60,1,0,16,.8f}}};a.clips[0].fadeIn=1;a.clips[0].fadeOut=1;
        std::vector<GroupEditing::Layout> original{a,b,midi},split=original;
        require(GroupEditing::edit(split,"split",2,4,0,120)&&split[0].clips.size()==3&&split[1].clips.size()==3&&split[2].notes.size()==3,"group range splits both audio clips and tempo-scaled MIDI notes");
        require(std::abs(ClipLayout::envelope(split[0].clips[1],3)-ClipLayout::envelope(a.clips[0],3))<1.e-6,"group split preserves the original fade envelope");
        auto removed=original;require(GroupEditing::edit(removed,"delete",2,4,0,120)&&removed[0].clips.size()==2&&removed[2].notes[0].length==4&&removed[2].notes[1].start==8,"range delete retains audio/MIDI outside selection without ripple");
        auto moved=original;require(GroupEditing::edit(moved,"move",2,4,1,120)&&moved[0].clips.back().start==3&&moved[0].clips.back().offset==2&&moved[2].notes.back().start==6,"group move uses seconds consistently across audio and MIDI");
        auto invalid=original;require(!GroupEditing::edit(invalid,"move",2,4,-3,120)&&invalid[0].clips.size()==1&&invalid[2].notes.size()==1,"negative destination rejects whole batch");
        auto full=b;full.clips.clear();for(int n=0;n<128;++n)full.clips.push_back({n+1,0,0,0,8});std::vector<GroupEditing::Layout> limited{a,full};require(!GroupEditing::edit(limited,"split",2,4,0,120)&&limited[0].clips.size()==1&&limited[1].clips.size()==128,"one full track prevents any partial group edit");
        auto duplicate=original;duplicate[1].track=0;require(!GroupEditing::edit(duplicate,"delete",2,4,0,120),"duplicate track membership rejected");
        auto core=std::make_unique<PlaybackCore>();core->deviceRate=1000;for(int id=0;id<2;++id){auto& t=core->tracks[id];t.sourceRate=1000;t.left.assign(8000,id==0?.2f:.3f);t.right=t.left;t.timelineEnabled=true;t.gain=1;t.clips=removed[id].clips;}require(core->tracks[0].sample(3,false)==0&&core->tracks[1].sample(3,false)==0&&std::abs(core->tracks[0].sample(5,false)-.2f)<1.e-6,"group range deletion creates aligned silence on both playback tracks");for(int id=0;id<2;++id)core->tracks[id].clips=original[id].clips;require(std::abs(core->tracks[0].sample(3,false)-.2f)<1.e-6&&std::abs(core->tracks[1].sample(3,false)-.3f)<1.e-6,"restoring group snapshots restores both playback layouts together");
    }


    {
        RecoveryStore::Policy policy;policy.dirty=true;policy.elapsed=119999;require(!policy.eligible(),"autosave waits for interval");policy.elapsed=120000;require(policy.eligible(),"dirty idle session becomes eligible");policy.lastRevision=0;require(!policy.eligible(),"unchanged dirty revisions are not repeatedly snapshotted");policy.revision=1;policy.playing=true;require(!policy.eligible(true),"manual snapshot cannot interrupt playback");policy.playing=false;policy.blocked=true;require(!policy.eligible(true),"recording/load/export busy state blocks even a manual snapshot");policy.blocked=false;policy.writing=true;require(!policy.eligible(true),"active automation capture blocks snapshot");policy.writing=false;policy.busy=true;require(!policy.eligible(true),"no overlapping recovery jobs");policy.busy=false;policy.enabled=false;require(!policy.eligible()&&policy.eligible(true),"manual snapshot remains available with periodic autosave off");
        const auto recovery=folder.getNonexistentChildFile("Recovery-tests","",true);require(recovery.createDirectory().wasOk(),"isolated recovery root");const auto media=recovery.getChildFile("Original/Media/Tone.wav");require(media.getParentDirectory().createDirectory().wasOk(),"recovery media folder");auto core=std::make_unique<PlaybackCore>();core->deviceRate=48000;core->tracks[0].sourceRate=48000;core->tracks[0].left.assign(4800,.23f);core->tracks[0].right=core->tracks[0].left;core->tracks[0].gain=1;core->refreshDuration();require(writeMixWav(*core,media,{0,.1,0,24},{},{}).error.isEmpty(),"recovery WAV fixture");
        SavedSession session;SessionTrack source;source.name="Unsaved guitar";source.audioFile=media;source.sources={media};source.timelineEnabled=true;source.clips={{1,0,.25,0,.1}};source.playlists={{1,"Main",source.clips,{}},{2,"Alternate",{{2,0,.5,0,.1}},{}}};source.activePlaylist=1;source.volumeAutomation={{0,.2f},{1,.8f}};source.automationMode=1;source.routing.sends[0]={1,.4f,true};session.tracks.push_back(source);SessionTrack aux;aux.name="Cue";aux.routing.aux=true;aux.routing.hardwarePair=1;session.tracks.push_back(aux);session.inserts=rack.save();
        const auto original=recovery.getChildFile("Original/Manual.tracksmith");require(SessionFile::write(original,SessionFile::encode(session,original)).wasOk(),"manual original exists");const auto originalBytes=original.loadFileAsString();const auto id=RecoveryStore::newRun();require(RecoveryStore::write(recovery,id,0,original,"Guitar",session).wasOk(),"first interrupted-session snapshot");auto entries=RecoveryStore::discover(recovery);require(entries.size()==1&&entries[0].original==original&&entries[0].generation==0&&original.loadFileAsString()==originalBytes,"snapshot is discovered without altering the manual save");
        SavedSession restored;require(SessionFile::read(entries[0].snapshot,restored).wasOk()&&restored.tracks[0].sources[0]==media&&restored.tracks[0].playlists.size()==2&&restored.tracks[0].volumeAutomation.size()==2&&restored.tracks[0].routing.sends[0].pre&&restored.tracks[1].routing.hardwarePair==1&&restored.inserts.size()==session.inserts.size(),"recovery preserves media paths, playlists, automation, routing and plugin state");
        SessionWorker loader;bool loaded=false;juce::String loadError;bool mediaCorrect=false;loader.onFinished=[&](SessionJobResult result){loadError=result.error;mediaCorrect=result.audio[0]&&near(result.audio[0]->left[0],.23f);loaded=true;};require(loader.open(entries[0].snapshot)&&loader.isBusy()&&!loader.open(entries[0].snapshot),"session loader refuses overlapping requests until result delivery");for(int attempt=0;attempt<150&&!loaded;++attempt)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);require(loaded&&!loader.isBusy()&&loadError.isEmpty()&&mediaCorrect,"recovered snapshot reopens and decodes its original WAV");
        session.tracks[0].name="Earlier valid";require(RecoveryStore::write(recovery,id,1,original,"Earlier",session).wasOk(),"second recovery generation");session.tracks[0].name="Newest";require(RecoveryStore::write(recovery,id,2,original,"Latest",session).wasOk(),"third recovery generation");const auto run=recovery.getChildFile(id);require(run.getChildFile("snapshot-2.tracksmith").replaceWithText("{interrupted"),"corrupt newest snapshot fixture");entries=RecoveryStore::discover(recovery);require(entries.size()==1&&entries[0].generation==1,"corrupt newest snapshot falls back to previous valid generation");
        require(RecoveryStore::write(recovery,id,3,original,"Rotated",session).wasOk()&&run.findChildFiles(juce::File::findFiles,false,"*.tracksmith").size()==3,"snapshot rotation retains three files");const auto previous=run.getChildFile("snapshot-0.tracksmith").loadFileAsString();auto invalid=session;invalid.tracks[0].gain=2;require(RecoveryStore::write(recovery,id,6,original,"Invalid",invalid).failed()&&run.getChildFile("snapshot-0.tracksmith").loadFileAsString()==previous,"invalid snapshot leaves prior valid slot unchanged");
        const auto blocked=recovery.getChildFile("Blocked-root");require(blocked.replaceWithText("folder unavailable"),"unwritable destination fixture");require(RecoveryStore::write(blocked,RecoveryStore::newRun(),0,{},"Blocked",session).failed(),"recovery write failure reports without altering original");require(original.loadFileAsString()==originalBytes,"all failed recovery writes preserve manual file");
        require(RecoveryStore::markClean(run)&&RecoveryStore::discover(recovery).empty(),"clean save/discard marker suppresses recovery offers without deleting snapshots");require(run.getChildFile("snapshot-0.tracksmith").existsAsFile()&&RecoveryStore::write(recovery,id,4,original,"New edits",session).wasOk()&&RecoveryStore::discover(recovery).size()==1,"new unsaved snapshot reopens a previously clean recovery run");
        RecoveryWorker worker;bool completed=false;RecoveryResult async;worker.onFinished=[&](RecoveryResult result){async=std::move(result);completed=true;};const auto asyncId=RecoveryStore::newRun();require(worker.save(recovery,asyncId,0,42,{},"Untitled",session)&&worker.isBusy()&&!worker.scan(recovery,{}),"background recovery refuses an overlapping job until result delivery");for(int attempt=0;attempt<150&&!completed;++attempt)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);require(completed&&!worker.isBusy()&&async.error.isEmpty()&&async.revision==42,"background snapshot returns matching revision");completed=false;require(worker.scan(recovery,{}),"background recovery discovery starts");for(int attempt=0;attempt<150&&!completed;++attempt)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);require(completed&&async.scanning&&async.entries.size()==2,"startup-style recovery scan returns interrupted runs including untitled session");

        SavedSession large;for(int id=0;id<127;++id){auto track=source;track.name="Track "+juce::String(id+1);track.routing.sends[0].target=127;track.clips.clear();for(int clip=0;clip<16;++clip)track.clips.push_back({clip+1,0,clip*.1,0,.005});track.playlists.clear();for(int playlist=0;playlist<8;++playlist)track.playlists.push_back({playlist+1,"Take "+juce::String(playlist+1),track.clips,{}});track.volumeAutomation.clear();track.panAutomation.clear();for(int point=0;point<128;++point){track.volumeAutomation.push_back({point*.1,.5f});track.panAutomation.push_back({point*.1,0});}large.tracks.push_back(std::move(track));}large.tracks.push_back(aux);const auto stressId=RecoveryStore::newRun();require(RecoveryStore::write(recovery,stressId,0,{},"128-track metadata",large).wasOk(),"128-track playlist/automation recovery snapshot writes within limits");auto stressEntries=RecoveryStore::discover(recovery);const auto stress=std::find_if(stressEntries.begin(),stressEntries.end(),[&](const auto& entry){return entry.id==stressId;});require(stress!=stressEntries.end(),"large snapshot passes startup validation");loaded=false;loader.onFinished=[&](SessionJobResult result){loadError=result.error;mediaCorrect=result.session.tracks.size()==128&&result.session.tracks[126].playlists.size()==8&&result.audio[126]&&near(result.audio[126]->left[0],.23f);loaded=true;};require(loader.open(stress->snapshot),"128-track recovered session starts loading");for(int attempt=0;attempt<300&&!loaded;++attempt)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);require(loaded&&loadError.isEmpty()&&mediaCorrect,"large metadata and shared-file sources reopen intact within the resident-audio budget");
        const auto unrelated=recovery.getChildFile("notes/keep.txt");require(unrelated.getParentDirectory().createDirectory().wasOk()&&unrelated.replaceWithText("keep"),"unrelated file fixture");for(int n=0;n<11;++n)require(RecoveryStore::write(recovery,RecoveryStore::newRun(),0,{},"Retention",session).wasOk(),"fill bounded run history");int runs=0;for(const auto& candidate:recovery.findChildFiles(juce::File::findDirectories,false,"run-*"))if(RecoveryStore::validRun(candidate))++runs;require(runs<=RecoveryStore::maximumRuns&&unrelated.existsAsFile()&&original.loadFileAsString()==originalBytes,"retention bounds only app-owned recovery files and retains unrelated/manual data");
    }

    std::cout << "All VST3 hosting, wet input monitoring, delay compensation, NAM, session and MIDI and WAV export tests passed.\n";
}





