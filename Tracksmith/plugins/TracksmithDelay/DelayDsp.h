#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace TracksmithDelay {
inline double safe(double v,double def,double lo,double hi){return std::isfinite(v)?std::clamp(v,lo,hi):def;}
struct Settings {float time=375,feedback=30,lowpass=18000,mix=25;bool bypass=false;};
class Engine {
 std::array<std::vector<double>,2> buffers;size_t position=0;double rate=48000,currentTime=18000,oldTime=18000,newTime=18000,fade=1,feedback=.3,mix=.25,wet=1,filter[2]{};
 double read(int ch,double delay)const{double p=double(position)-delay;const double length=double(buffers[ch].size());while(p<0)p+=length;const size_t a=size_t(p),b=(a+1)%buffers[ch].size();const double fraction=p-double(a);return buffers[ch][a]+fraction*(buffers[ch][b]-buffers[ch][a]);}
public:
 void prepare(double sr){rate=safe(sr,48000,8000,384000);for(auto& b:buffers)b.assign(size_t(rate*2.01)+4,0);position=0;currentTime=oldTime=newTime=rate*.375;fade=1;feedback=.3;mix=.25;wet=1;filter[0]=filter[1]=0;}
 void reset(){for(auto& b:buffers)std::fill(b.begin(),b.end(),0);position=0;filter[0]=filter[1]=0;}
 void process(float*const* channels,int count,int frames,const Settings& s){if(buffers[0].empty())return;const int channelCount=std::min(2,count);const double targetTime=safe(s.time,375,1,2000)*rate*.001,targetFeedback=safe(s.feedback,30,0,90)*.01,targetMix=safe(s.mix,25,0,100)*.01,coefficient=std::exp(-2*3.14159265358979323846*safe(s.lowpass,18000,1000,rate*.45)/rate),smooth=1-std::exp(-1/(rate*.01)),step=1/(rate*.005),fadeStep=1/(rate*.03);
 for(int n=0;n<frames;++n){if(fade>=1&&std::abs(targetTime-currentTime)>.1){oldTime=currentTime;newTime=targetTime;fade=0;}feedback+=smooth*(targetFeedback-feedback);mix+=smooth*(targetMix-mix);wet+=std::clamp((s.bypass?0.:1.)-wet,-step,step);
 for(int c=0;c<channelCount;++c){const double dry=std::isfinite(channels[c][n])?channels[c][n]:0;const double delayed=fade<1?read(c,oldTime)*(1-fade)+read(c,newTime)*fade:read(c,currentTime);filter[c]=delayed+coefficient*(filter[c]-delayed);buffers[c][position]=dry+feedback*filter[c];channels[c][n]=float(dry+wet*mix*(delayed-dry));}
 if(++position>=buffers[0].size())position=0;if(fade<1){fade=std::min(1.,fade+fadeStep);if(fade>=1)currentTime=newTime;}}
 }
};
}
