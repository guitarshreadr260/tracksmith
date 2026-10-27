# Tracksmith GEN 0.9 - experimental Riff Lab

Extract the ZIP, then run ANALYZE-RIFF.cmd. Choose the original guitar PCM WAV, enter the actual tempo (default 140), and leave start bar 2 if the first bar is a lead-in. This uses your existing Python setup.

The browser page shows attack markers, relative accents, quiet spans and repeated bar rhythms. Select the same WAV for playback. Click empty grid space to add an attack, drag a marker to move it, or select and Delete it. Change rhythm groups and phrase boundaries in the bar controls. Edits update the proposed drum hits.

Start with the sparse default, then try swing, ride, phrase crashes or fills. Export reviewed GGD drum MIDI. Import at bar 1 at the displayed tempo; the MIDI includes the empty first bar. Use GGD One Kit Wonder: Metal factory mapping. Save analysis + edits to keep your changes, and reopen that JSON from the page later.

This is an inspectable rhythm prototype, not full riff understanding: it does not transcribe notes/chords or identify true riffs. Attack detection and quiet spans are hypotheses; distortion, ringing notes, noise and tempo drift can fool them. Rhythm groups compare sixteenth-grid attacks. Phrase boundaries are manual.

DEMO-RiffLab.html and DEMO-Rhythm.wav provide a synthetic rhythm example, not a real guitar quality benchmark. Your original WAV is never modified. The existing GEN app and ALIGN-DRUMS workflow remain available.

Validation: 16 Python tests passed; arranger checks cover bar-2 start, swing preserving kick/snare timing, edits affecting patterns, muted bars, quiet spans and MIDI header. Real guitar listening and Windows browser interaction still need your trial.
