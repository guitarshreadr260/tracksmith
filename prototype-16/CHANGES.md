# Prototype 16 — October 15 development checkpoint

Packaged and tested October 16, 2026 UTC (October 15 local development day).

- Modern dark Edit/Mix/EZ UI, reference-inspired panels and instrument colors, brighter signed waveforms, compact digital transport, collapsible working Library sidebar, supplied startup/session artwork. Edit remains separate from Mix; no reference-video or inspector engine added.
- Startup VST3 scanning/cache/deduplication, channel insert removal and New/Load session hub. Plugin processing remains in process.
- EZ instrument-specific creation: Guitar embeds NAM; Drums embeds full Tracksmith Groove with native editor, Analyzer/Sound Lab and groove previews. Other native instruments are future work.
- Drum MIDI mapping and sound loading improvements; 225 MIDI grooves; new tracks default to Chango Luke Holland, saved sessions restore their kit. Learning work paused; earlier optional neural trial retained and defaults off. Generative conditioning is based on user-selected audio/grooves, not personal-model training.
- Track meters reflect actual signal; multi-track monitoring; mono/stereo routing; editable tempo; click track; track-scoped audio/MIDI selection and deletion; chronological edit undo/redo; real signed waveform envelopes and live recording display.
- Setup > I/O: named/enabled physical inputs, outputs, buses and track routing; stable disabled-channel numbering; adjacent stereo pairs; 32 input channels/16 output channels and up to 32 recording tracks; Main pair remap/disconnect; interface mismatch review; session format 10 opens versions 1–10.
- Adaptive bar/beat/subdivision grid and shared snapping. Independent Dynamic playback marker; numpad 1/2 move one 4/4 bar; numpad 3 records only the selected range; Space starts/stops. Old target audio is silent inside the actual punch and plays before/after while backing tracks continue.
- Nine stock VST3 plugins retained. Separate optional Groove 0.18 and historical archives preserved.

## Verified

Release build, TypeScript/Vite production build, four native suites, source UI interactions, I/O and grid math, packaged executable editing regression and ZIP integrity passed. Native tests include 32 independent WAV writers, physical channel holes, missing stereo paths, Main remap, interface mismatch mute, session 10 roundtrip, exact in-callback punch frame boundaries, backing playback, all nine Luke kit pads and saved-kit restore.

## Pending / limitations

Elastic Audio has no warp markers or time-stretch DSP yet. I/O lacks arbitrary nonadjacent stereo maps, nested buses, bus-input recording, sidechain buses, talkback and hardware inserts. Outputs 1/2 cannot currently be an independent cue when Main is elsewhere. Old sessions did not save their original interface map; review routing when opening them. Real hardware/listening and sustained multitrack disk testing remain pending. See the detailed implementation notes.
