#pragma once
#include "MidiLane.h"
#include <algorithm>
// Normalize imported retriggers before committing. Existing notes stay untouched.
inline bool appendMidiGroove(juce::InputStream& stream,double beat,std::vector<MidiNote>& notes)
{
 if(!std::isfinite(beat)||beat<0||beat>=100000)return false;
 juce::MidiFile midi;if(!midi.readFrom(stream)||midi.getTimeFormat()<=0)return false;
 std::vector<MidiNote> incoming;
 for(int lane=0;lane<midi.getNumTracks();++lane){juce::MidiMessageSequence sequence(*midi.getTrack(lane));sequence.updateMatchedPairs();for(int i=0;i<sequence.getNumEvents();++i){auto* event=sequence.getEventPointer(i);if(event->message.isNoteOn()){if(incoming.size()>=8192)return false;const auto& m=event->message;
 // JUCE orders note-offs before note-ons at the same tick; recognize an explicit zero gate.
 if(!event->noteOffObject&&!std::any_of(sequence.begin(),sequence.end(),[&](const auto* e){const auto& off=e->message;return off.isNoteOff()&&off.getNoteNumber()==m.getNoteNumber()&&off.getChannel()==m.getChannel()&&off.getTimeStamp()==m.getTimeStamp();}))return false;
 const double length=event->noteOffObject?(event->noteOffObject->message.getTimeStamp()-m.getTimeStamp())/midi.getTimeFormat():0;if(length<0)return false;MidiNote n{1,m.getNoteNumber(),m.getChannel(),beat+m.getTimeStamp()/midi.getTimeFormat(),std::max(1.0/midi.getTimeFormat(),length),m.getFloatVelocity()};if(!validMidiNote(n))return false;incoming.push_back(n);}}}
 if(incoming.empty())return false;
 std::sort(incoming.begin(),incoming.end(),[](const auto& a,const auto& b){if(a.start!=b.start)return a.start<b.start;if(a.pitch!=b.pitch)return a.pitch<b.pitch;return a.channel<b.channel;});
 std::vector<MidiNote> clean;
 for(const auto& n:incoming){if(!clean.empty()&&clean.back().pitch==n.pitch&&clean.back().channel==n.channel&&std::abs(clean.back().start-n.start)<1.e-9){clean.back().length=std::max(clean.back().length,n.length);clean.back().velocity=std::max(clean.back().velocity,n.velocity);}else clean.push_back(n);}
 // A new hit closes the preceding gate on the same pitch/channel, retaining every distinct attack.
 std::array<int,128*16> previous;previous.fill(-1);
 for(size_t i=0;i<clean.size();++i){auto& n=clean[i];auto& index=previous[static_cast<size_t>((n.channel-1)*128+n.pitch)];if(index>=0){auto& last=clean[static_cast<size_t>(index)];last.length=std::min(last.length,n.start-last.start);if(!validMidiNote(last))return false;}index=static_cast<int>(i);}
 auto candidate=notes;int id=0;for(const auto& n:notes)id=std::max(id,n.id);
 if(clean.size()+candidate.size()>MidiLane::maximumNotes)return false;
 for(auto n:clean){n.id=++id;if(!validMidiNote(n))return false;candidate.push_back(n);}
 std::sort(candidate.begin(),candidate.end(),[](const auto& a,const auto& b){return a.start<b.start;});
 for(size_t i=0;i<candidate.size();++i)for(size_t j=i+1;j<candidate.size();++j)if(candidate[i].pitch==candidate[j].pitch&&candidate[i].channel==candidate[j].channel&&candidate[j].start<candidate[i].start+candidate[i].length-1.e-8)return false;
 notes=std::move(candidate);return true;
}
