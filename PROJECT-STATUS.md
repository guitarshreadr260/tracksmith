# Where we left off

Updated October 15, 2026. Latest delivery: **combined stock-plugin test build**.

## Current delivery

[Windows portable build](Tracksmith-Windows-Stock-Plugins-Test.zip) · [Source snapshot](Tracksmith-Source-Stock-Plugins.zip) · [Plugin-only bundle](Tracksmith-Stock-Plugins-VST3.zip) · [Guide](Tracksmith-Stock-Plugins-Guide.md)

Nine bundled plugins: Tracksmith EQ 7, Gate, Bus Compressor, FET Compressor, Limiter, De-Esser, Plate, Digital Delay and NAM. All have native editors, host parameters, state saving and mono/stereo support. The delay has no modulation and uses milliseconds rather than tempo sync. For aux sends, set Plate/Delay Mix to 100%.

Tracksmith NAM replaces the displayed AVRX NAM name, adds bass/mid/treble, sweepable mid frequency and high pass after the model, and saves/loads .tsnampreset files. Model contents are not embedded. Missing/invalid preset files are rejected before settings change. VST3 component/controller IDs and original six parameter IDs/order remain unchanged. Old state defaults to flat EQ; DAW restore redirects old AVRX NAM descriptions to the bundled Tracksmith NAM path.

## Latest validation

- Automated DSP checks for the eight effects passed at 8, 44.1, 48, 96 and 192 kHz.
- VST3 discovery/loading and parameter state round trips passed; native editors rendered and were visually inspected.
- Plate: pre-delay shift, decay-energy ordering, dense tail, stereo decorrelation/width, reset, finite extreme settings and bypass.
- Delay: first-repeat timing, feedback decay, channel isolation, no static-time impulse spread, reset, finite parameter changes and bypass.
- Actual VST3 rendering produced reverb/delay tails.
- NAM: model loading, post-model EQ audio gain, preset save/load/overwrite, relative model lookup, missing/malformed/nonfinite preset rejection, legacy state and stable VST3 IDs.
- Existing VST3 hosting, NAM, wet monitoring, delay compensation, MIDI/session and WAV export checks passed, as did automation, routing and clip-edit suites.
- Package integrity and payload hashes verified; nine bundles included, legacy AVRX Drums and AVRX NAM bundles excluded.

[Raw stock test results](Tracksmith-Stock-Plugin-Test-Results.json) · [Dynamics notes and implementation limits](Tracksmith-Dynamics-Test-Update.md)

User listening tests of this new combined stock-plugin build are still pending. Do not mark them complete. Bus/FET processors are original behavioral implementations, not verified circuit emulations. The limiter is sample-peak, not true-peak. The de-esser uses a manually selected band and adaptive threshold, not SPL's proprietary scanning system. Extreme delay feedback can outlast the export dialog's 30-second tail limit.

## Previous confirmed milestones

- GGD groove import fix: all 225 saved grooves imported; the user confirmed it works. Duplicate hits are normalized and imported notes are selected/scrolled into view.
- First synthetic reliability pass completed: 60- and 120-second runs, 80 tracks, 16 controlled delay fixtures, 152 recorded WAVs, 20 session reopens, 20 recovery snapshots, 90 edits and 10 plugin reloads; 17,833 audio blocks with none over their processing budgets. This does not measure device xruns or a multi-hour heavy-instrument workload.
- First Windows installer trial went well according to the user. The earlier 0.1.0 MSI is archived here, but it does not include the new stock suite. Automated installer lifecycle testing was limited by Windows Installer service access; the user subsequently tested the installer manually.

## Core workflow retained

Recording and input monitoring with file playback, retake replacement, fades/crossfades, playlists, explicit group range editing, aux buses, pre/post sends, physical cue outputs, automation Read/Write/Touch/Latch, latency alignment, autosave/recovery, MIDI instrument tracks and inline piano roll, MIDI file drops, batch note selection/editing and shortcuts. Track deletion is not undoable and clears edit histories; source media remains. Session format 9 opens formats 1–9.

## Next

1. User tests all stock effects in the portable build, including NAM EQ/presets and plate/delay on inserts and sends.
2. Fix listening/workflow findings and run a longer interface session with representative plugins.
3. Update the installer for the current build after testing is satisfactory.
4. Continue the main DAW custom UI later. Plugin scanner improvements remain deferred.

## Decisions to preserve

- Keep the DAW separate from optional Tracksmith Groove 0.18; preserve original AVRX Groove 0.17 and earlier snapshots.
- AVRX Drums remains excluded.
- Source media stays intact when retakes replace ranges.
- Preserve compatibility and portable review builds.
- Do not claim a hardware listening pass or exact analog emulation based on automated tests alone.

This is a saved handoff, not a live mirror of the PC or chat. Source is stored as ZIP snapshots.
