#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace TracksmithGrooveFull {

class RecordingLocation
{
public:
    RecordingLocation() : folder (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Tracksmith Groove Recordings")) {}
    juce::File get() const { return folder; }
    bool set (const juce::File& next)
    { if (! next.isDirectory()) return false; folder = next; return true; }
    bool restore (const juce::File& preferences)
    {
        const auto path = preferences.loadFileAsString().trim();
        if (path.isEmpty() || ! juce::File::isAbsolutePath (path)) return false;
        folder = juce::File (path); return true;
    }
    bool save (const juce::File& preferences) const
    { return preferences.getParentDirectory().createDirectory().wasOk() && preferences.replaceWithText (folder.getFullPathName()); }
    juce::File nextTake (const juce::String& prefix)
    {
        if (folder.createDirectory().failed()) return {};
        return folder.getNonexistentChildFile (prefix + "_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S"), ".wav", false);
    }
private:
    juce::File folder;
};
}
