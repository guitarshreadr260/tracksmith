#include "../source/SessionFile.h"
#include <iostream>
#include <limits>
#include <cstdlib>
static void require(bool value, const char* message) { if(!value){ std::cerr<<"FAIL: "<<message<<'\n';std::exit(1); } }
int main()
{
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("work/session-format-tests");
    require(folder.createDirectory().wasOk(),"test folder");
    const auto file=folder.getChildFile("Mix.tracksmith"), wav=folder.getChildFile("Media/Tone.wav");
    require(wav.getParentDirectory().createDirectory().wasOk() && wav.replaceWithText("test reference"),"reference file");
    SavedSession source; source.bpm=97;source.master=0.6f;source.loop=true;source.metronome=true;source.preRollBars=2;source.metronomeGain=.4f;
    source.tracks.push_back({"Drums & Bass",wav,0.42f,-0.3f,true,false,true});
    source.tracks.push_back({"Empty keys",{},0.2f,0.75f,false,true,false});
    source.tracks[0].timelineEnabled=true;source.tracks[0].sources={wav,folder.getChildFile("Media/Second.wav")};source.tracks[0].clips={{1,0,2,.1,.5},{2,1,4,0,1}};
    source.tracks[1].midiTrack=true;source.tracks[1].notes={{1,36,10,1,.25,.85f}};
    source.tracks[0].volumeAutomation={{0,.2f},{1,.8f}};source.tracks[0].panAutomation={{0,-1},{2,1}};source.tracks[0].automationMode=2;
    const auto document=SessionFile::encode(source,file);
    auto legacy=SessionFile::encode(source,file);legacy.getDynamicObject()->setProperty("version",1);legacy.getDynamicObject()->removeProperty("inserts");
    legacy.getDynamicObject()->removeProperty("preRollBars");legacy.getDynamicObject()->removeProperty("metronomeGain");
    SavedSession legacySession;require(SessionFile::decode(legacy,file,legacySession).wasOk()&&legacySession.inserts.isEmpty(),"version 1 sessions open without inserts");require(legacySession.preRollBars==0&&legacySession.metronomeGain==1,"legacy click defaults preserved");
    require(SessionFile::write(file,document).wasOk(),"session writes through temporary file");
    SavedSession restored;require(SessionFile::read(file,restored).wasOk(),"JSON round trip");
    require(restored.tracks[0].automationMode==1&&restored.tracks[0].volumeAutomation.size()==2&&restored.tracks[0].panAutomation[1].value==1,"automation curves persist; Write saves as Read");
    auto legacy6=SessionFile::encode(source,file);legacy6.getDynamicObject()->setProperty("version",6);SavedSession from6;require(SessionFile::decode(legacy6,file,from6).wasOk()&&from6.tracks[0].volumeAutomation.empty()&&from6.tracks[0].automationMode==0,"version 6 defaults automation Off");
    auto badAutomation=SessionFile::encode(source,file);badAutomation.getProperty("tracks",{}).getArray()->getReference(0).getProperty("volumeAutomation",{}).getArray()->getReference(1).getDynamicObject()->setProperty("time",0);SavedSession rejected;require(SessionFile::decode(badAutomation,file,rejected).failed(),"duplicate automation times reject");
    badAutomation=SessionFile::encode(source,file);badAutomation.getProperty("tracks",{}).getArray()->getReference(0).getDynamicObject()->setProperty("automationMode",2);require(SessionFile::decode(badAutomation,file,rejected).failed(),"saved Write mode rejects");
    badAutomation=SessionFile::encode(source,file);badAutomation.getProperty("tracks",{}).getArray()->getReference(0).getProperty("panAutomation",{}).getArray()->getReference(0).getDynamicObject()->setProperty("value",2);require(SessionFile::decode(badAutomation,file,rejected).failed(),"out of range automation rejects");
    require(restored.tracks.size()==2 && restored.tracks[0].name=="Drums & Bass" && restored.tracks[1].audioFile==juce::File(),"names and empty tracks survive");
    require(restored.tracks[0].audioFile==wav && std::abs(restored.tracks[0].gain-0.42f)<1e-5f && restored.tracks[0].muted && restored.tracks[0].armed && restored.tracks[1].solo,"paths and channel settings survive");
    require(restored.bpm==97 && restored.loop && restored.metronome && std::abs(restored.master-0.6f)<1e-5f,"tempo, loop, click and master survive");
    require(restored.preRollBars==2 && std::abs(restored.metronomeGain-.4f)<1e-5f,"count-in and click level survive");
    require(restored.tracks[0].clips.size()==2&&restored.tracks[0].clips[0].start==2&&restored.tracks[0].clips[0].offset==.1&&restored.tracks[0].sources[1]==folder.getChildFile("Media/Second.wav"),"version 3 multi-source clip layout round trips");
    auto badClip=SessionFile::encode(source,file);badClip.getProperty("tracks",{}).getArray()->getReference(0).getProperty("clips",{}).getArray()->getReference(0).getDynamicObject()->setProperty("length",-1);
    require(SessionFile::decode(badClip,file,restored).failed(),"invalid clip bounds reject");
    auto version2=SessionFile::encode(source,file);version2.getDynamicObject()->setProperty("version",2);SavedSession old;require(SessionFile::decode(version2,file,old).wasOk()&&!old.tracks[0].timelineEnabled,"version 2 legacy lane remains readable");
    require(restored.tracks[1].midiTrack&&restored.tracks[1].notes.size()==1&&restored.tracks[1].notes[0].pitch==36,"version 4 MIDI track and notes round trip");
    auto legacy3=SessionFile::encode(source,file);legacy3.getDynamicObject()->setProperty("version",3);SavedSession from3;require(SessionFile::decode(legacy3,file,from3).wasOk()&&!from3.tracks[1].midiTrack,"version 3 audio sessions remain readable");
    const auto moved=folder.getChildFile("Moved");require(moved.getChildFile("Media").createDirectory().wasOk(),"relocation folder");
    require(wav.copyFileTo(moved.getChildFile("Media/Tone.wav")) && file.copyFileTo(moved.getChildFile("Mix.tracksmith")),"relocation copy");
    require(SessionFile::read(moved.getChildFile("Mix.tracksmith"),restored).wasOk() && restored.tracks[0].audioFile==moved.getChildFile("Media/Tone.wav"),"relative WAV paths relocate with session");
    source.tracks[0].inputSource=1;source.tracks[0].inputChannels=2;
    auto routing=SessionFile::encode(source,file);SavedSession routed;
    require(SessionFile::decode(routing,file,routed).wasOk()&&routed.tracks[0].inputSource==1&&routed.tracks[0].inputChannels==2,"per-track input and channel mode round trip");
    auto legacyRouting=SessionFile::encode(source,file);auto* firstRoute=legacyRouting.getProperty("tracks",{}).getArray()->getReference(0).getDynamicObject();firstRoute->removeProperty("inputSource");firstRoute->removeProperty("inputChannels");
    require(SessionFile::decode(legacyRouting,file,routed).wasOk()&&routed.tracks[0].inputSource==0&&routed.tracks[0].inputChannels==1,"older sessions default to first mono input");
    firstRoute->setProperty("inputSource",32);require(SessionFile::decode(legacyRouting,file,routed).failed(),"invalid saved recording input rejects");
    auto invalid=SessionFile::encode(source,file);invalid.getDynamicObject()->setProperty("version",99);
    const auto oldBpm=restored.bpm;require(SessionFile::decode(invalid,file,restored).failed() && restored.bpm==oldBpm,"unknown versions reject without changing model");
    invalid=SessionFile::encode(source,file);invalid.getDynamicObject()->setProperty("bpm","120");require(SessionFile::decode(invalid,file,restored).failed(),"numeric strings reject");
    invalid=SessionFile::encode(source,file);invalid.getDynamicObject()->setProperty("master",std::numeric_limits<double>::quiet_NaN());require(SessionFile::decode(invalid,file,restored).failed(),"non-finite controls reject");
    invalid=SessionFile::encode(source,file);invalid.getDynamicObject()->setProperty("bpm",301);require(SessionFile::decode(invalid,file,restored).failed(),"tempo bounds validate");
    invalid=SessionFile::encode(source,file);auto list=invalid.getProperty("tracks",{});list.getArray()->getReference(0).getDynamicObject()->setProperty("gain",2.0);require(SessionFile::decode(invalid,file,restored).failed(),"channel bounds validate");
    source.tracks.resize(129);require(SessionFile::decode(SessionFile::encode(source,file),file,restored).failed(),"too many tracks reject");
    const auto malformed=folder.getChildFile("Bad.tracksmith");require(malformed.replaceWithText("{bad json"),"malformed fixture");require(SessionFile::read(malformed,restored).failed(),"malformed JSON rejects");
    const auto missing=folder.getChildFile("Missing.tracksmith");require(SessionFile::read(missing,restored).failed(),"missing session rejects");
    const auto before=file.loadFileAsString();require(SessionFile::write(folder,document).failed() && file.loadFileAsString()==before,"failed write does not alter saved session");
    require(SessionFile::write(file,document).wasOk(),"existing session can be saved again");
    SavedSession busSession;SessionTrack audioTrack;audioTrack.name="Guitars";audioTrack.timelineEnabled=true;audioTrack.routing.output=1;audioTrack.routing.sends[0]={1,.4f};busSession.tracks.push_back(audioTrack);
    SessionTrack auxTrack;auxTrack.name="Shared Reverb";auxTrack.timelineEnabled=true;auxTrack.routing.aux=true;auxTrack.gain=1;busSession.tracks.push_back(auxTrack);
    auto busDocument=SessionFile::encode(busSession,file);SavedSession busRestored;
    require(SessionFile::decode(busDocument,file,busRestored).wasOk()&&busRestored.tracks[1].routing.aux&&busRestored.tracks[0].routing.output==1&&std::abs(busRestored.tracks[0].routing.sends[0].gain-.4f)<1.e-6,"aux, bus output and send levels round trip in version 5");
    auto invalidRoute=busDocument.clone();invalidRoute.getProperty("tracks",{}).getArray()->getReference(1).getDynamicObject()->setProperty("output",1);require(SessionFile::decode(invalidRoute,file,busRestored).failed(),"saved aux self feedback rejects");
    invalidRoute=busDocument.clone();invalidRoute.getProperty("tracks",{}).getArray()->getReference(0).getDynamicObject()->setProperty("output",99);require(SessionFile::decode(invalidRoute,file,busRestored).failed(),"missing bus rejects during session load");
    invalidRoute=busDocument.clone();invalidRoute.getProperty("tracks",{}).getArray()->getReference(1).getDynamicObject()->setProperty("armed",true);require(SessionFile::decode(invalidRoute,file,busRestored).failed(),"armed aux rejects");
    auto old4=document.clone();old4.getDynamicObject()->setProperty("version",4);require(SessionFile::decode(old4,file,busRestored).wasOk()&&!busRestored.tracks[0].routing.aux&&busRestored.tracks[0].routing.output==-1,"version 4 sessions retain direct master defaults");
    SavedSession playlistSession;SessionTrack recorded;recorded.name="Lead";recorded.timelineEnabled=true;recorded.audioFile=wav;recorded.sources={wav,wav};recorded.activePlaylist=2;
    TimelineClip faded{1,0,0,0,1};faded.fadeIn=.1;faded.fadeOut=.1;auto oldLayout=std::vector<TimelineClip>{faded};auto newLayout=oldLayout;require(ClipLayout::replaceRange(newLayout,.3,.5,{{1,1,.3,0,.2}}),"playlist replacement fixture");
    recorded.clips=newLayout;recorded.playlists={{1,"Original",oldLayout,{}},{2,"Retake",newLayout,{}}};playlistSession.tracks.push_back(recorded);
    SessionTrack midiPlaylist;midiPlaylist.name="Keys";midiPlaylist.timelineEnabled=true;midiPlaylist.midiTrack=true;midiPlaylist.activePlaylist=2;midiPlaylist.notes={{1,62,1,0,1,.8f}};midiPlaylist.playlists={{1,"First",{},{{1,60,1,0,1,.7f}}},{2,"Second",{},midiPlaylist.notes}};playlistSession.tracks.push_back(midiPlaylist);
    auto playlistsDocument=SessionFile::encode(playlistSession,file);SavedSession playlistsRestored;
    require(SessionFile::decode(playlistsDocument,file,playlistsRestored).wasOk()&&playlistsRestored.tracks[0].activePlaylist==2&&playlistsRestored.tracks[0].playlists.size()==2&&playlistsRestored.tracks[0].clips.size()==3,"active audio playlist and alternate arrangements round trip");
    require(playlistsRestored.tracks[0].playlists[0].clips[0].fadeIn==.1&&playlistsRestored.tracks[1].notes[0].pitch==62&&playlistsRestored.tracks[1].playlists[0].notes[0].pitch==60,"fade envelopes and active/alternate MIDI notes round trip");
    auto badPlaylist=playlistsDocument.clone();badPlaylist.getProperty("tracks",{}).getArray()->getReference(0).getDynamicObject()->setProperty("activePlaylist",99);require(SessionFile::decode(badPlaylist,file,playlistsRestored).failed(),"missing active playlist rejects");
    badPlaylist=playlistsDocument.clone();auto* oldClip=badPlaylist.getProperty("tracks",{}).getArray()->getReference(0).getProperty("playlists",{}).getArray()->getReference(0).getProperty("clips",{}).getArray()->getReference(0).getDynamicObject();oldClip->setProperty("fadeIn",-1);require(SessionFile::decode(badPlaylist,file,playlistsRestored).failed(),"bad fade on inactive playlist rejects");
    badPlaylist=playlistsDocument.clone();badPlaylist.getProperty("tracks",{}).getArray()->getReference(0).getProperty("playlists",{}).getArray()->getReference(1).getDynamicObject()->setProperty("id",1);require(SessionFile::decode(badPlaylist,file,playlistsRestored).failed(),"duplicate playlist IDs reject");
    auto version5=document.clone();version5.getDynamicObject()->setProperty("version",5);require(SessionFile::decode(version5,file,playlistsRestored).wasOk()&&playlistsRestored.tracks[0].playlists.size()==1,"version 5 sessions get a Main playlist");
    {
        source.tracks.resize(2);source.tracks[0].stereoLeft=.25f;source.tracks[0].stereoRight=-.75f;auto doc=SessionFile::encode(source,file);SavedSession result;
        require(SessionFile::decode(doc,file,result).wasOk()&&result.tracks[0].stereoLeft==.25f&&result.tracks[0].stereoRight==-.75f,"stereo pans round trip");
        auto* track=doc.getProperty("tracks",{}).getArray()->getReference(0).getDynamicObject();track->removeProperty("stereoLeft");track->removeProperty("stereoRight");
        require(SessionFile::decode(doc,file,result).wasOk()&&result.tracks[0].stereoLeft==-1&&result.tracks[0].stereoRight==1,"old sessions retain original stereo image");
        track->setProperty("stereoLeft",2);require(SessionFile::decode(doc,file,result).failed(),"invalid stereo pan rejects");
    }
    std::cout<<"All session format tests passed.\n";
}

