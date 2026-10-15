# Where we left off

Updated October 14, 2026. Latest app: **GGD MIDI Fix**, user confirmed working. Latest checkpoint: **first automated reliability pass completed**.

## Current checkpoint

The user confirmed the GGD fix works and asked for a reliability test. The first bounded automated reliability pass passed. Two paced synthetic runs lasted about three minutes total: 80 tracks (64 audio, 8 aux, 8 MIDI), 16 controlled delay-test VST3 instances, eight simultaneous recording writers, repeated seeks/edits, plugin replacement and file lifecycles.

- 17,833 audio blocks; 0 exceeded their individual processing-time budgets.
- 152 recorded WAV files checked across 19 recording passes.
- 20 session save/reopen cycles and 20 validated recovery snapshots.
- 90 edit cycles and 10 VST3 unload/reloads.
- No test failures, production crashes or recording writer warnings observed.
- Second-run average render/capture processing: 13.9% of available frame time. Peak private process memory: 49.25 MiB. Detailed measurements and short-run limits are in the report.

The 16 effects are controlled delay fixtures; MIDI lanes are not eight heavy third-party instruments. This does not measure device callbacks, ASIO xruns, real microphone timing or multi-hour behavior. No production changes were needed for this reliability pass. The running user DAW/session was untouched.

[Working Windows build](Tracksmith-Windows-GGD-MIDI-Fix.zip) · [Current source with test harness](Tracksmith-Source-Reliability-Checkpoint.zip) · [Reliability report](Tracksmith-Reliability-Report.md) · [Raw results and harness](Tracksmith-Reliability-Results.zip)

## GGD import regression — fixed

The user reported accepted groove drops with no visible notes. Saved GGD files reproduced an import rejection involving duplicate same-pitch/channel hits at the same tick and zero-duration gates produced by note matching. The fix merges identical-tick duplicates with the strongest velocity, retains distinct flams/retriggers and normalizes gates without changing source MIDI files. All 225 saved GGD Rock grooves import. Clear/reimport and conflict rollback tests pass. Imports now report success or failure and scroll/select imported notes. The user confirmed everything works.

[GGD guide](Tracksmith-GGD-MIDI-Fix-Guide.md) · [GGD validation](Tracksmith-GGD-MIDI-Fix-Validation.txt)

## Previous milestone: selection and shortcuts

The user reported being unable to highlight/delete MIDI notes and requested right-click track deletion plus more Pro Tools-style controls. The latest build adds rectangle and Shift-click MIDI selection, atomic group deletion with Undo, group movement/resizing, an internal MIDI clipboard, duplicate, transpose, velocity, note navigation and nudge shortcuts. Instrument tracks default to Select; F8 selects and F10 draws. Right-click a track name in Edit/Mix for New/Delete. A Shortcuts panel is inside the Arrangement toolbar.

The controls were checked against Avid documentation; this is a supported subset, with Tracksmith-specific behavior documented in the [guide](Tracksmith-Selection-Shortcuts-Guide.md). Track deletion cannot Undo, stops playback and clears histories. Source audio is retained; surviving track media, instruments, inputs and bus routes are remapped.

[Selection and Shortcuts Windows archive](Tracksmith-Windows-Selection-Shortcuts.zip) · [Selection and Shortcuts source archive](Tracksmith-Source-Selection-Shortcuts.zip) · [Validation](Tracksmith-Selection-Shortcuts-Validation.txt)

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

The user said the previous Instrument build was working, then reported the selection issue. The GGD fix subsequently superseded Selection and Shortcuts, and the user confirmed the working build. Formal hardware listening and longer representative-plugin/UI checks remain pending.

Session format is **9**, opening versions 1–9. Keep older session copies. Plugins run in-process. MIDI keyboard recording and note audition are still future work. The MIDI clipboard is internal to the current app run.

## Next

1. Run a longer hardware/interface session with representative plugins and normal UI use. The initial synthetic reliability pass is complete.
2. Fix any findings before treating reliability as finished.
3. Prepare the Windows installer when those checks are satisfactory.
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
