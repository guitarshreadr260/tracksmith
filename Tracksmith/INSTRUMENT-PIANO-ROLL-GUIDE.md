# Instrument tracks and groove drops

Extract the whole ZIP to a new folder. Save and close the older Tracksmith before opening this Tracksmith.exe. Keep Stock Plugins beside the application. Windows x64 and WebView2 required. Use a copy of an important session. Session format stays version 9.

1. Press Ctrl+Shift+N and choose Instrument.
2. In its Edit window lane, click Load instrument and choose your VST3 instrument for insert A. Effects can go in B–E. No general piano instrument is bundled; Tracksmith Groove remains a separate optional VST3.
3. Click Draw (F10) to draw notes directly on the piano-roll grid. Scroll vertically to change register. Select / move avoids adding notes on empty clicks. Drag a note to move it or change pitch; drag its right edge to change length. Choose grid, new-note length, velocity and MIDI channel in the lane toolbar. Velocity/channel also change a selected note. Fit notes centers the register around your notes.
4. Press Play to hear notes through the instrument. Piano key labels do not audition notes. Editing stops playback. Delete note or Delete removes a selected note on the selected track; Undo/Redo restores edits.
5. Drag one MIDI groove from a drum program onto the Instrument track's note grid at the desired beat. Grid snapping uses this lane's selected division. Notes are appended; existing notes remain. The imported MIDI's original beat offsets, pitches, channels and velocities are preserved. Session tempo controls playback; imported tempo changes/controllers are not imported. The result is editable notes, not a linked groove clip.

Drum programs must expose a .mid or .midi file when dragging. Proprietary drag payloads are not supported. If direct drag does not work, export MIDI from the drum program, then drag the saved file from Explorer. Inspector Import MIDI also works, but replaces that track's existing notes and imports at the original MIDI beat positions. Dropping audio loops is not part of this update.

Drops accept one PPQ MIDI file under 1 MiB, with at most 2048 notes on the destination and an end before beat 100000. Same-pitch/channel overlaps are rejected atomically. An unsuccessful import leaves the track intact. Check session status for errors. Undo removes a successful groove import in one step. Imported channels remain unchanged, so choose a matching channel in your instrument (drum files often use channel 10).

Notes are stored with the active playlist and saved/recovered with the session. Existing MIDI tracks display as Instrument tracks with this editor. MIDI keyboard recording and live note audition are future work; this update provides drawing, editing, file import and playback.

Validation: native Release, frontend production, TypeScript, shortcut and note-edit math checks pass. Native groove import checks cover placement, preserved MIDI properties, append behavior, overlaps, damaged MIDI, SMPTE rejection and timeline bounds. Existing VST3/MIDI playback, session, clip and automation suites pass. Direct drag from a particular drum plugin, native visual behavior and hardware listening remain to be checked by hand. Broader realtime reliability and Windows installer remain pending. AVRX Drums is excluded; original AVRX Groove is preserved.

Selection update: Select (F8) is now the default. Drag a box or Shift-click to highlight groups, then Delete/Backspace. New shortcuts and track deletion are documented in SELECTION-SHORTCUTS-GUIDE.md.

GGD import update: imported duplicate hits and retrigger gates are normalized instead of rejecting the entire groove. Imports now report results visibly and scroll to the imported note register. See GGD-MIDI-FIX-GUIDE.md.
