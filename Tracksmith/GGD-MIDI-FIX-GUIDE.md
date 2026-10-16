# GGD MIDI import fix — October 14, 2026

Save and close the older Tracksmith. Extract the complete ZIP into a new folder and open Tracksmith.exe, keeping Stock Plugins beside it. Session format stays version 9. Review using a session copy.

Drag a GGD groove onto an Instrument track in the Edit window. Both Select and Draw modes accept the drop. Imported notes are selected and the piano roll scrolls to their typical pitch range. A visible message inside the lane and the session status show the import result. Drop handling covers the whole instrument editor, including the toolbar and existing notes.

The reported accepted-drop/no-notes issue was reproduced as a MIDI import failure using the saved GGD Rock library. Some files contain duplicate same-pitch/channel attacks at the exact same tick. JUCE's note matching gives one duplicate a zero-duration gate, which the earlier importer treated as invalid and used to reject the complete groove. Some earlier busy-state checks could also return without explaining a rejected drop.

The importer now combines identical-tick duplicates using the strongest velocity and longest gate. Distinct attack positions, including flams and retriggers, remain separate. A later hit closes the previous same-pitch/channel gate as needed for Tracksmith's non-overlapping MIDI scheduler. Explicit zero-length drum triggers receive a one-tick gate. Pitch, channel, distinct start times and relative placement are retained. Original MIDI files are unchanged. This normalization affects imports, not ordinary note edits.

Drops append to existing notes. A drop overlapping existing same-pitch/channel notes still fails atomically; use an empty position or delete those existing notes first. Successful imports support one-step Undo. Clear/import-again is covered by regression tests. Files must still be PPQ MIDI under 1 MiB, at most 2048 resulting notes, and within beat 100000. Damaged/unsupported data and busy recording/loading/dialog states now report a result instead of silently ignoring the request. A successful edit stops playback and finishes any active automation pass safely.

All 225 saved GGD Rock grooves pass native import tests. Synthetic regressions cover duplicate attacks, flams, zero gates, keeping existing notes, rejected-overlap rollback, and importing again after deleting a selection. TypeScript, production frontend, drop payload/guard tests, note selection/keyboard tests, native VST3/MIDI host and session tests pass. The actual mouse drag from the user's live GGD instance and hardware listening remain manual review checks. Existing sessions were left untouched.

Selection, right-click track deletion, shortcuts and earlier DAW features remain included. See SELECTION-SHORTCUTS-GUIDE.md for the workflow. The latest GitHub snapshot remains the prior Selection and Shortcuts milestone until another update is published.
