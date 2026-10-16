# AVRX Drums and MIDI lanes

Close Tracksmith and run Run-Tracksmith-Release.ps1 with ExecutionPolicy Bypass. It backs up the project, applies the complete update and builds Release. The first build also compiles AVRX Drums; later builds reuse it.

## Try the drum plugin

1. On Edit, choose **New track → MIDI / Instrument**, then click **Add track**. Ctrl+Shift+N adds the chosen track type.
2. In the selected track's inspector or Mixer, click insert **A → Stock: AVRX Drums**. Click the insert name to open the plugin window.
3. Click a drum name in the plugin grid, or select a pad and click Audition pad. Built-in voices work without sample files.
4. To try Groove's pattern, enable **Sequencer follows DAW transport**, then press Space in Tracksmith. It follows the host tempo, play/stop and position. Click grid cells to toggle hits; choose one to eight 4/4 bars. This sequencer is off by default.

AVRX Drums is a distinct VST3 instrument. It has no guitar rig, NAM, recording-interface settings or separate device manager. Existing guitar/NAM work remains separate. The old Groove source/downloads have not been edited.

## Use your existing drum packs

Click Load .avrxpack and choose a kit.avrxpack inside your Groove 0.17 download. For example:

`C:\Users\tiaii\Downloads\AVRX_GROOVE_Studio_v0.17\AVRX_GROOVE_Studio_v0.17\Drum Packs\Chango Luke Holland\kit.avrxpack`

The folder must retain its Samples subfolder. Velocity groups and round-robin takes use the existing pack format. Samples are referenced on disk, not copied into the session or embedded in the plugin. Keep their folders available when reopening a session. You can also choose a pad/layer and load an individual mono/stereo WAV, AIFF or FLAC. There are four layer slots per pad; a loaded first-layer sample replaces that pad's pack velocity groups. Clear layer returns to remaining layers or the built-in voice if no samples remain.

The nine GM pads are Kick 36, Snare 38, Closed Hat 42, Open Hat 46, Rack Tom 1 48, Rack Tom 2 47, Floor Tom 41, Crash 49 and Ride 51. MIDI input on any channel plays matching pitches. Closed hat chokes open hat. Sample note-off does not cut a drum hit; panic/transport reset stops voices. Each pad currently retriggers its voice/layers rather than stacking arbitrary polyphony.

Select a pad to adjust gain, pan, tuning, mute/solo, four EQ gains and compressor threshold/ratio. EQ and compressor each have enable switches. This first drum-only editor uses the inherited fixed EQ band frequencies and compressor attack/release/blend; it does not reproduce every advanced Sound Lab control or the old application's visual layout. Output, room and overhead levels are separate. Audio-file decoding and reference analysis run on a worker; immutable kit changes publish outside the realtime callback. Loading a kit resets active hits/tails.

## Edit MIDI notes

Select a MIDI lane in Edit to open the piano roll below the timeline. Double-click a pitch row to insert a note, or use Add note. Select an existing note, change pitch, start, length, velocity or channel and click Apply note. Delete note, Delete/Backspace or Ctrl+B removes the selected note. Start/length use beats, with beat zero at the session beginning. Velocity displays 1–127. First pitch changes the 24 visible rows for melodic instruments.

Load an instrument in insert A and effects in B–E. Space starts playback; notes follow session tempo. The stock drum sequencer should be off when you want only piano-roll notes. Track and master gain/pan/mute/solo and insert processing apply to the resulting instrument audio. Ctrl+Z/Ctrl+Shift+Z undo/redo MIDI edits when a MIDI track is selected; audio clip edits and MIDI edits currently have separate bounded histories. MIDI edits stop playback to publish changes safely. Audio arming/record shortcuts do not capture MIDI input in this version.

Import MIDI loads PPQ .mid/.midi note events into the chosen lane, replacing its note list with an undoable edit. Channels and beat positions are kept. Imported tempo maps, program changes, controller lanes and SMPTE timing are not imported; the session tempo remains authoritative. Export MIDI writes the lane's notes and current session tempo. Files are limited to 1 MiB and 2048 notes per lane. Overlapping notes of the same pitch/channel are rejected on editing/import. Use a different channel or shorten one note if needed.

The current MIDI lane is a combined MIDI/instrument lane with one beat-based note sequence. Multiple MIDI clips, audio-style region trimming/splitting, cross-track MIDI routing, external keyboards/device capture, controller automation, quantize tools, sustain-pedal lanes and take playlists remain future work. Playback includes a short fixed drain after the last note; long instrument effect tails can exceed it. This is a supported subset of the Pro Tools workflow, not full parity.

## Recorded drums → analysis → MIDI → drums

Analyze the recorded WAV on its audio track using AVRX GROOVE. Select a MIDI lane and click **Use Groove analysis** to put the suggested one-bar kick/snare/hat notes into the piano roll. The transfer is undoable. Load AVRX Drums in insert A and press Play. Review and edit the hits by ear; the analysis remains a repeating-pattern heuristic, not a full performance transcription.

Inside the plugin, Analyze reference audio offers the same kind of suggested groove from an audio file (first 60 seconds). It fills the repeating grid and reports a suggested BPM without changing Tracksmith's tempo. Export pattern MIDI writes the plugin grid as .mid, which you can import into the lane. The suggested pattern and sample paths save in the VST3 state; MIDI lane notes/types save in version 4 .tracksmith sessions. Version 1/2/3 audio sessions still open; older builds cannot read version 4 saves.

## Validation and limits

Release builds use JUCE 8.0.10/MSVC. Native tests cover real VST3 discovery/instantiation, scheduled MIDI sample offsets and note-offs, seek chase/panic, host-synced sequencer playback, WAV-layer state loading, Groove velocity/round-robin packs, version 4 MIDI session round trips, legacy sessions and prior audio/punch/monitoring/NAM behavior. Keyboard/TypeScript/production asset builds pass. Hardware listening and visual mouse checks remain local user checks.

A kit is capped at 32 million stereo-equivalent source frames (at most about 256 MiB), 512 pack takes and two million frames per sample; stereo/mono 8–192 kHz samples are supported. Sample files remain external. No third-party drum samples are redistributed by this update.
