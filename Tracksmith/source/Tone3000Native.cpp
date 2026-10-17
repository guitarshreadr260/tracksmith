#include "Processor.h"
class TracksmithTone3000 final : public TONE3000Processor
{
public:
    juce::uint32 lastRevision=tracksmithChainRevision();
};
std::unique_ptr<juce::AudioProcessor> createTracksmithTone3000()
{
    return std::make_unique<TracksmithTone3000>();
}
bool consumeTracksmithTone3000Changes(juce::AudioProcessor* p)
{
    if(auto* tone=dynamic_cast<TracksmithTone3000*>(p)){
        const auto revision=tone->tracksmithChainRevision();
        if(revision!=tone->lastRevision){tone->lastRevision=revision;return true;}
    }
    return false;
}
