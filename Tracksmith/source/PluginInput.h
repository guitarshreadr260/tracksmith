#pragma once
namespace PluginInput {
constexpr int base=1024,channels=64,tracks=128,limit=base+channels*tracks;
inline bool encoded(int value){return value>=base&&value<limit;}
inline int track(int value){return (value-base)/channels;}
inline int channel(int value){return (value-base)%channels;}
inline int encode(int source,int first){return base+source*channels+first;}
inline int remap(int value,int removed){if(!encoded(value))return value;const int source=track(value);return source==removed?-1:encode(source>removed?source-1:source,channel(value));}
}
