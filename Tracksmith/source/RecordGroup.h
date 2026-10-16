#pragma once
#include "Recorder.h"
struct RecordRoute { int track=0, input=0, channels=1; juce::File file; };
struct TrackTake { int track=0; RecordedTake take; };
// Fixed membership during capture; each disk queue is allocated before attachment.
class RecordGroup
{
public:
    static constexpr size_t maximumTracks=32;
    juce::Result start(const juce::File& file,double rate,int channels,uint64_t limit)
    { return startGroup({{0,0,channels,file}},rate,limit,false); }
    juce::Result startGroup(const std::vector<RecordRoute>& requested,double rate,uint64_t limit,bool routed=true)
    {
        if(isActive()||!allStopped())return juce::Result::fail("The previous takes are still finishing.");
        if(requested.empty()||requested.size()>maximumTracks)return juce::Result::fail("Arm between one and 32 audio tracks.");
        entries.clear();explicitRoutes=routed;
        for(const auto& route:requested)
        {
            if(route.track<0||route.track>=128||route.input<0||route.input>=InputRouting::maximumInputs||route.input+route.channels>InputRouting::maximumInputs||route.channels<1||route.channels>2)
                return juce::Result::fail("Invalid recording input routing.");
            for(const auto& e:entries)if(e.route.track==route.track||e.route.file==route.file)return juce::Result::fail("Duplicate recording destination.");
            entries.push_back({route,std::make_unique<Recorder>()});
        }
        for(auto& e:entries)
        {
            auto result=e.recorder->start(e.route.file,rate,e.route.channels,limit);
            if(result.failed()){requestStop();for(auto& started:entries)started.recorder->waitForFinish(10000);return result;}
        }
        return juce::Result::ok();
    }
    void capture(const float* const* inputs,int channels,int count,int first=0)noexcept
    {
        for(auto& e:entries)e.recorder->capture(inputs,channels,count,explicitRoutes?e.route.input:first);
        bool stopped=false;for(const auto& e:entries)stopped=stopped||!e.recorder->isActive();
        if(stopped)requestStop();
    }
    void requestStop()noexcept{for(auto& e:entries)e.recorder->requestStop();}
    void inputStopped()noexcept{for(auto& e:entries)e.recorder->inputStopped();}
    bool isActive()const noexcept{for(const auto& e:entries)if(e.recorder->isActive())return true;return false;}
    bool isFinished()const noexcept{if(entries.empty())return false;for(const auto& e:entries)if(!e.recorder->isFinished())return false;return true;}
    double elapsed()const noexcept{return entries.empty()?0:entries.front().recorder->elapsed();}
    juce::Array<juce::var> waveformPreviews()const
    {juce::Array<juce::var> result;for(const auto& e:entries){auto* o=new juce::DynamicObject();o->setProperty("track",e.route.track);o->setProperty("waveform",e.recorder->waveformPreview());result.add(juce::var(o));}return result;}
    RecordedTake takeResult(){return entries.empty()?RecordedTake{}:entries.front().recorder->takeResult();}
    std::vector<TrackTake> takeResults(){std::vector<TrackTake> result;for(auto& e:entries)result.push_back({e.route.track,e.recorder->takeResult()});return result;}
    bool waitForFinish(int ms){bool ok=true;for(auto& e:entries)ok=e.recorder->waitForFinish(ms)&&ok;return ok;}
private:
    bool allStopped()const{for(const auto& e:entries)if(!e.recorder->waitForFinish(0))return false;return true;}
    struct Entry{RecordRoute route;std::unique_ptr<Recorder> recorder;};
    std::vector<Entry> entries;
    bool explicitRoutes=false;
};
