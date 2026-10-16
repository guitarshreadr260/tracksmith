#include "PluginProcessor.h"
#include "PackSample.h"
namespace TracksmithGrooveFull {

namespace
{
// Shared read/write field lists keep every sound-shaping control in the format.
void shapingXml (juce::XmlElement& xml, DrumShaping& s, bool reading)
{
#define FIELD(name) if (reading) s.name = static_cast<decltype(s.name)> (xml.getDoubleAttribute (#name, static_cast<double> (s.name))); else xml.setAttribute (#name, static_cast<double> (s.name));
    FIELD(eqOn) FIELD(highPassOn) FIELD(compressorOn) FIELD(lowBell) FIELD(highBell)
    FIELD(highPassHz) FIELD(eqMix) FIELD(inputDb) FIELD(outputDb) FIELD(thresholdDb)
    FIELD(ratio) FIELD(attackMs) FIELD(releaseMs) FIELD(compressorMix)
#undef FIELD
    for (size_t n = 0; n < 4; ++n)
    {
        const auto suffix = juce::String (static_cast<int> (n));
        if (reading) { s.frequency[n] = static_cast<float> (xml.getDoubleAttribute ("frequency" + suffix, s.frequency[n])); s.gainDb[n] = static_cast<float> (xml.getDoubleAttribute ("gain" + suffix)); s.q[n] = static_cast<float> (xml.getDoubleAttribute ("q" + suffix, s.q[n])); }
        else { xml.setAttribute ("frequency" + suffix, s.frequency[n]); xml.setAttribute ("gain" + suffix, s.gainDb[n]); xml.setAttribute ("q" + suffix, s.q[n]); }
    }
    s.sanitise();
}
float value (const juce::XmlElement& x, const char* name, float fallback, float lo, float hi)
{
    const auto f = static_cast<float> (x.getDoubleAttribute (name, fallback));
    return std::isfinite (f) ? juce::jlimit (lo, hi, f) : fallback;
}
}

bool AvrxGrooveProcessor::savePreset(const juce::File& file)
{
    if(isLoadingSample()||isAnalysing()||recorder.loading.load()||guitar.isRecording()){presetStatus="Finish recording / loading before saving";return false;}
    auto root=makePresetState();if(!root)return false;
    juce::TemporaryFile temporary(file);const bool success=file.getParentDirectory().createDirectory().wasOk()&&root->writeTo(temporary.getFile())&&temporary.overwriteTargetFileWithTemporary();
    presetStatus=success?"Saved: "+file.getFileNameWithoutExtension():"Could not save preset";return success;
}
std::unique_ptr<juce::XmlElement> AvrxGrooveProcessor::makePresetState()
{
    auto encodePath = [this] (const juce::File& path) { return packFolder != juce::File() && path.isAChildOf (packFolder) ? "@pack/" + path.getRelativePathFrom (packFolder).replaceCharacter ('\\', '/') : path.getFullPathName(); };
    auto ownedRoot=std::make_unique<juce::XmlElement>("AVRX_PRESET");auto& root=*ownedRoot; root.setAttribute ("version", 1);
    root.setAttribute ("grooveTitle", grooveTitle);root.setAttribute("referenceOffset",referenceOffset);root.setAttribute("referenceLength",referenceLength);
    if (auto state = parameters.copyState().createXml()) root.addChildElement (state.release());
    root.setAttribute ("trackLevel", getTrackLevel()); root.setAttribute ("detection", detectionStrength.load());
    root.setAttribute ("metronome", metronome.load()); root.setAttribute ("click", clickLevel.load());
    root.setAttribute ("inputChannel", guitar.inputChannel.load()); root.setAttribute ("inputGain", guitar.inputGain.load()); root.setAttribute ("guitarGain", guitar.outputGain.load());
    root.setAttribute ("guitarRecordMode", guitarRecordMode.load()); root.setAttribute ("guitarCountIn", guitarCountIn.load()); root.setAttribute ("recorderRecordMode", recorderRecordMode.load()); root.setAttribute ("recorderCountIn", recorderCountIn.load()); root.setAttribute ("armedTrack", armedTrack.load());
    const auto captureFile = guitar.captureFile();
    root.setAttribute ("nativeCapture", captureFolder != juce::File() && captureFile.isAChildOf (captureFolder) ? "@capture/" + captureFile.getRelativePathFrom (captureFolder).replaceCharacter ('\\', '/') : captureFile.getFullPathName()); root.setAttribute ("ampBypass", guitar.bypassNam.load());
    root.setAttribute ("ampDriveDb", guitar.ampDriveDb.load()); root.setAttribute ("ampOutputDb", guitar.ampOutputDb.load()); root.setAttribute ("bassDb", guitar.bassDb.load()); root.setAttribute ("midDb", guitar.midDb.load()); root.setAttribute ("trebleDb", guitar.trebleDb.load()); root.setAttribute ("gateOn", guitar.gateOn.load()); root.setAttribute ("gateThresholdDb", guitar.gateThresholdDb.load()); root.setAttribute ("gateReleaseMs", guitar.gateReleaseMs.load());
    root.setAttribute ("pickupConfiguration", guitar.pickupConfiguration.load());
    auto* rigXml = root.createNewChildElement ("GuitarRig");
    for (const auto& b : guitar.getRig()->blocks)
    {
        auto* item = rigXml->createNewChildElement ("Block"); item->setAttribute ("id", b.id); item->setAttribute ("type", static_cast<int> (b.type)); item->setAttribute ("enabled", b.enabled);
        for (int n = 0; n < 6; ++n) item->setAttribute ("p" + juce::String (n), b.params[static_cast<size_t> (n)]);
    }
    auto kit = *getKit(); auto* k = root.createNewChildElement ("Kit");
#define KIT(name) k->setAttribute (#name, kit.name);
    KIT(bars) KIT(numerator) KIT(denominator) KIT(division) KIT(midiChannel) KIT(roomLevel) KIT(overheadLevel) KIT(packName) KIT(humanAmount) KIT(humanTimingMs) KIT(humanSeed)
#undef KIT
    for (size_t n = 0; n < kit.pattern.size(); ++n)
    {
        auto* pad = k->createNewChildElement ("Pad"); pad->setAttribute ("index", static_cast<int> (n));
        pad->setAttribute ("gain", kit.gains[n]); pad->setAttribute ("pan", kit.pan[n]); pad->setAttribute ("mute", kit.muted[n]); pad->setAttribute ("solo", kit.solo[n]);
        pad->setAttribute ("note", kit.notes[n]); pad->setAttribute ("root", kit.rootNotes[n]); pad->setAttribute ("tuning", kit.tuning[n]); pad->setAttribute ("fine", kit.fineTuning[n]);
        shapingXml (*pad->createNewChildElement ("Shaping"), kit.shaping[n], false);
        for (int slot = 0; slot < DrumKit::maxLayers; ++slot)
        {
            auto* layer = pad->createNewChildElement ("Layer"); layer->setAttribute ("index", slot); layer->setAttribute ("gain", kit.layerGains[n][static_cast<size_t> (slot)]);
            layer->setAttribute ("file", encodePath (kit.layerFiles[n][static_cast<size_t> (slot)]));
        }
        for (const auto& group : kit.velocityGroups[n])
        {
            auto* groupXml = pad->createNewChildElement ("Velocity"); groupXml->setAttribute ("upper", group.upper);
            for (const auto& take : group.takes) groupXml->createNewChildElement ("Take")->setAttribute ("file", encodePath (take->sourceFile));
        }
        for (int cell = 0; cell < DrumKit::maxSteps; ++cell)
            if (kit.pattern[n][static_cast<size_t> (cell)] > 0) { auto* hit = pad->createNewChildElement ("Hit"); hit->setAttribute ("step", cell); hit->setAttribute ("velocity", kit.pattern[n][static_cast<size_t> (cell)]); }
    }
    for (int n = 0; n < MultitrackEngine::tracks; ++n)
    {
        auto* t = root.createNewChildElement ("Track"); auto& strip = recorder.strips[static_cast<size_t> (n)]; t->setAttribute ("index", n);
        t->setAttribute ("gain", strip.gain.load()); t->setAttribute ("pan", strip.pan.load()); t->setAttribute ("mute", strip.mute.load()); t->setAttribute ("solo", strip.solo.load()); t->setAttribute ("start", strip.start.load());
        t->setAttribute ("file", strip.sourceFile.getFullPathName());
        if (auto clip = recorder.getClip (n)) { t->setAttribute ("file", clip->file.getFullPathName()); t->setAttribute ("namPrinted", clip->namPrinted); }
        if (auto regions = recorder.getExtra (n)) for (const auto& region : *regions) { auto* c = t->createNewChildElement ("Clip"); c->setAttribute ("file", region.clip->file.getFullPathName()); c->setAttribute ("offset", region.offset); c->setAttribute ("namPrinted", region.clip->namPrinted); }
    }
    if (auto clip = std::atomic_load (&loadedClip)) root.setAttribute ("reference", clip->sourceFile.getFullPathName());
    if (auto clip = recorder.getPreview()) { root.setAttribute ("preview", clip->file.getFullPathName()); root.setAttribute ("previewPrinted", clip->namPrinted); root.setAttribute ("previewLane", recorder.getPreviewLane()); }
    return ownedRoot;
}

bool AvrxGrooveProcessor::loadPreset (const juce::File& file)
{
    if (isLoadingSample() || isAnalysing() || recorder.loading.load() || guitar.isRecording()) { presetStatus = "Finish recording / loading before opening a preset"; return false; }
    return restorePresetState(file.getSize()<=4*1024*1024?juce::XmlDocument::parse(file):nullptr,file);
}
bool AvrxGrooveProcessor::restorePresetState(std::unique_ptr<juce::XmlElement> root,const juce::File& file,bool restoreRun)
{
    auto* k = root ? root->getChildByName ("Kit") : nullptr;
    if (! root || ! root->hasTagName ("AVRX_PRESET") || root->getIntAttribute ("version") != 1 || ! k) { presetStatus = "Invalid or unsupported AVRX preset"; return false; }
    RigLayout savedRig;
    if (auto* rigXml = root->getChildByName ("GuitarRig"))
    {
        int position = 0;
        for (auto* item : rigXml->getChildIterator())
        {
            if (! item->hasTagName ("Block") || position >= RigLayout::slots) { presetStatus = "Invalid guitar rig"; return false; }
            auto& b = savedRig.blocks[static_cast<size_t> (position++)]; b.id = item->getIntAttribute ("id", -1); b.type = static_cast<RigType> (item->getIntAttribute ("type", -1)); b.enabled = item->getBoolAttribute ("enabled", true);
            b.params = rigDefaults (b.type);
            for (int n = 0; n < 6; ++n) b.params[static_cast<size_t> (n)] = static_cast<float> (item->getDoubleAttribute ("p" + juce::String (n), b.params[static_cast<size_t> (n)]));
        }
        if ((position != 8 && position != RigLayout::slots) || ! savedRig.valid()) { presetStatus = "Invalid guitar rig"; return false; }
    }
    auto resolvePath = [this, &file] (const juce::String& path) { return path.startsWith ("@pack/") ? packFolder.getChildFile (path.substring (6)) : file.getParentDirectory().getChildFile (path); };
    auto next = std::make_shared<DrumKit>(); auto& kit = *next;
    kit.bars = k->getIntAttribute ("bars"); kit.numerator = k->getIntAttribute ("numerator"); kit.denominator = k->getIntAttribute ("denominator"); kit.division = k->getIntAttribute ("division");
    if (kit.bars < 1 || kit.bars > 16 || kit.numerator < 1 || kit.numerator > 12 || (kit.denominator != 2 && kit.denominator != 4 && kit.denominator != 8 && kit.denominator != 16)
        || (kit.division != 1 && kit.division != 2 && kit.division != 3 && kit.division != 4 && kit.division != 6 && kit.division != 8) || kit.numerator * 4 * kit.division % kit.denominator != 0 || kit.stepCount() > DrumKit::maxSteps)
    { presetStatus = "Invalid preset sequence"; return false; }
    kit.midiChannel = juce::jlimit (1, 16, k->getIntAttribute ("midiChannel", 1)); kit.roomLevel = value (*k, "roomLevel", 0, 0, 1); kit.overheadLevel = value (*k, "overheadLevel", 0, 0, 1);
    kit.packName = k->getStringAttribute ("packName"); kit.humanAmount = value (*k, "humanAmount", 0, 0, 1); kit.humanTimingMs = value (*k, "humanTimingMs", 12, 0, 25); kit.humanSeed = k->getIntAttribute ("humanSeed", 137);
    for (auto& row : kit.pattern) row.fill (0);
    for (auto* pad : k->getChildIterator())
    {
        if (! pad->hasTagName ("Pad")) continue;
        const int drum = pad->getIntAttribute ("index", -1); if (drum < 0 || drum >= GrooveGenerator::drumCount) continue; const auto n = static_cast<size_t> (drum);
        kit.gains[n] = value (*pad, "gain", 1, 0, 1.5f); kit.pan[n] = value (*pad, "pan", 0, -1, 1); kit.muted[n] = pad->getBoolAttribute ("mute"); kit.solo[n] = pad->getBoolAttribute ("solo");
        kit.notes[n] = juce::jlimit (0, 127, pad->getIntAttribute ("note", kit.notes[n])); kit.rootNotes[n] = juce::jlimit (-1, 11, pad->getIntAttribute ("root", -1));
        kit.tuning[n] = value (*pad, "tuning", 0, -24, 24); kit.fineTuning[n] = value (*pad, "fine", 0, -100, 100);
        if (auto* shape = pad->getChildByName ("Shaping")) shapingXml (*shape, kit.shaping[n], true);
        for (auto* child : pad->getChildIterator())
        {
            if (child->hasTagName ("Hit")) { const int cell = child->getIntAttribute ("step", -1); if (cell >= 0 && cell < DrumKit::maxSteps) kit.pattern[n][static_cast<size_t> (cell)] = value (*child, "velocity", 0, 0, 1); }
            if (child->hasTagName ("Layer")) { const int slot = child->getIntAttribute ("index", -1); if (slot >= 0 && slot < DrumKit::maxLayers) kit.layerGains[n][static_cast<size_t> (slot)] = value (*child, "gain", 1, 0, 1.5f); }
        }
    }
    stopTogether(); recorder.playing.store (false); recorder.previewPlaying.store (false); setLiveFollow (false); guitar.monitor.store (false);
    if (auto* state = root->getChildByName (parameters.state.getType().toString())) parameters.replaceState (juce::ValueTree::fromXml (*state));
    if(!restoreRun)parameters.getParameter ("run")->setValueNotifyingHost (0);
    { const juce::ScopedLock lock (kitEditLock); std::shared_ptr<const DrumKit> ready = next; std::atomic_store (&loadedKit, ready); }
    restartGroove.store (true); useAnalysed.store (false);
    setTrackLevel (value (*root, "trackLevel", .5f, 0, 1)); setDetectionStrength (value (*root, "detection", 1, .5f, 3));
    metronome.store (root->getBoolAttribute ("metronome")); clickLevel.store (value (*root, "click", .18f, 0, 1));
    guitar.inputChannel.store (juce::jlimit (0, 1, root->getIntAttribute ("inputChannel"))); guitar.inputGain.store (value (*root, "inputGain", 1, 0, 4)); guitar.outputGain.store (value (*root, "guitarGain", .5f, 0, 2));
    guitarRecordMode.store (juce::jlimit (1, 2, root->getIntAttribute ("guitarRecordMode", 2))); guitarCountIn.store (juce::jlimit (0, 2, root->getIntAttribute ("guitarCountIn", 1)));
    recorderRecordMode.store (juce::jlimit (1, 2, root->getIntAttribute ("recorderRecordMode", 1))); recorderCountIn.store (juce::jlimit (0, 2, root->getIntAttribute ("recorderCountIn", 1))); armedTrack.store (juce::jlimit (0, 3, root->getIntAttribute ("armedTrack")));
    guitar.ampDriveDb.store (value (*root, "ampDriveDb", 0, -24, 24)); guitar.ampOutputDb.store (value (*root, "ampOutputDb", 0, -24, 12));
    guitar.bassDb.store (value (*root, "bassDb", 0, -12, 12)); guitar.midDb.store (value (*root, "midDb", 0, -12, 12)); guitar.trebleDb.store (value (*root, "trebleDb", 0, -12, 12));
    guitar.gateOn.store (root->getBoolAttribute ("gateOn")); guitar.gateThresholdDb.store (value (*root, "gateThresholdDb", -60, -80, 0)); guitar.gateReleaseMs.store (value (*root, "gateReleaseMs", 100, 10, 500));
    guitar.calibrating.store (false); guitar.pickupConfiguration.store (juce::jlimit (0, 3, root->getIntAttribute ("pickupConfiguration")));
    int missing = 0;
    guitar.clearCapture(); const juce::String capture; // Guitar belongs to the native NAM track.
    if (capture.isNotEmpty() && ! guitar.loadCapture (capture.startsWith ("@capture/") ? captureFolder.getChildFile (capture.substring (9)) : file.getParentDirectory().getChildFile (capture), true)) ++missing;
    guitar.bypassNam.store (root->getBoolAttribute ("ampBypass"));
    guitar.setRig (savedRig);
    grooveTitle = root->getStringAttribute ("grooveTitle", "Current groove");
    for (auto* pad : k->getChildIterator())
    {
        const int drum = pad->getIntAttribute ("index", -1); if (! pad->hasTagName ("Pad") || drum < 0 || drum >= GrooveGenerator::drumCount) continue;
        for (auto* layer : pad->getChildIterator())
        {
            const int slot = layer->getIntAttribute ("index", -1); const auto path = layer->getStringAttribute ("file");
            if (! layer->hasTagName ("Layer") || slot < 0 || slot >= DrumKit::maxLayers || path.isEmpty()) continue;
            const auto sample = resolvePath (path);
            editKit ([=] (DrumKit& target) { target.layerFiles[static_cast<size_t> (drum)][static_cast<size_t> (slot)] = sample; });
            if (sample.existsAsFile()) { loadSample (drum, sample, slot); if (sampleThread.joinable()) sampleThread.join(); if (! getKit()->layer (drum, slot)) ++missing; }
            else ++missing;
        }
    }
    for (int n = 0; n < MultitrackEngine::tracks; ++n) recorder.clear (n);
    const auto reference = root->getStringAttribute ("reference");
    std::atomic_store (&loadedClip, std::shared_ptr<const Clip>()); tempoPending.store (false);
    if (reference.isNotEmpty()) { auto clip = file.getParentDirectory().getChildFile (reference); if (clip.existsAsFile()) { loadAndAnalyse (clip, false,root->getDoubleAttribute("referenceOffset",0),root->getDoubleAttribute("referenceLength",180)); if (analysisThread.joinable()) analysisThread.join(); if (! hasTrack()) ++missing; } else ++missing; }
    const juce::String previewPath;
    recorder.clearPreview();
    if (previewPath.isNotEmpty()) { auto clip = file.getParentDirectory().getChildFile (previewPath); if (clip.existsAsFile()) { recorder.loadFile (-1, clip, root->getBoolAttribute ("previewPrinted")); recorder.waitForLoad(); recorder.setPreviewLane (root->getIntAttribute ("previewLane", -1)); } else ++missing; }
    for (auto* pad : k->getChildIterator())
    {
        const int drum = pad->getIntAttribute ("index", -1); if (! pad->hasTagName ("Pad") || drum < 0 || drum >= GrooveGenerator::drumCount) continue;
        std::vector<VelocityGroup> groups; float previous = 0;
        for (auto* groupXml : pad->getChildIterator())
        {
            if (! groupXml->hasTagName ("Velocity") || groups.size() >= 8) continue;
            VelocityGroup group; group.upper = value (*groupXml, "upper", 1, 0, 1); if (group.upper <= previous) continue; previous = group.upper;
            for (auto* take : groupXml->getChildIterator())
            {
                if (! take->hasTagName ("Take") || group.takes.size() >= 8) continue;
                try { group.takes.push_back (readPackSample (resolvePath (take->getStringAttribute ("file")))); } catch (const std::exception&) { ++missing; }
            }
            if (! group.takes.empty()) groups.push_back (std::move (group));
        }
        editKit ([&] (DrumKit& target) { target.velocityGroups[static_cast<size_t> (drum)] = std::move (groups); });
    }
    editKit ([&] (DrumKit& target) { target.packName = k->getStringAttribute ("packName"); });
    recorder.rewind();
    ++presetRevision;
    presetStatus = "Loaded: " + file.getFileNameWithoutExtension() + (missing > 0 ? " | " + juce::String (missing) + " missing / unreadable audio files" : "");
    return true;
}
}
