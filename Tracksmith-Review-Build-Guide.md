# Tracksmith review build — October 14, 2026

## Open this build

1. Save your work and close the older Tracksmith instance.
2. Extract the entire ZIP to a new folder. Keep Stock Plugins beside Tracksmith.exe.
3. Open Tracksmith.exe. Windows x64 and WebView2 are required.
4. Use a copy of an important session for review. Saves use version 9; older builds cannot open version 9 saves.

This is a portable review build, before the broader realtime reliability pass and Windows installer. Your earlier build ZIPs remain preserved.

## Suggested review

- Import/play audio and check that silent track meters stay dark. Try the Edit Grid ON/OFF and zoom.
- Check input monitoring while existing files play. Arm two audio tracks with their own inputs and try Monitor armed tracks. Record, then retake a section and check that the new take replaces that range.
- Recording alignment defaults ON and uses reported device input/output plus plugin latency. Check take timing on your interface. It is not a measured hardware calibration.
- Create an aux, try Pre/Post-fader sends, and choose a separate hardware output pair if your interface has one enabled. Master volume does not control a separate cue aux.
- Create, duplicate, reorder, rename and delete playlists. Undo should restore a deleted playlist. Source files are retained. Highlight a range and use Replace active range from an alternate take.
- Check Group on audio/MIDI tracks, highlight a range, and try group Split/Delete/Move. Undo should restore the whole batch. Group edits are explicit toolbar operations; ordinary drags stay single-track.
- Try volume/pan and plugin Read/Write/Touch/Latch. Turn Loop off before writing. Native plugin Touch/Latch requires gesture reporting. Audio recording and automation writing use separate passes.
- Save, close and reopen your review session. Check routing, playlists, plugins and automation.

## Autosave and recovery

Autosave defaults ON. Every two minutes, it snapshots new unsaved changes while the session is stopped and idle. It waits during playback, recording, automation writing, loading, plugin operations and export. It does not change the manual session file or clear the unsaved-changes indicator. Snapshot now writes immediately while stopped and idle. The autosave switch applies to the current app run.

Snapshots retain session metadata, playlists, automation, routing and plugin state, and reference existing audio files. They do not copy audio or finalize an interrupted recording. Only changes captured in a completed snapshot can be recovered.

Recovery folder opens the local Tracksmith recovery directory. Each document run retains three rotating snapshots; the ten most recent runs are retained. Older recovery copies rotate out. A successful manual save or an explicitly confirmed close/discard marks that run clean while keeping its snapshot files. A new unsaved snapshot makes the run eligible again.

After an interrupted run, the recovery panel offers the newest valid snapshot from each retained unclean run. A corrupt newest snapshot falls back to an earlier valid one. Recover copy opens an unsaved copy and asks for a filename when you save, preserving the manual original. Not now hides the panel without deleting recovery files. Copy an older snapshot out of the recovery folder before opening it manually if you want to keep that checkpoint unchanged.

There is no need to deliberately crash the app during this review; interrupted-run and damaged-snapshot fixtures have already been tested automatically. Normal close after confirming discard intentionally suppresses a recovery offer for that run.

## Validation and limits

Native Release build, production frontend, TypeScript and shortcut checks pass. Automated tests cover snapshot rotation, corrupt-snapshot fallback, original-file preservation, media path rebasing, session reopening, worker request guards, clean markers and retention. A 128-track metadata fixture with playlists/automation and WAV references also saves and reopens correctly. Existing recording, editing, monitoring/routing, automation, VST3 and export checks pass.

This does not establish realtime performance with 128 heavy plugin tracks. Broader realtime reliability, hardware listening, native visual verification and installer work remain pending. Native plugins still run in-process. Plugin scanning improvements and the custom UI remain deferred. Tracksmith Groove remains a separate optional VST3.

When reporting an issue, note what you were doing, whether playback/recording was running, the track/input/plugin involved and what you expected. Keep an affected session copy if possible.
