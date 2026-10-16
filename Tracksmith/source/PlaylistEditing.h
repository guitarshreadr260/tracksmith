#pragma once
#include "TrackPlaylist.h"
namespace PlaylistEditing {
inline bool manage(std::vector<TrackPlaylist>& list,int& active,int id,const juce::String& action,juce::String name={}){
    auto result=list;auto found=std::find_if(result.begin(),result.end(),[id](const auto& p){return p.id==id;});if(found==result.end())return false;
    int selected=active;const auto index=static_cast<size_t>(std::distance(result.begin(),found));
    if(action=="up"||action=="down"){if((action=="up"&&index==0)||(action=="down"&&index+1==result.size()))return false;std::swap(result[index],result[action=="up"?index-1:index+1]);}
    else if(action=="delete"){if(result.size()<=1)return false;result.erase(found);if(id==active)selected=result[std::min(index,result.size()-1)].id;}
    else if(action=="switch"){if(active==id)return false;selected=id;}
    else if(action=="new"||action=="duplicate"){
        if(result.size()>=maximumPlaylists)return false;int next=1;for(const auto& p:result)next=std::max(next,p.id+1);if(next>1000000000)return false;
        name=name.trim().substring(0,64);if(name.isEmpty())name="Playlist "+juce::String(next);for(const auto& p:result)if(p.name.equalsIgnoreCase(name))return false;
        TrackPlaylist added;added.id=next;added.name=name;if(action=="duplicate"){added.clips=found->clips;added.notes=found->notes;}result.push_back(std::move(added));selected=next;
    }else if(action=="rename"){
        name=name.trim().substring(0,64);if(name.isEmpty()||name==found->name)return false;for(const auto& p:result)if(p.id!=id&&p.name.equalsIgnoreCase(name))return false;found->name=name;
    }else return false;
    list=std::move(result);active=selected;return true;
}
}
