#include "../source/InputMonitor.h"
#include <iostream>
#include <cstdlib>
#include <limits>
static void require(bool condition,const char* message){if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int main()
{
    float input1[512],input2[512],left[512],right[512];std::fill_n(input1,512,0.2f);std::fill_n(input2,512,0.6f);
    const float* inputs[]{nullptr,input1,nullptr,input2};float* outputs[]{left,right};
    auto selected=InputRouting::select(inputs,4,3,1);require(selected[0]==input2,"physical input 4 selected despite disabled channels");
    require(InputRouting::select(inputs,4,1,1)[0]==input1,"physical input 2 keeps its channel identity");
    require(!InputRouting::select(inputs,4,1,2)[0],"stereo routing cannot skip disabled physical channels");
    InputMonitor monitor;float peak=0;bool clipped=false;
    auto render=[&](int channels,float master=1.0f){std::fill_n(left,512,0);std::fill_n(right,512,0);peak=0;clipped=false;monitor.mix(selected,channels,outputs,2,512,48000,master,peak,clipped);};
    render(1);require(left[511]==0&&right[511]==0,"monitor off defaults to silence");
    monitor.enabled=true;monitor.gain=0.5f;for(int n=0;n<10;++n)render(1);
    require(std::abs(left[511]-0.3f)<0.0001f&&left[511]==right[511],"mono selected input reaches both outputs at monitor level");
    const float* stereoInputs[]{input1,input2};selected=InputRouting::select(stereoInputs,2,0,2);for(int n=0;n<10;++n)render(2);
    require(std::abs(left[511]-0.1f)<0.0001f&&std::abs(right[511]-0.3f)<0.0001f,"stereo monitor preserves channel identity");
    for(int n=0;n<10;++n)render(2,0.5f);require(std::abs(right[511]-0.15f)<0.0001f,"master gain affects live monitor");
    monitor.enabled=false;for(int n=0;n<10;++n)render(2);require(peak<0.00001f,"switching monitor off fades to silence");
    monitor.enabled=true;monitor.gain=1;for(int n=0;n<10;++n)render(2);
    std::fill_n(left,512,0.9f);std::fill_n(right,512,0.9f);peak=0;clipped=false;monitor.mix(selected,2,outputs,2,512,48000,1,peak,clipped);
    require(clipped&&right[511]==1,"monitor plus playback reports clipping and bounds output");
    input1[0]=std::numeric_limits<float>::quiet_NaN();require(InputRouting::sample(input1,0)==0,"nonfinite input sanitized");
    selected=InputRouting::select(inputs,4,1,2);require(selected[0]==input2&&!selected[1],"unavailable stereo pair reported");
    render(2);require(std::isfinite(left[0])&&std::isfinite(right[0]),"missing input remains safe");
    std::cout<<"Input routing and monitoring tests passed.\n";
}
