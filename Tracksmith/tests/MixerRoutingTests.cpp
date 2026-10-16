#include "../source/PlaybackCore.h"
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
static bool near(float a,float b){return std::abs(a-b)<.0001f;}
struct Probe{int auxCalls=0,masterCalls=0;bool nonlinear=false;static void process(void* context,size_t channel,float* l,float* r,int frames,double,double,bool){auto& p=*static_cast<Probe*>(context);if(channel==2){++p.auxCalls;for(int n=0;n<frames;++n){l[n]=p.nonlinear?l[n]*l[n]:l[n]*2;r[n]=p.nonlinear?r[n]*r[n]:r[n]*2;}}if(channel==PlaybackCore::maxTracks){++p.masterCalls;for(int n=0;n<frames;++n){l[n]*=.5f;r[n]*=.5f;}}}};
int main()
{
    auto core=std::make_unique<PlaybackCore>();Probe probe;core->process=Probe::process;core->processContext=&probe;
    for(int id=0;id<2;++id){auto& t=core->tracks[id];t.left.assign(48000,id==0?.1f:.2f);t.right=t.left;t.gain=1;}
    MixerRoute aux;aux.aux=true;core->setRouting(2,aux);core->tracks[2].gain=1;
    MixerRoute a,b;a.sends[0]={2,.5f};b.sends[0]={2,.25f};core->setRouting(0,a);core->setRouting(1,b);core->refreshDuration();
    float l[256],r[256];float* out[]{l,r};
    const auto render=[&]{core->reset();core->primeMixLevels();core->playing=true;probe.auxCalls=probe.masterCalls=0;core->render(out,2,256);};
    render();require(near(l[255],.25f)&&near(r[255],.25f),"two tracks share one post-fader aux send and master mix");require(probe.auxCalls==1&&probe.masterCalls==1,"aux and master process exactly once per block");
    core->tracks[0].gain=.5f;render();require(near(l[255],.2f),"post-fader send follows source gain");core->tracks[0].gain=1;
    core->tracks[0].muted=true;render();require(near(l[255],.15f),"source mute removes dry and sent signal");core->tracks[0].muted=false;
    core->tracks[2].muted=true;render();require(near(l[255],.15f),"aux mute leaves dry mix intact");core->tracks[2].muted=false;
    core->tracks[0].solo=true;render();require(near(l[255],.1f),"source solo retains its shared aux return");core->tracks[0].solo=false;
    core->tracks[2].solo=true;render();require(near(l[255],.1f),"aux solo auditions wet bus while preserving its feeders");core->tracks[2].solo=false;
    a.sends[0]={-1,0};b.sends[0]={-1,0};a.output=2;core->setRouting(0,a);core->setRouting(1,b);render();require(near(l[255],.2f),"bus output replaces source direct output");
    b.output=2;core->setRouting(1,b);render();require(near(l[255],.3f),"grouped output passes both tracks through bus inserts");
    probe.nonlinear=true;render();require(near(l[255],.045f),"shared nonlinear effect processes the summed bus once");probe.nonlinear=false;
    core->tracks[2].gain=.5f;render();require(near(l[255],.15f),"aux fader controls grouped signal");core->tracks[2].gain=1;
    core->tracks[2].pan=1;render();require(near(l[255],0)&&near(r[255],.3f),"aux pan preserves stereo channel routing");core->tracks[2].pan=0;
    std::vector<MixerRoute> routes{a,b,aux};require(validMixerRoutes(routes),"valid graph accepts");routes[0].output=1;require(!validMixerRoutes(routes),"audio-track output destinations reject");routes[0]=a;routes[2].output=2;require(!validMixerRoutes(routes),"self-feedback rejects");routes[2]=aux;routes[2].sends[0]={2,1};require(!validMixerRoutes(routes),"aux send feedback rejects");routes[2]=aux;routes[0].sends[0]={5,.5f};require(!validMixerRoutes(routes),"missing bus rejects");
    routes.assign(17,aux);require(!validMixerRoutes(routes),"aux limit validates");
    {
        struct Echo{std::array<float,8> ring{};int cursor=0;static void run(void* data,size_t channel,float* l,float* r,int frames,double,double,bool){auto& e=*static_cast<Echo*>(data);if(channel==2)for(int n=0;n<frames;++n){const float old=e.ring[e.cursor];e.ring[e.cursor]=l[n];e.cursor=(e.cursor+1)%8;l[n]=r[n]=old;}}};
        auto tail=std::make_unique<PlaybackCore>();Echo echo;tail->deviceRate=1000;tail->process=Echo::run;tail->processContext=&echo;
        auto& source=tail->tracks[0];source.sourceRate=1000;source.left.assign(100,.2f);source.right=source.left;source.gain=1;
        MixerRoute routed;routed.sends[0]={2,1};tail->setRouting(0,routed);tail->setRouting(2,aux);tail->tracks[2].gain=1;tail->refreshDuration();tail->reset();tail->primeMixLevels();tail->offlineSourceEnd=.01;tail->offlineRenderEnd=.03;tail->playing=true;
        float l[30],r[30];float* out[]{l,r};tail->render(out,2,30);
        require(near(l[9],.4f)&&near(l[10],.2f)&&near(l[17],.2f)&&near(l[18],0),"shared aux effect tail survives after offline source cutoff with exact boundaries");
    }

    {
        auto core=std::make_unique<PlaybackCore>();core->deviceRate=1000;
        for(size_t id=0;id<2;++id){auto& t=core->tracks[id];t.sourceRate=1000;t.left.assign(4000,id==0?.4f:.2f);t.right=t.left;t.gain=id==0?0.f:1.f;}
        MixerRoute source;source.sends[0]={2,1,true};core->setRouting(0,source);MixerRoute cue;cue.aux=true;cue.hardwarePair=1;core->setRouting(2,cue);core->tracks[2].gain=1;core->masterGain=.5f;core->refreshDuration();core->primeMixLevels();core->playing=true;
        float l[256],r[256],cueL[256],cueR[256];float* out[]{l,r,cueL,cueR};core->render(out,4,256);require(near(l[255],.1f)&&near(r[255],.1f)&&near(cueL[255],.4f)&&near(cueR[255],.4f),"pre-fader cue is independent of source/master faders and reaches only its hardware pair");
        core->tracks[0].muted=true;core->render(out,4,256);require(near(cueL[255],0),"pre-fader cue follows source mute");core->tracks[0].muted=false;source.sends[0].pre=false;core->setRouting(0,source);core->render(out,4,256);require(near(cueL[255],0),"post-fader send follows zero source fader");
        core->tracks[0].gain=1;core->tracks[0].pan=-1;core->render(out,4,256);require(near(cueL[255],.4f)&&near(cueR[255],0),"post-fader cue follows source pan");source.sends[0].pre=true;core->setRouting(0,source);core->render(out,4,256);require(near(cueL[255],.4f)&&near(cueR[255],.4f),"pre-fader cue is before source pan");
        core->playing=false;core->render(out,4,256);require(near(cueL[255],0)&&near(cueR[255],0),"unused hardware outputs are cleared when stopped");
        std::vector<MixerRoute> routes(3);routes[2]=cue;require(validMixerRoutes(routes),"hardware aux route validates");routes[0].hardwarePair=1;require(!validMixerRoutes(routes),"hardware destinations require an aux return");routes[0].hardwarePair=0;routes[2].hardwarePair=8;require(!validMixerRoutes(routes),"hardware output pair limit enforced");
    }

    {
        auto core=std::make_unique<PlaybackCore>();core->deviceRate=1000;auto& t=core->tracks[0];t.left.assign(1000,.3f);t.right=t.left;t.sourceRate=1000;t.gain=1;MixerRoute source;source.output=1;core->setRouting(0,source);MixerRoute cue;cue.aux=true;cue.hardwarePair=3;core->setRouting(1,cue);core->tracks[1].gain=1;core->refreshDuration();core->primeMixLevels();core->playing=true;float l[256],r[256],a[256],b[256];float* out[]{l,r,nullptr,nullptr,nullptr,nullptr,a,b};core->render(out,8,256);require(near(l[255],0)&&near(a[255],.3f)&&near(b[255],.3f),"hardware routing uses physical channel indices across disabled output holes");
    }
    {
        auto c=std::make_unique<PlaybackCore>();auto& t=c->tracks[0];t.left.assign(48000,.1f);t.right.assign(48000,.2f);t.gain=1;c->refreshDuration();c->primeMixLevels();c->playing=true;float l[256],r[256];float* out[]{l,r};c->render(out,2,256);
        require(near(l[255],.1f)&&near(r[255],.2f),"default stereo pans preserve output");require(near(t.peakLeft.load(),.1f)&&near(t.peakRight.load(),.2f),"independent native meter peaks");
        t.stereoLeft=1;t.stereoRight=-1;c->render(out,2,256);require(near(l[255],.2f)&&near(r[255],.1f),"independent stereo pans swap channels");
        t.stereoLeft=-1;t.stereoRight=-1;c->render(out,2,256);require(near(l[255],.3f)&&near(r[255],0),"both stereo channels pan left");
        c->playing=false;c->render(out,2,256);require(t.peakLeft.load()==0&&t.peakRight.load()==0,"stopped meters reset");
    }
    std::cout<<"Mixer routing, buses, post-fader sends and solo tests passed.\n";
}
