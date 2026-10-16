#pragma once
#define createPluginFilter createGateTestFilter
#include "../plugins/TracksmithGate/GateProcessor.cpp"
#undef createPluginFilter
#include <iostream>
inline int runGateTests(const char* path,const char* preview){
 auto check=[](bool ok,const char* message){if(!ok){std::cerr<<"FAIL GATE: "<<message<<'\n';std::exit(1);}};
 using namespace TracksmithGate;
 for(double rate:{8000.,44100.,48000.,96000.,192000.}){
 Engine e;e.prepare(rate);Settings s;s.attack=.1f;s.hold=20;s.release=5;s.range=60;s.threshold=-30;
 float left[256]{},right[256]{};float* channels[]{left,right};auto signal=[&](float l,float r,int blocks){for(int b=0;b<blocks;++b){std::fill_n(left,256,l);std::fill_n(right,256,r);e.process(channels,2,256,s);for(float v:left)check(std::isfinite(v),"finite samples");}};
 signal(.5f,.25f,int(rate/256)+1);check(e.isOpen()&&std::abs(left[255]-.5f)<.001,"above threshold opens");check(std::abs(left[255]/right[255]-2)<.001,"stereo linked gain");
 signal(.001f,.0005f,int(rate/256)+1);check(!e.isOpen()&&e.reduction()>59.9f,"below threshold reaches range");check(std::abs(left[255]-.000001f)<1.e-7f,"range attenuation");
 s.bypass=true;signal(.1f,0,int(rate*.02/256)+2);check(std::abs(left[255]-.1f)<1.e-6f,"bypass unity");check(right[255]==0,"no crosstalk");
 }
 Engine e;e.prepare(48000);Settings s;s.threshold=-20;s.attack=.1f;s.hold=100;s.release=5;s.hysteresis=6;float a[480]{};float* mono[]{a};auto block=[&](float value){std::fill_n(a,480,value);e.process(mono,1,480,s);};block(.2f);for(int i=0;i<20;++i)block(.07f);check(e.isOpen(),"hysteresis prevents chatter");for(int i=0;i<5;++i)block(0);check(e.isOpen(),"hold retains open gate");for(int i=0;i<20;++i)block(0);check(!e.isOpen(),"hold expires");check(a[479]==0,"silence remains silence");
 juce::ScopedJuceInitialiser_GUI gui;juce::AudioPluginFormatManager formats;formats.addDefaultFormats();juce::OwnedArray<juce::PluginDescription> found;formats.getFormat(0)->findAllTypesForFile(found,juce::String::fromUTF8(path));check(found.size()==1,"VST3 discovery");juce::String error;auto plugin=formats.createPluginInstance(*found[0],48000,256,error);check(plugin!=nullptr,"VST3 load");check(plugin->getLatencySamples()==0,"zero latency");auto* parameter=plugin->getParameters()[0];parameter->setValueNotifyingHost(.25f);juce::MemoryBlock state;plugin->getStateInformation(state);parameter->setValueNotifyingHost(.5f);plugin->setStateInformation(state.getData(),int(state.getSize()));check(std::abs(parameter->getValue()-.25f)<.001,"state restore");
 GateProcessor native;native.prepareToPlay(48000,256);std::unique_ptr<juce::AudioProcessorEditor> editor(native.createEditorIfNeeded());editor->setVisible(true);juce::Image image(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true);{juce::Graphics g(image);editor->paintEntireComponent(g,true);}juce::FileOutputStream stream(juce::File(juce::String::fromUTF8(preview)));stream.setPosition(0);juce::PNGImageFormat png;check(png.writeImageToStream(image,stream),"preview render");stream.truncate();
 std::cout<<"PASS: gate opening, attenuation, range, hysteresis, hold, bypass, stereo link, silence, 5 sample rates, VST3 discovery/load, state recall and editor render\n";return 0;
}
