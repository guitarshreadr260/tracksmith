#pragma once
#include "../TracksmithEq/EqDsp.h"
namespace TracksmithDeesser {
struct Settings {float frequency=6000,threshold=-30,reduction=12,release=60;bool bypass=false,automatic=true,listen=false;};
class Engine {double rate=48000,bandLevel=0,wideLevel=0,gain=1,wet=1,listen=0,frequency=6000;int tick=0;TracksmithEq::Coeff coeff;TracksmithEq::State states[2];public:
 void prepare(double sr){rate=TracksmithEq::safe(sr,48000,8000,384000);bandLevel=wideLevel=0;gain=wet=1;listen=0;frequency=std::min(6000.,rate*.4);tick=0;states[0]={};states[1]={};}
 float reduction()const{return float(-20*std::log10(std::max(gain,1.e-12)));}
 void process(float*const* channels,int count,int frames,const Settings& s){using TracksmithEq::safe;const double targetFrequency=safe(s.frequency,6000,2000,12000),threshold=std::pow(10.,safe(s.threshold,-30,-60,0)/20),maxReduction=safe(s.reduction,12,0,20),release=std::exp(-1/(rate*safe(s.release,60,10,200)*.001)),attack=std::exp(-1/(rate*.001)),detectorRelease=std::exp(-1/(rate*.03)),controlSmooth=1-std::exp(-32/(rate*.02)),step=1/(rate*.005);
 for(int n=0;n<frames;++n){if(tick==0){frequency+=controlSmooth*(std::min(targetFrequency,rate*.45)-frequency);double w=2*TracksmithEq::pi*frequency/rate,alpha=std::sin(w)/4,a0=1+alpha;coeff={alpha/a0,0,-alpha/a0,-2*std::cos(w)/a0,(1-alpha)/a0};}tick=(tick+1)%32;double bands[2]{},bandPeak=0,widePeak=0;for(int c=0;c<std::min(2,count);++c){if(!std::isfinite(channels[c][n]))channels[c][n]=0;const double dry=channels[c][n];bands[c]=states[c].process(dry,coeff);bandPeak=std::max(bandPeak,std::abs(bands[c]));widePeak=std::max(widePeak,std::abs(dry));}
 bandLevel=std::max(bandPeak,bandLevel*detectorRelease);wideLevel=std::max(widePeak,wideLevel*detectorRelease);const double trigger=s.automatic?std::max(1.e-7,wideLevel*.45):threshold;const double reduction=std::clamp(20*std::log10(std::max(1.e-12,bandLevel)/trigger),0.,maxReduction),target=std::pow(10.,-reduction/20);gain=target+(gain-target)*(target<gain?attack:release);wet+=std::clamp((s.bypass?0.:1.)-wet,-step,step);listen+=std::clamp((s.listen?1.:0.)-listen,-step,step);
 for(int c=0;c<std::min(2,count);++c){const double dry=channels[c][n],processed=dry-(1-gain)*bands[c],monitored=processed+listen*(bands[c]-processed);channels[c][n]=float(dry+wet*(monitored-dry));}}
 }
};
}
