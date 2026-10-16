#include "../source/PlaybackCore.h"
#include <iostream>
#include <cstdlib>

static void require(bool condition, const char* message)
{ if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
static bool near(double a, double b, double tolerance = 1e-4) { return std::abs(a-b) < tolerance; }
struct InsertProbe { int largest = 0, trackCalls = 0, masterCalls = 0; };
static void insertProbe(void* context, size_t channel, float* l, float* r, int count, double, double, bool)
{
    auto& probe = *static_cast<InsertProbe*>(context); probe.largest = std::max(probe.largest, count);
    if (channel == 0) { ++probe.trackCalls; for (int n=0;n<count;++n) { l[n]+=0.2f;r[n]+=0.2f; } }
    if (channel == PlaybackCore::maxTracks) { ++probe.masterCalls; for (int n=0;n<count;++n) { l[n]+=0.1f;r[n]+=0.1f; } }
}
static void load(PlaybackCore& engine, size_t id, double rate, int frames, float l, float r)
{
    auto& track = engine.tracks[id]; track.sourceRate = rate; track.gain = 1.0f;
    track.left.assign(static_cast<size_t>(frames), l); track.right.assign(static_cast<size_t>(frames), r);
    engine.refreshDuration();
}
int main()
{
    float l[512], r[512]; float* outputs[] = {l,r};
    {
        PlaybackCore engine;
        load(engine,0,48000,48000,0.2f,0.1f); load(engine,1,44100,44100,0.3f,0.4f);
        engine.render(outputs,2,512); require(l[0]==0 && engine.seconds==0, "paused mix is silent");
        engine.playing=true;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.5) && near(r[511],0.5), "two WAVs sum with independent sample rates");
        require(near(engine.seconds,5120.0/48000.0,1e-8), "all tracks share the native clock");
        require(near(engine.tracks[0].peak,0.2) && near(engine.tracks[1].peak,0.4), "per-channel peaks remain independent");
        engine.tracks[0].muted=true;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.3) && near(r[511],0.4), "mute leaves other WAV audible");
        engine.tracks[0].muted=false; engine.tracks[0].solo=true;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.2) && near(r[511],0.1), "solo isolates the selected WAV");
        engine.tracks[1].solo=true;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.5) && near(r[511],0.5), "multiple soloed tracks mix together");
        engine.tracks[0].muted=true;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.3), "mute wins over solo");
        engine.tracks[0].muted=false; engine.tracks[0].solo=false; engine.tracks[1].solo=false;
        engine.tracks[127].solo=true; // Empty lane must not silence loaded WAVs.
        engine.tracks[0].pan=-1.0f; engine.tracks[1].pan=1.0f;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.2) && near(r[511],0.4), "independent stereo balance and empty-track solo");
        engine.masterGain=0.5f;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.1) && near(r[511],0.2), "master scales the complete mix");
        engine.playing=false; engine.seekSeconds=0.6; engine.render(outputs,2,512);
        require(near(engine.seconds,0.6,1e-8) && l[511]==0 && engine.peak==0, "paused seek is shared and meters clear");
    }
    {
        PlaybackCore engine;
        load(engine,0,48000,240,0.2f,0.2f); load(engine,1,48000,960,0.3f,0.3f);
        engine.playing=true; engine.render(outputs,2,512);
        require(engine.playing && l[511]>0.2f, "short WAV ending does not stop longer WAV");
        engine.render(outputs,2,512);
        require(!engine.playing && near(engine.seconds,0.02,1e-8) && l[511]==0, "transport stops at longest WAV and clears remainder");
        require(engine.tracks[0].peak==0, "finished track meter falls silent");
        engine.loop=true; engine.playing=true; engine.seekSeconds=0.019;
        engine.render(outputs,2,512);
        require(engine.playing && near(engine.seconds,0.0096666667,1e-8) && engine.tracks[0].peak>0, "session loop restarts all WAVs together");
        engine.loop=false; engine.playing=false; engine.seekSeconds=3.0;
        engine.render(outputs,2,512); require(near(engine.seconds,0.02,1e-8), "seek clamps to session duration");
    }
    {
        PlaybackCore engine; load(engine,0,44100,44100,0.9f,0.9f); load(engine,1,48000,48000,0.9f,0.9f);
        engine.playing=true;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(l[511]==1.0f && r[511]==1.0f && engine.clipping, "mix ceiling bounds output and reports clipping");
        engine.masterGain=0.25f;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.45) && !engine.clipping, "lower master restores unclipped mix");
        engine.tracks[1].left.clear(); engine.tracks[1].right.clear(); engine.refreshDuration(); engine.reset(); engine.playing=true;
        for(int i=0;i<10;++i) engine.render(outputs,2,512);
        require(near(l[511],0.225), "clearing a WAV preserves the other track");
    }
    {
        PlaybackCore engine; load(engine,0,48000,48000,0.2f,0.4f); engine.deviceRate=96000; engine.playing=true;
        float* sparse[]={nullptr,r,nullptr}; engine.render(sparse,3,512);
        require(near(engine.seconds,512.0/96000.0,1e-8) && std::isfinite(r[511]), "output-rate conversion and null device channels are safe");
        float* mono[]={l}; engine.render(mono,1,512); require(l[511]>0, "mono output downmixes the session");
    }
    {
        PlaybackCore engine; engine.metronome=true;engine.bpm=120;engine.playing=true;
        engine.render(outputs,2,512);
        require(engine.peak>0.1f && near(engine.seconds,512.0/48000.0,1e-8), "metronome runs from the native clock without WAVs");
        require(near(engine.duration(),32.0), "empty metronome session lasts 64 beats");
        engine.seekSeconds=0.1;engine.render(outputs,2,512);
        require(engine.peak==0, "seek between beats does not make an offbeat click");
        engine.seekSeconds=0.5;engine.render(outputs,2,512);
        const float ordinary=engine.peak.load();
        engine.seekSeconds=0.0;engine.render(outputs,2,512);
        require(engine.peak>ordinary, "first beat is accented");
        engine.metronome=false;engine.render(outputs,2,512);
        require(!engine.playing && engine.peak==0, "turning click off stops an empty transport");
        engine.metronome=true;engine.masterGain=0;engine.reset();engine.playing=true;engine.render(outputs,2,512);
        require(engine.peak==0, "master volume controls metronome too");
    }
    {
        PlaybackCore engine; load(engine,0,48000,480,0.3f,0.3f);load(engine,1,48000,480,0.2f,0.2f);
        engine.suppressRecordedPlayback=true;engine.recording=true;engine.recordTrack=1;engine.tracks[1].solo=true;engine.playing=true;engine.loop=true;
        engine.render(outputs,2,512);require(engine.playing&&engine.tracks[0].peak>0&&engine.tracks[1].peak==0, "recording excludes old target audio and its solo while backing plays");
        engine.render(outputs,2,512);require(engine.playing&&l[511]==0&&engine.seconds>0.02, "recording clock continues past backing EOF without looping");
        PlaybackCore empty;empty.recording=true;empty.playing=true;empty.render(outputs,2,512);
        require(empty.playing&&near(empty.seconds,512.0/48000.0,1e-8)&&l[511]==0,"recording without backing advances the clock silently");
    }
    {
        PlaybackCore engine; InsertProbe probe;
        engine.process=insertProbe;engine.processContext=&probe;
        load(engine,0,48000,48000,0.2f,0.2f);engine.tracks[0].gain=0.25f;engine.masterGain=0.5f;engine.playing=true;
        for(int i=0;i<10;++i)engine.render(outputs,2,512);
        require(near(l[511],0.15)&&near(r[511],0.15),"track inserts precede fader and master inserts follow master fader");
        require(probe.largest==256&&probe.trackCalls==20&&probe.masterCalls==20,"native insert calls use bounded 256-frame blocks");
        engine.render(outputs,2,117);require(near(l[116],0.15),"partial device block is processed without overrunning buffers");
        engine.playing=false;engine.render(outputs,2,512);require(l[511]==0,"paused insert chain is silent");
    }
    {
        PlaybackCore looped;load(looped,0,100,10,0,0);looped.deviceRate=100;looped.playbackLatencyFrames=4;looped.loop=true;looped.playing=true;
        looped.render(outputs,2,3);require(looped.seconds==0,"latency warm-up holds audible position at the start");
        looped.render(outputs,2,12);require(near(looped.seconds,0.01,1e-9),"compensated audible clock wraps with loop playback");
        looped.playing=false;looped.seekSeconds=0.05;looped.render(outputs,2,1);require(near(looped.seconds,0.05),"paused compensated seek displays requested position");
    }
    {
        PlaybackCore core;core.deviceRate=100;auto& track=core.tracks[0];track.left.resize(1000);track.right.resize(1000);for(int n=0;n<1000;++n)track.left[n]=track.right[n]=static_cast<float>(n)/1000;
        track.sourceRate=100;track.gain=1;core.refreshDuration();core.reset();track.smoothLeft=track.smoothRight=1;core.loop=true;core.loopRangeStart=2;core.loopRangeEnd=3;core.seekSeconds=2.9;core.playing=true;
        struct Segments {int count=0;std::array<double,8> time{};std::array<int,8> frames{};} segments;
        core.processContext=&segments;core.process=[](void* data,size_t channel,float*,float*,int frames,double seconds,double,bool){if(channel!=0)return;auto& s=*static_cast<Segments*>(data);s.time[s.count]=seconds;s.frames[s.count++]=frames;};
        float l[20],r[20];float* out[]{l,r};core.render(out,2,20);require(std::abs(l[0]-.29f)<1.e-5f&&std::abs(l[10]-.2f)<1.e-5f,"selection loop wraps to range start inside callback");
        require(segments.count==2&&segments.frames[0]==10&&segments.frames[1]==10&&std::abs(segments.time[1]-2)<1.e-8,"plugin/MIDI processing splits exactly at loop boundary");
    }
    std::cout << "All multitrack, metronome, insert routing and compensated clock tests passed.\n";
}
