# Tracksmith — plugin browser and New Tracks

Extract the entire Windows ZIP and open Tracksmith.exe. Keep Stock Plugins beside it.

## New Tracks

Press Ctrl+Shift+N or click Add track. Choose Audio, MIDI / Instrument, or Aux / Bus; enter a quantity and optional name, then Create. Enter creates; Escape cancels. Audio input format can be Mono or Stereo; MIDI/instrument output and aux returns remain stereo. Up to 128 tracks, including at most 16 aux returns. Tracksmith retains one existing master channel; this dialog does not create additional masters, folders or VCA tracks. Invalid quantities create no tracks.

This follows Avid’s New Tracks dialog workflow, using the track types currently supported by Tracksmith: https://apps.avid.com/proToolsFirstHelp/version12.3/enu/Pro%20Tools%20First%20Help/sess3.tracks.14.40.html

## Plugin browser

Click an empty insert, or an unavailable insert. Saved VST3 plugins appear by name and manufacturer. Choose Search saved plugins to filter by either; an empty search shows all. Add stock AVRX NAM or Add / rescan VST3 inspects one selected Windows x64 VST3 bundle, saves its descriptions, and reopens the browser so you can choose the plugin to load. Instruments are enabled only in insert A on MIDI / Instrument tracks. Plugins and bus routing remain shared across playlists.

Discovery runs in a separate Tracksmith helper process with a 30-second limit. Failed scans leave the DAW running and do not erase your saved library. This build adds bundles individually; it does not automatically scan folders. Cache and recovery records live under the Windows application-data Tracksmith folder.

## Loading recovery

If plugin creation, saved-state restoration or initial graph reset is interrupted, reopening Tracksmith records that plugin as unavailable. Opening the saved session keeps its insert description, state and bypass setting without automatically loading it again. Click the unavailable insert and choose Retry unavailable insert with saved settings. Successful retries clear its recovery block. Ordinary failed loads preserve existing inserts; failed session restores retain an unavailable placeholder. Save sessions regularly: recovery retains data from saved sessions, not changes lost in a crash.

Running plugins and their editors still execute inside Tracksmith. Discovery isolation and interrupted-load recovery do not protect against every plugin playback/editor crash or hang. Hardware and native-dialog listening/click-through checks remain for this build.

Recording, retake replacement, fades, playlists, monitoring, routing and WAV export remain included. Drums remain excluded; AVRX Groove is preserved separately.
