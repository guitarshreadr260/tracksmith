# Where we left off

Updated: October 14, 2026.

## Latest deliveries

### Tracksmith DAW — Automation build

- Volume and pan lanes on audio, MIDI/instrument and aux/bus tracks.
- Off / Read / Write modes; Write returns to Read on Stop.
- Editable points, separate automation undo/redo, session saving and WAV export.
- Source and bus automation aligned with plugin latency.
- Session format 7; older formats 1–6 still open.
- Build, native audio, session, routing and export regression tests passed. User fader/listening checks remain.

### Tracksmith Groove — VST3 0.18

- Saved AVRX Groove 0.17 retained unchanged, renamed in this separate development copy.
- VST3 instrument for insert A on a MIDI/instrument track.
- DAW tempo, play/stop, seeks and loop returns; external MIDI or manual groove preview.
- Full kit/pattern/mix state recall, independent instances and rendered WAV audio tested.
- Bundled Luke Holland and Time Traveller packs: 383 WAVs. GGD Rock library: 225 MIDI grooves across 15 songs.
- Supplied media verified against the preserved original source archive.
- Native window gestures and hardware listening remain user checks.

## Decisions to preserve

- Keep the DAW core separate from the optional Groove instrument.
- Leave plugin scanner improvements for later, as requested.
- Custom UI redesign is deferred until the core DAW is further developed.
- Preserve original recordings and older builds/session copies.
- Keep the AVRX Groove 0.17 standalone/source backups.

## Next development options

1. Collect user test feedback on automation and Groove inside Tracksmith.
2. Add plugin parameter automation, followed by Touch/Latch modes.
3. Later: improve plugin discovery/scanning and recovery, hardware MIDI recording, hardware recording-latency alignment and broader plugin isolation.

No new development item is authorized just by listing it here. Choose the next item in the working chat.
