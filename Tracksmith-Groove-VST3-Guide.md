# Tracksmith Groove VST3 — 0.18

Tracksmith Groove is the preserved AVRX Groove v0.17 studio converted into a separate Windows x64 VST3 instrument. The original standalone and saved v0.17 ZIPs remain unchanged. It is optional; Tracksmith DAW itself has not been changed or given a built-in drum engine.

## Try it now on this computer

Save your current Tracksmith session. Open Tracksmith-Groove-Ready-To-Play.tracksmith from this folder, then press Play in the DAW. The demo already references the plugin in this delivered folder. It plays an eight-bar loop at 120 BPM. Click insert A to open Tracksmith Groove. Follow DAW and Run groove start enabled.

The ready-to-play demo stores this computer's plugin location. If you move the bundle or use a different computer, use the portable demo below and load the plugin once, then save your own session.

## Load it into your own session

1. Keep the complete `Tracksmith Groove.vst3` folder intact, including Contents/Resources. Do not select or move only the inner binary. It contains both drum packs and the groove library.
2. In Tracksmith press Ctrl+Shift+N and create a MIDI / Instrument track.
3. Click insert A, choose Add / rescan VST3 (or Browse VST3 in earlier builds), and select the complete `Tracksmith Groove.vst3` folder. In the saved plugin browser, choose Tracksmith Groove to load it. A one-file inspection still occurs using the existing DAW browser; the DAW scanner has not been redesigned.
4. Leave Follow DAW on and Run groove enabled, then use the DAW's Play and Stop. Tempo and sequence position follow the DAW, including seeks and loop returns. To audition independently while the DAW is stopped, turn Follow DAW off and use Run groove. Turn Run groove off to use only MIDI notes from the track's piano roll.

An empty Tracksmith instrument lane currently has a short playback duration. Open the included portable `Tracksmith-Groove-Demo.tracksmith`, load the plugin into insert A, and press Play for an immediate eight-bar test. Its single note at pitch 127 is deliberately outside the default drum map and just keeps the lane eight bars long. Your other audio/MIDI tracks also extend playback normally.

## What's included

- The v0.17 drum engine, sequencer, sample layers, tuning, shaping, ambience and mixer.
- 383 supplied WAVs from the Luke Holland and Time Traveller packs, and 225 supplied GGD Rock MIDI grooves across 15 songs.
- Luke Holland loads by default. Choose another pack in Sound Lab. Use Groove Player and its bundled-library button to select other rhythms.
- The earlier guitar, analysis, effects and recorder pages remain present. Tracksmith's instrument slot supplies MIDI and stereo output, with its audio input disabled; use the DAW for live guitar recording and monitoring. This first VST3 is primarily the Groove instrument inside Tracksmith.
- Full kit/pattern/mix/groove and processor settings saved in the DAW's insert state, with independent instances. Old `.avrxpreset` files remain supported. Opening a preset stops its internal run toggle as before; enable Run groove afterward. Reopening a saved DAW session preserves that toggle instead.
- Relative paths for bundled samples, so the complete VST3 bundle can be moved. User-imported WAVs, captures and recordings remain external references; keep those files available with the session.

The original TONE3000 account workflow remains; no account, key or credentials are bundled. Hardware MIDI input recording and plugin MIDI output routing depend on the DAW's capabilities. In this Tracksmith build, use the piano roll for external drum notes; the DAW does not route the plugin's generated MIDI output to other tracks.

## Checks

Release VST3 build, instrument discovery, disabled-input/stereo-output installation in Tracksmith's actual rack code, sampled audio under host transport, full host-state recall in independent instances, WAV export and editor construction passed. Exact generated MIDI offsets were checked at 120/90 BPM, along with Stop, seek, loop return and manual preview. The complete previous offline audio/preset/rig/pack/MIDI suite passed. Native window gestures and hardware listening remain for your test.

Keep the entire bundle together. No system-wide installation is required to browse and load this delivered copy.
