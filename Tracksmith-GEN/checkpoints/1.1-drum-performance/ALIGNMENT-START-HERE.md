# Tracksmith GEN 0.8.2 alignment experiment

Double-click ALIGN-DRUMS.cmd, choose your original guitar PCM WAV, enter its
recording tempo (140 for Joe's test), and leave the first-bar time at 0 if bar
one starts at file zero. Choose a NEW MIDI filename.

Import the MIDI and original WAV at timeline zero in your DAW, set the DAW
tempo to 140, and load a drum instrument on the MIDI track. The JSON file beside
the MIDI records the tempo, downbeat, source hash and attack diagnostics.

This utility preserves leading silence and source bytes, schedules drums in
MIDI ticks without accumulating sample rounding, embeds the supplied tempo,
and ends the MIDI at the recording duration. It never overwrites existing MIDI
or report outputs. Use a new filename for another take.

This is a separate baseline experiment. TracksmithGEN.exe and its existing
audio variation workflow are unchanged. It does not synchronize or repair
ACE-Step/Stable Audio renders. There is no learned guitar conditioning yet.
Learned drum timing offsets remain, so hits can sit slightly off the grid.

Requires Python 3.11; the launcher looks for the existing Stable Audio trial
environment or asks you to select python.exe. No downloads or GPU are needed.
Input: mono/stereo uncompressed PCM WAV, 8/16/24/32 bit, fixed 4/4 tempo, up to
128 bars and ten minutes. Export MP3 or float WAV to PCM WAV in your DAW first.
Do not trim its beginning when converting.

Validation: automated synthetic PCM tests passed for preserved leading silence,
source byte identity, embedded 140 BPM tempo, exact MIDI duration, bounded notes,
overwrite rejection, repeatability, and a 128-bar grid with no accumulated drift.
Joe's original guitar recording has not been auditioned with this change.
Audible fit and groove quality still require that comparison.

The original START-HERE.md and DrumModel dataset attribution remain included.
