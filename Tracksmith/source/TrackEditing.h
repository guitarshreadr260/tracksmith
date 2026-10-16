#pragma once
#include "AudioEngine.h"
namespace TrackEditing {
inline int remap(int target,int removed){return target==removed?-1:target>removed?target-1:target;}
inline void moveTrack(PlaybackCore::Track& a,PlaybackCore::Track& b)
{
 a.left=std::move(b.left);a.right=std::move(b.right);a.sourceRate=b.sourceRate;a.extra=std::move(b.extra);a.clips=std::move(b.clips);a.timelineEnabled=b.timelineEnabled;
 a.volumeAutomation=std::move(b.volumeAutomation);a.panAutomation=std::move(b.panAutomation);a.automationMode=b.automationMode.load();a.volumeTouched=false;a.panTouched=false;a.volumeLatched=false;a.panLatched=false;
 a.routing=b.routing;for(size_t s=0;s<2;++s)a.sendLevels[s]=b.sendLevels[s].load();a.smoothSends={};a.midiTrack=b.midiTrack;a.midiSeconds=b.midiSeconds;a.gain=b.gain.load();a.pan=b.pan.load();a.peak=0;a.peakLeft=0;a.peakRight=0;a.stereoLeft=b.stereoLeft.load();a.stereoRight=b.stereoRight.load();a.muted=b.muted.load();a.solo=b.solo.load();a.smoothLeft=0;a.smoothRight=0;
}
inline bool remove(AudioEngine& e,int id,int count)
{
 if(count<=1||count>128||id<0||id>=count)return false;
 e.playback.playing=false;e.plugins.resetGraph();
 for(int i=id;i<count-1;++i){moveTrack(e.playback.tracks[i],e.playback.tracks[i+1]);e.plugins.channels[i]=std::move(e.plugins.channels[i+1]);e.plugins.midiLanes[i]=std::move(e.plugins.midiLanes[i+1]);e.trackInputs[i]=e.trackInputs[i+1].load();e.trackInputChannels[i]=e.trackInputChannels[i+1].load();e.armedMonitorMask[i]=e.armedMonitorMask[i+1].load();e.monitorOverrides[i]=e.monitorOverrides[i+1].load();e.monitorLevels[i]=e.monitorLevels[i+1].load();}
 PlaybackCore::Track empty;moveTrack(e.playback.tracks[count-1],empty);e.plugins.channels[count-1]={};e.plugins.midiLanes[count-1]={};e.trackInputs[count-1]=0;e.trackInputChannels[count-1]=1;e.armedMonitorMask[count-1]=false;e.monitorOverrides[count-1]=0;e.monitorLevels[count-1]=1;
 for(int i=0;i<count-1;++i){auto route=e.playback.routingSnapshot(i);route.output=remap(route.output,id);for(auto& send:route.sends){send.target=remap(send.target,id);if(send.target<0)send.gain=0;}e.playback.setRouting(i,route);e.plugins.midiLanes[i].reset();}
 const int monitor=e.monitor.track.load();if(monitor==id)e.monitor.enabled=false;e.monitor.track=std::max(0,remap(monitor,id));e.liveTrackSmooth.fill(0);e.playback.inputMonitorTracks.fill(false);e.playback.inputMonitorTrack=-1;e.plugins.monitorChannels.fill(false);e.plugins.monitorChannel=-1;
 e.playback.refreshDuration();e.playback.reset();return true;
}
}
