# Tracksmith — playlist management and group range editing

## Playlist management

Select an audio or MIDI track. The Inspector's playlist controls now include Move up, Move down and Delete playlist, alongside New, Duplicate, Rename and Switch. Reordering keeps stable playlist IDs and does not change which playlist plays. Names must be unique on the track, ignoring case. Each track retains at least one playlist; the last one cannot be deleted.

Deleting an active playlist selects an adjacent remaining playlist. Undo restores the removed playlist, its ordering, active choice and clip/note layout. Source audio files and loaded source buffers are retained; playlist deletion does not delete recordings from disk. Alternate playlist rows also have move/delete controls.

With alternate lanes visible, select a time range and choose Replace active range on an alternate playlist. The row displays the selected seconds and target. Empty alternate ranges are rejected instead of erasing the active take. If the source contains gaps within a nonempty range, those gaps replace the corresponding active audio/notes. Existing audio/MIDI comp copying and its normal edit Undo remain available.

## Group range editing

The Edit window has a Group checkbox on each audio/MIDI track header. Select the tracks explicitly, then highlight a time range on the ruler or lanes. The new group toolbar provides:

- Split range: splits every affected audio clip and MIDI note at the selection edges.
- Delete range: removes content inside the selected range, retaining content on either side without ripple.
- Move range: cuts the selected content and moves it by the entered offset in seconds. Positive moves later; negative moves earlier. MIDI timing follows the current tempo.

Only active playlists are edited. Aux tracks cannot join the group. Empty ranges on an individual selected track are skipped. The group validates before any layout is committed: limits, invalid positions or an overflowing track reject the whole operation. A negative destination is rejected. Audio fades and media offsets follow sliced content.

Moves do not shift the rest of the session or delete destination content. Moved audio is appended over any existing destination clips, following the DAW's clip-layer order; MIDI notes can overlap existing notes. Undo restores all participating layouts together. Group membership is an editing selection, not a named/persisted track group. Ordinary clip drags remain single-track edits; use the group toolbar for linked range operations. Group fades and linked dragging remain follow-ups.

## Undo and compatibility

Undo group / playlist and Redo restore whole batch changes; Ctrl+Z/Ctrl+Y and the normal toolbar Undo/Redo also route to the latest batch history when available. Histories are bounded, held in memory and not saved in sessions. Starting an unrelated editing category, comp copy, import or recording invalidates prior batch history so Undo cannot replace newer work with an old snapshot. Playlist operations clear earlier clip/MIDI history; grouped edits likewise begin their own history category.

Session format stays version 9. Playlist order, names, active IDs and remaining layouts use the existing session schema. Group selections and history do not persist. Earlier builds remain preserved. Source files are retained even if no current playlist uses them; media cleanup is a separate future feature.

## Validation and next work

Automated tests cover reorder/rename/delete/duplicate limits, active-playlist deletion, restoring snapshots, session ordering, mixed audio/MIDI split/delete/move, fade preservation, playback silence/restoration and whole-batch rejection on capacity/position errors. Existing automation, monitoring/routing, aligned recording, sessions and exports remain covered. Release build, TypeScript and shortcuts pass. Native visual/gesture checks and physical audio hardware verification remain pending.

This is an internal milestone, preserved without a review request. Next planned group: autosave, recovery and reliability. Plugin scanning and custom UI remain deferred. GitHub reference archives are unchanged and are not an automatic local mirror.
