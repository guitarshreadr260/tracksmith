#pragma once
#include <array>
namespace TracksmithGrooveFull {

// One bar of 4/4, in sixteenth notes. General MIDI drum notes on channel 10.
struct GrooveGenerator
{
    static constexpr int drumCount = 9;
    static constexpr std::array<int, drumCount> notes { 36, 38, 42, 46, 48, 47, 41, 49, 51 };
    static constexpr std::array<const char*, drumCount> names { "Kick", "Snare", "Closed Hat", "Open Hat", "Rack Tom 1", "Rack Tom 2", "Floor Tom", "Crash", "Ride" };
    static bool plays (int instrument, int step) noexcept
    {
        if (instrument == 0) return step == 0 || step == 6 || step == 8 || step == 14;
        if (instrument == 1) return step == 4 || step == 12;
        return instrument == 2;
    }
    static float velocity (int instrument, int step) noexcept
    {
        return instrument == 2 ? (step % 4 == 0 ? 0.65f : 0.42f) : 0.85f;
    }
};
}
