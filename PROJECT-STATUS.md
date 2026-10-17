# Build 30 - October 17, 2026

Local native TONE3000 preview replaces NAM for new inserts and guitar tracks. Legacy NAM sessions retain their original engine. Includes native chains/editor and OAuth configuration, isolated host device management/settings and session dirty tracking. Online sign-in/download and listening require user confirmation; distribution is subject to API access terms. [Notes](BUILD30-TONE3000.md). Published main remains Build 24.

---

# Build 29 - October 17, 2026

Local build: drag the bottom edge of Edit tracks to resize, choose height presets, and double-click to reset. Audio waveforms grow; instrument piano rolls show more pitches with aligned keys. Heights save in sessions as optional version-13 metadata. [Notes and validation](BUILD29-TRACK-HEIGHTS.md). Published main remains Build 24.

---

# Build 28 - October 17, 2026

Local build: opaque track-colored clips, mini-keyboard/note-grid pitch scrolling, expanded editable transport counters and selection fields, independent nudge, selection playback pre/post-roll and Big Counter/display presets. Session format remains 13. [Notes and validation](BUILD28-TRANSPORT-PIANO.md). Published main remains Build 24.

---

# Build 27 - October 17, 2026

Local edit-workflow build: selectable tools; R/T zoom; 256-bar arrangement space with a wider initial ruler; saved track display order/colors; insert-style send slots and controls; no piano roll on click tracks. Session format 13 reads 1-12. [Build notes and validation](BUILD27-EDIT-WORKFLOW.md). Published main remains Build 24.

---

# Build 26 - October 17, 2026

Local mixer-response and shortcut fix. User confirmed drums printed in Build 25. Lightweight fader feedback, Ctrl+= Edit/Mix switching, native browser zoom suppression and fader-focused Space transport are implemented. [Details and validation](BUILD26-MIXER-FIX.md). Real-device confirmation remains pending; published main remains Build 24.

---

# Build 25 - October 17, 2026

Local recording reliability build based on published Build 24 main. Fixes repeated Stop extending latency drain; expands recording, punch, printed playback and session recall regression coverage; supplies a fresh local Windows portable application. [Notes](BUILD25-RECORDING.md) | [Validation](BUILD25-VALIDATION.txt). Live GGD/ASIO testing remains pending.

---

# Current status - Build 24

Updated October 17, 2026. Build 24 promotes the cumulative Build 23 source to main and updates the repository documentation and application About label. No audio-engine or session-format changes in Build 24.

Current source: `Tracksmith/`. Session format: 12. Build 16 is historical and preserved under `build-16-checkpoint` and `prototype-16/`.

[Build 24 notes](BUILD24-RELEASE.md) | [Routing](BUILD22-ROUTING.md) | [Stop fix](BUILD23-STOP-FIX.md)

Synthetic routing/recording and Stop regressions passed in Build 23. Actual loaded GGD/ASIO recording and listening remain pending. Next: verify the user's normal GGD routing, recording and Stop workflow, then address findings before selecting further feature work.

The historical status below predates Builds 17-24 and is superseded by these current notes.

---

# Current status — Prototype 16

The October 15 work is preserved in [prototype-16](prototype-16/BUILD.md). See [changes](prototype-16/CHANGES.md), [native/UI validation](prototype-16/prototype16-test-results.json), [I/O details](prototype-16/IO-SETUP-NOTES.md), and [transport/grid/Elastic Audio status](prototype-16/TRANSPORT-GRID-ELASTIC-NOTES.md).

Current: custom UI and VST3 startup scanner are implemented; native Groove and NAM are integrated; session format 10 opens 1–10. Grid and Dynamic selection punch are implemented. Elastic Audio remains a researched next step. Drum learning is paused, with the prior neural trial optional/off by default. Real hardware, sustained disk-load and listening validation remain pending.

The code archive includes source/artwork/MIDI and excludes large runtime sample/model/executable payloads. Their exact inventory is preserved in external-assets.json. Full local source and portable ZIPs were built and verified; they have not been uploaded as GitHub release assets.

Next: real-interface testing, resolve workflow findings, then design/prototype offline pitch-preserving stretch with user anchors. Preserve original media, old projects, separate Groove release and stock plugins.

## Historical status (superseded where noted above)

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
