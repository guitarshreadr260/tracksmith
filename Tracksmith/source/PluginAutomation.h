#pragma once
#include <juce_core/juce_core.h>
#include "Automation.h"

namespace PluginAutomation {
constexpr size_t maxLanes=64;
struct Lane {
    juce::String id,name;
    Automation::Lane points;
    bool read=true;int mode=1;
};
inline juce::Array<juce::var> encode(const std::vector<Lane>& lanes) {
    juce::Array<juce::var> result;
    for(const auto& lane:lanes){auto* item=new juce::DynamicObject();item->setProperty("id",lane.id);item->setProperty("name",lane.name);item->setProperty("read",lane.read);juce::Array<juce::var> points;for(auto p:lane.points){auto* point=new juce::DynamicObject();point->setProperty("time",p.time);point->setProperty("value",p.value);points.add(juce::var(point));}item->setProperty("points",points);result.add(juce::var(item));}
    return result;
}
inline bool decode(const juce::var& input,std::vector<Lane>& output) {
    const auto* entries=input.getArray();if(!entries||entries->size()>static_cast<int>(maxLanes))return false;
    std::vector<Lane> result;
    for(const auto& entry:*entries){Lane lane;auto id=entry.getProperty("id",{}),name=entry.getProperty("name",{}),read=entry.getProperty("read",{});if(!id.isString()||id.toString().isEmpty()||id.toString().length()>512||!name.isString()||name.toString().length()>512||!read.isBool())return false;lane.id=id.toString();lane.name=name.toString();lane.read=static_cast<bool>(read);lane.mode=lane.read?1:0;for(const auto& old:result)if(old.id==lane.id)return false;auto pointsVar=entry.getProperty("points",{});const auto* points=pointsVar.getArray();if(!points||points->size()>static_cast<int>(Automation::maxPoints))return false;for(const auto& p:*points){auto t=p.getProperty("time",{}),v=p.getProperty("value",{});auto numeric=[](const auto& x){return x.isInt()||x.isInt64()||x.isDouble();};if(!numeric(t)||!numeric(v))return false;lane.points.push_back({static_cast<double>(t),static_cast<float>(v)});}if(!Automation::valid(lane.points,0,1))return false;result.push_back(std::move(lane));}
    output=std::move(result);return true;
}
}
