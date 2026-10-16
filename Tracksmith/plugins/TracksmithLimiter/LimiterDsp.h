#pragma once
#include <cmath>
#include <algorithm>
namespace TracksmithLimiter {
struct Settings {float drive=0,ceiling=-1,release=100;bool bypass=false;};
class Engine {double rate=48000,gain=1,drive=1,wet=1;public:
 void prepare(double sr){rate=std::isfinite(sr)?std::clamp(sr,8000.,384000.):48000;gain=drive=wet=1;}
 float reduction()const{return float(-20*std::log10(std::max(gain,1.e-12)));}
 void process(float*const* channels,int count,int frames,const Settings& s){auto valid=[](float v,float def,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):def;};const double ceiling=std::pow(10.,valid(s.ceiling,-1,-24,0)/20),targetDrive=std::pow(10.,valid(s.drive,0,0,24)/20),release=std::exp(-1/(rate*valid(s.release,100,10,1000)*.001)),smooth=1-std::exp(-1/(rate*.005)),step=1/(rate*.005);
 for(int n=0;n<frames;++n){drive+=smooth*(targetDrive-drive);wet+=std::clamp((s.bypass?0.:1.)-wet,-step,step);double peak=0;for(int c=0;c<std::min(2,count);++c){if(!std::isfinite(channels[c][n]))channels[c][n]=0;peak=std::max(peak,std::abs(double(channels[c][n]))*drive);}const double target=peak>ceiling?ceiling/peak:1;gain=target<gain?target:target+(gain-target)*release;
 for(int c=0;c<std::min(2,count);++c){const double dry=channels[c][n],limited=std::clamp(dry*drive*gain,-ceiling,ceiling);channels[c][n]=float(dry+wet*(limited-dry));}}
 }
};
}
