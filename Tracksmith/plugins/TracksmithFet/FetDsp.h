#pragma once
#include <cmath>
#include <algorithm>
namespace TracksmithFet {
struct Settings {float input=0,output=0,attack=400,release=200,mix=100;int ratio=0;bool bypass=false;};
class Engine {
 double rate=48000,gr=0,in=1,out=1,blend=1,wet=1;
public:
 void prepare(double sr){rate=std::isfinite(sr)?std::clamp(sr,8000.,384000.):48000;gr=0;in=out=blend=wet=1;}
 float reduction()const{return float(gr);}
 void process(float*const* channels,int count,int frames,const Settings& s){
 auto valid=[](float v,float def,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):def;};
 const double ratios[]{4,8,12,20,40};const int mode=std::clamp(s.ratio,0,4);const double ratio=ratios[mode],threshold=mode==4?-24:-18,slope=ratio-1;
 const double attack=std::exp(-1/(rate*valid(s.attack,400,20,800)*.000001)),release=std::exp(-1/(rate*valid(s.release,200,50,1100)*.001)),smooth=1-std::exp(-1/(rate*.005)),step=1/(rate*.005),input=std::pow(10.,valid(s.input,0,-24,36)/20),output=std::pow(10.,valid(s.output,0,-36,24)/20),mix=valid(s.mix,100,0,100)*.01;
 for(int n=0;n<frames;++n){in+=smooth*(input-in);out+=smooth*(output-out);blend+=smooth*(mix-blend);wet+=std::clamp((s.bypass?0.:1.)-wet,-step,step);double detector=0;
 for(int c=0;c<std::min(2,count);++c){if(!std::isfinite(channels[c][n]))channels[c][n]=0;detector=std::max(detector,double(std::abs(channels[c][n]))*in);}
 const double over=20*std::log10(std::max(1.e-12,detector))-threshold;
 // Implicit update of a feedback gain-control loop, avoiding a one-sample
 // high-ratio instability at the fastest attack setting.
 const double target=std::max(0.,over)*(1-1/ratio),coefficient=target>gr?attack:release;
 if(over<=0)gr*=coefficient;else gr=std::max(0.,(coefficient*gr+(1-coefficient)*slope*over)/(1+(1-coefficient)*slope));
 const double gain=std::pow(10.,-gr/20)*in*out;
 for(int c=0;c<std::min(2,count);++c){double dry=channels[c][n];channels[c][n]=float(dry*(1+wet*blend*(gain-1)));}
 }
 }
};
}
