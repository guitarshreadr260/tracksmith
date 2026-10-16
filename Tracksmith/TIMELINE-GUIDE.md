# Timeline editing and punch-in

Close Tracksmith, then run the packaged Run-Tracksmith-Release.ps1 with ExecutionPolicy Bypass. It backs up the existing project, applies the complete update, builds Release and opens the application.

## Arrange clips

- Import WAV appends a new clip to the selected lane's end. Drag a clip's middle to move it horizontally; drag its left/right edges to trim. Source WAVs are never rewritten.
- Click a clip to select it. Click the ruler to place the edit cursor; drag across the ruler or an empty lane to highlight a time range. Use the ruler when clips cover the lane.
- Split separates at the edit cursor, or at the highlighted selection boundaries when the selection has length. Trim start/end uses the cursor. Trim to selection keeps the overlapping selected section.
- Snap uses one-beat increments for mouse moves, trims and time selections. Turn it off for free placement. Existing clips stay at their stored seconds when tempo changes; audio is not time-stretched.
- When clips overlap, the last added clip plays on top. Delete it to reveal the older clip. You can undo/redo clip edits; clear/import/open resets the edit history because those operations change the source collection. Punch completion creates one undo entry that restores the surrounding old clip layout.

## Dynamic transport and selection punch

1. Drag on the ruler to highlight the section to replace.
2. Turn on Dynamic transport. Alt-click the ruler before the section to set the green playback start marker. The edit cursor/selection can move without moving that marker.
3. Arm one track, choose the correct input and enable Input monitor on that track. Turn on Selection punch. Other tracks play as backing.
4. Press Numpad 3, F12 or Ctrl+Space. Playback begins at the green marker, optionally after the count-in. The old armed-track clips play before the highlighted section; live input through its inserts is monitored inside it.
5. Capture ends automatically at the highlighted end. Playback continues, with existing clips playing outside the punch interval. The new take is installed at the punch start, and the original audio before/after it remains. Finishing and publishing the clip may briefly interrupt audio while the native graph is updated.
6. Press Space to stop early if needed. Only the recorded portion is replaced; the remainder stays intact. Stopping before punch-in preserves the previous clips. Undo restores the pre-punch layout; the new WAV remains on disk.

If Dynamic transport is off, selection punch starts playback at selection start. The playback marker must be at or before punch start. Normal recording creates a clip at the edit cursor, or at the marker with Dynamic transport on. Turning on Loop repeats the highlighted range, or the whole arrangement when there is no range. Loop is disabled during recording; this is automatic range punch, not continuous loop recording or Pro Tools' manual QuickPunch mode. Transport range/marker, punch mode and keyboard focus are not saved with the session.

## Pro Tools Windows shortcuts implemented

Mappings use Avid's [Pro Tools 12.6 Shortcuts Guide](https://resources.avid.com/SupportFiles/attach/Pro_Tools/12.6/Pro_Tools_Shortcuts.pdf). Only actions currently supported by Tracksmith are mapped. This is not the complete Pro Tools shortcut set or its complete editing/transport feature set. Windows means the Start/Windows modifier in Avid's guide; Windows itself may intercept some combinations.

| Action | Windows shortcut |
| --- | --- |
| Play / stop | Space, Numpad 0 |
| Start recording / finish active take | Numpad 3, F12, Ctrl+Space |
| Stop recording | Space |
| Return to beginning | Enter |
| Save / Open session | Ctrl+S / Ctrl+O |
| New track | Ctrl+Shift+N |
| Switch full-page Mixer / Edit view | Ctrl+= |
| Separate selected clip at cursor/selection | Ctrl+E |
| Trim selected clip to time selection | Ctrl+T |
| Trim start / end to insertion | Alt+Shift+7 / Alt+Shift+8 |
| Clear selected clip | Ctrl+B, Delete or Backspace |
| Undo / redo clip edit | Ctrl+Z / Ctrl+Shift+Z |
| Toggle Dynamic transport | Ctrl+Windows+P |

Enable the **a–z** button for Commands Keyboard Focus: A trims start, S trims end, B separates, Z undoes, Shift+Z redoes, R zooms out and T zooms in. Single-letter commands are off by default. Commands do not run while typing in a text/number field, a selection dropdown or editable text; the field keeps its own standard editing shortcuts. Editing is blocked during recording/finalization. Tracksmith uses its existing single-track creation rather than Pro Tools' New Tracks dialog. Edit and Mixer now occupy separate full-page views within the same application window; Ctrl+= switches between them, and the transport/inspector remain available in both.

## Limits and verification

Each lane supports at most 32 decoded source files and 128 clips; source memory still shares the 512 MiB session budget and 128 MiB per-WAV limit. Deleted clips keep their sources available for undo; Clear WAV frees the lane's source collection. Recording conservatively reserves clip capacity for punch boundary splits. Undo history is capped at 64 edits. Missing sources retain their clip layout and play silence; reopening validates available WAVs before replacing the current session. New saves are version 4 and cannot be opened by earlier Tracksmith builds.

Native tests verify source offsets/gaps, trims/splits, multi-file session loading, legacy sessions, range looping, punch samples across callback boundaries, dry WAV capture, preservation before/after punch, continuing playback and automatic monitoring switching. The Release and frontend builds and shortcut tests pass. The browser preview timed out; mouse interaction and listening through the Volt still require a local user check. Device/plugin recording-latency calibration, automatic fades/crossfades, playlists, simultaneous multi-track capture, cross-track clip dragging, copy/paste and export are not implemented by this update.

For MIDI/instrument lanes and the drum-only plugin, see DRUMS-MIDI-GUIDE.md.
