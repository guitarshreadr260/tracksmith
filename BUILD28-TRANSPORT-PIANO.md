# Build 28 - solid clips, pitch scrolling and transport controls

Based on Build 27 (`ce3e84c`). The user's transport screenshots guide the layout and functional controls. Earlier builds remain unchanged.

## Solid timeline colors

Audio clips now use the exact opaque track color instead of a dark mixed background. Selection retains that fill and uses an outline. Waveforms/text choose light or dark ink according to the track color. Inline MIDI notes are opaque instead of using velocity-dependent transparency. Track color assignment and session persistence are unchanged.

## Piano-roll scrolling

Octave up/down buttons are removed. The mini-keyboard has small up/down arrows and a vertical pitch scrollbar. Wheel scrolling over the mini-keyboard or inline note grid changes the visible register, one semitone per step; dragging the keyboard scrolls it too. The keyboard and notes share the same top pitch, clamped so all MIDI pitches 0-127 can be reached. Fit Notes remains. Plain scrolling is supported for convenience, as is the Pro Tools-style Ctrl+Alt+Windows modifier chord (Control+Option+Command on Mac-style keyboards). Shift-wheel is left to horizontal navigation in the note grid. Scrolling changes the view, not MIDI note pitches. Click tracks remain without a piano roll.

## Transport controls

- Main counter: Bars/Beats, Min:Secs or Samples. Enter a new position and press Enter or leave the field to locate there.
- Sub counter: independently selectable format, following playback. Cursor location readout follows the edit/hover cursor.
- Selection Start, End and Length: editable in the Main counter format. Durations use a zero-based bar/beat length; positions use bar 1 / beat 1. Invalid values are rejected, and selection bounds stay within 24 hours.
- Grid and Nudge have separate values. Nudge buttons and Alt+Left/Right use the Nudge setting for selected audio clips; without an audio clip, the buttons move the edit cursor. Arrow-key cursor movement also uses Nudge.
- Play Selection: optional playback pre/post-roll of 0/1/2/4/8 bars. Playback starts before the selection and stops at the planned end plus post-roll, clamped to available media. Loop must be off. This is selection audition, not a change to recording punch/pre-roll. The existing Count-in control remains separate.
- Rewind/Forward buttons move one bar using the existing shuttle action.
- Transport display menu: show/hide counter, selection, grid/nudge, roll and output sections; Minimal/All presets; resizable Big Counter panel inside the application.
- Output peak readout is dBFS. Clip Fade-in edits the selected audio clip's existing fade, not a recording-monitor fade.

Session format remains 13. Counter/display, pitch view and selection-roll settings are UI/session-run view state; they are not newly saved project properties. No external synchronization, MIDI wait-for-note, send automation or independent native Transport window is added.

## Validation

- Production TypeScript/Vite build and all nine UI suites pass. New timing tests cover bars/time/samples position and duration round trips at several tempos/sample rates, invalid input and pre/post-roll clamping. Audio nudge is verified independently from Grid.
- Headless Edge checks pass: exact opaque clip RGB/contrast; keyboard arrow, wheel, modifier-wheel and drag synchronization; retained MIDI editing; counter entry and selection fields; nudge; selection-roll request; Big Counter; display presets; invalid counter feedback; clip fade-in. Earlier routing, track colors/order, tools, click-track exclusion, clipboard, Time Warp and transport checks pass without browser errors. Browser state is mocked.
- New native transport test passes: pre/post-roll plan bounds and validation, exact selection playback endpoint with 128-sample latency, and no endpoint leakage into recording, export or reset.
- Native recording integration, full session suite and mixer/bus/send suite pass. Existing separate kick/snare WAV recording, Stop/drain, punch and printed playback checks remain covered.
- Windows Release builds with ASIO enabled. Packaged executable checks are recorded in the BUILD28 regression reports. Existing compiler warnings remain.

Live GGD/ASIO testing of the new controls remains for the user. The screenshots use mocked state. Native selection-end tests use controlled audio, not Kontakt/GGD. The selection audition boundary is transient and does not affect offline export or recorded take bounds.

## References

[Avid Notes view scrolling](https://apps.avid.com/protoolsfirsthelp/version12.3/enu/Pro%20Tools%20First%20Help/ed1.basics.23.16.html) and [Avid toolbar counters](https://apps.avid.com/proToolsFirstHelp/version12.3/enu/Pro%20Tools%20First%20Help/ed3.selections.25.26.html), plus the user's supplied screenshots.

Close the prior build and run `portable/Tracksmith.exe`, keeping the supplied folders beside it. Save a session copy for testing. No GitHub push or installer/release upload is included in this local build.
