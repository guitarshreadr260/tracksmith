# Tracksmith — plugin automation and meter fix

Extract the entire Windows ZIP. Save your work and close the older build before opening the new Tracksmith.exe. Keep Stock Plugins beside it.

## Plugin curves

1. Load an effect on an audio track, aux return or master, or an instrument in insert A of a MIDI track.
2. Select the track. Scroll down the left Inspector to Automation → Plugin curves. Choose its insert (master inserts are also listed).
3. Choose a Control. While stopped, click the graph or enter Time and Control value, then Add / replace point. Values use the plugin's normalized 0–1 scale; ranges and steps depend on that plugin.
4. Add another point at a later time, then press Play. Read follows the saved curve. Click a dot or select a saved point to edit/delete it. Clear lane removes that control's curve. Undo / Redo plugin curve applies to the selected insert and is separate from volume/pan automation undo.
5. Off holds the control's current value. Open the plugin window to adjust it manually. Adding a point enables Read. While Read runs, its curve takes control of that parameter.

Curves follow session seconds across playlists, seeks and loops. They apply to playback, live monitoring at the playhead, and WAV export. Plugin bypass suspends its automation and processing. Up to 64 controls per insert and 4,096 points per control. Editing and mode changes require stopped playback. Plugin Write/Touch/Latch are deferred; the existing track volume/pan Write mode is unchanged.

Plugin values update at each processing block (up to 256 samples, about 5.3 ms at 48 kHz). Discrete controls retain the plugin's own stepping. Delay compensation accounts for upstream inserts and bus stages; timing can vary within a block at range/loop boundaries. This is not sample-accurate VST3 parameter automation.

Curves are saved using stable VST3 control IDs rather than control positions. Missing plugins and unavailable control IDs retain their curves without assigning them to another control. Removing/replacing an insert removes its curves; reloading a missing insert retains them. Automation undo is temporary and is cleared when the insert/session is replaced.

## Meter fix

Removed an older style rule that colored the track meter's masking layer. Track meters now remain dark below the −60 dBFS display floor, including silence; green/yellow/red appears only in the exposed signal region. Existing horizontal input/master meter behavior is retained. Tiny real interface noise above −60 dBFS may still register.

## Session compatibility

New saves use version 8 and contain plugin curves as well as the existing volume/pan automation. This build reads versions 1–8. Save important older sessions under a new filename, since earlier builds cannot open version 8 files.

Plugin scanning improvements and the custom UI redesign remain deferred. Tracksmith Groove stays a separate optional VST3.
