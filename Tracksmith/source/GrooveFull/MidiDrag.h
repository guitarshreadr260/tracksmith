#pragma once
#include "DrumKit.h"
#include "HumanFeel.h"
#include <stdexcept>
namespace TracksmithGrooveFull {

struct MidiDrag
{
    static void write (const DrumKit& kit, double bpm, double swing, const juce::File& file)
    {
        constexpr int ppq = 960;
        const bool canSwing = kit.division == 2 || kit.division == 4 || kit.division == 8;
        const double amount = canSwing ? swing : 0;
        const double endTick = kit.bars * kit.numerator * 4.0 / kit.denominator * ppq;
        juce::MidiMessageSequence tempo, groove;
        tempo.addEvent (juce::MidiMessage::textMetaEvent (3, "AVRX - tempo"));
        tempo.addEvent (juce::MidiMessage::tempoMetaEvent (static_cast<int> (std::llround (60000000.0 / bpm))));
        tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (kit.numerator, kit.denominator));
        tempo.addEvent (juce::MidiMessage::endOfTrack(), endTick);
        groove.addEvent (juce::MidiMessage::textMetaEvent (3, "Tracksmith Groove"));
        for (int drum = 0; drum < GrooveGenerator::drumCount; ++drum)
            for (int cell = 0; cell < kit.stepCount(); ++cell)
            {
                const auto index = static_cast<size_t> (drum);
                const float velocity = kit.pattern[index][static_cast<size_t> (cell)];
                if (velocity <= 0 || ! kit.audible (index)) continue;
                const int withinBar = cell % kit.stepsPerBar();
                const bool paired = withinBar < (kit.stepsPerBar() / 2) * 2;
                const auto hit = HumanFeel::hit (velocity, drum, cell, kit.stepsPerBar(), kit.division, kit.humanAmount, kit.humanTimingMs, static_cast<uint32_t> (kit.humanSeed), bpm, amount);
                const double tick = hit.delaySeconds * bpm / 60 * ppq + (cell + (paired && withinBar % 2 ? amount : 0)) * ppq / kit.division;
                groove.addEvent (juce::MidiMessage::noteOn (kit.midiChannel, kit.notes[index], hit.velocity), tick);
                groove.addEvent (juce::MidiMessage::noteOff (kit.midiChannel, kit.notes[index]), juce::jmin (endTick, tick + 0.03 * bpm / 60 * ppq));
            }
        groove.addEvent (juce::MidiMessage::endOfTrack(), endTick); groove.updateMatchedPairs();
        juce::MidiFile midi; midi.setTicksPerQuarterNote (ppq); midi.addTrack (tempo); midi.addTrack (groove);
        if (file.existsAsFile()) throw std::runtime_error ("MIDI cache filename already exists");
        auto output = file.createOutputStream();
        if (! output || ! midi.writeTo (*output, 1)) throw std::runtime_error ("Could not create MIDI drag file");
    }
};
}
