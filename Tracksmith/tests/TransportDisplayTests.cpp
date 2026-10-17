#include "../source/PlaybackCore.h"
#include "../source/TransportSelection.h"
#include <iostream>
#include <stdexcept>
int main(){try{
 const auto p=TransportSelection::audition(8,12,120,1,2,30,48000);if(p.error||p.origin!=6||p.end!=16)throw std::runtime_error("pre/post roll bounds");
 const auto clipped=TransportSelection::audition(1,3,120,8,8,4,48000);if(clipped.error||clipped.origin!=0||clipped.end!=4)throw std::runtime_error("roll clamps to song");
 if(!TransportSelection::audition(2,1,120,0,0,30,48000).error||!TransportSelection::audition(1,2,120,9,0,30,48000).error)throw std::runtime_error("invalid roll plan accepted");
 auto core=std::make_unique<PlaybackCore>();auto& track=core->tracks[0];track.left.assign(48000,.2f);track.right=track.left;track.gain=1;core->refreshDuration();core->deviceRate=48000;core->playbackLatencyFrames=128;core->primeMixLevels();core->selectionPlaybackEnd=.25;core->seekSeconds=.1;core->playing=true;
 std::array<float,997> l{},r{};float* out[]{l.data(),r.data()};int blocks=0;while(core->playing&&blocks++<100)core->render(out,2,997);if(core->playing||std::abs(core->seconds.load()-.25)>1.e-8||blocks>10)throw std::runtime_error("selection playback exact endpoint with latency");
 core->selectionPlaybackEnd=.05;core->recording=true;core->seekSeconds=0;core->playing=true;for(int n=0;n<10;++n)core->render(out,2,997);if(!core->playing||core->seconds<=.1)throw std::runtime_error("selection endpoint leaked into recording");
 core->recording=false;core->offlineRenderEnd=.5;core->seekSeconds=0;core->playing=true;blocks=0;while(core->playing&&blocks++<100)core->render(out,2,997);if(std::abs(core->seconds.load()-.5)>1.e-8)throw std::runtime_error("selection endpoint leaked into export");
 core->reset();if(core->selectionPlaybackEnd!=-1)throw std::runtime_error("reset retains selection endpoint");
 std::cout<<"PASS: playback pre/post-roll bounds, invalid range rejection, exact selection end with latency, and no selection-end leakage into recording/export/reset.\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}return 0;}
