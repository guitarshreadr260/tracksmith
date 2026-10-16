#pragma once
#include <cmath>
#include <algorithm>
namespace TracksmithGate {
struct Settings {float threshold=-36,attack=2,hold=80,release=120,range=80,hysteresis=4,output=0;bool bypass=false;};
class Engine {double rate=48000,level=0,gain=1,wet=1,trim=1;int remaining=0;bool open=false;
public:
 void prepare(double sr){rate=std::clamp(sr,8000.,384000.);level=0;gain=1;wet=1;trim=1;remaining=0;open=false;}
 float reduction()const{return float(-20*std::log10(std::max(gain,1.e-10)));}
 bool isOpen()const{return open;}
 void process(float*const* channels,int count,int frames,const Settings& s){
 const auto valid=[](float v,float def,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):def;};
 const double threshold=std::pow(10.,valid(s.threshold,-36,-80,0)/20),closeLevel=threshold*std::pow(10.,-valid(s.hysteresis,4,0,12)/20),floor=std::pow(10.,-valid(s.range,80,0,100)/20);
 const double a=std::exp(-1/(rate*valid(s.attack,2,.1f,100)*.001)),r=std::exp(-1/(rate*valid(s.release,120,5,3000)*.001)),detectorRelease=std::exp(-1/(rate*.005)),trimTarget=std::pow(10.,valid(s.output,0,-18,18)/20),smooth=1-std::exp(-1/(rate*.01));const int hold=int(rate*valid(s.hold,80,0,1000)*.001);
 for(int n=0;n<frames;++n){double peak=0;for(int c=0;c<std::min(2,count);++c){if(!std::isfinite(channels[c][n]))channels[c][n]=0;peak=std::max(peak,double(std::abs(channels[c][n])));}level=std::max(peak,level*detectorRelease);
 if(level>=threshold){open=true;remaining=hold;}else if(open&&level<closeLevel){if(remaining>0)--remaining;else open=false;}
 const double target=open?1:floor;gain=target+(gain-target)*(target>gain?a:r);wet+=std::clamp((s.bypass?0.:1.)-wet,-1/(rate*.005),1/(rate*.005));trim+=smooth*(trimTarget-trim);
 for(int c=0;c<std::min(2,count);++c)channels[c][n]*=float(1+wet*(gain*trim-1));
 }
 }
};
}
