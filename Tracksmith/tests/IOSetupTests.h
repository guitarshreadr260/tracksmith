#pragma once
// Source coverage for the next authorized native test build.
static void ioSetupTests(const juce::File& folder)
{
    float a[512], b[512];std::fill_n(a,512,.2f);std::fill_n(b,512,.4f);
    const float* holes[]{nullptr,a,nullptr,b};
    require(InputRouting::select(holes,4,1,1)[0]==a,"Physical input 2 survives disabled input 1");
    require(InputRouting::select(holes,4,3,1)[0]==b,"Physical input 4 never compacts to input 2");
    require(!InputRouting::select(holes,4,1,2)[0],"Stereo cannot bridge a disabled channel");
    require(InputRouting::legacyPhysical(1,4,[&](int ch){return holes[ch]!=nullptr;})==3,"Legacy enabled index resolves once to physical input");
    require(InputRouting::legacyPhysical(3,4,[&](int ch){return holes[ch]!=nullptr;})==-1,"Missing legacy input stays unassigned");
    auto owner=std::make_unique<AudioEngine>();auto& engine=*owner;engine.playback.deviceRate=48000;
    engine.monitorOverrides[0]=1;engine.trackInputs[0]=31;engine.trackInputChannels[0]=1;engine.monitorLevels[0]=1;
    engine.playback.tracks[0].gain=1;engine.playback.masterGain=1;
    std::array<const float*,32> inputs{};inputs[31]=a;
    float l[512],r[512],third[512],fourth[512];float* out[]{l,r,third,fourth};
    for(int n=0;n<10;++n)engine.renderAudio(inputs.data(),32,out,4,512);
    require(std::abs(l[511]-.2f)<1.e-4f&&std::abs(r[511]-.2f)<1.e-4f,"Input 32 monitors through main");
    engine.mainOutputPair=1;engine.renderAudio(inputs.data(),32,out,4,512);
    require(l[511]==0&&r[511]==0&&std::abs(third[511]-.2f)<1.e-4f&&std::abs(fourth[511]-.2f)<1.e-4f,"Main remaps to physical outputs 3/4");
    engine.mainOutputPair=-1;engine.renderAudio(inputs.data(),32,out,4,512);
    require(l[511]==0&&r[511]==0&&third[511]==0&&fourth[511]==0,"Disconnected main is silent");
    engine.mainOutputPair=0;engine.ioDeviceMatches=false;engine.renderAudio(inputs.data(),32,out,4,512);
    require(l[511]==0&&r[511]==0&&!engine.playback.inputMonitorTracks[0],"Unreviewed interface change mutes hardware I/O");
    SavedSession original;original.mainOutputPair=2;original.ioDeviceKey="ASIO | Test input | Test output";original.preRollBars=4;
    SessionTrack track;track.name="Input 32";track.inputSource=31;original.tracks.push_back(track);
    const auto file=folder.getChildFile("IO-roundtrip.tracksmith");auto document=SessionFile::encode(original,file);SavedSession restored;
    require(SessionFile::decode(document,file,restored).wasOk()&&restored.mainOutputPair==2&&restored.ioDeviceKey==original.ioDeviceKey&&restored.tracks[0].inputSource==31&&!restored.legacyInputRouting&&restored.preRollBars==4,"Version 10 preserves physical routing and four-bar count-in");
    document.getDynamicObject()->setProperty("mainOutputPair",8);require(SessionFile::decode(document,file,restored).failed(),"Invalid main hardware pair rejects session");
    std::vector<RecordRoute> routes;inputs.fill(a);inputs[31]=b;
    const auto run=juce::Uuid().toString();
    for(int id=0;id<32;++id)routes.push_back({id,id,1,folder.getChildFile("IO-32-"+run+"-"+juce::String(id)+".wav")});
    require(engine.recorder.startGroup(routes,48000,512).wasOk(),"32 physical inputs prepare 32 disk writers");
    engine.recorder.capture(inputs.data(),32,512);require(engine.recorder.waitForFinish(10000),"32-input capture drains");
    auto takes=engine.recorder.takeResults();require(takes.size()==32,"32 takes retained");
    for(const auto& take:takes){require(take.take.error.isEmpty(),"Every physical input writes without error");const auto decoded=decodeWav(take.take.file,1000,[]{return false;});require(decoded.error.isEmpty()&&decoded.left.size()==512&&std::abs(decoded.left[200]-(take.track==31?.4f:.2f))<1.e-5f,"Each WAV keeps its physical input signal");}
    // Exercise automatic Dynamic punch across boundaries inside one callback.
    auto punchOwner=std::make_unique<AudioEngine>();auto& punch=*punchOwner;punch.playback.deviceRate=48000;
    for(int id=0;id<2;++id){auto& t=punch.playback.tracks[id];t.left.assign(48000,id==0?.1f:.05f);t.right=t.left;t.sourceRate=48000;t.gain=1;}
    punch.playback.refreshDuration();punch.playback.playing=true;
    const float* punchIn[]{a};float punchL[512],punchR[512];float* punchOut[]{punchL,punchR};
    for(int i=0;i<12;++i)punch.renderAudio(punchIn,1,punchOut,2,512);
    const auto punched=folder.getChildFile("Dynamic-boundaries-"+run+".wav");
    require(punch.recorder.startGroup({{0,0,1,punched}},48000,10000).wasOk(),"Dynamic punch prepares its WAV before playback");
    punch.takeWasPunch=true;punch.punchEnabled=false;punch.planTrack=0;punch.planPosition=0;punch.punchBegin=128;punch.punchEnd=384;punch.recordPlanActive=true;
    punch.playback.recording=true;punch.playback.recordTrack=0;punch.playback.recordTracks[0]=true;punch.recordingCorrection=0;
    punch.renderAudio(punchIn,1,punchOut,2,512);
    require(std::abs(punchL[64]-.15f)<1.e-4f&&std::abs(punchL[256]-.05f)<1.e-4f&&std::abs(punchL[480]-.15f)<1.e-4f,"Old target plays before/after punch, stays silent inside; backing track continues");
    require(punch.recorder.waitForFinish(10000),"Dynamic punch finalizes automatically at selection end");
    const auto punchedAudio=decodeWav(punched,1000,[]{return false;});require(punchedAudio.error.isEmpty()&&punchedAudio.left.size()==256&&std::abs(punchedAudio.left[100]-.2f)<1.e-5f,"Only highlighted frames reach the dry recorded WAV");
}
