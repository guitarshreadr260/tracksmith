#pragma once
#include "../plugins/TracksmithEq/EqProcessor.h"
#include "../source/PluginRack.h"
#include <iostream>
inline int runStockEqTests(const char* path,const char* preview){
 auto check=[](bool ok,const char* message){if(!ok){std::cerr<<"FAIL EQ: "<<message<<'\n';std::exit(1);}};
 using namespace TracksmithEq;auto bands=defaults();
 auto db=[](double v){return 20*std::log10(std::max(1.e-15,v));};
 for(auto kind:{highPass,lowPass})for(int slope=0;slope<4;++slope){Band b{true,kind,1000,0,.707,slope};check(std::abs(db(design(b,48000).magnitude(1000,48000))+3.0103)<.001,"all pass slopes are -3dB at cutoff");double stop=kind==highPass?100:10000;check(db(design(b,48000).magnitude(stop,48000))<-(slope+1)*18,"pass attenuation slope");}
 for(double gain:{-18.,-6.,6.,18.}){Band b{true,peak,1000,gain,1,1};check(std::abs(db(design(b,48000).magnitude(1000,48000))-gain)<.001,"bell centre gain");}
 check(design(Band{true,notch,1000,0,4,1},48000).magnitude(1000,48000)<1.e-10,"notch depth");
 check(db(design(Band{true,lowShelf,1000,12,.707,1},48000).magnitude(20,48000))>11.9,"low shelf gain");check(db(design(Band{true,highShelf,1000,-12,.707,1},48000).magnitude(20000,48000))<-11.9,"high shelf gain");
 for(double rate:{8000.,44100.,48000.,96000.,192000.}){Engine e;e.prepare(rate,bands);std::array<float,256> l{},r{};float* channels[]{l.data(),r.data()};for(int block=0;block<300;++block){auto target=bands;for(int b=0;b<7;++b){target[b].enabled=true;target[b].frequency=block%2?20:20000;target[b].gain=block%2?18:-18;target[b].q=18;target[b].slope=block%4;}for(int n=0;n<256;++n){l[n]=.001f*float(std::sin((block*256+n)*.03));r[n]=0;}e.process(channels,2,256,target,0,0,false,false);for(int n=0;n<256;++n){check(std::isfinite(l[n]),"extreme automated filters finite");check(r[n]==0,"stereo isolation");}}}
 Engine engine;engine.prepare(48000,bands);std::array<float,256> a{};a.fill(.1f);float* mono[]{a.data()};engine.process(mono,1,256,bands,0,0,false,false);check(std::abs(a[200]-.1f)<1.e-6,"default flat unity");engine.prepare(48000,bands,12,-6,true,true);a.fill(.1f);engine.process(mono,1,256,bands,12,-6,true,true);check(std::abs(a[200]-.1f)<1.e-6,"bypass excludes trims and polarity");engine.prepare(48000,bands,0,0,false,true);a.fill(.1f);engine.process(mono,1,256,bands,0,0,false,true);check(std::abs(a[200]+.1f)<1.e-6,"polarity inversion");
 juce::ScopedJuceInitialiser_GUI gui;auto rack=std::make_unique<PluginRack>();juce::OwnedArray<juce::PluginDescription> found;rack->formats.getFormat(0)->findAllTypesForFile(found,juce::String::fromUTF8(path));check(found.size()==1,"VST3 discovery");juce::String error;auto plugin=rack->formats.createPluginInstance(*found[0],48000,256,error);check(plugin!=nullptr,"VST3 instantiate");check(plugin->getLatencySamples()==0,"zero latency");check(plugin->getParameters().size()>=36,"automation parameters exposed");
 juce::AudioProcessorParameter* mf=nullptr;for(auto* parameter:plugin->getParameters())if(parameter->getName(100)=="MF Gain")mf=parameter;check(mf!=nullptr,"named automation parameter");mf->beginChangeGesture();mf->setValueNotifyingHost(.75f);mf->endChangeGesture();juce::MemoryBlock state;plugin->getStateInformation(state);mf->setValueNotifyingHost(.5f);plugin->setStateInformation(state.getData(),int(state.getSize()));check(std::abs(mf->getValue()-.75f)<.001,"VST3 state restore");plugin->setStateInformation("invalid",7);check(std::abs(mf->getValue()-.75f)<.001,"invalid state ignored");mf->setValueNotifyingHost(.5f);
 plugin->prepareToPlay(48000,256);juce::AudioBuffer<float> buffer(2,256);buffer.clear();juce::MidiBuffer midi;plugin->processBlock(buffer,midi);check(buffer.getMagnitude(0,256)==0,"silence remains silence");
 {std::unique_ptr<juce::AudioProcessorEditor> hosted(plugin->createEditorIfNeeded());check(hosted!=nullptr,"host editor creation");EqProcessor native;std::unique_ptr<juce::AudioProcessorEditor> editor(native.createEditorIfNeeded());check(editor!=nullptr,"native editor");editor->setVisible(true);juce::Image shot(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true);{juce::Graphics graphics(shot);editor->paintEntireComponent(graphics,true);}juce::FileOutputStream stream(juce::File(juce::String::fromUTF8(preview)));check(stream.openedOk(),"preview file");stream.setPosition(0);juce::PNGImageFormat png;check(png.writeImageToStream(shot,stream),"editor preview render");stream.truncate();}
 plugin->releaseResources();std::cout<<"PASS: EQ responses, slopes, extremes, stereo isolation, unity, bypass, polarity, VST3 discovery, state, silence and editor render\n";return 0;
}




