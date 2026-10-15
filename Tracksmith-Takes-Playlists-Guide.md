# Retakes, fades and track playlists

Re-record on the same armed audio track. The completed take replaces the active playlist only between the new take's start and actual end. Audio before and after it stays intact, even when you stop the retake early. Original WAVs remain saved. Undo restores the old clip arrangement; Redo restores the retake.

An overlapping recording also saves a Before take playlist backup while fewer than 16 playlists exist on that track. If the backup limit is reached, the status reports it; Undo and the source files remain available. Undo history is not saved, but playlists are.

## Track playlists

In the inspector, use Active track playlist to switch arrangements. New creates an empty playlist; Duplicate copies the current one; Rename uses the name entered above the buttons. Leave the name box blank for an automatic name when creating. Inserts, mixer settings and routing are shared. Only the active playlist plays or exports. Playlists work on audio and MIDI/instrument tracks; aux returns remain mixer channels.

Enable Alternate playlist lanes to show the selected track's alternates below the arrangement. Make active switches to an alternate for playback and editing. For a comp, keep the destination active, highlight a time range on the ruler, and click Copy selection to active on the desired alternate lane. That interval replaces the active arrangement while preserving the alternate. Copying an empty section can produce silence. Undo reverses copied sections. MIDI comp selections use session tempo.

The workflow follows Avid's active/alternate playlist and section-copy model: https://www.avid.com/pro-tools/user-guide/playlists . Current limits are 16 playlists per track and the existing source, clip, note and memory limits. Alternate lanes are read-only until made active. Automatic loop-record playlists, ratings, deletion/reordering and grouped comping are future work. Playlist create/rename/switch operations are not clip-edit Undo entries. Switching and copying stop playback and are blocked during capture.

## Fades and crossfades

Select an audio clip, set Fade ms, and click Fade in or Fade out. Set zero and apply the corresponding fade to remove it. The duration must fit inside the clip. Curves are linear and are applied in playback and WAV export. Splitting or shortening a clip preserves its original envelope; reapply fades after extending an edge when needed.

For a crossfade, select the later clip and click Crossfade previous. The earlier clip must touch or overlap it. Tracksmith uses available source audio handles to form an overlap, fades the earlier clip out and the later one in, and mixes them. It reports when handles are unavailable. Crossfades are explicit edits, not automatically added to recordings. Undo removes the complete edit.

Normal monitoring still combines the active playlist with live input. Selection punch plays old audio outside its selected interval and monitors live input inside it. The completed replacement covers only the captured portion. Use Dynamic transport/playback start for an independent recording origin, as described in TIMELINE-GUIDE.md.

## Sessions

New saves use version 6 and store playlists, active choices and fade envelopes. This build reads versions 1-5 and creates Main playlists. Older builds cannot open version 6 saves; keep an older session copy if needed. Source files are shared across playlists and retained under the existing memory budget. Clearing WAV audio now clears the active playlist while preserving source media for alternates.

Native checks cover exact replacement, shorter retakes, retained original sources, undo/redo data, fade samples, split envelopes, crossfade blends and missing handles. Session checks cover active/alternate audio and MIDI, saved fades, bad playlist IDs and old-version defaults. Export tests reopen real WAVs and verify replacement and faded samples. Existing recording, routing, monitoring and shortcut checks are retained. UI click-through and listening with your interface remain for local validation.
