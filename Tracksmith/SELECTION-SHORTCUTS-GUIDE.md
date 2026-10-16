# Tracksmith selection and shortcuts build — October 14, 2026

Save and close the older Tracksmith. Extract the entire ZIP to a new folder and open Tracksmith.exe. Keep Stock Plugins beside it. Windows x64 and WebView2 required. Use a session copy for review. Session format stays version 9 (reads 1–9).

## Highlight and delete MIDI notes

Instrument tracks now start in Select mode. Drag a rectangle across notes to highlight them. Any touched note is selected, including sustained notes crossing the rectangle. Shift-click adds/removes individual notes; Shift-drag adds a rectangle to the current selection. Shift-drag also selects while Draw mode is active.

Press Delete, Backspace or Ctrl+B, click Delete notes, or right-click a selected note and choose Clear selected. The whole selection is deleted in one Undo step. Ctrl+Z restores it. Ctrl+Shift+Z redoes it. Drag a selected note to move the group together, preserving timing and pitch intervals. Drag a selected note's right edge to resize the group together; the shortest note and timeline bounds limit the change. Native validation rejects same-pitch/channel overlaps without partially applying the change.

F8 or Ctrl+4 selects the Select tool; F10 or Ctrl+6 selects Draw. In Draw mode, Alt-click a note to erase it. Escape deselects. Right-click the note grid for Copy, Cut, Paste, Duplicate, Clear or Select all.

## MIDI shortcuts

| Keys | Action on the active Instrument track |
| --- | --- |
| Ctrl+A | Select all notes in its active playlist |
| Ctrl+C / Ctrl+X | Copy / cut selected notes |
| Ctrl+V | Paste at the last clicked empty-grid position |
| Ctrl+D | Place a copy immediately after the selected notes' latest end |
| Up / Down | Transpose selected notes by a semitone |
| Shift+Up / Down | Transpose by an octave |
| Alt+Up / Down | Copy and transpose by a semitone |
| Alt+Shift+Up / Down | Copy and transpose by an octave |
| Ctrl+Up / Down | Change velocity by 5 MIDI units |
| Ctrl+Shift+Up / Down | Change velocity by 15 MIDI units |
| Left / Right | Select previous / next note in time and pitch order |
| Shift+Left / Right | Extend selection to previous / next note |
| Numeric keypad + / − | Nudge selected notes by the lane's note-grid division |

Copy/paste uses an internal MIDI clipboard shared between Instrument tracks during this app run; it is not the operating system clipboard. Pasting retains pitch, channel, velocity, duration and relative spacing. Channel/velocity controls apply to all selected notes and display the first selected note's value. MIDI keyboard recording, live audition and MIDI controllers are still outside this build's scope.

## Track context menu

Right-click the track name/header in Edit, or its name in Mix. Choose New track or Delete track. The menu explicitly labels deletion as unable to Undo. Save a session copy if you want to retain the arrangement before removing a track. At least one track stays in the session.

Deleting a track removes its playlists, clips, notes, inserts and automation from the session; source audio files remain on disk. Sends to a deleted bus are disconnected, and tracks routed to it return to Master. References to later buses follow their new positions. Remaining tracks retain media, automation, instruments and input assignments. Track deletion stops playback and clears earlier edit histories so they cannot apply to a different track after the track order changes. Master inserts stay fixed.

## More editing controls

F2 turns snapping off for free placement; F4 enables snapping. Ctrl+Shift+I imports audio on an Audio track; Ctrl+Alt+I imports MIDI on an Instrument track (existing Import MIDI replaces the lane). Click Shortcuts in the Arrangement toolbar for these mappings and the earlier transport, editing, Save/Open, Undo/Redo and new-track shortcuts. Text and numeric input fields retain normal typing/editing behavior.

Groove MIDI drops and the inline piano roll remain included. See INSTRUMENT-PIANO-ROLL-GUIDE.md for loading a VST3 instrument, importing drum grooves and the MIDI limits. Tracksmith Groove remains a separate optional VST3. AVRX Drums remains excluded.

## Verification

Native Release, production frontend, TypeScript, shortcut dispatch/focus guards, note-selection and editing math pass. Native tests cover atomic multi-note deletion/update/paste, invalid IDs, overlap rejection, track removal at the start/middle/end, audio/automation retention, plugin and master slot identity, MIDI notes, input/monitor reassignment and bus remapping. Existing VST3/MIDI, session/recovery, routing, automation and clip suites pass. ZIP contents and native binary hashes verified.

Mouse gestures in the native application and hardware listening remain manual review checks. Broader realtime reliability, Windows installer, custom UI and scanner improvements remain pending.

## Reference

The MIDI selection, eraser, transposition and shortcut choices were checked against Avid's documentation. Tracksmith implements a subset suited to its current features; its Escape deselection, internal clipboard, duplicate span and track-deletion/history behavior are described above.

- [Selecting MIDI notes](https://apps.avid.com/proToolsFirstHelp/version12.0/enu/Pro%20Tools%20First%20Help/MIDI1.EditMIDI.29.016.html)
- [Deleting MIDI notes](https://apps.avid.com/proToolsFirstHelp/version12.0/enu/Pro%20Tools%20First%20Help/MIDI1.EditMIDI.29.033.html)
- [Track name right-click menu](https://apps.avid.com/protoolsfirsthelp/version12.3/enu/Pro%20Tools%20First%20Help/sess3.tracks.14.72.html)
- [Pro Tools 2024.6 Shortcuts Guide](https://resources.avid.com/SupportFiles/PT/Pro_Tools_Shortcuts_2024.6.pdf)

GGD import update: imported duplicate hits and retrigger gates are normalized instead of rejecting the entire groove. Imports now report results visibly and scroll to the imported note register. See GGD-MIDI-FIX-GUIDE.md.
