# Tracksmith — stock plugin test build

## Open this build

1. Extract the entire Tracksmith-Windows-Stock-Plugins-Test.zip into a new folder.
2. Close any running Tracksmith, then launch Tracksmith.exe from the extracted folder. Keep its Stock Plugins folder beside it.
3. Open an insert menu on a track, aux or master and choose **Add stock Tracksmith ...** for the effect you want.
4. Open the insert's editor to adjust the plugin. On an instrument track, use inserts B–E after the instrument in A.

This portable build runs alongside your existing installation. It does not replace the installed application. Back up a session before testing and use Save As for the test copy.

## Included plugins

- Tracksmith EQ 7 — seven bands, draggable response, filters, shelves and bells.
- Tracksmith Gate — threshold, attack, hold, release, range and hysteresis.
- Tracksmith Bus Compressor — threshold, ratio, attack, release/Auto, makeup, sidechain high pass and parallel mix.
- Tracksmith FET Compressor — input-driven fast compression, 4:1/8:1/12:1/20:1 and custom Slam mode.
- Tracksmith Limiter — input drive, sample-peak ceiling and release.
- Tracksmith De-Esser — selective sibilance-band reduction, Auto threshold and Listen band.
- Tracksmith Plate — decay, pre-delay, damping, low cut, width and mix.
- Tracksmith Digital Delay — delay time, feedback, repeat tone and mix. No modulation or tempo sync; times are in milliseconds.
- Tracksmith NAM — model loader, noise gate, new post-model EQ and preset save/load.

The drum plugin is excluded. The previously saved Groove project is unchanged and is not part of this stock-effects package.

## Reverb and delay

For a normal track insert, start around 10–25% mix. For an aux/send effect, set Mix to 100% and control the amount with the send or aux fader.

Plate starting point: 2.5 s decay, 20 ms pre-delay, damping around 9 kHz, low cut 120 Hz, width 100%. The decay control is a nominal setting; damping shortens higher-frequency decay.

Delay starting point: 375 ms, feedback 30%, repeat tone 18 kHz, mix 25%. Delay-time changes crossfade between fixed taps. There is no modulation LFO. Repeat tone filters the feedback loop; the first repeat is clean.

When exporting, allow a tail long enough for the effect to fade. High feedback and long delay times can ring longer than the export dialog's 30-second tail limit; lower feedback or automate a fade if needed.

## Tracksmith NAM EQ and presets

Load your .nam model first. The EQ follows the model: bass shelf at 120 Hz, mid bell with a sweepable 200–4000 Hz centre, treble shelf at 6 kHz, and a high-pass filter. Bass/mid/treble cover +/-12 dB. High pass at 20 Hz means off. Post-model EQ can be disabled.

Use Save preset... to save a .tsnampreset file, and Load preset to recall it. The preset includes the model location, EQ, input/output, bypass and gate settings. It does not embed the model itself. Keep the .nam file available. Moving the preset together with its model in the same relative layout is supported. Invalid presets and missing model files produce an error while retaining current settings.

Older AVRX NAM states restore with flat EQ. The plugin's original VST3 identities and original parameter IDs/order were retained. The DAW redirects older AVRX NAM descriptions to this bundled Tracksmith NAM. No cabinet IR loader has been added.

## Checks and current limits

All nine plugins loaded in automated VST3 tests. New effects passed DSP checks at 8, 44.1, 48, 96 and 192 kHz, saved-state checks and editor rendering. Actual VST3 audio rendering verified reverb/delay tails. NAM tests covered model loading, EQ audio response, preset save/load and overwrite, missing/invalid preset handling, relative model lookup and old-state restoration. Existing DAW monitoring, delay compensation, routing, automation, session, clip editing and WAV export checks passed.

Listening on your interface and testing these together in real sessions are the next checks. The FET and bus compressors are original behavioral designs, not verified analog emulations. The limiter protects sample peaks, not reconstructed true peaks. See the included test notes for details.
