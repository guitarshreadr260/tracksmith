#include <JuceHeader.h>

class SplitProcessor final : public juce::AudioProcessor {
public:
    SplitProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
    const juce::String getName() const override { return "Tracksmith SPLIT"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainInputChannelSet()==l.getMainOutputChannelSet() && (l.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()); }
    void processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override { for(int ch=getTotalNumInputChannels();ch<getTotalNumOutputChannels();++ch) b.clear(ch,0,b.getNumSamples()); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override { juce::MemoryOutputStream o(dest,true); o.writeString(source.getFullPathName()); o.writeString(output.getFullPathName()); o.writeInt(model); }
    void setStateInformation(const void* data,int size) override { juce::MemoryInputStream in(data,static_cast<size_t>(size),false); source=juce::File(in.readString()); output=juce::File(in.readString()); model=in.readInt(); }
    juce::File source,output; int model=0;
};

class SplitEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SplitEditor(SplitProcessor& p): AudioProcessorEditor(p), proc(p) {
        setSize(690,420); setResizable(true,true); setResizeLimits(620,390,1000,700);
        for(auto* b:{&choose,&out,&go,&open}) addAndMakeVisible(*b);
        for(auto* l:{&title,&subtitle,&source,&destination,&status}) { addAndMakeVisible(*l); l->setColour(juce::Label::textColourId,juce::Colour(0xffe9ddcb)); }
        title.setText("TRACKSMITH  SPLIT",juce::dontSendNotification); title.setFont(juce::FontOptions(25.0f,juce::Font::bold));
        subtitle.setText("OFFLINE STEM SEPARATION  /  VST3",juce::dontSendNotification);
        choose.setButtonText("CHOOSE AUDIO"); out.setButtonText("OUTPUT FOLDER"); go.setButtonText("SEPARATE STEMS"); open.setButtonText("OPEN STEM FOLDER");
        model.addItem("6 stems: vocals, drums, bass, guitar, piano, other",1); model.addItem("4 stems: quality",2); model.setSelectedId(proc.model==1?2:1); addAndMakeVisible(model);
        choose.onClick=[this] { selectSource(); };
        out.onClick=[this] { selectOutput(); };
        go.onClick=[this]{startSeparation();}; open.onClick=[this]{ auto f=result.isDirectory()?result:proc.output; if(f.isDirectory()) f.startAsProcess(); };
        model.onChange=[this]{proc.model=model.getSelectedId()==2?1:0;};
        if(proc.output==juce::File())proc.output=juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Tracksmith SPLIT"); refresh(); startTimer(250);
    }
    ~SplitEditor() override { stopTimer(); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff111416)); g.setColour(juce::Colour(0xff4a3824)); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8),6,1); }
    void resized() override { auto a=getLocalBounds().reduced(25); title.setBounds(a.removeFromTop(40)); subtitle.setBounds(a.removeFromTop(25)); a.removeFromTop(12); auto row=a.removeFromTop(42); choose.setBounds(row.removeFromLeft(170)); source.setBounds(row.reduced(12,0)); a.removeFromTop(8); row=a.removeFromTop(42); out.setBounds(row.removeFromLeft(170)); destination.setBounds(row.reduced(12,0)); a.removeFromTop(15); model.setBounds(a.removeFromTop(38)); a.removeFromTop(18); row=a.removeFromTop(42); go.setBounds(row.removeFromLeft(200)); row.removeFromLeft(10);open.setBounds(row.removeFromLeft(200)); a.removeFromTop(20);status.setBounds(a); }
private:
    void selectSource(){ chooser=std::make_unique<juce::FileChooser>("Choose a song",juce::File{},"*.wav;*.mp3;*.flac;*.m4a"); juce::Component::SafePointer<SplitEditor> safe(this); chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& f){if(safe && f.getResult().existsAsFile()){safe->proc.source=f.getResult();safe->refresh();}}); }
    void selectOutput(){ chooser=std::make_unique<juce::FileChooser>("Choose output folder"); juce::Component::SafePointer<SplitEditor> safe(this); chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& f){if(safe && f.getResult().isDirectory()){safe->proc.output=f.getResult();safe->refresh();}}); }
    void refresh(){source.setText(proc.source.existsAsFile()?proc.source.getFileName():"No source selected",juce::dontSendNotification);destination.setText(proc.output.getFullPathName(),juce::dontSendNotification);}
    void startSeparation(){
        if(child.isRunning())return;
        if(!proc.source.existsAsFile()){status.setText("Choose an audio file first.",juce::dontSendNotification);return;}
        auto py=juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Downloads/Tracksmith-SPLIT-Build1/Tracksmith-SPLIT-Build1/.venv/Scripts/python.exe");
        if(!py.existsAsFile()){status.setText("Build 1 environment not found in Downloads.",juce::dontSendNotification);return;}
        auto mdl=proc.model==1?juce::String("htdemucs_ft"):juce::String("htdemucs_6s"); result=proc.output.getChildFile(mdl).getChildFile(proc.source.getFileNameWithoutExtension());
        if(result.isDirectory() && result.findChildFiles(juce::File::findFiles,false,"*.wav").size()>0){status.setText("Output already exists. Choose a different output folder to preserve it.",juce::dontSendNotification);return;}
        juce::StringArray args{py.getFullPathName(),"-m","demucs","-n",mdl,"-d","cuda","--float32","-o",proc.output.getFullPathName(),proc.source.getFullPathName()};
        if(!child.start(args,0)){status.setText("Could not start separation.",juce::dontSendNotification);return;}
        running=true;go.setEnabled(false);status.setText("Separating on GPU in the background. Keep this window open.",juce::dontSendNotification);
    }
    void timerCallback() override { if(running && !child.isRunning()){ running=false;go.setEnabled(true);auto rc=child.getExitCode();status.setText(rc==0?"Separation complete. Open the folder and import the WAV stems into your DAW.":"Separation failed. Check CUDA and the Build 1 environment.",juce::dontSendNotification); } }
    SplitProcessor& proc; juce::Label title,subtitle,source,destination,status; juce::TextButton choose,out,go,open; juce::ComboBox model; juce::File result; std::unique_ptr<juce::FileChooser> chooser; juce::ChildProcess child; bool running=false;
};
juce::AudioProcessorEditor* SplitProcessor::createEditor(){return new SplitEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SplitProcessor();}

