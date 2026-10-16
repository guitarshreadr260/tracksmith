#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <array>
#include "ClipLayout.h"
#include "MidiLane.h"
#include "MixerRouting.h"
#include "TrackPlaylist.h"
#include "Automation.h"
#include "PluginAutomation.h"

struct SessionTrack
{
    juce::String name;
    juce::File audioFile;
    float gain = 0.75f, pan = 0.0f;
    bool muted = false, solo = false, armed = false;
    std::vector<juce::File> sources; std::vector<TimelineClip> clips; bool timelineEnabled = false;
    std::vector<TrackPlaylist> playlists;int activePlaylist=1;
    Automation::Lane volumeAutomation,panAutomation;int automationMode=0;
    MixerRoute routing;
    int inputSource=0,inputChannels=1;
    float stereoLeft=-1,stereoRight=1;
    bool midiTrack=false;std::vector<MidiNote> notes;
};
struct SavedSession
{
    double bpm = 120.0;
    float master = 1.0f, metronomeGain = 1.0f;
    int preRollBars = 0;
    int mainOutputPair = 0;
    juce::String ioDeviceKey;
    bool legacyInputRouting = false;
    bool loop = false, metronome = false;
    std::vector<SessionTrack> tracks;
    juce::Array<juce::var> inserts;
};
namespace SessionFile
{
inline juce::var encodeAutomation(const Automation::Lane& lane){juce::Array<juce::var> points;for(const auto& p:lane){auto* item=new juce::DynamicObject();item->setProperty("time",p.time);item->setProperty("value",p.value);points.add(juce::var(item));}return points;}
inline bool decodeAutomation(const juce::var& input,Automation::Lane& lane,float low,float high){const auto* points=input.getArray();if(!points||points->size()>static_cast<int>(Automation::maxPoints))return false;for(const auto& entry:*points){const auto t=entry.getProperty("time",{}),v=entry.getProperty("value",{});if(!(t.isInt()||t.isInt64()||t.isDouble())||!(v.isInt()||v.isInt64()||v.isDouble()))return false;lane.push_back({static_cast<double>(t),static_cast<float>(v)});}return Automation::valid(lane,low,high);}
inline juce::var encodeClip(const TimelineClip& clip)
{
    auto* c=new juce::DynamicObject();c->setProperty("id",clip.id);c->setProperty("source",clip.source);c->setProperty("start",clip.start);c->setProperty("offset",clip.offset);c->setProperty("length",clip.length);
    c->setProperty("fadeIn",clip.fadeIn);c->setProperty("fadeOut",clip.fadeOut);c->setProperty("fadeOffset",clip.fadeOffset);c->setProperty("fadeSpan",clip.fadeSpan);c->setProperty("blend",clip.blend);
    if(clip.elasticSource>=0){c->setProperty("elasticSource",clip.elasticSource);c->setProperty("elasticOffset",clip.elasticOffset);c->setProperty("elasticLength",clip.elasticLength);juce::Array<juce::var> anchors;for(const auto p:clip.elasticAnchors){auto* a=new juce::DynamicObject();a->setProperty("source",p.source);a->setProperty("target",p.target);anchors.add(juce::var(a));}c->setProperty("elasticAnchors",anchors);}return juce::var(c);
}
inline bool decodeClip(const juce::var& entry,TimelineClip& c,int sources,bool fades)
{
    const auto id=entry.getProperty("id",{}),source=entry.getProperty("source",{});if(!id.isInt()||!source.isInt())return false;c.id=static_cast<int>(id);c.source=static_cast<int>(source);
    const auto number=[&](const char* key,double& target){const auto v=entry.getProperty(key,{});if(!(v.isInt()||v.isInt64()||v.isDouble()))return false;target=static_cast<double>(v);return std::isfinite(target);};
    if(!number("start",c.start)||!number("offset",c.offset)||!number("length",c.length))return false;
    if(fades){const auto blend=entry.getProperty("blend",{});if(!blend.isBool()||!number("fadeIn",c.fadeIn)||!number("fadeOut",c.fadeOut)||!number("fadeOffset",c.fadeOffset)||!number("fadeSpan",c.fadeSpan))return false;c.blend=static_cast<bool>(blend);}
    if(entry.hasProperty("elasticSource")){const auto source=entry.getProperty("elasticSource",{});if(!source.isInt()||!number("elasticOffset",c.elasticOffset)||!number("elasticLength",c.elasticLength))return false;c.elasticSource=static_cast<int>(source);if(c.elasticSource<0||c.elasticSource>=sources)return false;const auto anchors=entry.getProperty("elasticAnchors",{});const auto* list=anchors.getArray();if(!list||list->size()>16)return false;for(const auto& point:*list){const auto a=point.getProperty("source",{}),b=point.getProperty("target",{});if(!(a.isInt()||a.isInt64()||a.isDouble())||!(b.isInt()||b.isInt64()||b.isDouble()))return false;c.elasticAnchors.push_back({static_cast<double>(a),static_cast<double>(b)});}}
    return ClipLayout::valid(c)&&c.source<sources;
}
inline juce::var encodeNote(const MidiNote& note)
{
    auto* n=new juce::DynamicObject();n->setProperty("id",note.id);n->setProperty("pitch",note.pitch);n->setProperty("channel",note.channel);n->setProperty("start",note.start);n->setProperty("length",note.length);n->setProperty("velocity",note.velocity);return juce::var(n);
}
inline bool decodeNote(const juce::var& entry,MidiNote& n)
{
    const auto id=entry.getProperty("id",{}),pitch=entry.getProperty("pitch",{}),channel=entry.getProperty("channel",{});if(!id.isInt()||!pitch.isInt()||!channel.isInt())return false;
    n.id=static_cast<int>(id);n.pitch=static_cast<int>(pitch);n.channel=static_cast<int>(channel);
    const auto read=[&](const char* key,double& value){const auto v=entry.getProperty(key,{});if(!(v.isDouble()||v.isInt()||v.isInt64()))return false;value=static_cast<double>(v);return std::isfinite(value);};double velocity=0;
    if(!read("start",n.start)||!read("length",n.length)||!read("velocity",velocity))return false;n.velocity=static_cast<float>(velocity);return validMidiNote(n);
}
inline juce::var encode(const SavedSession& session, const juce::File& destination)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", "TracksmithSession"); root->setProperty("version", 11);
    root->setProperty("mainOutputPair",session.mainOutputPair);root->setProperty("ioDeviceKey",session.ioDeviceKey);
    juce::Array<juce::var> inserts;for(const auto& original:session.inserts){auto item=original.clone();if(!item.hasProperty("automation"))item.getDynamicObject()->setProperty("automation",juce::Array<juce::var>());inserts.add(item);}root->setProperty("inserts", inserts);
    root->setProperty("preRollBars", session.preRollBars); root->setProperty("metronomeGain", session.metronomeGain);
    root->setProperty("bpm", session.bpm); root->setProperty("master", session.master);
    root->setProperty("loop", session.loop); root->setProperty("metronome", session.metronome);
    juce::Array<juce::var> tracks;
    for (const auto& track : session.tracks)
    {
        auto* item = new juce::DynamicObject(); item->setProperty("name", track.name);
        item->setProperty("aux",track.routing.aux);item->setProperty("output",track.routing.output);item->setProperty("hardwarePair",track.routing.hardwarePair);
        juce::Array<juce::var> sends;for(const auto& send:track.routing.sends){auto* entry=new juce::DynamicObject();entry->setProperty("target",send.target);entry->setProperty("gain",send.gain);entry->setProperty("pre",send.pre);sends.add(juce::var(entry));}item->setProperty("sends",sends);
        item->setProperty("audio", track.audioFile == juce::File() ? juce::String() : track.audioFile.getRelativePathFrom(destination.getParentDirectory()));
        item->setProperty("stereoLeft",track.stereoLeft);item->setProperty("stereoRight",track.stereoRight);
        item->setProperty("inputSource",track.inputSource);item->setProperty("inputChannels",track.inputChannels);
        item->setProperty("automationMode",track.automationMode==0?0:1);item->setProperty("volumeAutomation",encodeAutomation(track.volumeAutomation));item->setProperty("panAutomation",encodeAutomation(track.panAutomation));
        item->setProperty("gain", track.gain); item->setProperty("pan", track.pan);
        item->setProperty("muted", track.muted); item->setProperty("solo", track.solo); item->setProperty("armed", track.armed);
        juce::Array<juce::var> sources,clips;
        const auto files=track.sources.empty() ? std::vector<juce::File>{track.audioFile} : track.sources;
        for(const auto& file:files) sources.add(file==juce::File()?juce::String():file.getRelativePathFrom(destination.getParentDirectory()));
        for(const auto& clip:track.clips)clips.add(encodeClip(clip));
        item->setProperty("midiTrack",track.midiTrack);juce::Array<juce::var> notes;
        for(const auto& note:track.notes){auto* n=new juce::DynamicObject();n->setProperty("id",note.id);n->setProperty("pitch",note.pitch);n->setProperty("channel",note.channel);n->setProperty("start",note.start);n->setProperty("length",note.length);n->setProperty("velocity",note.velocity);notes.add(juce::var(n));}item->setProperty("notes",notes);
        juce::Array<juce::var> playlists;auto layouts=track.playlists;if(layouts.empty()&&!track.routing.aux)layouts.push_back({1,"Main",track.clips,track.notes});
        for(const auto& layout:layouts){auto* p=new juce::DynamicObject();p->setProperty("id",layout.id);p->setProperty("name",layout.name);juce::Array<juce::var> c,n;for(const auto& clip:layout.clips)c.add(encodeClip(clip));for(const auto& note:layout.notes)n.add(encodeNote(note));p->setProperty("clips",c);p->setProperty("notes",n);playlists.add(juce::var(p));}item->setProperty("playlists",playlists);item->setProperty("activePlaylist",track.routing.aux?0:track.activePlaylist);
        item->setProperty("timeline",track.timelineEnabled);item->setProperty("sources",sources);item->setProperty("clips",clips);
        tracks.add(juce::var(item));
    }
    root->setProperty("tracks", tracks); return juce::var(root);
}
inline juce::Result decode(const juce::var& root, const juce::File& source, SavedSession& output)
{
    auto fail = [] { return juce::Result::fail("Invalid or unsupported Tracksmith session."); };
    if (!root.isObject() || root.getProperty("format", {}).toString() != "TracksmithSession") return fail();
    const auto version = root.getProperty("version", {});
    if (!version.isInt() || (static_cast<int>(version) != 1 && static_cast<int>(version) != 2 && static_cast<int>(version) != 3 && static_cast<int>(version) != 4 && static_cast<int>(version) != 5 && static_cast<int>(version) != 6 && static_cast<int>(version) != 7 && static_cast<int>(version) != 8 && static_cast<int>(version) != 9 && static_cast<int>(version) != 10 && static_cast<int>(version) != 11)) return fail();
    const auto number = [](const juce::var& object, const char* key, double low, double high, double& result)
    {
        const auto value = object.getProperty(key, {});
        if (!(value.isDouble() || value.isInt() || value.isInt64())) return false;
        result = static_cast<double>(value);
        return std::isfinite(result) && result >= low && result <= high;
    };
    const auto boolean = [](const juce::var& object, const char* key, bool& result)
    {
        const auto value = object.getProperty(key, {}); if (!value.isBool()) return false;
        result = static_cast<bool>(value); return true;
    };
    SavedSession parsed; double master = 1.0;
    if (!number(root,"bpm",20,300,parsed.bpm) || !number(root,"master",0,1,master)
        || !boolean(root,"loop",parsed.loop) || !boolean(root,"metronome",parsed.metronome)) return fail();
    parsed.master = static_cast<float>(master);
    parsed.legacyInputRouting=static_cast<int>(version)<10;
    if(static_cast<int>(version)>=10){const auto pair=root.getProperty("mainOutputPair",{}),device=root.getProperty("ioDeviceKey",{});
        if(!pair.isInt()||static_cast<int>(pair)<-1||static_cast<int>(pair)>7||!device.isString()||device.toString().length()>2048)return fail();
        parsed.mainOutputPair=static_cast<int>(pair);parsed.ioDeviceKey=device.toString();}
    double click = 1;
    if (root.hasProperty("metronomeGain") && !number(root,"metronomeGain",0,1,click)) return fail();
    parsed.metronomeGain = static_cast<float>(click);
    if (root.hasProperty("preRollBars"))
    { const auto bars = root.getProperty("preRollBars", {}); if (!bars.isInt() || static_cast<int>(bars)<0 || static_cast<int>(bars)>4) return fail(); parsed.preRollBars = static_cast<int>(bars); }
    const auto list = root.getProperty("tracks", {});
    const auto* array = list.getArray();
    if (array == nullptr || array->size() < 1 || array->size() > 128) return fail();
    for (const auto& item : *array)
    {
        if (!item.isObject()) return fail();
        const auto name = item.getProperty("name", {}), audio = item.getProperty("audio", {});
        if (!name.isString() || !audio.isString() || name.toString().trim().isEmpty() || name.toString().length() > 80 || audio.toString().length() > 32768) return fail();
        SessionTrack track; track.name = name.toString().trim(); double gain = 0.75, pan = 0.0;
        if (!number(item,"gain",0,1,gain) || !number(item,"pan",-1,1,pan)
            || !boolean(item,"muted",track.muted) || !boolean(item,"solo",track.solo) || !boolean(item,"armed",track.armed)) return fail();
        track.gain = static_cast<float>(gain); track.pan = static_cast<float>(pan);
        double pl=-1,pr=1;if(item.hasProperty("stereoLeft")&&!number(item,"stereoLeft",-1,1,pl))return fail();if(item.hasProperty("stereoRight")&&!number(item,"stereoRight",-1,1,pr))return fail();track.stereoLeft=static_cast<float>(pl);track.stereoRight=static_cast<float>(pr);
        if(item.hasProperty("inputSource")){const auto value=item.getProperty("inputSource",{});if(!value.isInt()||static_cast<int>(value)<(parsed.legacyInputRouting?0:-1)||static_cast<int>(value)>(parsed.legacyInputRouting?3:31))return fail();track.inputSource=static_cast<int>(value);}
        if(item.hasProperty("inputChannels")){const auto value=item.getProperty("inputChannels",{});if(!value.isInt()||static_cast<int>(value)<1||static_cast<int>(value)>2)return fail();track.inputChannels=static_cast<int>(value);}
        const auto path = audio.toString();
        if (path.isNotEmpty()) track.audioFile = juce::File::isAbsolutePath(path) ? juce::File(path) : source.getParentDirectory().getChildFile(path);
        if(static_cast<int>(version)>=3)
        {
            if(!boolean(item,"timeline",track.timelineEnabled))return fail();
            const auto media=item.getProperty("sources",{}), layout=item.getProperty("clips",{});
            const auto* files=media.getArray();const auto* clips=layout.getArray();
            if(!files||files->size()<1||files->size()>static_cast<int>(ClipLayout::maximumSources)||!clips||clips->size()>static_cast<int>(ClipLayout::maximumClips))return fail();
            for(const auto& file:*files){if(!file.isString()||file.toString().length()>32768)return fail();const auto path=file.toString();track.sources.push_back(path.isEmpty()?juce::File():juce::File::isAbsolutePath(path)?juce::File(path):source.getParentDirectory().getChildFile(path));}
            std::vector<int> ids;
            for(const auto& entry:*clips)
            {
                TimelineClip c;if(!decodeClip(entry,c,files->size(),static_cast<int>(version)>=6)||std::find(ids.begin(),ids.end(),c.id)!=ids.end())return fail();
                ids.push_back(c.id);track.clips.push_back(c);
            }
            if(!track.timelineEnabled && (files->size()!=1 || !clips->isEmpty()))return fail();
            track.audioFile=track.sources[0];
        }
        if(static_cast<int>(version)>=4)
        {
            if(!boolean(item,"midiTrack",track.midiTrack))return fail();const auto noteList=item.getProperty("notes",{});const auto* notes=noteList.getArray();if(!notes||notes->size()>static_cast<int>(MidiLane::maximumNotes))return fail();
            std::vector<int> ids;for(const auto& entry:*notes){MidiNote n;const auto id=entry.getProperty("id",{}),pitch=entry.getProperty("pitch",{}),channel=entry.getProperty("channel",{});double velocity=0;if(!id.isInt()||!pitch.isInt()||!channel.isInt()||!number(entry,"start",0,100000,n.start)||!number(entry,"length",1.e-5,100000,n.length)||!number(entry,"velocity",.00001,1,velocity))return fail();n.id=static_cast<int>(id);n.pitch=static_cast<int>(pitch);n.channel=static_cast<int>(channel);n.velocity=static_cast<float>(velocity);if(!validMidiNote(n)||std::find(ids.begin(),ids.end(),n.id)!=ids.end())return fail();ids.push_back(n.id);track.notes.push_back(n);}
            if(track.midiTrack && (!track.clips.empty() || track.audioFile!=juce::File() || track.sources.size()!=1))return fail();if(!track.midiTrack&&!track.notes.empty())return fail();
        }
        if(static_cast<int>(version)>=5)
        {
            const auto busOutput=item.getProperty("output",{});if(!boolean(item,"aux",track.routing.aux)||!busOutput.isInt())return fail();track.routing.output=static_cast<int>(busOutput);
            const auto sendList=item.getProperty("sends",{});const auto* sends=sendList.getArray();if(!sends||sends->size()!=2)return fail();
            for(int n=0;n<2;++n){const auto& send=(*sends)[n];const auto target=send.getProperty("target",{});double sendGain=0;if(!send.isObject()||!target.isInt()||!number(send,"gain",0,1,sendGain))return fail();track.routing.sends[n]={static_cast<int>(target),static_cast<float>(sendGain)};}
            if(static_cast<int>(version)>=9){const auto pair=item.getProperty("hardwarePair",{});if(!pair.isInt())return fail();track.routing.hardwarePair=static_cast<int>(pair);for(int n=0;n<2;++n){const auto pre=(*sends)[n].getProperty("pre",{});if(!pre.isBool())return fail();track.routing.sends[n].pre=static_cast<bool>(pre);}}
            if(track.routing.aux&&(track.midiTrack||track.armed||track.audioFile!=juce::File()||!track.clips.empty()||!track.notes.empty()||track.sources.size()!=1))return fail();
        }
        if(static_cast<int>(version)>=6)
        {
            const auto list=item.getProperty("playlists",{}),active=item.getProperty("activePlaylist",{});const auto* layouts=list.getArray();
            if(!layouts||!active.isInt()||layouts->size()>static_cast<int>(maximumPlaylists)||(track.routing.aux?(!layouts->isEmpty()||static_cast<int>(active)!=0):layouts->isEmpty()))return fail();
            track.activePlaylist=static_cast<int>(active);std::vector<int> ids;
            for(const auto& value:*layouts)
            {
                TrackPlaylist p;const auto id=value.getProperty("id",{}),name=value.getProperty("name",{});if(!id.isInt()||static_cast<int>(id)<=0||static_cast<int>(id)>1000000000||!name.isString()||name.toString().trim().isEmpty()||name.toString().length()>64||std::find(ids.begin(),ids.end(),static_cast<int>(id))!=ids.end())return fail();p.id=static_cast<int>(id);p.name=name.toString().trim();ids.push_back(p.id);
                const auto c=value.getProperty("clips",{}),n=value.getProperty("notes",{});const auto* clips=c.getArray();const auto* notes=n.getArray();if(!clips||!notes||clips->size()>static_cast<int>(ClipLayout::maximumClips)||notes->size()>static_cast<int>(MidiLane::maximumNotes)||(track.midiTrack?!clips->isEmpty():!notes->isEmpty()))return fail();
                std::vector<int> clipIds,noteIds;for(const auto& entry:*clips){TimelineClip clip;if(!decodeClip(entry,clip,static_cast<int>(track.sources.size()),true)||std::find(clipIds.begin(),clipIds.end(),clip.id)!=clipIds.end())return fail();clipIds.push_back(clip.id);p.clips.push_back(clip);}for(const auto& entry:*notes){MidiNote note;if(!decodeNote(entry,note)||std::find(noteIds.begin(),noteIds.end(),note.id)!=noteIds.end())return fail();noteIds.push_back(note.id);p.notes.push_back(note);}track.playlists.push_back(std::move(p));
            }
            if(!track.routing.aux){const auto selected=std::find_if(track.playlists.begin(),track.playlists.end(),[&](const auto& p){return p.id==track.activePlaylist;});if(selected==track.playlists.end())return fail();track.clips=selected->clips;track.notes=selected->notes;if(!track.timelineEnabled&&!track.clips.empty())return fail();}
        }
        else if(!track.routing.aux)track.playlists.push_back({1,"Main",track.clips,track.notes});
        if(static_cast<int>(version)>=7){const auto mode=item.getProperty("automationMode",{});if(!mode.isInt()||static_cast<int>(mode)<0||static_cast<int>(mode)>1||!decodeAutomation(item.getProperty("volumeAutomation",{}),track.volumeAutomation,0,1)||!decodeAutomation(item.getProperty("panAutomation",{}),track.panAutomation,-1,1))return fail();track.automationMode=static_cast<int>(mode);}
        parsed.tracks.push_back(std::move(track));
    }
    std::vector<MixerRoute> routes;for(const auto& track:parsed.tracks)routes.push_back(track.routing);if(!validMixerRoutes(routes))return fail();
    if (static_cast<int>(version) >= 2)
    {
        const auto inserts = root.getProperty("inserts", {}); const auto* entries = inserts.getArray();
        if (!entries || entries->size() > 645) return fail();
        std::array<std::array<bool, 5>, 129> used {};
        for (const auto& item : *entries)
        {
            const auto channel = item.getProperty("channel", {}), slot = item.getProperty("slot", {});
            const auto description = item.getProperty("description", {}), state = item.getProperty("state", {}), bypass = item.getProperty("bypass", {});
            if (!channel.isInt() || !slot.isInt() || !description.isString() || !state.isString() || !bypass.isBool()) return fail();
            const int c = static_cast<int>(channel), s = static_cast<int>(slot);
            if (c < 0 || c > 128 || (c != 128 && c >= static_cast<int>(parsed.tracks.size())) || s < 0 || s >= 5) return fail();
            if (used[static_cast<size_t>(c)][static_cast<size_t>(s)] || description.toString().length() > 32768 || state.toString().length() > 16 * 1024 * 1024) return fail();
            auto xml = juce::parseXML(description.toString()); juce::MemoryBlock decoded;
            if (!xml || !xml->hasTagName("PLUGIN") || !decoded.fromBase64Encoding(state.toString())) return fail();
            if(static_cast<int>(version)>=8){std::vector<PluginAutomation::Lane> lanes;if(!PluginAutomation::decode(item.getProperty("automation",{}),lanes))return fail();}
            used[static_cast<size_t>(c)][static_cast<size_t>(s)] = true; parsed.inserts.add(item);
        }
    }
    output = std::move(parsed); return juce::Result::ok();
}
inline juce::Result read(const juce::File& file, SavedSession& result)
{
    if (!file.existsAsFile() || file.getSize() > 64 * 1024 * 1024) return juce::Result::fail("Session is missing or exceeds 64 MiB.");
    juce::var root; const auto parsed = juce::JSON::parse(file.loadFileAsString(), root);
    if (parsed.failed()) return juce::Result::fail("The session file is not valid JSON.");
    return decode(root, file, result);
}
inline juce::Result write(const juce::File& file, const juce::var& document)
{
    if (file.isDirectory()) return juce::Result::fail("Choose a session filename, not a folder.");
    SavedSession validation;
    if (decode(document, file, validation).failed()) return juce::Result::fail("Session data or plugin state exceeds the supported limits. Previous session file kept.");
    const auto text = juce::JSON::toString(document, false);
    if (text.getNumBytesAsUTF8() > 64 * 1024 * 1024) return juce::Result::fail("Session metadata exceeds 64 MiB.");
    // Commit through a sibling temporary file; a failed write preserves the old session.
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text) || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not save the session. Check the folder is writable.");
    return juce::Result::ok();
}
}
