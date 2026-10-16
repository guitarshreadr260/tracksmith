#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace Automation
{
struct Point { double time=0;float value=0; };
using Lane=std::vector<Point>;
constexpr size_t maxPoints=4096;
constexpr double maxTime=86400;
inline bool valid(const Lane& lane,float low,float high)
{
    if(lane.size()>maxPoints)return false;double last=-1;
    for(const auto& p:lane){if(!std::isfinite(p.time)||!std::isfinite(p.value)||p.time<0||p.time>maxTime||p.time<=last||p.value<low||p.value>high)return false;last=p.time;}return true;
}
inline float value(const Lane& lane,double time,float fallback) noexcept
{
    if(lane.empty())return fallback;
    auto next=std::upper_bound(lane.begin(),lane.end(),time,[](double t,const Point& p){return t<p.time;});
    if(next==lane.begin())return next->value;if(next==lane.end())return lane.back().value;
    const auto& a=*std::prev(next);return a.value+(next->value-a.value)*static_cast<float>((time-a.time)/(next->time-a.time));
}
inline bool put(Lane& lane,Point point,float low,float high)
{
    if(!std::isfinite(point.time)||!std::isfinite(point.value)||point.time<0||point.time>maxTime||point.value<low||point.value>high)return false;
    auto next=std::lower_bound(lane.begin(),lane.end(),point.time,[](const Point& p,double t){return p.time<t;});
    if(next!=lane.end()&&std::abs(next->time-point.time)<1.e-8){*next=point;return true;}
    if(lane.size()>=maxPoints)return false;lane.insert(next,point);return true;
}
// Keep the original curve immediately outside the written interval.
inline bool replace(Lane& lane,const Lane& written,float fallback,float low,float high)
{
    if(written.empty()||!valid(written,low,high))return false;
    const double start=written.front().time,end=written.back().time,edge=.000001;
    Lane result;for(const auto& p:lane)if(p.time<start||p.time>end)result.push_back(p);
    if(start>0&&!put(result,{std::max(0.0,start-edge),value(lane,std::max(0.0,start-edge),fallback)},low,high))return false;
    if(end+edge<=maxTime&&!put(result,{end+edge,value(lane,end+edge,fallback)},low,high))return false;
    for(const auto& p:written)if(!put(result,p,low,high))return false;
    lane=std::move(result);return true;
}

// Modes: Off, Read, Write, Touch, Latch. Offline rendering always reads saved curves.
inline bool reads(int mode,bool touched,bool latched,bool bypass=false,bool offline=false) noexcept {
    return !bypass&&mode!=0&&(offline||mode==1||(mode==3&&!touched)||(mode==4&&!latched));
}
inline bool writes(int mode,bool touched,bool latched) noexcept {return mode==2||(mode==3&&touched)||(mode==4&&latched);}
struct Writer {
    std::vector<Lane> segments;bool active=false,failed=false;size_t count=0;float fallback=0;
    void sample(double time,float v,bool writing,float low,float high){
        if(failed)return;
        if(!writing){active=false;return;}
        if(!std::isfinite(time)||time<0||time>maxTime||!std::isfinite(v)||v<low||v>high){failed=true;return;}
        if(!active){if(!segments.empty()&&time<segments.back().back().time){failed=true;return;}if(segments.empty())fallback=v;if(segments.size()>=256){failed=true;return;}segments.emplace_back();active=true;}
        auto& lane=segments.back();if(!lane.empty()&&time<lane.back().time){failed=true;return;}
        if(!lane.empty()&&std::abs(time-lane.back().time)<1.e-8){lane.back().value=v;return;}
        if(lane.size()>1){const auto& a=lane[lane.size()-2];const auto& b=lane.back();const double predicted=a.value+(v-a.value)*(b.time-a.time)/(time-a.time);if(std::abs(predicted-b.value)<.000001){lane.back()={time,v};return;}}
        if(count>=maxPoints){failed=true;return;}lane.push_back({time,v});++count;
    }
    bool commit(Lane& destination,float low,float high)const {if(failed||segments.empty())return false;auto result=destination;for(const auto& lane:segments)if(!replace(result,lane,fallback,low,high))return false;destination=std::move(result);return true;}
};

struct Pass
{
    Lane volume,pan;float oldGain=.75f,oldPan=0;bool failed=false;
    bool sample(double time,float gain,float position)
    {
        if(failed)return false;
        if(!std::isfinite(time)||time<0||time>maxTime||(!volume.empty()&&time<volume.back().time)||!std::isfinite(gain)||!std::isfinite(position)||gain<0||gain>1||position< -1||position>1){failed=true;return false;}
        if(volume.empty()){oldGain=gain;oldPan=position;}
        const auto append=[&](Lane& lane,float v){
            if(!lane.empty()&&std::abs(lane.back().time-time)<1.e-8){lane.back().value=v;return true;}
            if(lane.size()>=2){const auto& a=lane[lane.size()-2];const auto& b=lane.back();const double predicted=a.value+(v-a.value)*(b.time-a.time)/(time-a.time);if(std::abs(predicted-b.value)<.000001){lane.back()={time,v};return true;}}
            if(lane.size()>=maxPoints)return false;lane.push_back({time,v});return true;
        };
        failed=!append(volume,gain)||!append(pan,position);return !failed;
    }
};

}
