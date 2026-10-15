# Where we left off

Updated October 14, 2026. Latest delivery: **Selection and Shortcuts build**.

## Most recent request and result

The user reported being unable to highlight/delete MIDI notes and requested right-click track deletion plus more Pro Tools-style controls. The latest build adds rectangle and Shift-click MIDI selection, atomic group deletion with Undo, group movement/resizing, an internal MIDI clipboard, duplicate, transpose, velocity, note navigation and nudge shortcuts. Instrument tracks default to Select; F8 selects and F10 draws. Right-click a track name in Edit/Mix for New/Delete. A Shortcuts panel is inside the Arrangement toolbar.

The controls were checked against Avid documentation; this is a supported subset, with Tracksmith-specific behavior documented in the [guide](Tracksmith-Selection-Shortcuts-Guide.md). Track deletion cannot Undo, stops playback and clears histories. Source audio is retained; surviving track media, instruments, inputs and bus routes are remapped.

[Latest Windows build](Tracksmith-Windows-Selection-Shortcuts.zip) · [Current source snapshot](Tracksmith-Source-Selection-Shortcuts.zip) · [Validation](Tracksmith-Selection-Shortcuts-Validation.txt)

## Recent progress since the initial GitHub upload

- Plugin parameter automation and Read/Write/Touch/Latch; native Touch/Latch requires plugin gesture reporting.
- Recording alignment using reported driver input/output and plugin latency; hardware timing still needs listening/calibration checks.
- Armed-track monitoring with assigned inputs, pre/post sends and separate physical stereo cue outputs.
- Playlist create/duplicate/rename/reorder/delete, alternate lanes, range promotion and explicit multi-track range edits.
- Edit grid and corrected silent meter display.
- Autosave/recovery with background guards, rotating snapshots, corrupt-snapshot fallback and recovered-copy Save As. Snapshots reference audio files; they do not copy media.
- Instrument-track choice and inline piano roll in the Edit window.
- MIDI groove file drops at the drop position from drum programs exposing .mid/.midi; proprietary drag formats need export first.
- Latest MIDI selection fix, batch editing, context menus and keyboard workflow.

## Validation and current limits

Native Release and production frontend compile. TypeScript, keyboard/focus checks and note-edit math pass. Native tests cover atomic batch edits, MIDI groove imports, track deletion/remapping and existing VST3/MIDI playback, sessions/recovery, routing, clips and automation.

The 128-track recovery fixture tests session metadata with WAV references, playlists and automation. It does not demonstrate realtime performance with 128 heavy plugin tracks.

The user said the previous Instrument build was working, then reported the selection issue. The latest Selection and Shortcuts build has been delivered; its mouse gestures, native menus and hardware listening have not yet been confirmed by the user. Do not mark those manual checks complete.

Session format is **9**, opening versions 1–9. Keep older session copies. Plugins run in-process. MIDI keyboard recording and note audition are still future work. The MIDI clipboard is internal to the current app run.

## Next

1. Review the latest selection/deletion/context-menu workflow and fix any findings.
2. Run the broader realtime reliability and hardware pass.
3. Prepare the Windows installer.
4. Continue with the custom UI after core behavior and reliability are ready.

Plugin scanning improvements remain deferred by user request. This list records the direction, not an instruction to start unrelated work automatically.

## Decisions to preserve

- Keep the DAW core separate from Tracksmith Groove 0.18, the optional VST3.
- Preserve original AVRX Groove 0.17 and existing source/build backups.
- AVRX Drums stays excluded.
- Core workflow before custom UI.
- Retakes replace the covered audio range; source audio files stay intact.
- Maintain portable review builds while the installer is pending.

This is a saved handoff. GitHub is not a live mirror of the development PC or conversation.
