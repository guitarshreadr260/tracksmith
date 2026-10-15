# Tracksmith

Private development reference for **Tracksmith DAW** and the optional **Tracksmith Groove** VST3, owned by guitarshreadr260.

Updated October 14, 2026 — **GGD MIDI Fix + first reliability checkpoint**.

## Start here

- [Current progress, decisions and next steps](PROJECT-STATUS.md)
- [Latest Windows DAW build](Tracksmith-Windows-GGD-MIDI-Fix.zip)
- [Latest DAW source snapshot](Tracksmith-Source-Reliability-Checkpoint.zip)
- [How to highlight notes, delete tracks and use the new shortcuts](Tracksmith-Selection-Shortcuts-Guide.md)
- [Build history](BUILD-HISTORY.md)
- [Working away from your PC](WORKING-AWAY-FROM-PC.md)

Download and extract the complete Windows ZIP, save and close the older Tracksmith, then open Tracksmith.exe. Keep Stock Plugins beside the application. Windows x64 and WebView2 required. Session format is now **9**, reading versions 1–9. Earlier builds may not open newer saves; use a session copy for review.

## What is included

Audio recording and monitoring, retake replacement, fades/crossfades, audio and MIDI playlists, group range editing, VST3 hosting, aux buses and pre/post sends, separate hardware cue outputs, volume/pan/plugin automation, latency alignment, autosave/recovery, WAV export and the Edit grid.

Instrument tracks now have a piano roll directly in the Edit window. Draw and edit notes, import MIDI grooves from compatible drum programs, highlight groups with a rectangle or Shift-click, delete/move/resize them together, copy/cut/paste/duplicate, transpose and nudge. Right-click track names in Edit or Mix for New/Delete. The Arrangement toolbar includes a Shortcuts panel.

The user confirmed the GGD MIDI Fix build works. The first bounded automated reliability pass also passed: two runs totaling about three minutes with 80 synthetic tracks and 16 controlled VST3 effects. Longer hardware/interface, representative-plugin and normal-UI checks remain before final installer readiness. Windows installer and custom UI are pending. Plugin scanning improvements remain deferred.

## Latest checkpoint

- [GGD import fix and its checks](Tracksmith-GGD-MIDI-Fix-Guide.md): all 225 saved GGD Rock grooves now import; duplicates and retriggers are normalized, import results are visible and imported notes scroll into view.
- [Reliability report](Tracksmith-Reliability-Report.md): 152 recorded WAVs checked, 20 session reopens, 20 recovery snapshots, 10 plugin reloads and 90 edit cycles. No crashes, recording warnings or timed blocks over budget in these runs.
- [Reliability raw results and reusable harness](Tracksmith-Reliability-Results.zip)

The current source checkpoint includes the reliability harness. The working application is the GGD MIDI Fix build; reliability testing required no production-code changes. These are synthetic engine tests, not measured ASIO/device xruns or a multi-hour heavy-instrument test.

## Current guides

- [Selection, track deletion and shortcuts](Tracksmith-Selection-Shortcuts-Guide.md)
- [Instrument piano roll and MIDI groove drops](Tracksmith-Instrument-Piano-Roll-Guide.md)
- [Autosave/recovery review guide](Tracksmith-Review-Build-Guide.md)
- [Playlist management and group edits](Tracksmith-Playlist-Group-Editing-Guide.md)
- [Monitoring, routing and hardware cues](Tracksmith-Monitoring-Routing-Guide.md)
- [Plugin parameter automation](Tracksmith-Plugin-Automation-Guide.md)
- [Tracksmith Groove VST3](Tracksmith-Groove-VST3-Guide.md)
- [Latest validation and limits](Tracksmith-Selection-Shortcuts-Validation.txt)

Track deletion is not undoable and clears prior edit histories; source audio files remain on disk. MIDI note-group edits support Undo. See the guide before removing a track.

## Tracksmith Groove

Tracksmith Groove 0.18 remains a separate optional VST3 instrument; [its release downloads](../../releases/tag/groove-v0.18.0) and [code snapshot](Tracksmith-Groove-Source-Code-v0.18.zip) are preserved. Original AVRX Groove 0.17 is retained locally. AVRX Drums remains excluded from the DAW.

## About this repository

This repository stores source ZIP snapshots, builds, guides and progress notes. Source is not yet an expanded Git working tree; extract the source ZIP to browse or build it. Third-party licenses remain applicable. Repository visibility stays private. Older archives remain for reference; README links point to the current DAW build.

GitHub receives updates when we publish them. It does not automatically mirror the home PC or the live development conversation.
