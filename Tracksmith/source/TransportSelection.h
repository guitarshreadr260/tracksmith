#pragma once
#include <algorithm>
#include <cmath>

// Transport start and record bounds are deliberately independent.
namespace TransportSelection {
inline bool hasRange(double start,double end,double rate){
    return std::isfinite(start)&&std::isfinite(end)&&std::isfinite(rate)&&rate>0&&start>=0&&end<=86400&&end-start>=1/rate;
}
inline double nudge(double marker,int direction,double bpm){
    if(!std::isfinite(marker)||!std::isfinite(bpm)||bpm<=0||(direction!=-1&&direction!=1))return marker;
    return std::clamp(marker+direction*4*60/bpm,0.,86400.); // Current session meter is 4/4.
}
struct Plan {
    bool punch=false;
    double origin=0,start=0,end=0;
    const char* error=nullptr;
};
inline Plan plan(bool dynamic,bool explicitPunch,double marker,double selectedStart,double selectedEnd,double insertion,double rate){
    Plan p;
    const bool range=hasRange(selectedStart,selectedEnd,rate);
    p.punch=explicitPunch||(dynamic&&range);
    if(explicitPunch&&!range){p.error="Highlight a non-empty range before punch recording.";return p;}
    p.start=p.punch?selectedStart:dynamic?marker:insertion;
    p.end=p.punch?selectedEnd:0;
    p.origin=p.punch?(dynamic?marker:selectedStart):p.start;
    if(!std::isfinite(p.origin)||!std::isfinite(p.start)||p.origin<0||p.start<0){p.error="Choose a valid playback start.";return p;}
    if(p.punch&&p.origin>p.start){p.error="Move the playback start before the highlighted punch range.";return p;}
    return p;
}
}
