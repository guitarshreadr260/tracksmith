#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Accompaniment.h"
#include <cmath>
namespace TracksmithGrooveFull {

juce::AudioProcessorValueTreeState::ParameterLayout AvrxGrooveProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tempo", 1 }, "Tempo", 30.0f, 300.0f, 110.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "swing", 1 }, "Swing", 0.0f, 0.45f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "level", 1 }, "Output", 0.0f, 1.0f, 0.7f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "run", 1 }, "Run groove", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID {"hostSync",1}, "Follow DAW", true));
    return layout;
}

AvrxGrooveProcessor::AvrxGrooveProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "AVRX_STATE", makeParameters()) {
        const auto resources=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("Native Instruments/Groove");
        packFolder=resources.getChildFile("Drum Packs");grooveFolder=resources.getChildFile("Grooves");
        presetStatus="Tracksmith Groove / Native instrument";
        loadDefaultDrumKit();
    }

AvrxGrooveProcessor::~AvrxGrooveProcessor()
{
    cancelAnalysis.store (true);
    if (analysisThread.joinable()) analysisThread.join();
    if (sampleThread.joinable()) sampleThread.join();
}

juce::String AvrxGrooveProcessor::getAnalysisStatus() const
{
    const juce::ScopedLock lock (statusLock);
    return analysisStatus;
}

std::shared_ptr<const GrooveAnalysis> AvrxGrooveProcessor::getAnalysis() const
{
    auto clip = std::atomic_load (&loadedClip);
    return clip ? std::shared_ptr<const GrooveAnalysis> (clip, &clip->analysis) : nullptr;
}

void AvrxGrooveProcessor::loadAndAnalyse (const juce::File& file, bool generateGroove, double sourceOffset, double sourceLength)
{
    if (analysing.exchange (true)) return;
    clearNeuralPerformance();
    referenceOffset=sourceOffset;referenceLength=sourceLength;
    if (analysisThread.joinable()) analysisThread.join();
    setLiveFollow (false); stopTogether();
    { const juce::ScopedLock lock (statusLock); analysisStatus = "Analysing " + file.getFileName() + "..."; }
    const float strength = detectionStrength.load();
    const double requestedTempo=requestedSongTempo.load();const auto selectedLibrary=grooveFolder;const auto referenceKit=getKit();const bool neural=requestedNeural.load();const float influence=referenceInfluence.load();
    analysisThread = std::thread ([this, file, strength, generateGroove, sourceOffset, sourceLength, requestedTempo, selectedLibrary, referenceKit, neural, influence]
    {
        auto setStatus = [this] (const juce::String& status)
        { const juce::ScopedLock lock (statusLock); analysisStatus = status; };
        try
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (! reader || reader->sampleRate <= 0 || reader->numChannels == 0 || reader->lengthInSamples <= 0)
                setStatus ("Could not read this file. Try WAV, AIFF, FLAC, OGG or MP3. Previous audio is retained.");
            else if (reader->sampleRate > 192000 || reader->numChannels > 2)
                setStatus ("Use mono/stereo audio at 192 kHz or below. Previous audio is retained.");
            else
            {
                auto clip = std::make_shared<Clip>();
                clip->rate = reader->sampleRate; clip->sourceFile = file;
                const auto first=juce::jlimit<juce::int64>(0,reader->lengthInSamples-1,static_cast<juce::int64>(std::max(0.0,sourceOffset)*reader->sampleRate));
                const auto limit = static_cast<juce::int64> (reader->sampleRate * std::min(180.0,std::max(.001,sourceLength)));
                const int length = static_cast<int> (juce::jmin (reader->lengthInSamples-first, limit));
                clip->audio.setSize (static_cast<int> (reader->numChannels), length);
                if (! reader->read (&clip->audio, 0, length, first, true, true))
                    setStatus ("Audio decoding failed. Previous audio is retained.");
                else if (! cancelAnalysis.load())
                {
                    for (int channel = 0; channel < clip->audio.getNumChannels(); ++channel)
                    {
                        auto* samples = clip->audio.getWritePointer (channel);
                        for (int n = 0; n < length; ++n)
                            if (! std::isfinite (samples[n])) samples[n] = 0;
                    }
                    clip->waveform = WaveformData::make (clip->audio, clip->rate, file.getFileName());
                    clip->analysis = GrooveAnalysis::analyse (clip->audio, clip->rate, strength);
                    Accompaniment generated;NeuralResult neuralResult;
                    if(generateGroove){
                        setStatus("Scanning your groove library and matching the selected performance...");
                        const double tempo=requestedTempo>=20&&requestedTempo<=300?requestedTempo:clip->analysis.bpm;
                        generated=Accompaniment::generate(clip->audio,clip->rate,tempo,selectedLibrary,*referenceKit,[this]{return cancelAnalysis.load();});
                        if(neural&&generated.groove.valid){
                            setStatus("Running local GrooVAE using your audio rhythm and selected groove references...");
                            neuralResult=generateNeural(clip->audio,clip->rate,generated.groove,influence,[this]{return cancelAnalysis.load();});
                            generated.groove.valid=neuralResult.ok;generated.summary=neuralResult.status;
                            if(neuralResult.ok){for(auto& row:generated.groove.pattern)row.fill(0);for(const auto& note:neuralResult.notes){const int step=juce::jlimit(0,generated.groove.bars*16-1,(int)std::round(note.beat*4));generated.groove.pattern[note.drum][step]=std::max(generated.groove.pattern[note.drum][step],note.velocity);}}
                        }
                        clip->analysis.valid=generated.groove.valid;clip->analysis.bpm=tempo;clip->analysis.error=generated.groove.valid?juce::String():generated.summary;
                    }
                    if (! cancelAnalysis.load())
                    {
                        std::shared_ptr<const Clip> ready = clip;std::atomic_store (&loadedClip, ready);
                        if(generateGroove&&generated.groove.valid)editKit([&](DrumKit& kit){kit.bars=generated.groove.bars;kit.numerator=4;kit.denominator=4;kit.division=4;kit.pattern=generated.groove.pattern;});
                        if(neuralResult.ok){std::shared_ptr<const std::vector<NeuralPerformanceNote>> performance=std::make_shared<std::vector<NeuralPerformanceNote>>(std::move(neuralResult.notes));std::atomic_store(&neuralPerformance,performance);}
                        if(generateGroove){useAnalysed.store(false);tempoPending.store(generated.groove.valid);setStatus(generated.summary);}
                        else setStatus(file.getFileName()+" | Audio analysis complete");
                    }
                }
            }
        }
        catch (const std::exception&) { setStatus ("Loading failed. Try a smaller audio file. Previous audio is retained."); }
        analysing.store (false);
    });
}

void AvrxGrooveProcessor::applyAnalysisTempo()
{
    if (! tempoPending.exchange (false)) return;
    if (auto analysis = getAnalysis())
    {
        auto* tempo = parameters.getParameter ("tempo");
        tempo->beginChangeGesture();
        tempo->setValueNotifyingHost (tempo->convertTo0to1 (static_cast<float> (analysis->bpm)));
        tempo->endChangeGesture();
        auto* swing = parameters.getParameter ("swing");
        swing->setValueNotifyingHost (0.0f);
    }
}

void AvrxGrooveProcessor::playTogether()
{
    recorder.playing.store (false); recorder.previewPlaying.store (false);
    applyAnalysisTempo();
    if (auto analysis = getAnalysis()) requestedStart.store (analysis->valid ? analysis->firstHitSeconds : 0);
    else requestedStart.store (0);
    rewindRequest.fetch_add (1);
    restartGroove.store (true);
    trackPlaying.store (hasTrack());
    parameters.getParameter ("run")->setValueNotifyingHost (1.0f);
}

void AvrxGrooveProcessor::setTrackPlaying (bool value)
{
    const auto clip = std::atomic_load (&loadedClip);
    if (value && clip && displayedPosition.load() >= clip->audio.getNumSamples() / clip->rate) rewindTrack();
    trackPlaying.store (value && clip != nullptr);
}

void AvrxGrooveProcessor::stopTogether()
{
    if (guitar.isRecording()) stopRecording();
    recorder.playing.store (false); recorder.previewPlaying.store (false);
    trackPlaying.store (false);
    parameters.getParameter ("run")->setValueNotifyingHost (0.0f);
    rewindTrack();
}

void AvrxGrooveProcessor::useDefaultGroove()
{
    clearNeuralPerformance();editKit ([] (DrumKit& kit) { kit.resetPattern(); });
    useAnalysed.store (false);
    restartGroove.store (true);
    const juce::ScopedLock lock (statusLock);
    analysisStatus = "Default groove selected. Edit the steps or analyse another track.";
}

void AvrxGrooveProcessor::editKit (const std::function<void (DrumKit&)>& edit)
{
    const juce::ScopedLock lock (kitEditLock);
    auto next = std::make_shared<DrumKit> (*getKit());
    edit (*next);
    std::shared_ptr<const DrumKit> ready = next;
    std::atomic_store (&loadedKit, ready);nativeStateChanged=true;
}

void AvrxGrooveProcessor::setStepVelocity (int drum, int cell, float velocity)
{
    clearNeuralPerformance();
    if (drum < 0 || drum >= GrooveGenerator::drumCount || cell < 0 || cell >= getKit()->stepCount()) return;
    editKit ([=] (DrumKit& kit) { kit.pattern[static_cast<size_t> (drum)][static_cast<size_t> (cell)] = juce::jlimit (0.0f, 1.0f, velocity); });
}
void AvrxGrooveProcessor::setPadGain (int drum, float gain)
{
    if (drum >= 0 && drum < GrooveGenerator::drumCount)
        editKit ([=] (DrumKit& kit) { kit.gains[static_cast<size_t> (drum)] = juce::jlimit (0.0f, 1.5f, gain); });
}
void AvrxGrooveProcessor::setPadPan (int drum, float pan)
{
    if (drum >= 0 && drum < GrooveGenerator::drumCount)
        editKit ([=] (DrumKit& kit) { kit.pan[static_cast<size_t> (drum)] = juce::jlimit (-1.0f, 1.0f, pan); });
}
void AvrxGrooveProcessor::setPadSolo (int drum, bool solo)
{
    if (drum >= 0 && drum < GrooveGenerator::drumCount)
        editKit ([=] (DrumKit& kit) { kit.solo[static_cast<size_t> (drum)] = solo; });
}
void AvrxGrooveProcessor::setPadMuted (int drum, bool muted)
{
    if (drum >= 0 && drum < GrooveGenerator::drumCount)
        editKit ([=] (DrumKit& kit) { kit.muted[static_cast<size_t> (drum)] = muted; });
}
void AvrxGrooveProcessor::setMidiNote (int drum, int note)
{
    if (drum >= 0 && drum < GrooveGenerator::drumCount)
        editKit ([=] (DrumKit& kit) { const int target=juce::jlimit(0,127,note), previous=kit.notes[(size_t)drum]; for(int i=0;i<GrooveGenerator::drumCount;++i)if(i!=drum && kit.notes[(size_t)i]==target)kit.notes[(size_t)i]=previous; kit.notes[(size_t)drum]=target; });
}
void AvrxGrooveProcessor::setMidiChannel (int channel)
{ editKit ([=] (DrumKit& kit) { kit.midiChannel = juce::jlimit (1, 16, channel); }); }
void AvrxGrooveProcessor::resetMidiMap (bool logic)
{ editKit ([=] (DrumKit& kit) { kit.notes = GrooveGenerator::notes; kit.midiChannel = logic ? 1 : 10; }); }
void AvrxGrooveProcessor::setAmbience (float room, float overhead)
{ editKit ([=] (DrumKit& kit) { kit.roomLevel = juce::jlimit (0.0f, 1.0f, room); kit.overheadLevel = juce::jlimit (0.0f, 1.0f, overhead); }); }
void AvrxGrooveProcessor::clearSample (int drum, int layer)
{
    if (drum < 0 || drum >= GrooveGenerator::drumCount || layer < 0 || layer >= DrumKit::maxLayers) return;
    editKit ([=] (DrumKit& kit) { kit.setLayer (drum, layer, nullptr); if (layer == 0) kit.velocityGroups[static_cast<size_t> (drum)].clear(); kit.packName = "Custom kit"; });
}
juce::String AvrxGrooveProcessor::getKitStatus() const
{ const juce::ScopedLock lock (statusLock); return kitStatus; }

void AvrxGrooveProcessor::loadSample (int drum, const juce::File& file, int layer)
{
    if (drum < 0 || drum >= GrooveGenerator::drumCount || layer < 0 || layer >= DrumKit::maxLayers || loadingSample.exchange (true)) return;
    if (sampleThread.joinable()) sampleThread.join();
    { const juce::ScopedLock lock (statusLock); kitStatus = "Loading sample..."; }
    sampleThread = std::thread ([this, drum, file, layer]
    {
        auto status = [this] (const juce::String& text) { const juce::ScopedLock lock (statusLock); kitStatus = text; };
        try
        {
            juce::WavAudioFormat wav;
            auto stream = file.createInputStream();
            std::unique_ptr<juce::AudioFormatReader> reader (stream ? wav.createReaderFor (stream.release(), true) : nullptr);
            if (! reader || reader->numChannels < 1 || reader->numChannels > 2 || reader->sampleRate <= 0 || reader->sampleRate > 192000
                || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * 10.0)
                status ("Use a mono/stereo WAV one-shot, up to 10 seconds and 192 kHz. Previous sound retained.");
            else
            {
                auto sound = std::make_shared<DrumSample>();
                sound->sampleRate = reader->sampleRate; sound->name = file.getFileName(); sound->sourceFile = file;
                sound->audio.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
                if (! reader->read (&sound->audio, 0, sound->audio.getNumSamples(), 0, true, true)) status ("Sample decoding failed. Previous sound retained.");
                else
                {
                    for (int channel = 0; channel < sound->audio.getNumChannels(); ++channel)
                        for (int n = 0; n < sound->audio.getNumSamples(); ++n)
                            if (! std::isfinite (sound->audio.getSample (channel, n))) sound->audio.setSample (channel, n, 0);
                    sound->waveform = WaveformData::make (sound->audio, sound->sampleRate, sound->name);
                    if (! cancelAnalysis.load())
                    {
                        editKit ([=] (DrumKit& kit) { kit.setLayer (drum, layer, sound); if (layer == 0) kit.velocityGroups[static_cast<size_t> (drum)].clear(); kit.packName = "Custom kit"; });
                        status (juce::String (GrooveGenerator::names[static_cast<size_t> (drum)]) + " layer " + juce::String (layer + 1) + ": " + sound->name);
                    }
                }
            }
        }
        catch (const std::exception&) { status ("Could not load sample. Previous sound retained."); }
        loadingSample.store (false);
    });
}

bool AvrxGrooveProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return (layout.getMainInputChannelSet().isDisabled() || layout.getMainInputChannelSet() == juce::AudioChannelSet::mono() || layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo())
        && (layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void AvrxGrooveProcessor::setLayerGain (int drum, int layer, float gain)
{
    if (drum < 0 || drum >= GrooveGenerator::drumCount || layer < 0 || layer >= DrumKit::maxLayers) return;
    editKit ([=] (DrumKit& kit) { kit.layerGains[static_cast<size_t> (drum)][static_cast<size_t> (layer)] = juce::jlimit (0.0f, 1.5f, gain); });
}
void AvrxGrooveProcessor::setTuning (int drum, float semitones, float cents)
{
    if (drum < 0 || drum >= GrooveGenerator::drumCount) return;
    editKit ([=] (DrumKit& kit) { kit.tuning[static_cast<size_t> (drum)] = juce::jlimit (-24.0f, 24.0f, semitones); kit.fineTuning[static_cast<size_t> (drum)] = juce::jlimit (-100.0f, 100.0f, cents); });
}
void AvrxGrooveProcessor::setRootNote (int drum, int pitchClass)
{
    if (drum < 0 || drum >= GrooveGenerator::drumCount) return;
    editKit ([=] (DrumKit& kit) { kit.rootNotes[static_cast<size_t> (drum)] = juce::jlimit (-1, 11, pitchClass); });
}
void AvrxGrooveProcessor::matchSongKey (int drum, int pitchClass)
{
    const auto kit = getKit();
    if (drum < 0 || drum >= GrooveGenerator::drumCount || kit->rootNotes[static_cast<size_t> (drum)] < 0)
    { const juce::ScopedLock lock (statusLock); kitStatus = "Set the sample's original root note before matching the song key."; return; }
    int shift = (pitchClass - kit->rootNotes[static_cast<size_t> (drum)] + 12) % 12;
    if (shift > 6) shift -= 12;
    setTuning (drum, static_cast<float> (shift), 0);
}

bool AvrxGrooveProcessor::setSequence (int bars, int numerator, int denominator, int division)
{
    if (bars < 1 || bars > 16 || numerator < 1 || numerator > 12
        || (denominator != 2 && denominator != 4 && denominator != 8 && denominator != 16)
        || (division != 1 && division != 2 && division != 3 && division != 4 && division != 6 && division != 8)
        || numerator * 4 * division % denominator != 0 || bars * numerator * 4 * division / denominator > DrumKit::maxSteps)
    { const juce::ScopedLock lock (statusLock); kitStatus = "This note division does not evenly fit the time signature."; return false; }
    editKit ([=] (DrumKit& kit)
    {
        const auto original = kit.pattern;
        const int oldSteps = kit.stepCount(), oldDivision = kit.division;
        kit.bars = bars; kit.numerator = numerator; kit.denominator = denominator; kit.division = division;
        const int newSteps = kit.stepCount();
        for (auto& row : kit.pattern) row.fill (0);
        for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
        {
            for (int cell = 0; cell < oldSteps; ++cell)
            {
                const int target = static_cast<int> (std::llround (cell * static_cast<double> (division) / oldDivision));
                if (target < newSteps)
                    kit.pattern[static_cast<size_t> (drum)][static_cast<size_t> (target)] = juce::jmax (kit.pattern[static_cast<size_t> (drum)][static_cast<size_t> (target)], original[static_cast<size_t> (drum)][static_cast<size_t> (cell)]);
            }
            const int resampledLength = juce::jmax (1, static_cast<int> (std::round (oldSteps * static_cast<double> (division) / oldDivision)));
            for (int cell = resampledLength; cell < newSteps; ++cell)
                kit.pattern[static_cast<size_t> (drum)][static_cast<size_t> (cell)] = kit.pattern[static_cast<size_t> (drum)][static_cast<size_t> (cell % resampledLength)];
        }
    });
    restartGroove.store (true);
    return true;
}

juce::File AvrxGrooveProcessor::createMidiDragFile()
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("AVRX_GROOVE_MIDI");
    if (! folder.createDirectory()) throw std::runtime_error ("Cannot create MIDI drag cache");
    const auto file = folder.getNonexistentChildFile ("AVRX_Groove_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S"), ".mid", false);
    MidiDrag::write (*getKit(), parameters.getRawParameterValue ("tempo")->load(), parameters.getRawParameterValue ("swing")->load(), file);
    return file;
}

void AvrxGrooveProcessor::prepareToPlay (double rate, int block)
{
    expectedHostPpq=std::numeric_limits<double>::quiet_NaN();
    sampleRate = rate;
    juce::ignoreUnused(block); recorder.prepare (rate); liveRhythm.prepare (rate); click.reset(); learnedBar = 0;
    untilNext = 0.0;
    step = 0;
    wasRunning = false;
    for (auto& voice : voices) voice.stop();
    ambience.prepare (sampleRate);
    for (auto& channel : channelStrips) channel.prepare (sampleRate);
    for (auto& reduction : drumReduction) reduction.store (0);
    noteOffCountdown.fill (0);
    displayedStep.store (-1);
    trackCursor = 0;
    activeClip = nullptr;
    displayedPosition.store (0);
}

void AvrxGrooveProcessor::trigger (int instrument, float velocity, const DrumKit& kit)
{
    voices[static_cast<size_t> (instrument)].start (instrument, velocity, kit);
}

void AvrxGrooveProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int countInSamples = 0;
    if (resetClick.exchange (false)) click.reset();

    audio.clear(); recorder.beginBlock();
    double tempo=parameters.getRawParameterValue("tempo")->load();
    bool running=parameters.getRawParameterValue("run")->load()>.5f;
    if (!running) previewingGroove.store(false);
    bool following=false;
    if(!previewingGroove.load() && parameters.getRawParameterValue("hostSync")->load()>.5f)if(auto* head=getPlayHead())if(auto position=head->getPosition()){
        if(auto bpm=position->getBpm())if(auto ppq=position->getPpqPosition())if(std::isfinite(*bpm)&&*bpm>0&&std::isfinite(*ppq)){
            following=true;tempo=juce::jlimit(30.0,300.0,*bpm);running=running&&position->getIsPlaying();
            const double beat=std::max(0.0,*ppq);
            if(!std::isfinite(expectedHostPpq)||std::abs(beat-expectedHostPpq)>.0001)restartGroove.store(true);
            expectedHostPpq=position->getIsPlaying()?beat+audio.getNumSamples()*tempo/(60*sampleRate):beat;
            transportSeek.store(beat*60/tempo);hostTempo.store(tempo);
        }
    }
    hostAvailable.store(following);if(!following)expectedHostPpq=std::numeric_limits<double>::quiet_NaN();
    const auto kit = getKit();
    const bool drumSolo = kit->anySolo();
    std::array<float, GrooveGenerator::drumCount> blockPeaks {};
    for (size_t drum = 0; drum < channelStrips.size(); ++drum) channelStrips[drum].update (kit->shaping[drum]);
    const double baseStep = sampleRate * 60.0 / tempo / kit->division;
    std::array<double, GrooveGenerator::drumCount> pitchRatios {};
    for (size_t drum = 0; drum < pitchRatios.size(); ++drum)
        pitchRatios[drum] = std::pow (2.0, (kit->tuning[drum] + kit->fineTuning[drum] / 100.0) / 12.0);
    const double swing = (kit->division == 2 || kit->division == 4 || kit->division == 8) ? parameters.getRawParameterValue ("swing")->load() : 0;
    const float level = parameters.getRawParameterValue ("level")->load();
    const auto clip = std::atomic_load (&loadedClip);
    if (activeDivision != kit->division || activeStepCount != kit->stepCount())
    { activeDivision = kit->division; activeStepCount = kit->stepCount(); step = 0; untilNext = 0; }
    if (running)
    {
        const bool requestedRestart = restartGroove.exchange (false);
        if (! wasRunning || requestedRestart) { step = 0; untilNext = 0.0; feelDelay.fill (0); }
    }
    if (running)
    {
        const double seek = transportSeek.exchange (-1);
        if (seek >= 0)
        {
            const double elapsed = std::fmod (seek * sampleRate, kit->stepCount() * baseStep);
            double boundary = 0;
            for (int cell = 0; cell < kit->stepCount(); ++cell)
            {
                const int withinBar = cell % kit->stepsPerBar();
                const bool paired = withinBar < (kit->stepsPerBar() / 2) * 2;
                const double duration = baseStep * (paired ? (withinBar % 2 == 0 ? 1 + swing : 1 - swing) : 1);
                if (elapsed < boundary + duration - 0.01)
                {
                    if (std::abs (elapsed - boundary) < 0.01) { step = cell; untilNext = 0; }
                    else { step = (cell + 1) % kit->stepCount(); untilNext = boundary + duration - elapsed; }
                    break;
                }
                boundary += duration;
            }
        }
    }
    const unsigned rewind = rewindRequest.load();
    if (clip.get() != activeClip || rewind != lastRewindRequest)
    {
        trackCursor = clip ? requestedStart.load() * clip->rate : 0;
        activeClip = clip.get();
        lastRewindRequest = rewind;
    }
    bool playing = trackPlaying.load();
    const float sourceLevel = trackLevel.load();
    if (! running && wasRunning)
    {
        for (size_t i = 0; i < noteOffCountdown.size(); ++i)
        {
            if (noteOffCountdown[i] > 0)
                midi.addEvent (juce::MidiMessage::noteOff (activeMidiChannels[i], activeMidiNotes[i]), 0);
            noteOffCountdown[i] = 0;
        }
    }
    if (running && ! wasRunning) click.reset();
    wasRunning = running;
    // Keep incoming events intact; synthesize incoming drum notes at their offsets.
    auto incoming = midi.cbegin();
    const auto incomingEnd = midi.cend();
    juce::MidiBuffer generated;
    auto fire = [&] (int drum, float velocity, int offset)
    {
        const auto index = static_cast<size_t> (drum);
        if (! kit->audible (index)) return;
        trigger (drum, velocity, *kit);
        if (noteOffCountdown[index] > 0)
            generated.addEvent (juce::MidiMessage::noteOff (activeMidiChannels[index], activeMidiNotes[index]), offset);
        activeMidiNotes[index] = kit->notes[index]; activeMidiChannels[index] = kit->midiChannel;
        generated.addEvent (juce::MidiMessage::noteOn (kit->midiChannel, kit->notes[index], velocity), offset);
        noteOffCountdown[index] = juce::jmax (1, static_cast<int> (sampleRate * 0.03));
    };
    if (! running) feelDelay.fill (0);
    const int audition = pendingAudition.exchange (-1);
    if (audition >= 0 && audition < GrooveGenerator::drumCount) fire (audition, 0.9f, 0);
    if (! running) displayedStep.store (-1);
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        while (incoming != incomingEnd && (*incoming).samplePosition <= sample)
        {
            const auto message = (*incoming).getMessage();
            if(message.isAllNotesOff()||message.isAllSoundOff()){for(auto& voice:voices)voice.stop();feelDelay.fill(0);noteOffCountdown.fill(0);}
            if (message.isNoteOn() && !previewingGroove.load())
                for (int i = 0; i < GrooveGenerator::drumCount; ++i)
                    if ((message.getNoteNumber() == kit->notes[static_cast<size_t> (i)] || (kit->notes==GrooveGenerator::notes && MidiGroove::drumForNote(message.getNoteNumber(),*kit)==i)) && kit->audible (static_cast<size_t> (i)))
                        trigger (i, message.getFloatVelocity(), *kit);
            ++incoming;
        }
        for (size_t i = 0; i < noteOffCountdown.size(); ++i)
            if (noteOffCountdown[i] > 0 && --noteOffCountdown[i] == 0)
                generated.addEvent (juce::MidiMessage::noteOff (activeMidiChannels[i], activeMidiNotes[i]), sample);
        const bool countIn = recordingIntoTracks.load() && countInSamples > sample;
        const bool newBar = running && ! countIn && untilNext <= 0.0 && step % kit->stepsPerBar() == 0;
        const double sequencePosition = (step + kit->stepCount() - untilNext / baseStep);
        const int inputCell = static_cast<int> (std::llround (sequencePosition)) % kit->stepsPerBar();
        liveRhythm.process (0, inputCell, newBar);
        if (running && ! countIn && untilNext <= 0.0)
        {
            displayedStep.store (step);
            for (int i = 0; i < GrooveGenerator::drumCount; ++i)
                if (kit->pattern[static_cast<size_t> (i)][static_cast<size_t> (step)] > 0.0f)
                {
                    const auto index = static_cast<size_t> (i);
                    const auto hit = HumanFeel::hit (kit->pattern[index][static_cast<size_t> (step)], i, step, kit->stepsPerBar(), kit->division, kit->humanAmount, kit->humanTimingMs, static_cast<uint32_t> (kit->humanSeed), tempo, swing);
                    const int delay = static_cast<int> (std::llround (hit.delaySeconds * sampleRate));
                    if (delay == 0) fire (i, hit.velocity, sample); else { feelDelay[index] = delay + 1; feelVelocity[index] = hit.velocity; }
                }
            const int withinBar = step % kit->stepsPerBar();
            const bool paired = withinBar < (kit->stepsPerBar() / 2) * 2;
            untilNext += baseStep * (paired ? (withinBar % 2 == 0 ? 1.0 + swing : 1.0 - swing) : 1.0);
            step = (step + 1) % kit->stepCount();
        }
        if (running && ! countIn)
            for (size_t n = 0; n < feelDelay.size(); ++n) if (feelDelay[n] > 0 && --feelDelay[n] == 0) fire (static_cast<int> (n), feelVelocity[n], sample);
        if (running && ! countIn) untilNext -= 1.0;
        std::array<float, 2> dry {}, overheadSend {};
        for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
        {
            const auto index = static_cast<size_t> (drum);
            const auto sound = channelStrips[index].render (voices[index].render (drum, sampleRate, pitchRatios[index]));
            const float gain = (kit->muted[index] || (drumSolo && ! kit->solo[index])) ? 0 : kit->gains[static_cast<size_t> (drum)] * level;
            for (size_t channel = 0; channel < 2; ++channel)
            {
                const float balance = channel == 0 ? juce::jmin (1.0f, 1 - kit->pan[index]) : juce::jmin (1.0f, 1 + kit->pan[index]);
                const float output = sound[channel] * gain * balance;
                blockPeaks[index] = juce::jmax (blockPeaks[index], std::abs (output));
                dry[channel] += output;
                overheadSend[channel] += output * (drum == 2 || drum == 3 || drum == 7 || drum == 8 ? 1.0f : 0.15f);
            }
        }
        const auto buses = ambience.render (dry, overheadSend, kit->roomLevel, kit->overheadLevel);
        const std::array<float, 2> tracks{}; // Recording belongs to the DAW, not this drum instrument.
        const std::array<float,2> guitarMonitor{};
        const float tick = click.render (metronome.load() || countInSamples > sample, sampleRate, tempo,
                                         kit->numerator, kit->denominator, clickLevel.load());
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            float source = 0.0f;
            if (clip && playing && trackCursor < clip->audio.getNumSamples())
            {
                const int position = static_cast<int> (trackCursor);
                const int next = juce::jmin (position + 1, clip->audio.getNumSamples() - 1);
                const float fraction = static_cast<float> (trackCursor - position);
                const int sourceChannel = juce::jmin (channel, clip->audio.getNumChannels() - 1);
                source = clip->audio.getSample (sourceChannel, position) * (1.0f - fraction)
                       + clip->audio.getSample (sourceChannel, next) * fraction;
            }
            const auto out = static_cast<size_t> (juce::jmin (channel, 1));
            const float drums = dry[out] + buses[out] + buses[out + 2];
            audio.setSample (channel, sample, juce::jlimit (-1.0f, 1.0f, drums + source * sourceLevel + guitarMonitor[out] + tick + tracks[out]));
        }
        if (clip && playing)
        {
            trackCursor += clip->rate / sampleRate;
            if (trackCursor >= clip->audio.getNumSamples()) { playing = false; trackPlaying.store (false); }
        }
    }
    for (size_t drum = 0; drum < channelStrips.size(); ++drum)
    { drumReduction[drum].store (channelStrips[drum].gainReduction()); drumPeak[drum].store (blockPeaks[drum]); }
    recorder.endBlock();
    midi.addEvents (generated, 0, audio.getNumSamples(), 0);
    displayedPosition.store (clip ? juce::jmin (trackCursor, static_cast<double> (clip->audio.getNumSamples())) / clip->rate : 0);
}

juce::AudioProcessorEditor* AvrxGrooveProcessor::createEditor() { return new AvrxGrooveEditor (*this); }
void AvrxGrooveProcessor::reset(){for(auto& voice:voices)voice.stop();feelDelay.fill(0);noteOffCountdown.fill(0);untilNext=0;step=0;wasRunning=false;click.reset();expectedHostPpq=std::numeric_limits<double>::quiet_NaN();}
void AvrxGrooveProcessor::getStateInformation(juce::MemoryBlock& data)
{
    waitForBackgroundWork();
    if(auto xml=makePresetState()){if(previewingGroove.load())if(auto* params=xml->getChildByName(parameters.state.getType().toString()))for(auto* param:params->getChildIterator())if(param->getStringAttribute("id")=="run")param->setAttribute("value",0);copyXmlToBinary(*xml,data);}
}
void AvrxGrooveProcessor::setStateInformation(const void* data,int size)
{
    if(size<=0||size>4*1024*1024)return;
    waitForBackgroundWork();
    previewingGroove.store(false);
    if(auto xml=getXmlFromBinary(data,size)){
        if(xml->hasTagName(parameters.state.getType()))parameters.replaceState(juce::ValueTree::fromXml(*xml));
        else if(xml->hasTagName("AVRX_PRESET"))restorePresetState(std::move(xml),packFolder.getParentDirectory().getChildFile("Host-state.avrxpreset"),true);
    }
}



std::shared_ptr<const WaveformData> AvrxGrooveProcessor::getWaveform() const
{
    auto clip = std::atomic_load (&loadedClip);
    return clip ? std::shared_ptr<const WaveformData> (clip, &clip->waveform) : nullptr;
}
void AvrxGrooveProcessor::setLiveFollow (bool enabled)
{
    liveRhythm.enabled.store (false);
    learnedBar = 0;
    if (enabled)
    {
        parameters.getParameter ("swing")->setValueNotifyingHost (0);
        editKit ([] (DrumKit& kit) { for (size_t d = 0; d < 3; ++d) kit.pattern[d].fill (0); });
        restartGroove.store (true);
        parameters.getParameter ("run")->setValueNotifyingHost (1);
    }
    liveRhythm.enabled.store (enabled);
}
void AvrxGrooveProcessor::applyLiveGroove()
{
    if (! liveRhythm.enabled.load()) return;
    const int bar = liveRhythm.bars.load();
    if (bar < 2 || bar == learnedBar) return;
    learnedBar = bar;
    const auto accents = liveRhythm.consume();
    const auto kit = getKit();
    float maximum = 0;
    for (int n = 0; n < kit->stepsPerBar(); ++n) maximum = juce::jmax (maximum, accents[static_cast<size_t> (n)]);
    if (maximum == 0) return; // Silence never replaces a learned groove.
    editKit ([&] (DrumKit& next)
    {
        for (size_t d = 0; d < 3; ++d) next.pattern[d].fill (0);
        for (int n = 0; n < next.stepCount(); ++n)
        {
            const int c = n % next.stepsPerBar();
            const float accent = accents[static_cast<size_t> (c)] / maximum;
            if (accent > 0.4f) next.pattern[0][static_cast<size_t> (n)] = 0.45f + accent * 0.45f;
            const double quarter = static_cast<double> (c) / next.division;
            if (std::abs (quarter - std::round (quarter)) < 0.001 && static_cast<int> (std::round (quarter)) % 2 == 1)
                next.pattern[1][static_cast<size_t> (n)] = 0.7f;
            if (next.division % 2 == 0 && c % (next.division / 2) == 0)
                next.pattern[2][static_cast<size_t> (n)] = 0.25f + accent * 0.25f;
            else if (next.division % 2 != 0 && c % next.division == 0) next.pattern[2][static_cast<size_t> (n)] = 0.3f;
        }
    });
}
juce::String AvrxGrooveProcessor::liveStatus() const
{
    if (! liveRhythm.enabled.load()) return "Live follow is off. Enable it to learn your guitar accents at the selected tempo.";
    return juce::String (liveRhythm.bars.load() < 2 ? "Listening for one bar" : "Following strum accents")
        + " | " + juce::String (liveRhythm.hits.load()) + " attacks detected. Play to the metronome; tempo stays under your control.";
}
bool AvrxGrooveProcessor::startGuitarTake (const juce::File& file, bool processed, int preRollBars)
{
    const auto kit = getKit();
    const double duration = juce::jlimit (0, 2, preRollBars) * kit->numerator * 4.0 / kit->denominator
                            * 60 / parameters.getRawParameterValue ("tempo")->load();
    if (guitar.isRecording() || recorder.loading.load()) return false;
    recorder.previewPlaying.store (false);
    recordingDestination = armedTrack.load(); recordedNam = processed; recordingIntoTracks.store (false);
    recorder.recordingStart.store (recorder.position.load());
    resetClick.store (true);
    return guitar.startRecording (file, processed, duration);
}

void AvrxGrooveProcessor::reanalyseTrack()
{ if (const auto clip = std::atomic_load (&loadedClip)) loadAndAnalyse (clip->sourceFile); }


bool AvrxGrooveProcessor::startMultitrackTake (int track, const juce::File& file, bool processed, int preRollBars)
{
    if (track < 0 || track >= MultitrackEngine::tracks || guitar.isRecording() || recorder.loading.load()) return false;
    const double start = recorder.position.load();
    if (start > 3540) { recorder.setStatus ("Move the cursor to 59 minutes or earlier before recording."); return false; }
    if (! startGuitarTake (file, processed, preRollBars)) return false;
    previousRecordingOffset = recorder.strips[static_cast<size_t> (track)].start.load();
    recorder.recordingLane.store (track);
    recordingDestination = track; recordedNam = processed; recordingIntoTracks.store (true);
    recorder.recordingStart.store (start);
    seekMultitrack (start); trackPlaying.store (false); recorder.previewPlaying.store (false); recorder.playing.store (true);
    restartGroove.store (true); parameters.getParameter ("run")->setValueNotifyingHost (1);
    return true;
}
void AvrxGrooveProcessor::serviceTone3000()
{
    if (guitar.isRecording()) return;
    const auto file = tone3000.takeReady();
    if (file == juce::File()) return;
    if (guitar.loadCapture (file, true)) { guitarRecordMode.store (2); guitar.reportStatus ("TONE3000 capture selected: " + file.getFileName() + " | " + guitar.status()); }
    tone3000.reportStatus (guitar.status());
}

void AvrxGrooveProcessor::serviceRecording()
{
    if (guitar.isRecording()) return;
    guitar.finishRecordingIfNeeded();
    const auto file = juce::File (guitar.recordingPath());
    if (guitar.recordedSeconds() <= 0)
    {
        if (recordingIntoTracks.exchange (false))
        {
            recorder.strips[static_cast<size_t> (recordingDestination)].start.store (previousRecordingOffset);
            recorder.recordingLane.store (-1); recorder.playing.store (false); parameters.getParameter ("run")->setValueNotifyingHost (0);
        }
        return;
    }
    if (! file.existsAsFile() || importedTake == file.getFullPathName()) return;
    if (recorder.loadFile (recordingDestination, file, recordedNam, true, true, recorder.recordingStart.load()))
    {
        importedTake = file.getFullPathName(); recordingIntoTracks.store (false); recorder.recordingLane.store (-1);
        if (recordingDestination >= 0) { recorder.playing.store (false); parameters.getParameter ("run")->setValueNotifyingHost (0); }
    }
}
void AvrxGrooveProcessor::stopRecording()
{
    guitar.stopRecording();
    if (recordingIntoTracks.load()) { recorder.playing.store (false); parameters.getParameter ("run")->setValueNotifyingHost (0); }
    serviceRecording();
}
void AvrxGrooveProcessor::toggleMultitrack()
{
    if (guitar.isRecording()) { stopRecording(); return; }
    const bool start = ! recorder.playing.load();
    recorder.previewPlaying.store (false); recorder.playing.store (start);
    if (start) { trackPlaying.store (false); seekMultitrack (recorder.position.load()); restartGroove.store (true); }
    parameters.getParameter ("run")->setValueNotifyingHost (start ? 1.0f : 0.0f);
}
void AvrxGrooveProcessor::toggleTransport (int page)
{
    if (guitar.isRecording()) { stopRecording(); return; }
    if (recorder.playing.load() || recorder.previewPlaying.load() || isTrackPlaying() || parameters.getRawParameterValue ("run")->load() > 0.5f)
    {
        recorder.playing.store (false); recorder.previewPlaying.store (false); trackPlaying.store (false);
        parameters.getParameter ("run")->setValueNotifyingHost (0); return;
    }
    if (page == 7) { playGroove(); return; }
    if (page == 4 || page == 6) { toggleMultitrack(); return; }
    if (page == 1 && recorder.getPreview()) { recorder.togglePreview(); return; }
    if (page == 3) { if (isTrackPlaying()) stopTogether(); else playTogether(); return; }
    const bool running=parameters.getRawParameterValue("run")->load()>.5f;
    parameters.getParameter ("run")->setValueNotifyingHost (running ? 0.0f : 1.0f);
}


void AvrxGrooveProcessor::setDrumShaping (int drum, DrumShaping settings)
{
    if (drum < 0 || drum >= GrooveGenerator::drumCount) return;
    settings.sanitise(); editKit ([=] (DrumKit& kit) { kit.shaping[static_cast<size_t> (drum)] = settings; });
}

bool AvrxGrooveProcessor::applyGroove (const MidiGroove& groove)
{
    if (! groove.valid || guitar.isRecording() || isAnalysing()) return false;
    clearNeuralPerformance();grooveTitle = groove.name;
    stopTogether(); recorder.playing.store (false); recorder.previewPlaying.store (false);
    editKit ([&] (DrumKit& kit) { kit.pattern = groove.pattern; kit.bars = groove.bars; kit.numerator = groove.numerator; kit.denominator = groove.denominator; kit.division = groove.division; });
    auto* tempo = parameters.getParameter ("tempo"); tempo->setValueNotifyingHost (tempo->convertTo0to1 (static_cast<float> (groove.bpm)));
    parameters.getParameter ("swing")->setValueNotifyingHost (0);
    useAnalysed.store (false); tempoPending.store (false); restartGroove.store (true);
    { const juce::ScopedLock lock (statusLock); analysisStatus = "Groove selected: " + groove.name + " | " + juce::String (groove.bars) + " bars"; }
    return true;
}
void AvrxGrooveProcessor::playGroove()
{
    previewingGroove.store(true);transportSeek.store(-1);
    trackPlaying.store (false); recorder.playing.store (false); recorder.previewPlaying.store (false); restartGroove.store (true);
    parameters.getParameter ("run")->setValueNotifyingHost (1);
}
}
