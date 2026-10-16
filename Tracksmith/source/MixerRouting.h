#pragma once
#include <array>
#include <vector>
#include <cmath>
struct MixerSend { int target=-1;float gain=0;bool pre=false; };
struct MixerRoute { bool aux=false;int output=-1;std::array<MixerSend,2> sends {};int hardwarePair=0; };
inline bool validMixerRoutes(const std::vector<MixerRoute>& routes)
{
    if(routes.empty()||routes.size()>128)return false;
    int auxiliaries=0;
    for(size_t id=0;id<routes.size();++id)
    {
        const auto& r=routes[id];if(r.aux&&++auxiliaries>16)return false;
        const auto valid=[&](int target){return target==-1||(target>=0&&target<static_cast<int>(routes.size())&&target!=static_cast<int>(id)&&routes[target].aux);};
        if(r.hardwarePair<0||r.hardwarePair>7||(!r.aux&&r.hardwarePair!=0))return false;
        if(!valid(r.output)||(r.aux&&r.output!=-1))return false;
        for(const auto& send:r.sends)
            if(!valid(send.target)||!std::isfinite(send.gain)||send.gain<0||send.gain>1||(r.aux&&send.target!=-1))return false;
    }
    return true; // Aux returns feed master only: no bus feedback or cycles.
}
