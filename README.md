# Tracksmith

## Current checkpoint: UI Prototype 16

Updated October 16, 2026 UTC for the October 15 development session. The current checkpoint adds the modern Edit/Mix/EZ interface, startup artwork, native Groove/NAM instruments, I/O setup, session format 10, visible musical grid and Dynamic selection punch recording.

[Today’s changes and validation](prototype-16/CHANGES.md) · [Restore current source and build instructions](prototype-16/BUILD.md) · [Automated results](prototype-16/prototype16-test-results.json) · [Current status](PROJECT-STATUS.md)

**Current code is stored as `prototype-16/Tracksmith-Prototype-16-Code.zip`.** Run the supplied verification script to check the ZIP. Large sample/model/runtime payloads and the Windows portable build remain local; their manifests/hashes are included. The older Windows downloads below predate this UI checkpoint.

Grid and Dynamic punch are implemented. **Elastic Audio is planned, not implemented.** Hardware/interface and listening validation remain pending. Drum-learning work is paused.

## Historical stock-plugin checkpoint


Development reference for Tracksmith DAW and the separate optional Tracksmith Groove VST3. Updated October 15, 2026.

## Current build: stock plugin test suite

[Download the Windows DAW test build](Tracksmith-Windows-Stock-Plugins-Test.zip) · [Current source snapshot](Tracksmith-Source-Stock-Plugins.zip) · [Plugin-only VST3 bundle](Tracksmith-Stock-Plugins-VST3.zip)

Extract the whole Windows ZIP, close the other Tracksmith, and run Tracksmith.exe from the extracted folder. Keep Stock Plugins beside the application. Choose **Add stock Tracksmith ...** from a track/aux/master insert menu. This is a portable test build, not an installer update.

The build includes nine stock VST3 plugins:

| Plugin | Current features |
| --- | --- |
| Tracksmith EQ 7 | Seven-band EQ with response graph, filters, shelves and bells |
| Tracksmith Gate | Threshold, attack, hold, release, range and hysteresis |
| Tracksmith Bus Compressor | Compression, Auto release, makeup, detector high pass and parallel mix |
| Tracksmith FET Compressor | Input-driven fast compression, four ratios and custom Slam mode |
| Tracksmith Limiter | Sample-peak ceiling, input drive and release |
| Tracksmith De-Esser | Selective-band reduction, adaptive threshold and band listen |
| Tracksmith Plate | Plate-style reverb with decay, pre-delay, damping, low cut and stereo width |
| Tracksmith Digital Delay | Clean stereo repeats, feedback and repeat tone; no modulation |
| Tracksmith NAM | Renamed AVRX NAM, with post-model EQ and preset save/load |

[Build guide and current limits](Tracksmith-Stock-Plugins-Guide.md) · [Automated results](Tracksmith-Stock-Plugin-Test-Results.json) · [Dynamics development notes](Tracksmith-Dynamics-Test-Update.md) · [Current status and next steps](PROJECT-STATUS.md)

NAM presets store the model location and settings; keep the .nam model available. Old AVRX NAM identity and original parameter IDs are preserved. The DAW redirects old AVRX NAM session entries to the renamed bundled plugin.

Automated processing, VST3 loading/state, editor rendering and existing DAW regression checks passed. Listening and real-session checks of this latest combined build are pending. The compressors are original behavioral designs, not verified analog models. The limiter is sample-peak, not true-peak.

## Preview

![Tracksmith NAM](Tracksmith-NAM-Preview.png)
![Plate reverb](Tracksmith-Plate-Preview.png)
![Digital delay](Tracksmith-Digital-Delay-Preview.png)

## Earlier milestones

The user confirmed the GGD MIDI fix works. [GGD guide](Tracksmith-GGD-MIDI-Fix-Guide.md) · [First synthetic reliability report](Tracksmith-Reliability-Report.md) · [Reliability raw results](Tracksmith-Reliability-Results.zip)

The first Windows installer trial also went well according to the user. [Earlier 0.1.0 installer](Tracksmith-Setup-0.1.0-x64.msi) · [Installer guide](Tracksmith-Installer-Guide.md) · [Installer validation](Tracksmith-Installer-Validation.md). That earlier installer predates the stock-plugin suite; use the portable ZIP above for current plugins. Longer hardware/interface testing remains before reliability is considered complete.

## Core workflow guides

- [MIDI selection, track deletion and shortcuts](Tracksmith-Selection-Shortcuts-Guide.md)
- [Instrument tracks and inline piano roll](Tracksmith-Instrument-Piano-Roll-Guide.md)
- [Playlist management and group edits](Tracksmith-Playlist-Group-Editing-Guide.md)
- [Monitoring, routing and hardware cues](Tracksmith-Monitoring-Routing-Guide.md)
- [Plugin automation](Tracksmith-Plugin-Automation-Guide.md)
- [Recording](Tracksmith-Recording-Guide.md)
- [Build history](BUILD-HISTORY.md)
- [Working away from your PC](WORKING-AWAY-FROM-PC.md)

Session format 9 reads versions 1–9. Keep session copies when testing. Plugins run in-process. Plugin scanning improvements and the main DAW custom UI remain deferred.

## Groove and preserved work

Tracksmith Groove 0.18 remains a separate optional instrument: [release downloads](https://github.com/guitarshreadr260/tracksmith/releases/tag/groove-v0.18.0) · [source snapshot](Tracksmith-Groove-Source-Code-v0.18.zip). Original AVRX Groove 0.17 is preserved locally. AVRX Drums stays excluded from the DAW package.

This repository stores ZIP source snapshots, builds, guides and progress notes. Source is not an expanded Git working tree; extract the current source ZIP to browse or build it. Third-party licenses still apply. GitHub is updated when requested; it does not automatically mirror the PC or chat.
