# Tracksmith development plan

Updated October 14, 2026.

Windows DAW using the current React/TypeScript interface with JUCE native audio. Custom UI design is deferred until more core features are complete. AVRX Drums is removed. AVRX Groove v0.17 is preserved separately for future work. AVRX NAM remains a separate guitar VST3.

## Working foundation

- WAV recording, multitrack playback, live processed monitoring and Volt/ASIO support.
- Save/Open, track naming, gain, pan, mute, solo, arming and master controls.
- Five VST3 inserts per track/master, plugin editors, saved states and bounded playback latency compensation.
- AVRX NAM guitar plugin, metronome, pre-roll and supported Windows editing shortcuts.
- Multiple clips, movement, trimming, splitting, deletion, undo/redo, beat snapping, selected-range looping and selection punch.
- General MIDI/instrument lanes, piano-roll note editing and MIDI import/export. Hardware MIDI recording remains pending.
- Stereo WAV export through the mix and plugins: full session/selection, 16/24-bit PCM, current sample rate, effects tail, progress/cancel and safe destination replacement. See EXPORT-GUIDE.md.

## Next builds

1. Recording: separate track inputs and simultaneous capture up to eight armed audio tracks are implemented. Next: hardware latency-aligned takes and multi-track live monitoring.
2. Routing: implemented stereo aux/bus tracks, two post-fader sends, source output selection, saved routing and aux-stage delay compensation. Next: pre-fader sends, nested buses, wider hardware routing and stereo channel meters.
3. Editing: fades/crossfades, take playlists and comping.
4. Plugin workflow: cached browser, loading recovery, sidechains and automation.
5. Reliability: large-session streaming, recovery, stress testing and installation packaging.
6. Custom UI: resume Main Arrangement design when the core workflow is further developed.

Export follow-ups: selectable sample-rate conversion, dithering/normalization, wider plugin compatibility tests. Current plugin instances are used exclusively while live playback is detached during export.

Recording update: existing clips play with live input through one track/master insert path; per-track input routes persist in sessions, and grouped capture shares count-in/punch boundaries. See RECORDING-GUIDE.md.

Routing milestone: see MIXER-ROUTING-GUIDE.md. Sessions now save as version 5 and read versions 1–4.

Implemented retakes, linear fades, crossfades and audio/MIDI playlists with selected-range comp copying. New saves use version 6 and read versions 1-5. See TAKES-PLAYLISTS-GUIDE.md. Loop playlists, ratings, playlist deletion/reordering and grouped comping remain future work.


Plugin browser milestone: cached per-bundle VST3 discovery in a child process, saved-name/manufacturer search, interrupted-loading recovery and preserved-state retry, plus Ctrl+Shift+N New Tracks dialog with quantity/name/input format. Next: volume/pan/plugin automation. Full plugin runtime isolation and folder scanning remain future work.


Automation milestone: audio/instrument/aux track volume and pan lanes, Off/Read/Write, buffered transport-clock capture, interval replacement and grouped automation undo, version 7 persistence, sample-level curve rendering and source/aux latency alignment. Plugin scanner deferred as requested. Next: plugin parameter automation and Touch/Latch.

Plugin automation milestone: per-insert stable-ID parameter lanes, Read/Off, point editing, bounded per-insert undo/redo, missing-plugin/control retention, version 8 persistence and block-resolution playback/monitor/export. Silent track meters corrected by removing an obsolete important background rule. Next: Touch/Latch and plugin Write capture; scanner and custom UI work remain deferred. See PLUGIN-AUTOMATION-GUIDE.md.

Combined milestone: implemented track and plugin Touch/Latch, plugin Write capture, global automation compare, reported driver/graph recording alignment with capture-tail handling, and zoom-aligned Edit beat/bar grid. Automated tests pass; hardware timing and native visual/gesture checks remain pending. User prefers internal milestones until custom UI work. Next group: multiple-track monitoring, pre-fader sends and hardware outputs. Scanning and redesign stay deferred. See COMBINED-BUILD-GUIDE.md.

Monitoring/routing milestone: armed-track monitoring, two pre/post-fader sends, physical stereo hardware destinations for aux returns (3–4 through 15–16), sparse input/output handling, main-only export and version 9 session persistence. Automated integration checks pass; hardware listening and native visuals pending. Next: playlist management and group editing. Preserve internal milestones until custom UI review. See MONITORING-ROUTING-GUIDE.md.

Playlist/group editing milestone: playlist reorder/delete/rename with bounded snapshot Undo; explicit track membership and atomic group range split/delete/move across audio/MIDI; clear range-comp labels and empty-source rejection. Session schema remains version 9. Native visuals pending; automated tests pass. Next: autosave/recovery and reliability, then custom UI. See PLAYLIST-GROUP-EDITING-GUIDE.md.

Review milestone: autosave/recovery with three snapshots per document run, ten-run retention, clean markers, corrupt-snapshot fallback, recovered-copy Save As and guarded background workers. Snapshot/reopen tests include 128-track metadata. Portable review build requested before realtime reliability and installer work. See REVIEW-BUILD-GUIDE.md.

Instrument milestone: inline Edit piano roll, Instrument track creation, draw/move/resize/velocity/channel, MIDI groove file drops with atomic append and Undo. Native import and frontend math checks pass; specific drum-program drag and hardware/native interaction remain manual checks.

Selection/shortcuts milestone: default marquee tool, Shift selection, atomic group MIDI deletion/movement/resize and clipboard, transpose/velocity/navigation/nudge shortcuts, F2/F4 snap modes, import shortcuts and in-app help. Native right-click track New/Delete with routing and slot remapping, source files retained. Track deletion clears histories and cannot undo. Native/frontend checks pass; physical UI review pending.

GGD import fix: duplicate-tick and zero-duration trigger normalization, preserved distinct flams/retriggers, capture drops over whole editor, explicit import acknowledgements including busy states and automatic drum-register selection/scroll. All 225 saved GGD Rock MIDI files import; native/frontend regressions pass. Live GGD mouse drag remains manual review.

Initial reliability pass: 60s + 120s paced synthetic workloads with 80 tracks, 16 controlled VST3 effects, eight recording writers, edit cycles, session/recovery and plugin lifecycle checks pass. Application unchanged. A longer device/hardware and representative plugin/UI run remains before final installer readiness. See local Tracksmith-Reliability-Report.md.

First Windows x64 MSI preview authored for the working GGD build: per-user install, Start menu shortcut, stock NAM/notices/guides, runtime checks and uninstall metadata. Package/preflight/cabinet hash validation passes; actual install/repair/uninstall is pending because the restricted test environment cannot connect to Windows Installer service. No production application change. See INSTALLER-GUIDE.md.
