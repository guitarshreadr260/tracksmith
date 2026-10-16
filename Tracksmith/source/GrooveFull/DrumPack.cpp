#include "PluginProcessor.h"
#include "PackSample.h"
#include <map>
namespace TracksmithGrooveFull {

bool AvrxGrooveProcessor::loadDefaultDrumKit()
{
    const auto manifest=packFolder.getChildFile("Chango Luke Holland/kit.avrxpack");
    if(!manifest.existsAsFile()){
        const juce::ScopedLock lock(statusLock);
        kitStatus="Luke Holland default kit missing. Keep Native Instruments beside Tracksmith.exe.";
        return false;
    }
    if(!loadDrumPack(manifest))return false;
    // Complete before exposing a new instrument; saved host state is restored afterwards.
    waitForBackgroundWork();
    return getKit()->packName=="Chango Luke Holland";
}

bool AvrxGrooveProcessor::loadDrumPack (const juce::File& file)
{
    if (guitar.isRecording() || isAnalysing() || loadingSample.exchange (true)) return false;
    if (sampleThread.joinable()) sampleThread.join();
    { const juce::ScopedLock lock (statusLock); kitStatus = "Loading drum pack..."; }
    sampleThread = std::thread ([this, file]
    {
        try
        {
            auto xml = file.getSize() <= 1024 * 1024 ? juce::XmlDocument::parse (file) : nullptr;
            if (! xml || ! xml->hasTagName ("AVRX_PACK") || xml->getIntAttribute ("version") != 1) throw std::runtime_error ("Invalid drum-pack manifest");
            auto prepared = std::make_shared<DrumKit>(); std::array<bool, 9> seen {};
            std::map<juce::String, std::shared_ptr<const DrumSample>> cache;
            for (auto* pad : xml->getChildIterator())
            {
                if (! pad->hasTagName ("Pad")) continue;
                const int drum = pad->getIntAttribute ("index", -1); if (drum < 0 || drum >= 9 || seen[static_cast<size_t> (drum)]) throw std::runtime_error ("Invalid drum assignment");
                const auto index = static_cast<size_t> (drum); seen[index] = true;
                prepared->tuning[index] = static_cast<float> (juce::jlimit (-24.0, 24.0, pad->getDoubleAttribute ("tuning")));
                float previous = 0;
                for (auto* groupXml : pad->getChildIterator())
                {
                    if (! groupXml->hasTagName ("Velocity")) continue;
                    if (prepared->velocityGroups[index].size() >= 8) throw std::runtime_error ("Too many velocity groups");
                    VelocityGroup group; group.upper = static_cast<float> (groupXml->getDoubleAttribute ("upper"));
                    if (! std::isfinite (group.upper) || group.upper <= previous || group.upper > 1) throw std::runtime_error ("Invalid velocity boundaries");
                    previous = group.upper;
                    for (auto* take : groupXml->getChildIterator())
                    {
                        if (! take->hasTagName ("Take")) continue;
                        if (group.takes.size() >= 8) throw std::runtime_error ("Too many alternate takes");
                        const auto sampleFile = file.getParentDirectory().getChildFile (take->getStringAttribute ("file"));
                        if (! sampleFile.isAChildOf (file.getParentDirectory())) throw std::runtime_error ("Pack WAV must be inside its pack folder");
                        const auto key = sampleFile.getFullPathName();
                        if (cache.find (key) == cache.end()) cache[key] = readPackSample (sampleFile);
                        group.takes.push_back (cache[key]);
                    }
                    if (group.takes.empty()) throw std::runtime_error ("Empty velocity group");
                    prepared->velocityGroups[index].push_back (std::move (group));
                }
                if (! prepared->velocityGroups[index].empty()) prepared->setLayer (drum, 0, prepared->velocityGroups[index].back().takes.front());
            }
            if (cache.empty()) throw std::runtime_error ("Pack has no mapped WAV samples");
            editKit ([&] (DrumKit& kit)
            {
                kit.samples = prepared->samples; kit.extraSamples = prepared->extraSamples; kit.layerFiles = prepared->layerFiles;
                kit.velocityGroups = prepared->velocityGroups; kit.tuning = prepared->tuning; kit.fineTuning.fill (0); kit.rootNotes.fill (-1);
                for (auto& row : kit.layerGains) row.fill (1);
                kit.packName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
            });
            const juce::ScopedLock lock (statusLock); kitStatus = xml->getStringAttribute ("name") + " | Velocity layers + alternate takes loaded";
        }
        catch (const std::exception& error) { const juce::ScopedLock lock (statusLock); kitStatus = juce::String (error.what()) + " | Previous kit retained"; }
        loadingSample.store (false);
    });
    return true;
}

void AvrxGrooveProcessor::useBuiltInKit()
{
    if (isLoadingSample() || guitar.isRecording()) return;
    editKit ([] (DrumKit& kit) { DrumKit empty; kit.samples = empty.samples; kit.extraSamples = empty.extraSamples; kit.layerFiles = empty.layerFiles; kit.velocityGroups = {}; kit.tuning.fill (0); kit.fineTuning.fill (0); kit.packName = "Built-in drums"; });
}
void AvrxGrooveProcessor::setHumanFeel (float amount, float timingMs)
{
    editKit ([=] (DrumKit& kit) { kit.humanAmount = std::isfinite (amount) ? juce::jlimit (0.0f, 1.0f, amount) : 0; kit.humanTimingMs = std::isfinite (timingMs) ? juce::jlimit (0.0f, 25.0f, timingMs) : 12; });
}
}
