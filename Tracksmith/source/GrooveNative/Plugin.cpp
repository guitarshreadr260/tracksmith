#include "Processor.h"
namespace TracksmithNativeGroove {
static juce::String padId(int pad,const char* control){return "pad"+juce::String(pad)+"_"+control;}
juce::AudioProcessorValueTreeState::ParameterLayout DrumsProcessor::layout()
{
 juce::AudioProcessorValueTreeState::ParameterLayout p;
 auto f=[&](const juce::String& id,const juce::String& name,float low,float high,float value){p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id,1),name,low,high,value));};
 auto b=[&](const juce::String& id,const juce::String& name,bool value){p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id,1),name,value));};
 b("sequence","Host-synced sequencer",false);f("level","Drum output",0,1,.5f);f("room","Room",0,1,0);f("overhead","Overhead",0,1,0);
 p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("bars",1),"Sequence bars",1,8,4));
 for(int n=0;n<9;++n){const auto name=juce::String(GrooveGenerator::names[n])+" ";
  f(padId(n,"gain"),name+"gain",0,1.5f,1);f(padId(n,"pan"),name+"pan",-1,1,0);f(padId(n,"tune"),name+"tuning",-24,24,0);
  b(padId(n,"mute"),name+"mute",false);b(padId(n,"solo"),name+"solo",false);b(padId(n,"eq"),name+"EQ",false);b(padId(n,"comp"),name+"compressor",false);
  for(int band=0;band<4;++band)f(padId(n,("eq"+juce::String(band)).toRawUTF8()),name+"EQ band "+juce::String(band+1),-15,15,0);
  f(padId(n,"threshold"),name+"threshold",-48,0,-18);f(padId(n,"ratio"),name+"ratio",4,20,4);
 }return p;
}
DrumsProcessor::DrumsProcessor():AudioProcessor(BusesProperties().withOutput("Drums",juce::AudioChannelSet::stereo(),true)),Thread("Tracksmith Groove drum files"),parameters(*this,nullptr,"AVRX_DRUMS",layout())
{
 sequenceValue=parameters.getRawParameterValue("sequence");barsValue=parameters.getRawParameterValue("bars");levelValue=parameters.getRawParameterValue("level");roomValue=parameters.getRawParameterValue("room");overheadValue=parameters.getRawParameterValue("overhead");
 for(int d=0;d<9;++d){auto& c=padControls[d];c.gain=parameters.getRawParameterValue(padId(d,"gain"));c.pan=parameters.getRawParameterValue(padId(d,"pan"));c.tune=parameters.getRawParameterValue(padId(d,"tune"));c.mute=parameters.getRawParameterValue(padId(d,"mute"));c.solo=parameters.getRawParameterValue(padId(d,"solo"));c.eq=parameters.getRawParameterValue(padId(d,"eq"));c.comp=parameters.getRawParameterValue(padId(d,"comp"));c.threshold=parameters.getRawParameterValue(padId(d,"threshold"));c.ratio=parameters.getRawParameterValue(padId(d,"ratio"));for(int b=0;b<4;++b)c.bands[b]=parameters.getRawParameterValue(padId(d,("eq"+juce::String(b)).toRawUTF8()));}
 kit->bars=4;kit->resetPattern();startThread();startTimerHz(30);
}
DrumsProcessor::~DrumsProcessor(){stopTimer();signalThreadShouldExit();notify();stopThread(-1);}
bool DrumsProcessor::isBusesLayoutSupported(const BusesLayout& buses)const{return buses.getMainInputChannelSet().isDisabled()&&(buses.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||buses.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
void DrumsProcessor::prepareToPlay(double sampleRate,int){rate=sampleRate;for(auto& s:strips)s.prepare(rate);ambience.prepare(rate);reset();smoothLevel=raw("level");}
void DrumsProcessor::reset(){for(auto& v:voices)v.stop();lastCell=-1;lastPpq=-100;displayedStep=-1;for(auto& s:strips)s.prepare(rate);ambience.prepare(rate);}
void DrumsProcessor::trigger(int drum,float velocity){if(drum>=0&&drum<9){if(drum==2)voices[3].stop();voices[drum].start(drum,velocity,*kit);}}
void DrumsProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)
{
 juce::ScopedNoDenormals denormals;audio.clear();readers.fetch_add(1);
 if(updating.load()){readers.fetch_sub(1);return;}
 const auto position=getPlayHead()?getPlayHead()->getPosition():juce::Optional<juce::AudioPlayHead::PositionInfo>{};
 const bool playing=position&&position->getIsPlaying();const double tempo=position&&position->getBpm()?*position->getBpm():120;
 const double ppq=position&&position->getPpqPosition()?*position->getPpqPosition():0;
 if((wasPlaying&&!playing) || (playing && std::abs(ppq-lastPpq)>tempo/rate/60*audio.getNumSamples()+.01)){lastCell=-1;for(auto& voice:voices)voice.stop();}
 lastPpq=ppq;wasPlaying=playing;const bool sequence=playing&&sequenceValue->load()>.5f;const int count=static_cast<int>(barsValue->load())*16;
 const int pad=pendingPad.exchange(-1);
 std::array<float,9> gain{},pan{},pitch{},blockPeak{};bool solo=false;std::array<bool,9> mute{},solos{};
 for(int d=0;d<9;++d){gain[d]=padControls[d].gain->load();pan[d]=padControls[d].pan->load();pitch[d]=std::pow(2.0,padControls[d].tune->load()/12);mute[d]=padControls[d].mute->load()>.5f;solos[d]=padControls[d].solo->load()>.5f;solo=solo||solos[d];
  DrumShaping shape;shape.eqOn=padControls[d].eq->load()>.5f;shape.compressorOn=padControls[d].comp->load()>.5f;shape.thresholdDb=padControls[d].threshold->load();shape.ratio=padControls[d].ratio->load();for(int b=0;b<4;++b)shape.gainDb[b]=padControls[d].bands[b]->load();strips[d].update(shape);}
 auto iterator=midi.begin();const auto end=midi.end();
 const float level=levelValue->load(),room=roomValue->load(),overhead=overheadValue->load(),smoothing=static_cast<float>(1-std::exp(-1/(rate*.005)));
 for(int frame=0;frame<audio.getNumSamples();++frame)
 {
  while(iterator!=end&&(*iterator).samplePosition<=frame){const auto event=(*iterator).getMessage();if(event.isNoteOn()){for(int d=0;d<9;++d)if(event.getNoteNumber()==GrooveGenerator::notes[d])trigger(d,event.getFloatVelocity());}else if(event.isAllNotesOff()||event.isAllSoundOff())for(auto& v:voices)v.stop();++iterator;}
  if(frame==0 && pad>=0)trigger(pad,.85f);
  if(sequence){const auto cell=static_cast<int64_t>(std::floor((ppq+frame*tempo/(60*rate))*4+1.e-8));if(cell!=lastCell){lastCell=cell;const int step=static_cast<int>((cell%count+count)%count);displayedStep=step;for(int d=0;d<9;++d)if(kit->pattern[d][step]>0)trigger(d,kit->pattern[d][step]);}}
  std::array<float,2> dry{},cymbals{};
  for(int d=0;d<9;++d){auto sound=strips[d].render(voices[d].render(d,rate,pitch[d]));if(mute[d]||(solo&&!solos[d]))continue;const float l=sound[0]*gain[d]*(pan[d]>0?1-pan[d]:1),r=sound[1]*gain[d]*(pan[d]<0?1+pan[d]:1);dry[0]+=l;dry[1]+=r;if(d==2||d==3||d>=7){cymbals[0]+=l;cymbals[1]+=r;}blockPeak[d]=std::max(blockPeak[d],std::max(std::abs(l),std::abs(r)));}
  const auto wet=ambience.render(dry,cymbals,room,overhead);smoothLevel+=smoothing*(level-smoothLevel);const float l=(dry[0]+wet[0]+wet[2])*smoothLevel,r=(dry[1]+wet[1]+wet[3])*smoothLevel;
  if(audio.getNumChannels()==1)audio.setSample(0,frame,(l+r)*.5f);else{audio.setSample(0,frame,l);audio.setSample(1,frame,r);}
 }
 for(int d=0;d<9;++d)peaks[d]=blockPeak[d];readers.fetch_sub(1);
}
float DrumsProcessor::stepVelocity(int d,int step){const juce::ScopedLock lock(jobs);return d>=0&&d<9&&step>=0&&step<128?kit->pattern[d][step]:0;}
void DrumsProcessor::toggleStep(int d,int step){const juce::ScopedLock lock(jobs);if(loading.load())return;if(d<0||d>=9||step<0||step>=128)return;if(!pending)pending=std::make_unique<DrumKit>(*kit);auto& value=pending->pattern[d][step];value=value>0?0:.85f;}
void DrumsProcessor::clearLayer(int d,int layer){const juce::ScopedLock lock(jobs);if(loading.load())return;if(d<0||d>=9||layer<0||layer>=4)return;if(!pending)pending=std::make_unique<DrumKit>(*kit);pending->setLayer(d,layer,nullptr);if(layer==0)pending->velocityGroups[d].clear();}
juce::String DrumsProcessor::status(){const juce::ScopedLock lock(jobs);return message;}
void DrumsProcessor::requestFile(int kind,const juce::File& file,int drum,int layer){const juce::ScopedLock lock(jobs);if(loading.load()||jobPending){message="Wait for the current drum file operation.";return;}loading=true;requested=file;jobKind=kind;jobDrum=drum;jobLayer=layer;jobPending=true;message="Loading "+file.getFileName()+"...";notify();}
static std::shared_ptr<const DrumSample> loadDrumSample(const juce::File& file,int64_t& total,juce::String& error)
{
 juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
 if(!reader||reader->numChannels<1||reader->numChannels>2||reader->lengthInSamples<=0||reader->lengthInSamples>2*1024*1024||!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>192000||total+reader->lengthInSamples>32*1024*1024){error="Unsupported/missing sample or 256 MiB kit budget exceeded: "+file.getFileName();return {};}
 auto sample=std::make_shared<DrumSample>();sample->audio.setSize(static_cast<int>(reader->numChannels),static_cast<int>(reader->lengthInSamples));if(!reader->read(&sample->audio,0,sample->audio.getNumSamples(),0,true,true)){error="Sample read failed.";return {};}
 for(int ch=0;ch<sample->audio.getNumChannels();++ch)for(int n=0;n<sample->audio.getNumSamples();++n){auto& value=sample->audio.getWritePointer(ch)[n];value=std::isfinite(value)?juce::jlimit(-1.0f,1.0f,value):0;}
 sample->sampleRate=reader->sampleRate;sample->name=file.getFileName();sample->sourceFile=file;total+=reader->lengthInSamples;return sample;
}
static bool loadPack(DrumKit& kit,const juce::File& file,juce::String& error)
{
 if(!file.existsAsFile()||file.getSize()>1024*1024){error="Choose an existing kit.avrxpack.";return false;}auto xml=juce::parseXML(file);if(!xml||!xml->hasTagName("AVRX_PACK")){error="Invalid AVRX pack.";return false;}
 for(int d=0;d<9;++d){kit.velocityGroups[d].clear();for(int l=0;l<4;++l)kit.setLayer(d,l,nullptr);}int64_t total=0;int takes=0;
 for(auto* pad:xml->getChildIterator())
 {
  const int d=pad->getIntAttribute("index",-1);if(!pad->hasTagName("Pad")||d<0||d>=9)continue;
  for(auto* group:pad->getChildIterator())if(group->hasTagName("Velocity"))
  {VelocityGroup velocity;velocity.upper=static_cast<float>(group->getDoubleAttribute("upper",1));if(!std::isfinite(velocity.upper)||velocity.upper<=0||velocity.upper>1){error="Invalid velocity group.";return false;}
   for(auto* take:group->getChildIterator())if(take->hasTagName("Take")){if(++takes>512){error="Pack exceeds 512 takes.";return false;}const auto relative=take->getStringAttribute("file");const auto source=file.getParentDirectory().getChildFile(relative);if(juce::File::isAbsolutePath(relative)||!source.isAChildOf(file.getParentDirectory())){error="Pack samples must remain inside its folder.";return false;}auto sound=loadDrumSample(source,total,error);if(!sound)return false;velocity.takes.push_back(sound);}
   if(!velocity.takes.empty())kit.velocityGroups[d].push_back(std::move(velocity));}
  std::sort(kit.velocityGroups[d].begin(),kit.velocityGroups[d].end(),[](const auto& a,const auto& b){return a.upper<b.upper;});
 }kit.packName=xml->getStringAttribute("name",file.getParentDirectory().getFileName());return takes>0;
}
void DrumsProcessor::run()
{
 while(!threadShouldExit())
 {
  juce::File file;int kind=0,d=0,l=0;juce::ValueTree state;std::unique_ptr<DrumKit> next;
  {const juce::ScopedLock lock(jobs);if(jobPending){file=requested;kind=jobKind;d=jobDrum;l=jobLayer;state=restored;restored={};jobPending=false;next=std::make_unique<DrumKit>(*kit);}}
  if(!next){wait(100);continue;}juce::String error;bool ok=true;
  try{
   if(kind==1)ok=loadPack(*next,file,error);
   else if(kind==2&&d>=0&&d<9&&l>=0&&l<4){int64_t total=0;for(int pad=0;pad<9;++pad){for(int layer=0;layer<4;++layer)if(auto sample=next->layer(pad,layer))total+=sample->audio.getNumSamples();for(const auto& group:next->velocityGroups[pad])for(const auto& take:group.takes)total+=take->audio.getNumSamples();}if(auto old=next->layer(d,l))total-=old->audio.getNumSamples();if(l==0)for(const auto& group:next->velocityGroups[d])for(const auto& take:group.takes)total-=take->audio.getNumSamples();auto sample=loadDrumSample(file,total,error);ok=sample!=nullptr;if(ok){next->setLayer(d,l,sample);if(l==0)next->velocityGroups[d].clear();}}
   else if(kind==3){juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));if(!reader||reader->numChannels<1||reader->numChannels>2||reader->lengthInSamples<=0||!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>192000){ok=false;error="Choose a readable mono/stereo reference.";}else{const int frames=static_cast<int>(std::min<int64_t>(reader->lengthInSamples,static_cast<int64_t>(reader->sampleRate*60)));juce::AudioBuffer<float> audio(2,frames);ok=reader->read(&audio,0,frames,0,true,true);if(ok){auto result=GrooveAnalysis::analyse(audio,reader->sampleRate);ok=result.valid;if(ok){for(auto& row:next->pattern)row.fill(0);for(int pad=0;pad<3;++pad)for(int step=0;step<128;++step)next->pattern[pad][step]=result.pattern[pad][step%16];error="Suggested "+juce::String(result.bpm,0)+" BPM; host tempo stays authoritative.";}else error=result.error;}}}
   else if(kind==4&&state.isValid()){
    next=std::make_unique<DrumKit>();
    const auto path=state.getProperty("packPath").toString();if(path.isNotEmpty())ok=loadPack(*next,juce::File(path),error);
    int64_t total=0;for(const auto& groups:next->velocityGroups)for(const auto& group:groups)for(const auto& take:group.takes)total+=take->audio.getNumSamples();
    for(int pad=0;pad<9;++pad)for(int layer=0;layer<4;++layer){const auto path=state.getProperty("sample"+juce::String(pad)+"_"+juce::String(layer)).toString();if(path.isNotEmpty()&&juce::File::isAbsolutePath(path)){auto sound=loadDrumSample(juce::File(path),total,error);if(sound)next->setLayer(pad,layer,sound);else ok=false;}}
    for(int pad=0;pad<9;++pad)if(state.hasProperty("groupEnabled"+juce::String(pad))&&!static_cast<bool>(state.getProperty("groupEnabled"+juce::String(pad))))next->velocityGroups[pad].clear();
    for(int pad=0;pad<9;++pad){const auto values=juce::StringArray::fromTokens(state.getProperty("pattern"+juce::String(pad)).toString(),",","");for(int cell=0;cell<128;++cell){const auto value=cell<values.size()?values[cell].getFloatValue():0;next->pattern[pad][cell]=std::isfinite(value)?juce::jlimit(0.0f,1.0f,value):0;}}
   }
  }catch(const std::exception&){ok=false;error="Not enough memory or invalid drum file.";}
  if(threadShouldExit())return;const juce::ScopedLock lock(jobs);loading=false;if(ok){pending=std::move(next);if(kind==1)packPath=file.getFullPathName();if(kind==4)packPath=state.getProperty("packPath").toString();message=error.isEmpty()?"Drum files loaded. "+pending->packName:error;}else message="Load failed: "+error+" Previous kit kept.";
 }
}
void DrumsProcessor::timerCallback(){const juce::ScopedLock lock(jobs);if(!pending)return;updating=true;if(readers.load()!=0)return;for(auto& v:voices)v.stop();kit=std::move(pending);updating=false;updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));}
void DrumsProcessor::getStateInformation(juce::MemoryBlock& bytes){auto state=parameters.copyState();const juce::ScopedLock lock(jobs);const auto& current=pending?*pending:*kit;state.setProperty("packPath",packPath,nullptr);for(int d=0;d<9;++d){state.setProperty("groupEnabled"+juce::String(d),!current.velocityGroups[d].empty(),nullptr);juce::StringArray cells;for(int n=0;n<128;++n)cells.add(juce::String(current.pattern[d][n]));state.setProperty("pattern"+juce::String(d),cells.joinIntoString(","),nullptr);for(int l=0;l<4;++l)state.setProperty("sample"+juce::String(d)+"_"+juce::String(l),current.layerFiles[d][l].getFullPathName(),nullptr);}copyXmlToBinary(*state.createXml(),bytes);}
void DrumsProcessor::setStateInformation(const void* bytes,int size){auto xml=getXmlFromBinary(bytes,size);if(!xml)return;auto state=juce::ValueTree::fromXml(*xml);if(state.hasType("TRACKSMITH_GROOVE_DRUMS")){auto migrated=parameters.copyState();for(int i=0;i<migrated.getNumChildren();++i){auto setting=migrated.getChild(i);const auto id=setting.getProperty("id").toString();juce::String oldId=id;if(id.startsWith("pad")){const auto tail=id.fromFirstOccurrenceOf("_",false,false);oldId=tail+id.substring(3,id.indexOfChar('_'));}for(int n=0;n<state.getNumChildren();++n){const auto old=state.getChild(n);if(old.getProperty("id").toString()!=oldId)continue;float value=static_cast<float>(old.getProperty("value"));if(id=="level"||id.endsWith("_gain"))value=juce::Decibels::decibelsToGain(value);setting.setProperty("value",value,nullptr);break;}}parameters.replaceState(migrated);return;}if(!state.hasType("AVRX_DRUMS"))return;parameters.replaceState(state);{const juce::ScopedLock lock(jobs);restored=state;}requestFile(4,{});}
juce::Result DrumsProcessor::exportMidi(const juce::File& file){juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4,4));for(int d=0;d<9;++d)for(int n=0;n<bars()*16;++n){const auto value=stepVelocity(d,n);if(value>0){auto on=juce::MidiMessage::noteOn(10,GrooveGenerator::notes[d],value),off=juce::MidiMessage::noteOff(10,GrooveGenerator::notes[d]);on.setTimeStamp(n*240);off.setTimeStamp(n*240+120);sequence.addEvent(on);sequence.addEvent(off);}}sequence.updateMatchedPairs();juce::MidiFile midi;midi.setTicksPerQuarterNote(960);midi.addTrack(sequence);juce::TemporaryFile temp(file);auto stream=temp.getFile().createOutputStream();if(!stream||!midi.writeTo(*stream)){return juce::Result::fail("MIDI export failed.");}stream->flush();stream.reset();return temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Could not commit MIDI file.");}

}
