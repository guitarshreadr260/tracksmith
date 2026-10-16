# Transport, grid and elastic timing — source work

Grid and Dynamic Transport are included in Windows Prototype 16. Drum-learning work remains paused. Elastic Audio remains research/design only.

## Verified reference

Avid, Pro Tools Reference Guide 2025.6:
https://resources.avid.com/SupportFiles/PT/Pro_Tools_Reference_Guide_2025.6.pdf

Relevant printed pages (PDF page numbers are 97 higher):

- 703–707: Dynamic Transport, independent Play Start marker, keypad 1/2 relocation,
  and recording from the marker with punch boundaries supplied by the timeline selection.
- 746: audio punch over a specified range, timeline/edit linking, pre/post-roll.
- 1251–1260: Elastic Audio, analysis, timebases, rendered and real-time processing.
- 1265 onward: separate event markers and user warp anchors.

## Tracksmith source changes

Desired example: at 120 BPM in 4/4, highlight 8–10 seconds. The play start follows the
new selection to 8 seconds. Numpad 1 twice moves only the playback marker to 4 seconds.
Numpad 3 starts transport there; the recording gate opens at 8 seconds and closes at
10 seconds. Space remains start/stop. The target's old take plays during the lead-in,
is suppressed inside the actual recording range, then is available again after punch-out.

- Added a visible Dynamic toggle and independent playback-marker strip.
- Dynamic is enabled by default. A non-empty range in Dynamic mode implies auto punch;
  the separate Punch toggle can still explicitly request range recording.
- Range selection, playback marker and current playback position have separate roles.
- Rewind/forward uses the independent marker in Dynamic mode and does not erase the range.
- Capture graphics now use a logical capture-active flag and the actual punch start.
- Count-in is labeled as count-in; it is not confused with audible timeline pre-roll.
- Loop playback remains an explicit Tracksmith choice. This source work does not copy
  Avid's automatic loop-enabling or all timeline/edit-link preference behavior.
- Current meter is 4/4; one bar means four quarter-note beats.

## Musical grid

- Real bar numbers and beat positions replace the previous misleading ruler labels.
- Grid choices: bar, quarter, eighth, sixteenth, thirty-second, eighth-note triplets.
- Pointer ranges, independent marker positioning and audio clip moving/trimming share
  the selected beat subdivision when Grid is enabled. Slip remains free positioning.
- Strong bar lines, lighter beat lines and finer subdivision lines share one scale with
  the ruler and extend into the unused arrangement area. Lines remain visible over clips.
- Density adapts to zoom/available pixels; subdivisions are hidden when they become too
  close to distinguish, while the selected snap resolution stays explicit.
- Existing piano-roll tools retain their own editor subdivision control.

## Elastic timing: implementation proposal, not implemented DSP

Start with non-destructive manual warp editing and a background-rendered cache. Keep
the recorded source untouched. Use an independent C++ time/pitch engine; the React UI
edits markers and displays processing state. Audio callbacks play ready immutable buffers.

Suggested data separation:

| Layer | Stored data |
| --- | --- |
| Source analysis | File identity/hash, sample rate, frame count, detected events/confidence, optional source tempo |
| Track timing | Sample or beat timebase, enable state, requested processing mode |
| Clip warp edit | Source-frame anchors, destination seconds/beats, pinned edges, transpose/formant options |
| Render cache | Source + edit + tempo + algorithm/version fingerprint, output length, peak envelope |

Analysis view should let the user correct detected events without automatically moving
audio. Warp view should let the user pin and drag timing anchors. Anchors must remain
ordered in source and destination time. One stereo or grouped multitrack edit must use
the same timing map; independent channel stretching would damage relationships.

First milestone: move one late guitar note between two pinned surrounding anchors, retain
pitch, undo/redo the edit and reopen the session with the same result. Then add adjustable
strength/grid quantization and opt-in tempo following on beat-based tracks. A normal sample-
based recording must remain fixed in time when the session tempo changes.

Candidate to evaluate: Signalsmith Stretch, a native C++ pitch/time library under MIT,
with MSVC support documented by its author. Its documentation favors moderate time
changes (roughly 0.75–1.5x) and describes latency, seek and flush requirements. It also
depends on Signalsmith Linear. This is a candidate, not a verified quality substitute
for Avid's proprietary algorithms.

https://github.com/Signalsmith-Audio/signalsmith-stretch
https://github.com/Signalsmith-Audio/linear

An initial rendered implementation needs exact output lengths, original-source re-rendering,
stereo-aware processing, render cancellation/revision checks, disk cache eviction, audible
transition checks and a session format update (current format is version 9). Waveforms must
come from the rendered result. Repeated edits must not repeatedly stretch previously
rendered files. No hidden automatic tempo estimates should override a user's source tempo.

Later material-specific behavior should be evaluated with drum transients, single-note
bass/lead lines, strummed guitars/chords and full stereo mixes. Grouped drum edits need
separate phase/coherence evaluation. Real-time stretching and additional processing modes
should follow listening and timing tests rather than being exposed as unverified features.

## Validation status

Passed without an application build: TypeScript no-emit checks, actual grid-module math
checks, and source-preview UI checks for grid selection, preserved ranges on keypad
rewind, pre-punch waveform suppression and recording-region placement.

Native Release compilation and transport regressions passed for Prototype 16. An automatic-punch test splits one audio callback across punch-in and punch-out, verifies old-target playback before/after and silence inside, and checks the exact dry WAV frame count. Real-interface latency and monitoring still need hardware checks.
Elastic DSP remains research/design work; no stretching engine was integrated here.
