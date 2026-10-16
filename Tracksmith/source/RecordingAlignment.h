#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
namespace RecordingAlignment {
inline int correction(int input,int output,int plugins,bool enabled=true)noexcept {return enabled?static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(std::max(0,input))+std::max(0,output)+std::max(0,plugins),0,1048576)):0;}
inline int64_t delayed(int64_t position,int correction)noexcept {return position>std::numeric_limits<int64_t>::max()-correction?std::numeric_limits<int64_t>::max():position+correction;}
}
