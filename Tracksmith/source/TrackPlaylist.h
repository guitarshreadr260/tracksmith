#pragma once
#include "ClipLayout.h"
#include "MidiLane.h"
struct TrackPlaylist {int id=1;juce::String name="Main";std::vector<TimelineClip> clips;std::vector<MidiNote> notes;};
constexpr size_t maximumPlaylists=16;
