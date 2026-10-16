#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
struct MidiNote { int id=1,pitch=36,channel=10;double start=0,length=.25;float velocity=.85f; };
inline bool validMidiNote(const MidiNote& n){return n.id>0&&n.id<1000000000&&n.pitch>=0&&n.pitch<128&&n.channel>=1&&n.channel<=16&&std::isfinite(n.start)&&std::isfinite(n.length)&&std::isfinite(n.velocity)&&n.start>=0&&n.length>=1.e-5&&n.start+n.length<=100000&&n.velocity>0&&n.velocity<=1;}
struct MidiLane
{
 bool enabled=false;std::vector<MidiNote> notes;double expected=-1;
 static constexpr size_t maximumNotes=2048;
 double endBeat()const{double end=32;for(const auto& n:notes)end=std::max(end,n.start+n.length);return end;}
 void reset(){expected=-1;}
 void emit(juce::MidiBuffer& midi,double seconds,double bpm,double rate,int frames,bool playing)
 {
  midi.clear();if(!enabled)return;
  const bool discontinuity=expected<0||std::abs(seconds-expected)>2/rate;
  if((!playing&&expected!=-2)||(playing&&discontinuity)){for(int c=1;c<=16;++c)midi.addEvent(juce::MidiMessage::allNotesOff(c),0);}
  if(!playing){expected=-2;return;}
  const auto send=[&](const MidiNote& n,bool on,int frame){midi.addEvent(on?juce::MidiMessage::noteOn(n.channel,n.pitch,n.velocity):juce::MidiMessage::noteOff(n.channel,n.pitch),frame);};
  for(const auto& n:notes)
  {
   const auto start=n.start*60/bpm,end=(n.start+n.length)*60/bpm;
   if(discontinuity&&start<seconds-.5/rate&&end>seconds)send(n,true,0);
   const auto on=static_cast<int64_t>(std::llround((start-seconds)*rate)),off=static_cast<int64_t>(std::llround((end-seconds)*rate));
   if(on>=0&&on<frames)send(n,true,static_cast<int>(on));if(off>=0&&off<frames)send(n,false,static_cast<int>(off));
  }
  expected=seconds+frames/rate;
 }
};
