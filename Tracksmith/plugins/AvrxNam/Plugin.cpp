#include <juce_audio_utils/juce_audio_utils.h>
#include "NativeNam.h"
#include "NoiseGate.h"
#include "NamPresets.h"
#include "../TracksmithEq/EqDsp.h"

class NamProcessor final : public juce::AudioProcessor, private juce::Thread, private juce::Timer
{
public:
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeak { 0 }, outputPeak { 0 }, gateLevel { 1 };
    NamProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)), Thread("Tracksmith NAM model loader"),
        parameters(*this, nullptr, "AVRX_NAM", layout())
    {
        inputGain = parameters.getRawParameterValue("input"); outputGain = parameters.getRawParameterValue("output");
        gateOn = parameters.getRawParameterValue("gate"); threshold = parameters.getRawParameterValue("threshold"); release = parameters.getRawParameterValue("release");
        for(int i=0;i<6;++i)eqRaw[i]=parameters.getRawParameterValue(eqIds[i]);
        bypass = parameters.getRawParameterValue("bypass"); startThread(); startTimerHz(20);
    }
    ~NamProcessor() override { stopTimer(); signalThreadShouldExit(); notify(); stopThread(-1); }
    const juce::String getName() const override { return "Tracksmith NAM"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool isBusesLayoutSupported(const BusesLayout& buses) const override
    {
        const auto input = buses.getMainInputChannelSet();
        return input == buses.getMainOutputChannelSet() && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
    }
    void prepareToPlay(double sampleRate, int block) override
    {
        const juce::ScopedLock lock(jobs);
        rate = sampleRate; maximumBlock = std::max(1, block); mono.setSize(1, maximumBlock.load()); gateGains.resize(static_cast<size_t>(maximumBlock.load())); gate.reset();
        eq.prepare(sampleRate,eqBands());
        smoothInput = juce::Decibels::decibelsToGain(inputGain->load()); smoothOutput = juce::Decibels::decibelsToGain(outputGain->load());
        if (model)
        {
            juce::String error; const bool prepared = model->prepare(sampleRate, maximumBlock.load(), error); usable = prepared && model->ready();
            if (!error.isEmpty()) { const juce::ScopedLock lock(jobs); message = error; }
        }
    }
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        // Sequential-consistent gate lets the message thread replace/destruct DSP
        // without locking, waiting or freeing objects on the audio thread.
        readers.fetch_add(1);
        if (updating.load() || !usable.load() || bypass->load() > 0.5f || !model || audio.getNumSamples() > mono.getNumSamples())
        {
            float peak = 0; for(int ch=0;ch<audio.getNumChannels();++ch) for(int n=0;n<audio.getNumSamples();++n) peak=std::max(peak,std::abs(audio.getSample(ch,n)));
            inputPeak=peak;outputPeak=peak;gateLevel=1;readers.fetch_sub(1); return;
        } // Dry pass-through while loading/unavailable.
        const int count = audio.getNumSamples(); const float in = juce::Decibels::decibelsToGain(inputGain->load());
        const float out = juce::Decibels::decibelsToGain(outputGain->load());
        const float smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate.load() * 0.005)));
        float rawPeak = 0, wetPeak = 0;
        const bool enabled = gateOn->load() > .5f; const float cutoff = threshold->load(), releaseTime = release->load();
        for (int n = 0; n < count; ++n)
        {
            smoothInput += smoothing * (in - smoothInput);
            float sample = 0; for (int ch = 0; ch < audio.getNumChannels(); ++ch) sample += audio.getSample(ch, n);
            sample /= static_cast<float>(audio.getNumChannels());
            rawPeak = std::max(rawPeak, std::abs(sample));
            gateGains[static_cast<size_t>(n)] = gate.next(sample, enabled, cutoff, releaseTime, rate.load());
            mono.setSample(0, n, sample * smoothInput);
        }
        model->process(mono, count);
        eq.process(mono.getArrayOfWritePointers(),1,count,eqBands(),0,0,eqRaw[5]->load()<.5f,false);
        for (int n = 0; n < count; ++n)
        {
            smoothOutput += smoothing * (out - smoothOutput);
            const float sample = mono.getSample(0, n) * smoothOutput * gateGains[static_cast<size_t>(n)];
            wetPeak = std::max(wetPeak, std::abs(sample));
            for (int ch = 0; ch < audio.getNumChannels(); ++ch) audio.setSample(ch, n, sample);
        }
        inputPeak = rawPeak; outputPeak = wetPeak; gateLevel = gate.gain;
        readers.fetch_sub(1);
    }
    void requestCapture(const juce::File& file)
    {
        const juce::ScopedLock lock(jobs); requested = file;
        ++generation; requestPending = true; message = "Loading " + file.getFileName() + "..."; notify();
    }
    juce::String status() { const juce::ScopedLock lock(jobs); return message; }
    void getStateInformation(juce::MemoryBlock& destination) override
    {
        auto state = parameters.copyState();
        { const juce::ScopedLock lock(jobs); state.setProperty("modelPath", savedPath, nullptr); }
        copyXmlToBinary(*state.createXml(), destination);
    }
    void setStateInformation(const void* bytes, int count) override
    {
        if (auto xml = getXmlFromBinary(bytes, count))
        {
            auto state = juce::ValueTree::fromXml(*xml); if (!state.hasType("AVRX_NAM")) return;
            for (const auto& setting : { std::pair<const char*,float>{"gate",0}, {"threshold",-60}, {"release",120}, {"bass",0}, {"mid",0}, {"treble",0}, {"midfreq",1000}, {"highpass",20}, {"eq",1} })
            {
                bool found = false; for (int n=0;n<state.getNumChildren();++n) if(state.getChild(n).getProperty("id").toString()==setting.first) found=true;
                if(!found) { juce::ValueTree child("PARAM");child.setProperty("id",setting.first,nullptr);child.setProperty("value",setting.second,nullptr);state.appendChild(child,nullptr); }
            }
            parameters.replaceState(state);
            const auto path = state.getProperty("modelPath").toString();
            if (path.isNotEmpty() && juce::File::isAbsolutePath(path))
            { { const juce::ScopedLock lock(jobs); savedPath = path; } requestCapture(juce::File(path)); }
            else if (path.isEmpty()) requestCapture(juce::File());
        }
    }
    juce::Result savePreset(const juce::File& file){auto tree=parameters.copyState();{const juce::ScopedLock lock(jobs);tree.setProperty("modelPath",savedPath,nullptr);}return NamPresets::save(file,tree);}
    juce::Result loadPreset(const juce::File& file){juce::ValueTree tree;auto result=NamPresets::load(file,tree);if(result.failed())return result;juce::MemoryBlock bytes;copyXmlToBinary(*tree.createXml(),bytes);setStateInformation(bytes.getData(),int(bytes.getSize()));for(auto* parameter:getParameters()){parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->getValue());parameter->endChangeGesture();}updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));return juce::Result::ok();}
private:
    const char* eqIds[6]{"bass","mid","treble","midfreq","highpass","eq"};std::array<std::atomic<float>*,6> eqRaw{};TracksmithEq::Engine eq;
    std::array<TracksmithEq::Band,7> eqBands()const{auto b=TracksmithEq::defaults();for(auto& band:b)band.enabled=false;b[0]={eqRaw[4]->load()>20.01f,TracksmithEq::highPass,eqRaw[4]->load(),0,.707,1};b[1]={true,TracksmithEq::lowShelf,120,eqRaw[0]->load(),.707,1};b[3]={true,TracksmithEq::peak,eqRaw[3]->load(),eqRaw[1]->load(),.8,1};b[5]={true,TracksmithEq::highShelf,6000,eqRaw[2]->load(),.707,1};return b;}
    static juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout result;
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("input", 1), "Input dB", -24.0f, 24.0f, 0.0f));
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("output", 1), "Output dB", -24.0f, 12.0f, 0.0f));
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false));
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("gate",1), "Noise gate", false));
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("threshold",1), "Gate threshold dBFS", -90.0f, -20.0f, -60.0f));
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("release",1), "Gate release ms", 20.0f, 500.0f, 120.0f));
        for(const auto* id:{"bass","mid","treble"})result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},juce::String(id)=="bass"?"Bass dB":juce::String(id)=="mid"?"Mid dB":"Treble dB",-12.f,12.f,0.f));
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"midfreq",1},"Mid frequency Hz",juce::NormalisableRange<float>(200,4000,.01f,.45f),1000));
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"highpass",1},"High pass Hz (20 = off)",juce::NormalisableRange<float>(20,400,.01f,.5f),20));
        result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"eq",1},"Post-model EQ",true));return result;
    }
    void run() override
    {
        while (!threadShouldExit())
        {
            juce::File file; int job = 0;
            { const juce::ScopedLock lock(jobs); if (requestPending) { file = requested; job = generation; requestPending = false; } }
            if (job == 0) { wait(100); continue; }
            auto next = std::make_unique<NativeNam>(); juce::String error;
            const bool loaded = file == juce::File() || next->load(file, rate.load(), maximumBlock.load(), error, true);
            if (threadShouldExit()) return;
            const juce::ScopedLock lock(jobs);
            if (job != generation) continue;
            if (loaded) { pending = std::move(next); pendingGeneration = job; }
            else message = "Load failed: " + error + ". Previous model kept.";
        }
    }
    void timerCallback() override
    {
        const juce::ScopedLock lock(jobs);
        if (!pending) return;
        if (pendingGeneration != generation) { pending.reset(); updating = false; return; }
        updating = true;
        if (readers.load() != 0) return;
        juce::String error; const bool prepared = pending->prepare(rate.load(), maximumBlock.load(), error);
        model = std::move(pending); usable = prepared && model->ready();
        const bool pathChanged = savedPath != model->file().getFullPathName();
        savedPath = model->file().getFullPathName();
        message = usable ? "Loaded " + model->file().getFileName() + " · mono model"
                         : (model->file() == juce::File() ? "Load a .nam model. Audio passes through until a model is ready." : error);
        updating = false;
        if (pathChanged) updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
    }
    std::atomic<float>* inputGain = nullptr; std::atomic<float>* outputGain = nullptr; std::atomic<float>* bypass = nullptr;
    std::atomic<double> rate { 48000.0 }; std::atomic<int> maximumBlock { 256 }, readers { 0 };
    std::atomic<bool> updating { false }, usable { false };
    float smoothInput = 1, smoothOutput = 1;
    NoiseGate gate; std::vector<float> gateGains;
    std::atomic<float>* gateOn = nullptr; std::atomic<float>* threshold = nullptr; std::atomic<float>* release = nullptr;
    juce::AudioBuffer<float> mono;
    std::unique_ptr<NativeNam> model, pending;
    juce::CriticalSection jobs; // Loader/message/state threads only.
    juce::File requested; juce::String savedPath, message { "Load a .nam model. Audio passes through until a model is ready." };
    bool requestPending = false; int generation = 0, pendingGeneration = 0;
};

class NamEditor final:public juce::AudioProcessorEditor,private juce::Timer {
 NamProcessor& processor;juce::LookAndFeel_V4 look;juce::TextButton loadModel,loadPreset,savePreset;juce::ToggleButton bypass,gate,eq;juce::Slider input,output,threshold,release;std::array<juce::Slider,5> tone;std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,5> toneLinks;std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliders;std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttons;std::unique_ptr<juce::FileChooser> chooser;juce::String feedback;
 const char* ids[5]{"bass","mid","treble","midfreq","highpass"};const char* names[5]{"BASS / 120 Hz","MID","TREBLE / 6 kHz","MID FREQUENCY","HIGH PASS"};
 void attach(juce::Slider& s,const char* id,const char* unit){s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,85,23);s.setTextValueSuffix(unit);addAndMakeVisible(s);sliders.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,id,s));}
 void toggle(juce::ToggleButton& b,const char* id,const char* text){b.setButtonText(text);addAndMakeVisible(b);buttons.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,id,b));}
 void choose(int kind){if(chooser)return;const auto start=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);chooser=std::make_unique<juce::FileChooser>(kind==0?"Load a NAM model":kind==1?"Load a Tracksmith NAM preset":"Save a Tracksmith NAM preset",start,kind==0?"*.nam":"*.tsnampreset");const int flags=(kind==2?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles;chooser->launchAsync(flags,[safe=juce::Component::SafePointer<NamEditor>(this),kind](const juce::FileChooser& choice){if(!safe)return;auto file=choice.getResult();if(file!=juce::File()){if(kind==0){safe->feedback.clear();safe->processor.requestCapture(file);}else {if(kind==2&&!file.hasFileExtension("tsnampreset"))file=file.withFileExtension("tsnampreset");auto result=kind==1?safe->processor.loadPreset(file):safe->processor.savePreset(file);safe->feedback=result.wasOk()?(kind==1?"Preset loaded: ":"Preset saved: ")+file.getFileName():result.getErrorMessage();safe->repaint();}}juce::MessageManager::callAsync([safe]{if(safe)safe->chooser.reset();});});}
public:
 NamEditor(NamProcessor& p):AudioProcessorEditor(p),processor(p){setLookAndFeel(&look);loadModel.setButtonText("Load .nam model");loadPreset.setButtonText("Load preset");savePreset.setButtonText("Save preset...");for(auto* b:{&loadModel,&loadPreset,&savePreset})addAndMakeVisible(b);loadModel.onClick=[this]{choose(0);};loadPreset.onClick=[this]{choose(1);};savePreset.onClick=[this]{choose(2);};toggle(bypass,"bypass","Bypass");toggle(gate,"gate","Noise gate");toggle(eq,"eq","Post-model EQ");attach(input,"input"," dB");attach(output,"output"," dB");attach(threshold,"threshold"," dBFS");attach(release,"release"," ms");for(int i=0;i<5;++i){auto& s=tone[i];s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,125,23);s.setTextValueSuffix(i<3?" dB":" Hz");s.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff84c4b9));s.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff101a24));auto* parameter=p.parameters.getParameter(ids[i]);s.setDoubleClickReturnValue(true,parameter->convertFrom0to1(parameter->getDefaultValue()));addAndMakeVisible(s);toneLinks[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,ids[i],s);}setSize(820,640);startTimerHz(15);}
 ~NamEditor()override{stopTimer();setLookAndFeel(nullptr);}
 void resized()override{loadModel.setBounds(24,65,180,30);loadPreset.setBounds(220,65,150,30);savePreset.setBounds(385,65,150,30);bypass.setBounds(690,23,105,28);input.setBounds(85,119,300,30);output.setBounds(480,119,310,30);eq.setBounds(24,175,200,28);for(int i=0;i<5;++i)tone[i].setBounds(24+i*156,228,150,120);gate.setBounds(24,365,180,28);threshold.setBounds(105,408,680,30);release.setBounds(105,451,680,30);}
 void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff101a24));g.setColour(juce::Colour(0xffe4edf2));g.setFont(juce::FontOptions(25,juce::Font::bold));g.drawText("TRACKSMITH NAM",24,18,600,35,juce::Justification::centredLeft);g.setFont(juce::FontOptions(11));g.setColour(juce::Colour(0xffa4bac8));g.drawText("INPUT",24,119,60,30,juce::Justification::centredLeft);g.drawText("OUTPUT",412,119,65,30,juce::Justification::centredLeft);for(int i=0;i<5;++i)g.drawText(names[i],24+i*156,207,150,20,juce::Justification::centred);g.drawText("THRESHOLD",24,408,80,30,juce::Justification::centredLeft);g.drawText("RELEASE",24,451,80,30,juce::Justification::centredLeft);const auto meter=[](float value){return value<.00003f?juce::String("-inf"):juce::String(juce::Decibels::gainToDecibels(value),1);};g.drawText("Input "+meter(processor.inputPeak.load())+" dBFS    Output "+meter(processor.outputPeak.load())+" dBFS    Gate "+meter(processor.gateLevel.load())+" dB",24,501,770,24,juce::Justification::centredLeft);g.drawFittedText((feedback.isNotEmpty()?feedback+"\n":"")+processor.status(),24,539,770,56,juce::Justification::centredLeft,3);g.setColour(juce::Colour(0xff84c4b9));g.drawText("High pass at 20 Hz = off | Presets save model location and all settings; keep the .nam model available",24,610,780,18,juce::Justification::centredLeft);}
 void timerCallback()override{const bool on=processor.parameters.getRawParameterValue("eq")->load()>.5f;for(auto& t:tone)t.setEnabled(on);repaint();}
};
juce::AudioProcessorEditor* NamProcessor::createEditor(){return new NamEditor(*this);}
#if TRACKSMITH_NATIVE_INSTRUMENTS
std::unique_ptr<juce::AudioProcessor> createTracksmithNativeNam(){auto result=std::make_unique<NamProcessor>();const auto model=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Native Instruments/Guitar/Default Guitar.nam");if(model.existsAsFile())result->requestCapture(model);return result;}
#else
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NamProcessor;}
#endif

