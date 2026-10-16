#include "../source/PlaybackCore.h"
#include <iostream>
#include <cstdlib>
static void require(bool value,const char* message){if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
static bool near(float a,float b,float tolerance=.0001f){return std::abs(a-b)<tolerance;}
int main()
{
    Automation::Lane lane{{0,.2f},{1,.8f}};
    require(near(Automation::value(lane,-1,1),.2f)&&near(Automation::value(lane,.5,1),.5f)&&near(Automation::value(lane,2,1),.8f),"linear interpolation and endpoint holds");
    require(!Automation::valid({{1,.5f},{0,.5f}},0,1)&&!Automation::valid({{0,2}},0,1)&&!Automation::valid({{0,0},{0,1}},0,1),"invalid order, range and duplicate times reject");
    auto original=lane;require(Automation::replace(lane,{{.25,.1f},{.75,.9f}},.4f,0,1),"write replaces interval");
    require(near(Automation::value(lane,.1,0),Automation::value(original,.1,0))&&near(Automation::value(lane,.9,0),Automation::value(original,.9,0))&&near(Automation::value(lane,.5,0),.5f),"write preserves outside curve and interpolates new pass");
    lane=original;require(!Automation::replace(lane,{{.2,2}},0,0,1)&&lane.size()==2&&lane[1].value==.8f,"invalid pass leaves original unchanged");
    Automation::Pass pass;for(int i=0;i<10000;++i)require(pass.sample(i*.01,.3f,.1f),"long held fader stays bounded");require(pass.volume.size()==2&&pass.pan.size()==2&&std::abs(pass.volume.back().time-99.99)<1.e-8,"constant capture simplifies without losing endpoints");
    require(pass.sample(100,.7f,-.4f)&&pass.volume.size()==3,"movement retains prior plateau");require(pass.sample(100,.8f,-.5f)&&pass.volume.size()==3&&pass.volume.back().value==.8f,"same-clock control event replaces current point");
    Automation::Pass overflow;for(size_t i=0;i<Automation::maxPoints;++i)require(overflow.sample(i*.01,i%2?.2f:.8f,0),"capture fits exact limit");require(!overflow.sample(99,.3f,0)&&overflow.failed,"capture overflow flags whole pass for retention of previous curve");
    Automation::Pass backwards;require(backwards.sample(1,.3f,0)&&!backwards.sample(.5,.4f,0),"backward seek does not create invalid pass");
    auto core=std::make_unique<PlaybackCore>();core->deviceRate=1000;auto& t=core->tracks[0];t.sourceRate=1000;t.left.assign(2000,.5f);t.right=t.left;t.gain=.9f;t.pan=0;t.volumeAutomation={{0,.2f},{.5,.2f},{.6,.8f},{2,.8f}};t.panAutomation={{0,-1},{1,-1},{1.1,1},{2,1}};t.automationMode=1;core->refreshDuration();core->primeMixLevels();core->playing=true;
    float left[256]{},right[256]{};float* outputs[]{left,right};core->render(outputs,2,256);require(near(left[0],.1f)&&near(right[0],0),"automation applies to first primed sample");
    for(int i=0;i<4;++i)core->render(outputs,2,256);require(near(left[255],0)&&near(right[255],.4f),"volume and pan curves render source samples");
    core->playing=false;core->seekSeconds=.2;core->render(outputs,2,1);core->playing=true;core->render(outputs,2,256);require(near(left[255],.1f)&&near(right[255],0),"seek reads automation at absolute session time");
    t.automationMode=0;core->render(outputs,2,256);require(near(left[255],.45f)&&near(right[255],.45f),"Off restores manual controls");
    t.automationMode=2;core->render(outputs,2,256);require(near(left[255],.45f)&&near(right[255],.45f),"Write uses manual controls live");
    core->playing=false;core->seekSeconds=0;core->render(outputs,2,1);t.volumeAutomation={{0,.4f}};t.panAutomation={{0,0}};t.automationMode=1;
    MixerRoute route;route.sends[0]={1,.5f};core->setRouting(0,route);MixerRoute bus;bus.aux=true;core->setRouting(1,bus);auto& aux=core->tracks[1];aux.gain=1;aux.automationMode=1;aux.volumeAutomation={{0,.5f}};core->playing=true;core->primeMixLevels();core->render(outputs,2,256);require(near(left[255],.25f)&&near(right[255],.25f),"source automation feeds post-fader send and aux automation applies once");
    t.solo=true;t.muted=true;core->render(outputs,2,256);require(near(left[255],0)&&near(right[255],0),"automation cannot override mute");
    core->loop=true;core->loopRangeStart=.2;core->loopRangeEnd=.4;t.muted=false;t.solo=false;core->seekSeconds=.2;core->playing=true;core->render(outputs,2,256);require(std::abs(core->automationTime(.205,10)-.395)<1.e-8&&std::abs(core->automationTime(.2,200)-.2)<1.e-8,"latency-adjusted Read automation wraps within loop range, including a full-loop delay");

    {
        require(Automation::reads(3,false,false)&&!Automation::reads(3,true,false),"Touch reads until gesture and returns on release");
        require(Automation::reads(4,false,false)&&!Automation::reads(4,false,true),"Latch stops reading once touched until reset");
        require(!Automation::reads(1,false,false,true)&&Automation::reads(4,false,true,false,true),"global bypass and offline saved-curve policy");
        Automation::Writer touch;Automation::Lane curve{{0,.1f},{10,.9f}};auto original=curve;
        touch.sample(2,.3f,true,0,1);touch.sample(3,.4f,true,0,1);touch.sample(3,.4f,false,0,1);touch.sample(7,.7f,true,0,1);touch.sample(8,.8f,true,0,1);
        require(touch.commit(curve,0,1)&&near(Automation::value(curve,2.5,0),.35f)&&near(Automation::value(curve,7.5,0),.75f)&&near(Automation::value(curve,5,0),Automation::value(original,5,0)),"separate Touch intervals preserve the curve between gestures");
        Automation::Writer bad;bad.sample(2,.2f,true,0,1);bad.sample(1,.4f,true,0,1);auto before=curve;require(!bad.commit(curve,0,1)&&curve.size()==before.size(),"failed pass leaves curve unchanged");
        auto audio=std::make_unique<PlaybackCore>();auto& track=audio->tracks[0];audio->deviceRate=1000;track.sourceRate=1000;track.left.assign(3000,.5f);track.right=track.left;track.volumeAutomation={{0,.2f}};track.panAutomation={{0,0}};track.gain=.8f;track.automationMode=3;audio->refreshDuration();audio->primeMixLevels();audio->playing=true;
        float l[256],r[256];float* output[]{l,r};audio->render(output,2,256);require(near(l[255],.1f),"Touch plays saved curve before touching");track.volumeTouched=true;audio->render(output,2,256);require(near(l[255],.4f),"Touch moves manual fader without overriding untouched pan");track.volumeTouched=false;audio->render(output,2,256);require(near(l[255],.1f),"Touch releases to saved curve");track.automationMode=4;track.volumeLatched=true;audio->render(output,2,256);require(near(l[255],.4f),"Latch holds manual fader after release");audio->automationBypass=true;audio->render(output,2,256);require(near(l[255],.4f),"bypass uses manual fader");
    }

    std::cout<<"Automation curves, pass capture, bounds, sample playback, seek, modes and aux/send tests passed.\n";
}
