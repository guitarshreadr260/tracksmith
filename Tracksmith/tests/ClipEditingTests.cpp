#include "../source/PlaybackCore.h"
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
static bool near(float a,float b){return std::abs(a-b)<.00001f;}
int main()
{
    auto core=std::make_unique<PlaybackCore>();auto& t=core->tracks[0];t.sourceRate=1000;t.left.assign(2000,.2f);t.right=t.left;t.extra.push_back({std::vector<float>(1000,.8f),std::vector<float>(1000,.8f),1000});t.timelineEnabled=true;t.clips={{1,0,0,0,2}};
    const auto original=t.clips;require(ClipLayout::replaceRange(t.clips,.5,.7,{{1,1,.5,0,.2}}),"replace recorded range");
    require(t.clips.size()==3&&near(t.sample(.499,false),.2f)&&near(t.sample(.5,false),.8f)&&near(t.sample(.699,false),.8f)&&near(t.sample(.7,false),.2f),"new take replaces exact range and preserves surrounding audio");
    for(size_t a=0;a<t.clips.size();++a)for(size_t b=a+1;b<t.clips.size();++b)require(t.clips[a].id!=t.clips[b].id,"overwrite clip IDs remain unique");
    auto replacement=t.clips;t.clips=original;require(near(t.sample(.6,false),.2f),"undo can restore original take");t.clips=replacement;require(near(t.sample(.6,false),.8f),"redo restores replacement");
    require(ClipLayout::replaceRange(t.clips,.55,.6,{{1,0,.55,.55,.05}})&&near(t.sample(.54,false),.8f)&&near(t.sample(.56,false),.2f)&&near(t.sample(.61,false),.8f),"shorter second replacement preserves the rest of the earlier take");
    t.clips={{1,0,0,0,1}};require(ClipLayout::edit(t.clips,1,"fadeIn",.1,2)&&ClipLayout::edit(t.clips,1,"fadeOut",.1,2),"fade controls apply");
    require(near(t.sample(0,false),0)&&near(t.sample(.05,false),.1f)&&near(t.sample(.5,false),.2f)&&near(t.sample(.95,false),.1f),"linear fade samples have correct endpoints and midpoint");
    require(ClipLayout::edit(t.clips,1,"split",.06,2)&&near(t.sample(.05,false),.1f)&&near(t.sample(.075,false),.15f)&&near(t.sample(.95,false),.1f),"split preserves original fade envelope without restarting it");
    t.clips={{1,0,0,0,1},{2,1,1,0,1}};require(ClipLayout::crossfade(t.clips,2,.1,[&](int id){return t.sourceDuration(id);}),"crossfade creates overlap from right source handles");
    require(near(t.sample(1,false),.2f)&&near(t.sample(1.05,false),.5f)&&near(t.sample(1.1,false),.8f),"crossfade blends outgoing and incoming clips with complementary gains");
    t.clips={{1,1,0,0,1},{2,0,1,.2,.8}};require(ClipLayout::crossfade(t.clips,2,.1,[&](int id){return t.sourceDuration(id);})&&near(t.sample(.95,false),.5f),"crossfade uses left handles when outgoing source ends");
    t.clips={{1,1,0,0,1},{2,1,1,0,1}};require(!ClipLayout::crossfade(t.clips,2,.1,[&](int id){return t.sourceDuration(id);}),"missing source handles reject without fabricating audio");
    auto invalid=t.clips[0];invalid.fadeIn=.8;invalid.fadeOut=.5;require(!ClipLayout::valid(invalid),"invalid fade bounds reject");
    std::cout<<"Overwrite, fades, crossfades, handles and clip undo data tests passed.\n";
}
