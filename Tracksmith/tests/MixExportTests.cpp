#include "../source/MixExport.h"
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
static std::unique_ptr<juce::AudioFormatReader> read(const juce::File& f){juce::WavAudioFormat wav;return std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(f.createInputStream().release(),true));}
struct Effect
{
    std::array<float,17> delay{}; int index=0; bool stopped=false;
    static void process(void* context,size_t channel,float* l,float* r,int count,double,double,bool playing)
    {
        auto& fx=*static_cast<Effect*>(context);
        if(channel==0){fx.stopped=fx.stopped||!playing;for(int n=0;n<count;++n){l[n]*=.5f;r[n]*=.5f;}}
        if(channel==PlaybackCore::maxTracks)for(int n=0;n<count;++n){const float old=fx.delay[fx.index];fx.delay[fx.index]=l[n];fx.index=(fx.index+1)%17;l[n]=r[n]=old;}
    }
};
int main()
{
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("work/export-tests");require(folder.createDirectory().wasOk(),"test folder");
    auto core=std::make_unique<PlaybackCore>();core->deviceRate=48000;
    auto& t=core->tracks[0];t.left.assign(48000,.5f);t.right.assign(48000,.25f);t.gain=1;t.pan=0;
    core->refreshDuration();core->metronome=true;core->loop=true;core->inputMonitorTrack=0;core->seconds=.2;
    auto file=folder.getChildFile("stereo.wav");
    auto result=writeMixWav(*core,file,{0,1,0,24},[]{return false;},{});
    require(result.error.isEmpty()&&result.frames==48000,"full session exact frame count");
    auto reader=read(file);require(reader&&reader->numChannels==2&&reader->sampleRate==48000&&reader->bitsPerSample==24,"stereo 24-bit WAV metadata");
    juce::AudioBuffer<float> audio(2,48000);require(reader->read(&audio,0,48000,0,true,true),"read exported stereo");
    require(std::abs(audio.getSample(0,0)-.5f)<1.e-5f&&std::abs(audio.getSample(1,47999)-.25f)<1.e-5f,"exact stereo values, no click or start ramp");reader.reset();
    require(core->loop&&core->metronome&&core->inputMonitorTrack==0&&!core->playing&&core->offlineRenderEnd<0&&core->seekSeconds==.2,"transport and monitor restored, stopped");
    t.pan=1;
    result=writeMixWav(*core,file,{.123,.456,.01,16},[]{return false;},{});
    reader=read(file);require(reader&&reader->lengthInSamples==16464&&reader->bitsPerSample==16,"selection plus tail exact frames");
    audio.setSize(2,16464);reader->read(&audio,0,16464,0,true,true);
    require(std::abs(audio.getSample(0,100))<1.e-6f&&std::abs(audio.getSample(1,100)-.25f)<1.e-4f,"pan preserved");
    require(std::abs(audio.getSample(1,15983)-.25f)<1.e-4f&&std::abs(audio.getSample(1,15984))<1.e-6f,"selection cuts at exact sample, tail silent for dry audio");reader.reset();
    t.pan=0;Effect fx;core->process=Effect::process;core->processContext=&fx;core->playbackLatencyFrames=17;
    result=writeMixWav(*core,file,{0,1,0,24},[]{return false;},{});
    reader=read(file);audio.setSize(2,48000);reader->read(&audio,0,48000,0,true,true);
    require(result.frames==48000&&std::abs(audio.getSample(0,0)-.25f)<1.e-5f&&std::abs(audio.getSample(0,47999)-.25f)<1.e-5f,"track effect, master delay and latency trim preserve first/last samples");reader.reset();
    fx={};core->playbackLatencyFrames=0;
    result=writeMixWav(*core,file,{.1,.2,.01,24},[]{return false;},{});
    reader=read(file);audio.setSize(2,5280);reader->read(&audio,0,5280,0,true,true);
    require(fx.stopped&&std::abs(audio.getSample(0,4800)-.25f)<1.e-5f&&std::abs(audio.getSample(0,4817))<1.e-6f,"tail flushes delay after source cutoff and stops MIDI transport");reader.reset();
    {
        auto automated=std::make_unique<PlaybackCore>();automated->deviceRate=48000;auto& track=automated->tracks[0];track.sourceRate=48000;track.left.assign(48000,.5f);track.right=track.left;track.gain=1;track.volumeAutomation={{0,.2f},{.25,.2f},{.5,.8f},{1,.8f}};track.panAutomation={{0,-1},{.5,-1},{.75,1},{1,1}};track.automationMode=1;automated->refreshDuration();
        auto exported=writeMixWav(*automated,file,{0,1,0,24},{},{});auto rendered=read(file);juce::AudioBuffer<float> samples(2,48000);require(exported.error.isEmpty()&&rendered&&rendered->read(&samples,0,48000,0,true,true),"automation WAV renders");
        require(std::abs(samples.getSample(0,0)-.1f)<1.e-5f&&std::abs(samples.getSample(1,0))<1.e-6f,"export begins at automated fader values");
        require(std::abs(samples.getSample(0,47999))<1.e-5f&&std::abs(samples.getSample(1,47999)-.4f)<1.e-5f,"export ends at automated volume and pan");rendered.reset();
        auto selected=writeMixWav(*automated,file,{.8,.9,0,24},{},{});rendered=read(file);samples.setSize(2,4800);require(selected.error.isEmpty()&&rendered&&rendered->read(&samples,0,4800,0,true,true)&&std::abs(samples.getSample(1,200)-.4f)<1.e-4f,"selection export evaluates absolute automation time");rendered.reset();
        track.automationMode=2;selected=writeMixWav(*automated,file,{.8,.9,0,24},{},{});rendered=read(file);require(rendered&&rendered->read(&samples,0,4800,0,true,true)&&std::abs(samples.getSample(1,200)-.4f)<1.e-4f&&track.automationMode==2,"export reads armed Write curve without changing live mode");rendered.reset();
    }
    const auto bytes = [&] { juce::MemoryBlock block; file.loadFileAsData(block); return block; }; auto before=bytes();
    int blocks=0;result=writeMixWav(*core,file,{0,1,0,24},[&]{return ++blocks>3;},{});
    require(result.cancelled&&bytes()==before,"cancel leaves existing destination unchanged");
    result=writeMixWav(*core,file,{1,0,0,24},{},{});require(result.error.isNotEmpty()&&bytes()==before,"reject invalid range without overwriting");
    core->process=nullptr;t.muted=true;
    result=writeMixWav(*core,file,{0,1,0,24},{},{});reader=read(file);audio.setSize(2,48000);reader->read(&audio,0,48000,0,true,true);
    require(audio.getMagnitude(0,48000)==0,"muted track excluded");reader.reset();
    int validations=0;
    before=bytes();result=writeMixWav(*core,file,{0,1,0,24},{},{},[&]{return ++validations>2?juce::String("Latency changed"):juce::String();});
    require(result.error=="Latency changed"&&bytes()==before,"latency change aborts export without replacing destination");
    t.muted=false;core->tracks[1].left.assign(48000,.1f);core->tracks[1].right.assign(48000,.1f);core->tracks[1].solo=true;core->tracks[1].gain=1;
    result=writeMixWav(*core,file,{0,1,0,24},{},{});reader=read(file);reader->read(&audio,0,48000,0,true,true);
    require(std::abs(audio.getSample(0,200)-.1f)<1.e-5f,"solo excludes other track");reader.reset();
    result=writeMixWav(*core,folder.getChildFile("missing/blocked.wav"),{0,1,0,24},{},{});require(result.error.isNotEmpty(),"write failure reported");
    core->process=nullptr;core->tracks[1].solo=false;core->tracks[1].left.clear();core->tracks[1].right.clear();
    for(double rate : {44100.0,96000.0,192000.0})
    {
        core->deviceRate=rate;t.sourceRate=rate;t.left.assign(static_cast<size_t>(rate),.2f);t.right=t.left;core->refreshDuration();
        result=writeMixWav(*core,file,{.01,.1,.02,24},{},{});
        reader=read(file);require(reader&&reader->sampleRate==rate&&reader->lengthInSamples==static_cast<int64_t>(std::llround(rate*.11)),"current sample rate and range respected at 44.1/96/192 kHz");reader.reset();
    }
    core->deviceRate=48000;t.sourceRate=48000;t.left.assign(48000,2);t.right=t.left;core->refreshDuration();
    result=writeMixWav(*core,file,{0,1,0,24},{},{});require(result.clipped,"output ceiling reported");
    std::cout<<"WAV export tests passed\n";
}

