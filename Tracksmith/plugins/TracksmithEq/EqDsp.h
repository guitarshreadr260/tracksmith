#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>
namespace TracksmithEq {
constexpr double pi=3.14159265358979323846;
enum Kind { highPass, lowShelf, peak, highShelf, lowPass, notch };
struct Band {bool enabled=true;Kind kind=peak;double frequency=1000,gain=0,q=.70710678;int slope=1;};
inline std::array<Band,7> defaults(){return {{{false,highPass,40,0,.70710678,1},{true,lowShelf,100,0,.70710678,1},{true,peak,400,0,1,1},{true,peak,1000,0,1,1},{true,peak,3000,0,1,1},{true,highShelf,10000,0,.70710678,1},{false,lowPass,16000,0,.70710678,1}}};}
inline double safe(double value,double fallback,double low,double high){return std::isfinite(value)?std::clamp(value,low,high):fallback;}
struct Coeff {double b0=1,b1=0,b2=0,a1=0,a2=0;
 double magnitude(double hz,double rate)const{const auto z=std::polar(1.,-2*pi*hz/rate);return std::abs((b0+b1*z+b2*z*z)/(1.+a1*z+a2*z*z));}
};
struct Design {std::array<Coeff,2> sections{};int count=1;
 double magnitude(double hz,double rate)const{double result=1;for(int i=0;i<count;++i)result*=sections[i].magnitude(hz,rate);return result;}
};
inline Coeff biquad(Kind kind,double f,double gain,double q,double rate){
 f=safe(f,1000,10,rate*.475);q=safe(q,.70710678,.1,18);gain=safe(gain,0,-18,18);
 const double w=2*pi*f/rate,c=std::cos(w),s=std::sin(w),alpha=s/(2*q),A=std::pow(10.,gain/40);double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
 switch(kind){
 case highPass:b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
 case lowPass:b0=(1-c)/2;b1=1-c;b2=b0;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
 case notch:b0=1;b1=-2*c;b2=1;a0=1+alpha;a1=-2*c;a2=1-alpha;break;
 case peak:b0=1+alpha*A;b1=-2*c;b2=1-alpha*A;a0=1+alpha/A;a1=-2*c;a2=1-alpha/A;break;
 case lowShelf:{const double v=2*std::sqrt(A)*alpha;b0=A*((A+1)-(A-1)*c+v);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-v);a0=(A+1)+(A-1)*c+v;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-v;break;}
 case highShelf:{const double v=2*std::sqrt(A)*alpha;b0=A*((A+1)+(A-1)*c+v);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-v);a0=(A+1)-(A-1)*c+v;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-v;break;}
 }return {b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};
}
inline Coeff firstOrder(bool hp,double f,double rate){const double k=std::tan(pi*safe(f,1000,10,rate*.475)/rate),a=1/(1+k),b=hp?a:k*a;return{b,hp?-b:b,0,(k-1)*a,0};}
inline Design design(const Band& band,double rate){Design d;if(band.kind==highPass||band.kind==lowPass){const bool hp=band.kind==highPass;switch(std::clamp(band.slope,0,3)){case 0:d.sections[0]=firstOrder(hp,band.frequency,rate);break;case 1:d.sections[0]=biquad(band.kind,band.frequency,0,.7071067811865476,rate);break;case 2:d.count=2;d.sections[0]=biquad(band.kind,band.frequency,0,1,rate);d.sections[1]=firstOrder(hp,band.frequency,rate);break;case 3:d.count=2;d.sections[0]=biquad(band.kind,band.frequency,0,.541196100146197,rate);d.sections[1]=biquad(band.kind,band.frequency,0,1.306562964876377,rate);break;}}else d.sections[0]=biquad(band.kind,band.frequency,band.gain,band.q,rate);return d;}
struct State {double x1=0,x2=0,y1=0,y2=0;
 double process(double x,const Coeff& c){const double y=c.b0*x+c.b1*x1+c.b2*x2-c.a1*y1-c.a2*y2;x2=x1;x1=x;y2=y1;y1=std::abs(y)<1.e-30?0:y;return y;}
};
class Engine {
 struct Filter {Band current;Design coefficients;double mix=0;std::array<std::array<State,2>,2> state{};};
 std::array<Filter,7> filters;double rate=48000,input=1,output=1,wet=1,polarity=1;int tick=0;
 static double approach(double current,double target,double step){return current+std::clamp(target-current,-step,step);}
public:
 void prepare(double sampleRate,const std::array<Band,7>& bands,double inDb=0,double outDb=0,bool bypass=false,bool invert=false){rate=safe(sampleRate,48000,8000,384000);input=std::pow(10.,safe(inDb,0,-18,18)/20);output=std::pow(10.,safe(outDb,0,-18,18)/20);wet=bypass?0:1;polarity=invert?-1:1;tick=0;for(size_t b=0;b<7;++b){filters[b]={};filters[b].current=bands[b];filters[b].coefficients=design(bands[b],rate);filters[b].mix=bands[b].enabled?1:0;}}
 void reset(const std::array<Band,7>& bands,double inDb,double outDb,bool bypass,bool invert){prepare(rate,bands,inDb,outDb,bypass,invert);}
 void process(float* const* channels,int channelCount,int frames,const std::array<Band,7>& targets,double inDb,double outDb,bool bypass,bool invert){
  const double inTarget=std::pow(10.,safe(inDb,0,-18,18)/20),outTarget=std::pow(10.,safe(outDb,0,-18,18)/20),step=1/(rate*.005),smooth=1-std::exp(-1/(rate*.02)),controlSmooth=1-std::exp(-32/(rate*.02));
  for(int n=0;n<frames;++n){
   if(tick==0)for(size_t b=0;b<7;++b){auto& f=filters[b];const auto& target=targets[b];if((f.current.kind!=target.kind||f.current.slope!=target.slope)&&f.mix<=0){f.state={};f.current.kind=target.kind;f.current.slope=target.slope;}
    f.current.frequency+=controlSmooth*(safe(target.frequency,1000,20,20000)-f.current.frequency);f.current.gain+=controlSmooth*(safe(target.gain,0,-18,18)-f.current.gain);f.current.q+=controlSmooth*(safe(target.q,.707,.1,18)-f.current.q);f.coefficients=design(f.current,rate);
   }
   tick=(tick+1)%32;input+=smooth*(inTarget-input);output+=smooth*(outTarget-output);wet=approach(wet,bypass?0:1,step);polarity=approach(polarity,invert?-1:1,step*2);
   for(size_t b=0;b<7;++b){auto& f=filters[b];const auto& target=targets[b];const bool changing=f.current.kind!=target.kind||f.current.slope!=target.slope;f.mix=approach(f.mix,target.enabled&&!changing?1:0,step);}
   for(int ch=0;ch<std::min(2,channelCount);++ch){const double dry=std::isfinite(channels[ch][n])?channels[ch][n]:0;double value=dry*input;for(auto& f:filters){double filtered=value;for(int s=0;s<f.coefficients.count;++s)filtered=f.state[static_cast<size_t>(ch)][static_cast<size_t>(s)].process(filtered,f.coefficients.sections[static_cast<size_t>(s)]);value+=f.mix*(filtered-value);}const double result=dry+wet*(value*output*polarity-dry);channels[ch][n]=std::isfinite(result)?static_cast<float>(result):0;}
  }
 }
};
}
