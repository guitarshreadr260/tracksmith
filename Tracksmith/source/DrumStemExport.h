#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "MidiLane.h"
#include "PluginAutomation.h"
#include <functional>
struct StemOutput {int id=0,first=0,channels=2;juce::String name;};
struct StemResult {juce::String error;bool cancelled=false,clipped=false;int64_t frames=0;juce::File folder;juce::StringArray silent;};
namespace DrumStems {
inline std::vector<StemOutput> outputs(juce::AudioProcessor& plugin){
 auto layout=plugin.getBusesLayout();for(auto& bus:layout.inputBuses)bus=juce::AudioChannelSet::disabled();
 // Expose the instrument's native output buses only on the export instance.
 for(int i=0;i<layout.outputBuses.size();++i){const auto* bus=plugin.getBus(false,i);auto channels=bus->getLastEnabledLayout();if(channels.isDisabled())channels=bus->getDefaultLayout();if(channels.isDisabled())channels=juce::AudioChannelSet::stereo();layout.outputBuses.getReference(i)=channels;}
 if(!plugin.setBusesLayout(layout)||plugin.getTotalNumOutputChannels()>64)return {};
 std::vector<StemOutput> result;int first=0;
 for(int bus=0;bus<plugin.getBusCount(false);++bus){const auto* output=plugin.getBus(false,bus);const int channels=output->getNumberOfChannels();for(int c=0;c<channels;c+=2){const int width=std::min(2,channels-c);auto name=output->getName();if(channels>2)name+=" "+juce::String(c+1)+"-"+juce::String(c+width);result.push_back({static_cast<int>(result.size()),first+c,width,name});}first+=channels;}
 return result;
}
struct Clock:juce::AudioPlayHead {double time=0,bpm=120,rate=48000;bool playing=true;juce::Optional<PositionInfo> getPosition()const override{PositionInfo p;p.setTimeInSeconds(time);p.setTimeInSamples(static_cast<int64_t>(std::llround(time*rate)));p.setBpm(bpm);p.setTimeSignature(TimeSignature{4,4});p.setPpqPosition(time*bpm/60);p.setPpqPositionOfLastBarStart(std::floor(time*bpm/240)*4);p.setIsPlaying(playing);return p;}};
inline double endBeat(const std::vector<MidiNote>& notes){double end=0;for(auto n:notes)end=std::max(end,n.start+n.length);return std::ceil(end/4)*4;}
inline StemResult write(juce::AudioProcessor& plugin,const std::vector<MidiNote>& notes,const std::vector<StemOutput>& selected,double rate,double bpm,double tail,const juce::File& destination,const std::function<bool()>& cancelled,const std::function<void(double)>& progress){
 StemResult result;result.folder=destination;const auto stop=endBeat(notes)*60/bpm;
 if(notes.empty()||selected.empty()||selected.size()>64||!std::isfinite(rate)||rate<8000||rate>192000||!std::isfinite(bpm)||bpm<20||bpm>300||!std::isfinite(tail)||tail<0||tail>30||stop<=0||stop>3600||destination.exists()){result.error="Choose MIDI up to one hour, valid outputs and a new destination folder.";return result;}
 for(auto n:notes)if(!validMidiNote(n)){result.error="Invalid song MIDI.";return result;}
 int channels=0;for(size_t i=0;i<selected.size();++i){const auto& output=selected[i];if(output.first<0||output.channels<1||output.channels>2||output.first+output.channels>plugin.getTotalNumOutputChannels()){result.error="The instrument output layout changed. Prepare stems again.";return result;}for(size_t j=0;j<i;++j)if(selected[j].id==output.id){result.error="Duplicate stem output.";return result;}channels+=output.channels;}
 const auto frames=static_cast<int64_t>(std::llround((stop+tail)*rate));if(frames*6>0xffff0000LL||destination.getParentDirectory().getBytesFreeOnVolume()<frames*3*channels+1048576){result.error="Insufficient disk space or WAV size limit exceeded.";return result;}
 const auto staging=destination.getParentDirectory().getChildFile(".tracksmith-stems-"+juce::Uuid().toString());if(!staging.createDirectory().wasOk()){result.error="Cannot create the stem export folder.";return result;}
 struct Cleanup{juce::File folder;~Cleanup(){if(folder.getFileName().startsWith(".tracksmith-stems-")&&folder.getParentDirectory().isDirectory())folder.deleteRecursively();}}cleanup{staging};
 std::vector<std::unique_ptr<juce::AudioFormatWriter>> writers;juce::StringArray files;std::vector<double> peaks(selected.size(),0);juce::WavAudioFormat format;
 for(const auto& output:selected){const auto file=juce::String(output.id+1).paddedLeft('0',2)+" - "+juce::File::createLegalFileName(output.name).substring(0,80)+".wav";auto stream=staging.getChildFile(file).createOutputStream();std::unique_ptr<juce::OutputStream> base(std::move(stream));if(!base){result.error="Cannot create a stem WAV.";return result;}auto writer=format.createWriterFor(base,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(output.channels).withBitsPerSample(24));if(!writer){result.error="Cannot create a 24-bit stem writer.";return result;}files.add(file);writers.push_back(std::move(writer));}
 Clock clock;clock.bpm=bpm;clock.rate=rate;
 struct Release{juce::AudioProcessor& p;~Release(){p.releaseResources();p.setPlayHead(nullptr);p.setNonRealtime(false);}}release{plugin};
 try{
 plugin.setPlayHead(&clock);plugin.setNonRealtime(true);plugin.setRateAndBufferSizeDetails(rate,512);plugin.prepareToPlay(rate,512);plugin.reset();const int latency=plugin.getLatencySamples();if(latency<0||latency>65536)throw std::runtime_error("Unsupported instrument latency.");const auto sourceFrames=static_cast<int64_t>(std::llround(stop*rate));
 MidiLane midiLane;midiLane.enabled=true;midiLane.notes=notes;juce::MidiBuffer midi;juce::AudioBuffer<float> buffer(std::max(1,plugin.getTotalNumOutputChannels()),512);
 for(int64_t position=0;position<frames+latency;){if(cancelled&&cancelled()){result.cancelled=true;return result;}const int count=static_cast<int>(std::min<int64_t>(512,frames+latency-position));clock.time=position/rate;clock.playing=position<sourceFrames;buffer.clear();midiLane.emit(midi,clock.time,bpm,rate,count,clock.playing);plugin.processBlock(buffer,midi);if(plugin.getLatencySamples()!=latency)throw std::runtime_error("Instrument latency changed during rendering. Try again after it settles.");
  if(position+count>latency){const int skip=static_cast<int>(std::max<int64_t>(0,latency-position));for(size_t i=0;i<selected.size();++i){const auto& output=selected[i];const float* data[2]={buffer.getReadPointer(output.first)+skip,output.channels==2?buffer.getReadPointer(output.first+1)+skip:nullptr};for(int c=0;c<output.channels;++c)for(int n=0;n<count-skip;++n){const double value=data[c][n];if(!std::isfinite(value))throw std::runtime_error("Instrument returned invalid audio. Export discarded.");peaks[i]=std::max(peaks[i],std::abs(value));}if(!writers[i]->writeFromFloatArrays(data,output.channels,count-skip))throw std::runtime_error("Stem WAV write failed. Check disk space.");}result.frames+=count-skip;}
  position+=count;if(progress)progress(static_cast<double>(position)/(frames+latency));
 }
 for(auto& writer:writers)if(!writer->flush())throw std::runtime_error("Could not finish writing the stems.");writers.clear();
 auto* manifest=new juce::DynamicObject();manifest->setProperty("instrument",plugin.getName());manifest->setProperty("bpm",bpm);manifest->setProperty("sampleRate",rate);manifest->setProperty("bits",24);manifest->setProperty("frames",result.frames);manifest->setProperty("tailSeconds",tail);manifest->setProperty("startSeconds",0);manifest->setProperty("noteCount",static_cast<int>(notes.size()));manifest->setProperty("processing","Instrument outputs only; no DAW inserts, faders, sends or master processing.");juce::Array<juce::var> stems;
 for(size_t i=0;i<selected.size();++i){auto* item=new juce::DynamicObject();item->setProperty("file",files[static_cast<int>(i)]);item->setProperty("output",selected[i].name);item->setProperty("firstChannel",selected[i].first+1);item->setProperty("channels",selected[i].channels);item->setProperty("peak",peaks[i]);stems.add(juce::var(item));if(peaks[i]<1.e-8)result.silent.add(selected[i].name);if(peaks[i]>=1)result.clipped=true;}manifest->setProperty("stems",stems);if(!staging.getChildFile("Stem manifest.json").replaceWithText(juce::JSON::toString(juce::var(manifest))))throw std::runtime_error("Cannot write the stem manifest.");
 if(cancelled&&cancelled()){result.cancelled=true;return result;}if(!staging.moveFileTo(destination))throw std::runtime_error("Cannot finish the stem folder. Existing exports were kept.");
 }catch(const std::exception& e){result.error=e.what();}catch(...){result.error="Instrument stem render failed.";}return result;
}
}
class StemWorker:private juce::Thread,private juce::AsyncUpdater{
public:
 std::function<void(StemResult)> onFinished;std::atomic<double> progress{0};
 struct Job{std::unique_ptr<juce::AudioProcessor> instrument;std::vector<MidiNote> notes;std::vector<StemOutput> outputs;double rate=48000,bpm=120,tail=2;juce::File folder;};
 StemWorker():Thread("Drum stem render"){}~StemWorker(){shutdown();}void shutdown(){onFinished={};signalThreadShouldExit();stopThread(-1);cancelPendingUpdate();job.instrument.reset();}void cancel(){signalThreadShouldExit();}void poll(){handleUpdateNowIfNeeded();}
 bool start(Job next){if(isThreadRunning())return false;job=std::move(next);progress=0;return startThread();}
private:
 void run()override{try{result=DrumStems::write(*job.instrument,job.notes,job.outputs,job.rate,job.bpm,job.tail,job.folder,[this]{return threadShouldExit();},[this](double p){progress=p;});}catch(const std::exception& e){result.error=e.what();}catch(...){result.error="Instrument stem renderer failed.";}triggerAsyncUpdate();}
 void handleAsyncUpdate()override{job.instrument.reset();if(onFinished)onFinished(result);}
 Job job;StemResult result;
};
