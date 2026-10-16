#pragma once
#include "SessionFile.h"
#include <set>
inline bool editMidiBatch(std::vector<MidiNote>& destination,const juce::var& message)
{
 const auto action=message.getProperty("action",{}).toString();auto result=destination;std::set<int> seen;
 if(action=="delete"){const auto value=message.getProperty("ids",{});const auto* list=value.getArray();if(!list||list->isEmpty()||list->size()>2048)return false;for(const auto& id:*list){if(!id.isInt()||!seen.insert(static_cast<int>(id)).second)return false;auto at=std::find_if(result.begin(),result.end(),[&](const auto& n){return n.id==static_cast<int>(id);});if(at==result.end())return false;result.erase(at);}}
 else if(action=="add"||action=="update"){const auto value=message.getProperty("notes",{});const auto* list=value.getArray();if(!list||list->isEmpty()||list->size()>2048)return false;for(const auto& item:*list){MidiNote n;if(!SessionFile::decodeNote(item,n)||!seen.insert(n.id).second)return false;auto at=std::find_if(result.begin(),result.end(),[&](const auto& old){return old.id==n.id;});if(action=="add"){if(at!=result.end()||result.size()>=MidiLane::maximumNotes)return false;result.push_back(n);}else{if(at==result.end())return false;*at=n;}}}else return false;
 std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.start<b.start;});
 for(size_t a=0;a<result.size();++a)for(size_t b=a+1;b<result.size();++b)if(result[a].pitch==result[b].pitch&&result[a].channel==result[b].channel&&result[b].start<result[a].start+result[a].length-1.e-8)return false;
 destination=std::move(result);return true;
}
