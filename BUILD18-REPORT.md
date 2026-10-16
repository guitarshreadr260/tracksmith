# Tracksmith Build 18 — editing and insert transfer

Created on the separate local `build-18-editing` branch/worktree from Build 17 commit `b042425`. Builds 16 and 17 were left unchanged. This build has not been pushed or merged.

## Controls

| Action | Windows control |
|---|---|
| Timeline zoom out / in | R / T |
| Switch Edit / Mix | Ctrl+Shift+M |
| Move an audio clip | Drag its middle |
| Duplicate an audio clip in its lane | Alt-drag its middle |
| Duplicate immediately after selected clip | Ctrl+D or Clip menu |
| Trim | Drag either edge, or use numeric clip editor |
| Ignore grid for a drag | Hold Shift |
| Nudge selected clip by grid interval | Alt+Left / Alt+Right |
| Trim selected clip to time selection | Ctrl+T |
| Fade | Drag upper corner handles, use numeric editor, or Clip menu |
| Crossfade with previous clip | Numeric clip editor or Clip menu |
| Cancel an uncommitted clip drag | Escape |
| Move a plugin insert | Drag to an empty insert on the same/another track |
| Copy plugin settings and automation | Alt-drag to an empty insert |

Avid documents Alt-drag for Windows clip duplication in its [Edit/Mix shortcut guide](https://apps.avid.com/ProToolsFirstHelp/version2018.4/enu/keyshortcuts.6.10.html), plain drag to move an insert and Alt-drag to copy it in [Moving and Duplicating Plug-In Inserts](https://apps.avid.com/protoolsfirsthelp/version12.3/enu/Pro%20Tools%20First%20Help/plugs1.plugins.40.10.html), and Ctrl+D to duplicate after the selection in [Duplicating Clips](https://apps.avid.com/protoolsfirsthelp/version12.3/enu/Pro%20Tools%20First%20Help/Arr3.clips.33.26.html). Tracksmith uses these Windows mappings. It is not a full implementation of Pro Tools editing modes.

Ctrl+= is no longer assigned to the mixer because it conflicts with browser page zoom. Host/browser zoom can still respond to Ctrl+=; R/T controls the arrangement zoom and Ctrl+Shift+M switches workspaces.

## Implementation

- New AudioClip component with source-bounded drag previews, snap/Shift override, precise pointer-release commits, cancel handling, fade handles and Alt-drag duplication. A click without a meaningful drag does not send a clip edit.
- New ClipInspector for numeric clip position, start/end trim, fade durations, duplication and crossfade length. Each edit uses the existing native clip history.
- New pure clipEditing helpers and tests for geometry, bounds, snapping and fade limits.
- Native ClipLayout duplicate operation preserves source, offset, duration and fade envelopes; rejects duplicate IDs, invalid placement and capacity limits. Audio is referenced non-destructively; no extra WAV file is written.
- Native duplicate results select the new clip in Studio so repeated Ctrl+D appends copies. Bridge types include the new operation and result.
- Clip menu includes duplicate, nudge, fades and crossfade. Audio selections must resolve to an existing clip. Piano roll is rendered only for MIDI tracks.
- Insert drag/drop sends an explicit move/copy command. Native transfer validates source/destination, track/slot compatibility and source token; protects occupied slots and rejects transfers during recording, writing automation or other busy operations.
- Insert copy creates an independent instance using plugin state, bypass and automation curves/read status. Copied automation write modes are restored as Read/Off, matching existing session-restoration behavior. Insert move transfers the existing instance and automation. Instruments require insert A of a MIDI track; native click instruments use Create Click Track instead.
- Native diagnostic for insert transfer and an additional CMake target for clip regression tests.

The stock plugins/runtime assets in portable/ are copies from the previous portable build. Stock plugin source and DSP were not changed. LegacyApp only exports its existing Waveform renderer for reuse; Studio uses the new AudioClip component.

## Validation

- PASS: TypeScript and Vite production build.
- PASS: clipEditing.mjs, commands.mjs, shortcuts.mjs, midiEditing.mjs, midiDrop.mjs and midiKeyboard.mjs.
- PASS: headless Edge browser checks for numeric position edits, duplicate/nudge shortcuts, Alt-drag clip-copy dispatch, Escape cancellation, mixer shortcut, insert copy/move dispatch, duplicate selection and consecutive Ctrl+D placement. No browser page errors. Browser checks simulate native state.
- PASS: ASIO-enabled MSVC Release app build. Existing compiler warnings remain. Separate stock plugin builds disabled for this validation; packaged stock binaries are reused.
- PASS: StudioClipTests: existing overwrite/fade/crossfade/source-handle checks plus duplicate source/envelope preservation, ID collision and destination bounds.
- PASS: StudioSessionTests: session serialization/validation.
- PASS: actual app --insert-transfer-regression: independent native NAM copy, parameter state, bypass, automation, move/source clearing and occupied/invalid destination protection. See BUILD18-insert-regression.txt. This does not certify every third-party VST3's state implementation.
- PASS: actual app --editing-regression: duplicate undo/redo, audio/MIDI delete undo/redo across selected-track changes, range deletion/undo, waveform extraction and neural groove generation. See BUILD18-editing-regression.txt.

## Package and remaining limits

Run portable/Tracksmith.exe with its sibling Native Instruments and Stock Plugins folders. Close the earlier app first for manual audio testing.

Cross-track insert transfer is supported. Audio Alt-drag currently duplicates within its existing lane; cross-track clip transfer, multi-clip group selection, ripple editing, time stretching and consolidated renders are not added here. Existing range and playlist editing remain available. Crossfades require touching/overlapping neighbors with sufficient source handles; the native engine reports an explanation when it cannot create one. Drag edits stop playback through the existing engine editing path.

Insert drops require an empty destination instead of replacing an occupied insert as Pro Tools can. Insert transfers are saved in session snapshots but do not use the clip/MIDI undo history. Clips retain their existing undo/redo support.

Live ASIO interface selection, hardware recording/monitoring, third-party VST3 copying and listening quality still require manual checks. Automated native diagnostics use fixtures without exercising physical hardware. No GitHub push or merge was performed for Build 18.
