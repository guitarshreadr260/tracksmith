# Tracksmith GEN 0.8.3 - One Kit Wonder: Metal mapping

For MIDI already generated: run REMAP-GGD.cmd, keep Channel 10 selected for GEN exports, choose the ORIGINAL General MIDI drum file, and save a new GGD copy. Import that copy onto your Kontakt / OKW Metal track with the factory Invasion mapping. The original stays intact. Do not remap an already mapped file.

For NEW MIDI: run ALIGN-DRUMS.cmd and select GGD One Kit Wonder: Metal - factory Invasion (the default). Enter your recording tempo and first-bar start as before. Import the resulting MIDI and original audio at timeline zero. General MIDI remains available as an alternate export option.

GGD-Factory-Map-Test.mid is already mapped. Import it directly to check the kit. At 120 BPM it plays kick, snare, closed hat, open hat, floor tom, middle rack tom, high rack tom, crash and ride.

| Voice | GEN/GM note | GGD factory note |
|---|---:|---:|
| Kick | 36 | 24 |
| Snare | 38 | 26 |
| Closed hi-hat tip | 42 | 43 |
| Open hi-hat 2 | 46 | 46 |
| Low tom / floor tom 1 | 45 | 35 |
| Middle tom / rack tom 2 | 47 | 34 |
| High tom / rack tom 1 | 50 | 33 |
| Crash 1 | 49 | 52 |
| Ride bow | 51 | 62 |

Numeric MIDI notes are used because DAWs label octaves differently. The model has three tom voices; mapping cannot recover articulations the model previously merged.

The remapper changes note-on, note-off and poly-pressure pitch bytes on the selected channel. It retains timing, tempo, velocities, channels, controllers and other tracks. Unmapped notes remain and are counted. It refuses to overwrite the original or an existing output. All channels is intended only for drum-only MIDI.

The main TracksmithGEN.exe is unchanged from 0.8.2. This patch changes the separate MIDI exporter/alignment utility. No model training or audio generation behavior was changed. For installation, extract this package into a new folder or close GEN and use UPDATE-GEN.cmd to install into your existing folder. Saved takes and settings are retained; updated program files are backed up.

Mapping reference: the map publisher's original OKW Metal factory Invasion chart, https://drumverter.com/engines/ggd-metal/ . The installed kit was located on the user's PC, but its active Kontakt mapping was not read or changed. Verify using the included test MIDI.

Validation: four Python tests passed for all nine voices, matching on/off notes, unchanged time/tempo/velocity/channel, song-start offsets, repeatability and 128-bar timing. The C# remapper compiled. Its runtime check was blocked by automatic approval review over an unverified telemetry request and remains unverified. No Kontakt listening test was performed.

The existing START-HERE.md and ALIGNMENT-START-HERE.md describe the preceding generation/alignment workflow; this file supersedes their mapping instructions. Complete source and the original GMD attribution/license are included.
