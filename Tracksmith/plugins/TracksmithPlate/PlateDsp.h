#pragma once
#include "../TracksmithDelay/DelayDsp.h"
namespace TracksmithPlate {
struct Settings {float decay=2.5f,predelay=20,damping=9000,lowcut=120,mix=25,width=100;bool bypass=false;};
struct Line {std::vector<double> data;size_t position=0;void prepare(size_t length){data.assign(std::max(size_t(2),length),0);position=0;}double read()const{return data[position];}void write(double x){data[position]=x;if(++position>=data.size())position=0;}double allpass(double x){const double y=read()-.65*x;write(x+.65*y);return y;}};
class Engine {
 std::array<Line,8> tank;std::array<std::array<Line,4>,2> diffusers;std::array<std::vector<double>,2> pre;size_t prePosition=0;double rate=48000,damped[8]{},previous[2]{},highpass[2]{},mix=.25,width=1,wet=1,decay=2.5,predelay=.02;
 double preRead(int c,double samples)const{double p=double(prePosition)-samples;const double len=double(pre[c].size());while(p<0)p+=len;const size_t a=size_t(p),b=(a+1)%pre[c].size();return pre[c][a]+(p-double(a))*(pre[c][b]-pre[c][a]);}
public:
 void prepare(double sr){rate=TracksmithDelay::safe(sr,48000,8000,384000);const int lengths[]{1423,1601,1867,1999,2131,2341,2593,2797};const int ap[2][4]{{225,397,607,829},{269,431,659,887}};for(int i=0;i<8;++i)tank[i].prepare(size_t(lengths[i]*rate/48000));for(int c=0;c<2;++c){for(int i=0;i<4;++i)diffusers[c][i].prepare(size_t(ap[c][i]*rate/48000));pre[c].assign(size_t(rate*.205)+4,0);previous[c]=highpass[c]=0;}std::fill_n(damped,8,0);prePosition=0;mix=.25;width=wet=1;decay=2.5;predelay=.02;}
 void reset(){for(auto& line:tank){std::fill(line.data.begin(),line.data.end(),0);line.position=0;}for(auto& channel:diffusers)for(auto& line:channel){std::fill(line.data.begin(),line.data.end(),0);line.position=0;}for(auto& b:pre)std::fill(b.begin(),b.end(),0);std::fill_n(damped,8,0);std::fill_n(previous,2,0);std::fill_n(highpass,2,0);prePosition=0;}
 void process(float*const* channels,int count,int frames,const Settings& s){if(pre[0].empty())return;using TracksmithDelay::safe;const double targetDecay=safe(s.decay,2.5,.2,10),targetPre=safe(s.predelay,20,0,200)*.001,targetMix=safe(s.mix,25,0,100)*.01,targetWidth=safe(s.width,100,0,100)*.01,damp=std::exp(-2*3.14159265358979323846*safe(s.damping,9000,1000,rate*.45)/rate),hp=std::exp(-2*3.14159265358979323846*safe(s.lowcut,120,20,800)/rate),smooth=1-std::exp(-1/(rate*.02)),step=1/(rate*.005);double feedback[8];for(int i=0;i<8;++i)feedback[i]=std::pow(10.,-3*double(tank[i].data.size())/(rate*targetDecay));
 for(int n=0;n<frames;++n){mix+=smooth*(targetMix-mix);width+=smooth*(targetWidth-width);wet+=std::clamp((s.bypass?0.:1.)-wet,-step,step);predelay+=smooth*(targetPre-predelay);double dry[2]{};for(int c=0;c<std::min(2,count);++c)dry[c]=std::isfinite(channels[c][n])?channels[c][n]:0;if(count==1)dry[1]=dry[0];double input[2]{};for(int c=0;c<2;++c){const double cut=hp*(highpass[c]+dry[c]-previous[c]);highpass[c]=cut;previous[c]=dry[c];pre[c][prePosition]=cut;input[c]=preRead(c,predelay*rate);for(auto& ap:diffusers[c])input[c]=ap.allpass(input[c]);}if(++prePosition>=pre[0].size())prePosition=0;
 double values[8],sum=0,left=0,right=0;const double ls[]{1,1,-1,-1,1,-1,1,-1},rs[]{1,-1,1,-1,-1,1,1,-1};for(int i=0;i<8;++i){double value=tank[i].read();damped[i]=value+damp*(damped[i]-value);values[i]=damped[i];sum+=values[i];left+=ls[i]*value;right+=rs[i]*value;}
 for(int i=0;i<8;++i){double injection=(input[0]+ls[i]*input[1])*.125;tank[i].write((.25*sum-values[i])*feedback[i]+injection);}
 left*=.5;right*=.5;double mid=(left+right)*.5,side=(left-right)*.5*width;left=mid+side;right=mid-side;if(count==1)left=(left+right)*.5;
 if(count>0)channels[0][n]=float(dry[0]+wet*mix*(left-dry[0]));if(count>1)channels[1][n]=float(dry[1]+wet*mix*(right-dry[1]));}
 }
};
}
