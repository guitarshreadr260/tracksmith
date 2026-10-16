#pragma once
#include "TrackPlaylist.h"
namespace GroupEditing {
struct Layout {int track=0,playlist=1;bool midi=false;std::vector<TimelineClip> clips;std::vector<MidiNote> notes;};
inline bool audioRange(std::vector<TimelineClip>& clips,const juce::String& action,double start,double end,double delta=0){
    if(!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(delta)||start<0||end<=start||end>ClipLayout::maximumSeconds||(action=="move"&&(start+delta<0||std::abs(delta)<1.e-10)))return false;
    if(action!="split"&&action!="delete"&&action!="move")return false;
    int next=1;for(const auto& c:clips)next=std::max(next,c.id+1);std::vector<TimelineClip> result,incoming;bool changed=false;
    for(const auto& c:clips){const double a=std::max(start,c.start),b=std::min(end,c.start+c.length);if(b<=a){result.push_back(c);continue;}
        if(action=="split"){
            double from=c.start;bool first=true;for(const auto edge:{start,end,c.start+c.length})if(edge>from+1.e-8&&edge<=c.start+c.length+1.e-8){auto part=ClipLayout::slice(c,from,edge,first?c.id:next++);result.push_back(part);changed=changed||!first;first=false;from=edge;}
        }else{changed=true;if(c.start<a)result.push_back(ClipLayout::slice(c,c.start,a,c.id));if(b<c.start+c.length)result.push_back(ClipLayout::slice(c,b,c.start+c.length,next++));if(action=="move"){auto moved=ClipLayout::slice(c,a,b,next++);moved.start+=delta;incoming.push_back(moved);}}
    }
    if(!changed)return false;result.insert(result.end(),incoming.begin(),incoming.end());if(result.size()>ClipLayout::maximumClips)return false;for(const auto& c:result)if(!ClipLayout::valid(c))return false;clips=std::move(result);return true;
}
inline bool midiRange(std::vector<MidiNote>& notes,const juce::String& action,double start,double end,double delta=0){
    if(!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(delta)||start<0||end<=start||end>100000||(action=="move"&&(start+delta<0||std::abs(delta)<1.e-10)))return false;
    if(action!="split"&&action!="delete"&&action!="move")return false;
    int next=1;for(const auto& n:notes)next=std::max(next,n.id+1);std::vector<MidiNote> result,incoming;bool changed=false;
    for(const auto& n:notes){const double a=std::max(start,n.start),b=std::min(end,n.start+n.length);if(b<=a){result.push_back(n);continue;}
        auto slice=[&](double from,double to,int id){auto part=n;part.id=id;part.start=from;part.length=to-from;return part;};
        if(action=="split"){double from=n.start;bool first=true;for(const auto edge:{start,end,n.start+n.length})if(edge>from+1.e-8&&edge<=n.start+n.length+1.e-8){result.push_back(slice(from,edge,first?n.id:next++));changed=changed||!first;first=false;from=edge;}}
        else{changed=true;if(n.start<a)result.push_back(slice(n.start,a,n.id));if(b<n.start+n.length)result.push_back(slice(b,n.start+n.length,next++));if(action=="move"){auto moved=slice(a,b,next++);moved.start+=delta;incoming.push_back(moved);}}
    }
    if(!changed)return false;result.insert(result.end(),incoming.begin(),incoming.end());if(result.size()>MidiLane::maximumNotes)return false;for(const auto& n:result)if(!validMidiNote(n))return false;notes=std::move(result);return true;
}
inline bool edit(std::vector<Layout>& layouts,const juce::String& action,double start,double end,double delta,double bpm){
    if(layouts.empty()||layouts.size()>128||!std::isfinite(bpm)||bpm<40||bpm>240)return false;
    auto result=layouts;bool any=false;
    for(size_t i=0;i<result.size();++i){auto& item=result[i];if(item.track<0||item.track>=128||item.playlist<=0)return false;for(size_t j=0;j<i;++j)if(result[j].track==item.track)return false;
        // Empty ranges on an individual track are allowed; invalid edits are not.
        const double scale=item.midi?bpm/60:1,a=start*scale,b=end*scale,d=delta*scale;
        if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(d)||a<0||b<=a||b>(item.midi?100000:ClipLayout::maximumSeconds)||(action=="move"&&(a+d<0||std::abs(d)<1.e-10)))return false;
        bool intersects=false;if(item.midi){for(const auto& n:item.notes)intersects=intersects||(n.start<b&&n.start+n.length>a);}else for(const auto& c:item.clips)intersects=intersects||(c.start<b&&c.start+c.length>a);
        if(!intersects)continue;
        if(action=="split"){bool needs=false;const auto cuts=[&](double from,double length){for(double edge:{a,b})if(edge>from+1.e-8&&edge<from+length-1.e-8)needs=true;};if(item.midi){for(const auto& n:item.notes)cuts(n.start,n.length);}else for(const auto& c:item.clips)cuts(c.start,c.length);if(!needs)continue;}
        bool change=item.midi?midiRange(item.notes,action,a,b,d):audioRange(item.clips,action,a,b,d);
        if(!change)return false;any=true;
    }
    if(!any)return false;layouts=std::move(result);return true;
}
}
