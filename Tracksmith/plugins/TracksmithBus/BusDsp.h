#pragma once
#include <cmath>
#include <algorithm>
namespace TracksmithBus {
struct Settings {float threshold=-12,attack=10,release=100,ratio=4,makeup=0,sidechain=0,mix=100;bool bypass=false,automatic=false;};
class Engine {double rate=48000,fast=0,slow=0,wet=1,trim=1,blend=1,hpMix=0,lastGr=0;double x[2]{},y[2]{};
public:
 void prepare(double sr){rate=std::clamp(sr,8000.,384000.);fast=slow=0;wet=trim=blend=1;hpMix=0;lastGr=0;x[0]=x[1]=y[0]=y[1]=0;}
 float reduction()const{return float(lastGr);}
 static double curve(double db,double threshold,double ratio){const double over=db-threshold,knee=6,slope=1-1/ratio;if(over<=-knee/2)return 0;if(over>=knee/2)return slope*over;return slope*(over+knee/2)*(over+knee/2)/(2*knee);}
 void process(float*const* channels,int count,int frames,const Settings& s){const auto valid=[](float v,float def,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):def;};
 const double threshold=valid(s.threshold,-12,-60,12),ratio=valid(s.ratio,4,1.5f,40),attack=std::exp(-1/(rate*valid(s.attack,10,.1f,30)*.001)),release=std::exp(-1/(rate*valid(s.release,100,100,1200)*.001)),autoFast=std::exp(-1/(rate*.05)),autoSlow=std::exp(-1/(rate*.6)),slowCharge=std::exp(-1/(rate*.2)),filter=std::exp(-2*3.14159265358979323846*std::max(10.f,valid(s.sidechain,0,0,500))/rate),step=1/(rate*.005),smooth=1-std::exp(-1/(rate*.01));
 const double targetTrim=std::pow(10.,valid(s.makeup,0,0,24)/20),targetBlend=valid(s.mix,100,0,100)*.01;
 for(int n=0;n<frames;++n){double detector=0;hpMix+=std::clamp((s.sidechain>=10?1.:0.)-hpMix,-step,step);for(int c=0;c<std::min(2,count);++c){double dry=channels[c][n];if(!std::isfinite(dry))dry=channels[c][n]=0;double hp=filter*(y[c]+dry-x[c]);x[c]=dry;y[c]=hp;detector=std::max(detector,std::abs(dry+hpMix*(hp-dry)));}
 const double target=curve(20*std::log10(std::max(1.e-12,detector)),threshold,ratio);const double rc=s.automatic?autoFast:release;fast=target+(fast-target)*(target>fast?attack:rc);slow=target+(slow-target)*(target>slow?slowCharge:autoSlow);const double gr=s.automatic?.7*fast+.3*slow:fast;
 lastGr=gr;trim+=smooth*(targetTrim-trim);blend+=smooth*(targetBlend-blend);wet+=std::clamp((s.bypass?0.:1.)-wet,-step,step);double gain=1+wet*blend*(std::pow(10.,-gr/20)*trim-1);for(int c=0;c<std::min(2,count);++c)channels[c][n]*=float(gain);
 }
 }
};
}

