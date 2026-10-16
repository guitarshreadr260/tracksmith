#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
struct TimelineClip
{
    int id = 0, source = 0;
    double start = 0, offset = 0, length = 0;
    double fadeIn=0,fadeOut=0,fadeOffset=0,fadeSpan=0;bool blend=false;
};
namespace ClipLayout
{
constexpr size_t maximumClips = 128, maximumSources = 32;
constexpr double maximumSeconds = 86400;
inline bool valid(const TimelineClip& c)
{ return c.id>0 && c.id<=1000000000 && c.source>=0 && c.source<static_cast<int>(maximumSources) && std::isfinite(c.start) && std::isfinite(c.offset) && std::isfinite(c.length) && c.start>=0 && c.offset>=0 && c.length>0 && c.start+c.length<=maximumSeconds && c.offset+c.length<=maximumSeconds && std::isfinite(c.fadeIn)&&std::isfinite(c.fadeOut)&&std::isfinite(c.fadeOffset)&&std::isfinite(c.fadeSpan)&&c.fadeIn>=0&&c.fadeOut>=0&&c.fadeOffset>=0&&c.fadeSpan>=0&&c.fadeSpan<=maximumSeconds&&c.fadeIn+c.fadeOut<=(c.fadeSpan>0?c.fadeSpan:c.length)+1.e-8&&(c.fadeSpan>0?c.fadeOffset+c.length<=c.fadeSpan+1.e-8:c.fadeOffset==0); }
inline TimelineClip slice(const TimelineClip& original,double start,double end,int id)
{
    auto c=original;const double delta=start-original.start;c.id=id;c.start=start;c.offset+=delta;c.length=end-start;
    c.fadeSpan=original.fadeSpan>0?original.fadeSpan:original.length;c.fadeOffset+=delta;return c;
}
inline float envelope(const TimelineClip& c,double time)
{
    const double x=time-c.start+c.fadeOffset,span=c.fadeSpan>0?c.fadeSpan:c.length;
    const double in=c.fadeIn>0?std::clamp(x/c.fadeIn,0.0,1.0):1,out=c.fadeOut>0?std::clamp((span-x)/c.fadeOut,0.0,1.0):1;
    return static_cast<float>(in*out);
}
inline bool replaceRange(std::vector<TimelineClip>& clips,double start,double end,const std::vector<TimelineClip>& incoming)
{
    if(!std::isfinite(start)||!std::isfinite(end)||start<0||end<=start)return false;
    int next=1;for(const auto& c:clips)next=std::max(next,c.id+1);std::vector<TimelineClip> result;
    for(const auto& c:clips){const auto stop=c.start+c.length;if(stop<=start||c.start>=end)result.push_back(c);else{if(c.start<start)result.push_back(slice(c,c.start,start,c.id));if(stop>end)result.push_back(slice(c,end,stop,next++));}}
    for(auto c:incoming){c.id=next++;if(!valid(c))return false;result.push_back(c);}
    if(result.size()>maximumClips)return false;clips=std::move(result);return true;
}
template<class Duration> inline bool crossfade(std::vector<TimelineClip>& clips,int id,double seconds,Duration duration)
{
    if(!std::isfinite(seconds)||seconds<=0)return false;auto selected=std::find_if(clips.begin(),clips.end(),[&](const auto& c){return c.id==id;});if(selected==clips.end())return false;
    auto previous=clips.end();for(auto c=clips.begin();c!=clips.end();++c)if(c->id!=id&&c->start<selected->start&&c->start+c->length>=selected->start-1.e-7&&(previous==clips.end()||c->start>previous->start))previous=c;
    if(previous==clips.end())return false;auto left=*previous,right=*selected;seconds=std::min({seconds,left.length,right.length});
    double begin=right.start,end=begin+seconds;
    if(left.offset+(end-left.start)<=duration(left.source)+1.e-8)left.length=end-left.start;
    else if(right.offset>=seconds && left.start+left.length<=right.start+1.e-7){end=left.start+left.length;begin=end-seconds;right.offset-=right.start-begin;right.length+=right.start-begin;right.start=begin;}
    else return false;
    for(const auto& c:clips)if(c.id!=left.id&&c.id!=right.id&&c.start<end&&c.start+c.length>begin)return false;
    left.fadeSpan=left.fadeOffset=right.fadeSpan=right.fadeOffset=0;left.fadeOut=seconds;left.fadeIn=std::min(left.fadeIn,left.length-seconds);right.fadeIn=seconds;right.fadeOut=std::min(right.fadeOut,right.length-seconds);right.blend=true;
    if(!valid(left)||!valid(right))return false;*previous=left;clips.erase(std::remove_if(clips.begin(),clips.end(),[&](const auto& c){return c.id==right.id;}),clips.end());clips.push_back(right);return true;
}
inline bool edit(std::vector<TimelineClip>& clips, int id, const char* operation, double position, int newId)
{
    auto found=std::find_if(clips.begin(),clips.end(),[id](const auto& c){return c.id==id;});
    if(found==clips.end() || !std::isfinite(position))return false;
    auto changed=*found;
    const std::string action(operation);
    if(action=="delete") {clips.erase(found);return true;}
    if(action=="move") changed.start=position;
    else if(action=="trimLeft")
    { if(position<changed.start){const auto delta=changed.start-position;changed.start=position;changed.offset-=delta;changed.length+=delta;changed.fadeSpan=0;changed.fadeOffset=0;changed.fadeIn=changed.fadeOut=0;}else changed=slice(changed,position,changed.start+changed.length,changed.id); }
    else if(action=="trimRight"){if(position>changed.start+changed.length){changed.length=position-changed.start;changed.fadeSpan=0;changed.fadeOffset=0;changed.fadeIn=changed.fadeOut=0;}else changed=slice(changed,changed.start,position,changed.id);}
    else if(action=="split")
    {
        const auto leftLength=position-changed.start;
        if(leftLength<=1.e-6 || leftLength>=changed.length-1.e-6 || clips.size()>=maximumClips || newId<=0)return false;
        auto right=slice(changed,position,changed.start+changed.length,newId);changed=slice(changed,changed.start,position,changed.id);if(!valid(right)||!valid(changed))return false;*found=changed;clips.push_back(right);return true;
    }
    else if(action=="fadeIn"||action=="fadeOut")
    { if(position<0||position>changed.length)return false;changed.fadeOffset=changed.fadeSpan=0;if(action=="fadeIn"){changed.fadeIn=position;changed.fadeOut=std::min(changed.fadeOut,changed.length-position);}else{changed.fadeOut=position;changed.fadeIn=std::min(changed.fadeIn,changed.length-position);} }
    else return false;
    if(!valid(changed) || changed.length<1.e-6)return false;
    if(changed.start==found->start && changed.offset==found->offset && changed.length==found->length && changed.fadeIn==found->fadeIn && changed.fadeOut==found->fadeOut && changed.fadeOffset==found->fadeOffset && changed.fadeSpan==found->fadeSpan)return false;
    *found=changed;return true;
}
}
