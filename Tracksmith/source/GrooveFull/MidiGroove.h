#pragma once
#include "DrumKit.h"
#include <vector>
namespace TracksmithGrooveFull {

struct GrooveImportOptions
{
    bool ggd = false;
    bool kitMapping = false;
    double tempo = 0;
    int numerator = 0, denominator = 0;
    juce::String name;
};

struct MidiGroove
{
    bool valid = false;
    juce::String name, error;
    double bpm = 120;
    int bars = 1, numerator = 4, denominator = 4, division = 4, mapped = 0, ignored = 0, merged = 0;
    std::array<std::array<float, DrumKit::maxSteps>, GrooveGenerator::drumCount> pattern {};
    static int drumForNote (int note, const DrumKit& kit)
    {
        juce::ignoreUnused(kit); // Source GM mapping is independent of the destination map.
        for (const auto& pair : std::array<std::pair<int, int>, 21> {{{35,0},{36,0},{37,1},{38,1},{40,1},{42,2},{44,2},{46,3},{48,4},{50,4},{45,5},{47,5},{41,6},{43,6},{49,7},{52,7},{55,7},{57,7},{51,8},{53,8},{59,8}}}) if (pair.first == note) return pair.second;
        return -1;
    }
    static int ggdDrumForNote (int note)
    {
        switch (note)
        {
            case 36: return 0;
            case 37: case 38: case 39: case 69: return 1;
            case 10: case 24: case 42: case 44: case 54: case 62: return 2;
            case 5: case 25: case 46: case 58: case 60: case 70: case 56: return 3;
            case 50: return 4;
            case 48: return 5;
            case 41: case 43: return 6;
            case 49: case 52: case 55: case 57: case 64: case 74: case 75: return 7;
            case 51: case 53: case 59: return 8;
            default: return -1;
        }
    }
    static GrooveImportOptions libraryOptions (const juce::File& file)
    {
        auto directory = file.getParentDirectory();
        for (int depth = 0; depth < 6; ++depth)
        {
            const auto manifest = directory.getChildFile ("library.avrxgrooves");
            auto xml = manifest.existsAsFile() && manifest.getSize() <= 1024 * 1024 ? juce::XmlDocument::parse (manifest) : nullptr;
            if (xml && xml->hasTagName ("AVRX_GROOVE_LIBRARY") && xml->getIntAttribute ("version") == 1)
            {
                const auto path = file.getRelativePathFrom (directory).replaceCharacter ('\\', '/');
                for (auto* item : xml->getChildIterator()) if (item->hasTagName ("Groove") && item->getStringAttribute ("file").equalsIgnoreCase (path))
                {
                    GrooveImportOptions options; options.ggd = xml->getStringAttribute ("mapping") == "ggd-groove";
                    options.tempo = item->getDoubleAttribute ("tempo"); options.numerator = item->getIntAttribute ("numerator"); options.denominator = item->getIntAttribute ("denominator"); options.name = item->getStringAttribute ("name"); return options;
                }
            }
            const auto parent = directory.getParentDirectory(); if (parent == directory) break; directory = parent;
        }
        return {};
    }
    static MidiGroove read (const juce::File& file, const DrumKit& kit, double fallbackTempo, const GrooveImportOptions& options = {})
    {
        MidiGroove result; result.name = file.getFileNameWithoutExtension(); result.bpm = fallbackTempo;
        if (! file.existsAsFile() || file.getSize() > 2 * 1024 * 1024) { result.error = "Use a MIDI groove file up to 2 MB."; return result; }
        auto input = file.createInputStream(); juce::MidiFile midi; int fileType = 0;
        if (! input || ! midi.readFrom (*input, false, &fileType)) { result.error = "Could not read this MIDI file. Current groove retained."; return result; }
        if (fileType == 2) { result.error = "Use MIDI format 0 or 1, not independent format-2 sequences."; return result; }
        const int ppq = midi.getTimeFormat();
        if (ppq <= 0) { result.error = "Use beat-based MIDI (PPQ); SMPTE files are not supported."; return result; }
        struct Hit { double beat; int drum; float velocity; };
        std::vector<Hit> hits; bool haveTempo = false, haveMeter = false;
        double end = 0, firstTempoTick = 1.0e100, firstMeterTick = 1.0e100;
        // Earliest tempo/meter wins regardless of MIDI track ordering.
        for (int t = 0; t < midi.getNumTracks(); ++t)
            for (int n = 0; n < midi.getTrack (t)->getNumEvents(); ++n)
            {
                const auto& m = midi.getTrack (t)->getEventPointer (n)->message;
                if (m.isTempoMetaEvent() && m.getTimeStamp() < firstTempoTick && m.getTempoSecondsPerQuarterNote() > 0)
                { firstTempoTick = m.getTimeStamp(); result.bpm = 60 / m.getTempoSecondsPerQuarterNote(); haveTempo = true; }
                if (m.isTimeSignatureMetaEvent() && m.getTimeStamp() < firstMeterTick)
                { firstMeterTick = m.getTimeStamp(); m.getTimeSignatureInfo (result.numerator, result.denominator); haveMeter = true; }
            }
        if (options.tempo != 0) result.bpm = options.tempo;
        if (options.numerator != 0) { result.numerator = options.numerator; result.denominator = options.denominator; }
        if (options.name.isNotEmpty()) result.name = options.name;
        const auto mapNote = [&] (int note) { if(options.kitMapping){for(size_t i=0;i<kit.notes.size();++i)if(kit.notes[i]==note)return (int)i;return -1;}return options.ggd ? ggdDrumForNote (note) : drumForNote (note, kit); };
        for (int t = 0; t < midi.getNumTracks(); ++t)
            for (int n = 0; n < midi.getTrack (t)->getNumEvents(); ++n)
            {
                const auto& m = midi.getTrack (t)->getEventPointer (n)->message;
                if (m.getTimeStamp() < 0 || ! std::isfinite (m.getTimeStamp())) { result.error = "MIDI contains invalid timing."; return result; }
                if (options.tempo == 0 && m.isTempoMetaEvent() && haveTempo && std::abs (60 / juce::jmax (1.0e-9, m.getTempoSecondsPerQuarterNote()) - result.bpm) > 0.1)
                { result.error = "Use a groove with one steady tempo."; return result; }
                if (options.numerator == 0 && m.isTimeSignatureMetaEvent() && haveMeter)
                { int num = 0, den = 0; m.getTimeSignatureInfo (num, den); if (num != result.numerator || den != result.denominator) { result.error = "Use a groove with one time signature."; return result; } }
                if (m.isEndOfTrackMetaEvent()) end = juce::jmax (end, m.getTimeStamp() / ppq);
                if (m.isNoteOn())
                {
                    const int drum = mapNote (m.getNoteNumber());
                    if (drum < 0) { ++result.ignored; continue; }
                    hits.push_back ({ m.getTimeStamp() / ppq, drum, m.getFloatVelocity() });
                    end = juce::jmax (end, m.getTimeStamp() / ppq + 1.0e-6);
                }
                else if (m.isNoteOff() && mapNote (m.getNoteNumber()) >= 0) end = juce::jmax (end, m.getTimeStamp() / ppq);
            }
        if (hits.empty()) { result.error = "No notes match this kit or General MIDI drums. Current groove retained."; return result; }
        if (result.numerator < 1 || result.numerator > 12 || (result.denominator != 2 && result.denominator != 4 && result.denominator != 8 && result.denominator != 16))
        { result.error = "This time signature is outside the supported range."; return result; }
        if (! std::isfinite (result.bpm) || result.bpm < 30 || result.bpm > 300) { result.error = "Use a tempo between 30 and 300 BPM."; return result; }
        const double barBeats = result.numerator * 4.0 / result.denominator;
        const double length = std::ceil ((end - 1.0e-8) / barBeats);
        if (length > 16) { result.error = "Use a MIDI drum groove of 16 bars or fewer."; return result; }
        result.bars = juce::jmax (1, static_cast<int> (length));
        double best = 1.0e100;
        for (int div : { 4, 8, 3, 6 })
        {
            if (result.numerator * 4 * div % result.denominator != 0 || result.bars * result.numerator * 4 * div / result.denominator > DrumKit::maxSteps) continue;
            double error = 0; for (const auto& hit : hits) error += std::abs (hit.beat * div - std::round (hit.beat * div)) / div;
            if (error < best - 1.0e-8) { best = error; result.division = div; }
        }
        if (best == 1.0e100) { result.error = "This meter cannot fit a supported grid."; return result; }
        const int steps = result.bars * result.numerator * 4 * result.division / result.denominator;
        for (const auto& hit : hits)
        {
            const int step = juce::jlimit (0, steps - 1, static_cast<int> (std::llround (hit.beat * result.division)));
            auto& cell = result.pattern[static_cast<size_t> (hit.drum)][static_cast<size_t> (step)];
            if (cell > 0) ++result.merged;
            cell = juce::jmax (cell, hit.velocity); ++result.mapped;
        }
        result.valid = true; return result;
    }
    static MidiGroove starter (int choice)
    {
        MidiGroove g; g.valid = true; g.bars = choice == 3 ? 1 : choice == 4 ? 2 : 4;
        g.bpm = choice == 1 ? 90 : 120; g.division = choice == 2 ? 3 : 4;
        g.name = std::array<const char*, 5> { "Straight Rock", "Half-Time", "Shuffle", "Tom Fill", "Sparse" }[static_cast<size_t> (juce::jlimit (0, 4, choice))];
        const int barSteps = 4 * g.division;
        for (int bar = 0; bar < g.bars; ++bar)
        {
            auto add = [&] (int drum, int step, float v) { g.pattern[static_cast<size_t> (drum)][static_cast<size_t> (bar * barSteps + step)] = v; ++g.mapped; };
            add (0, 0, .9f); add (0, 2 * g.division, .8f);
            add (1, choice == 1 ? 2 * g.division : g.division, .8f); if (choice != 1) add (1, 3 * g.division, .85f);
            for (int step = 0; step < barSteps; ++step) if (choice == 2 ? step % 3 != 1 : step % (choice == 4 ? 4 : 2) == 0) add (2, step, step % g.division == 0 ? .65f : .4f);
            if (choice == 3) { add (4, 10, .7f); add (4, 11, .65f); add (5, 13, .7f); add (6, 14, .8f); add (6, 15, .7f); add (7, 0, .7f); }
        }
        return g;
    }
};
}
