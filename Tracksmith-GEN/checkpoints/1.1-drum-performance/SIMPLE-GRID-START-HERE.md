# Tracksmith GEN 0.8.4 - simple beat, exact grid, bar 2 start

Run ALIGN-DRUMS.cmd and choose your original guitar PCM WAV. Leave GGD One Kit Wonder: Metal factory mapping selected. Set tempo to 140 for your current recording. First drum bar now defaults to 2.

Import the MIDI at BAR 1, BEAT 1. Bar 1 is empty inside the MIDI; the first kick and hi-hat land exactly at BAR 2, BEAT 1. Placing the file at bar 2 would add the gap twice. Keep your guitar at its intended song position, and set the DAW to the same tempo. The WAV must cover the song timeline from bar 1 for this workflow; if your source audio was trimmed to bar 2, export it from bar 1 with the silent first bar included.

The repeating 4/4 pattern has kick on beats 1 and 3, snare on beats 2 and 4, and closed hi-hat on straight eighth notes. No fills, toms, crashes, random extra hits, learned timing offsets, or velocity randomization are used in simple mode. All note-on positions are exact eighth-note MIDI ticks. The generator ends at your recording's duration without overwriting the original audio or existing output files.

For an immediate check, import GGD-Simple-140-Bar2.mid into your DAW at bar 1 with tempo 140 BPM. It contains one silent bar followed by eight identical bars of this beat. Load your working OKW Metal factory mapping. Generate fresh MIDI with ALIGN-DRUMS.cmd to match your recording's duration.

This MIDI generator follows the specified grid, not the accents or structure of your guitar. The old statistical probabilities remain available only through the optional CLI --pattern learned setting; that mode is now quantized too. The main TracksmithGEN.exe and its audio-generation workflow are unchanged; run ALIGN-DRUMS.cmd for these changes.

The JSON report records start bar, tempo, simple/learned mode, mapping and grid lock. The launcher asks for a musical bar number instead of seconds. CLI --downbeat remains supported for explicit source-file timing; --start-bar 2 is the new musical start option.

Installation: extract into a new folder, or close GEN and run UPDATE-GEN.cmd to copy these utilities into your existing folder. Saved creations and settings remain intact. The GGD mapping from the user-confirmed 0.8.3 export remains unchanged. REMAP-GGD.cmd is only for older General MIDI files; do not remap new GGD exports.

Validation: automated checks cover the exact repeating kick/snare/hat pattern, no extra instruments, fixed velocities, seed independence, 128-bar timing, GGD mapping, note-on/off pairs, source preservation, partial endings, and the bar-2 default through the alignment CLI. Listening against the user's guitar still needs user confirmation. Prior remapper runtime verification remains limited as documented in GGD-MAPPING-START-HERE.md; no new remapper executable was built or run for this update.
