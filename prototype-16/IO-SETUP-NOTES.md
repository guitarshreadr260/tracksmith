# I/O setup source changes — 2026-10-15

Included in Windows Prototype 16. Production UI and native Release build passed; native recording, session, mixer, instrument/I/O, and application command checks passed. Real hardware checks remain below.

## Routing model

- 32 physical hardware inputs, 16 physical hardware outputs, and up to 32 simultaneous audio recording tracks. Disk/driver performance still needs hardware validation.
- Mono input paths select one physical channel; stereo paths select two adjacent physical channels. Disabled holes never compact or substitute another signal. Incomplete stereo paths are unavailable.
- Setup > I/O provides Inputs, Outputs, Buses, and Track routing. Each device channel can be enabled and given a friendly name. Hardware names remain visible.
- Channel labels save atomically in application data under `Tracksmith/IO`, keyed by driver plus input/output device names. Native device XML continues to save sample rate, buffer size, and active channels. There is no preset import/export UI yet.
- Main mix can be disconnected or assigned to an enabled stereo output pair. A genuinely mono device supports its mono output. Main retains master processing, and its selected pair is reserved from aux writes. No fallback to another pair when a main path is unavailable.
- Stereo aux buses feed Main or hardware pairs 3/4 through 15/16. Two source sends support existing pre/post and level controls in Mix. The aux routing command previously returned before hardware assignment could execute; this is fixed.
- Sources feed Main or an aux bus. Direct source-to-hardware assignment, arbitrary nonadjacent stereo mappings, nested buses, recording bus inputs, sidechain buses, talkback, independent send panning, and hardware inserts are not implemented here.
- Physical outputs 1/2 remain reserved by the graph's Main slot; they cannot be used as an independent aux cue when Main is assigned elsewhere. This requires a subsequent graph change.
- I/O edits and device setup are blocked during playback/recording. Existing individual monitor controls remain available.

## Session safety

- Format version 10 stores the main output pair and interface key; input indices now mean physical channels. Versions 1–9 still decode.
- Legacy compact inputs resolve once using the current interface's enabled-channel order. If an old stereo assignment resolves to nonadjacent physical channels, it becomes No input. Old sessions never stored the original hardware map, so it cannot be reconstructed if that configuration has changed.
- Saved interface mismatch silences hardware input/output and blocks recording/monitoring until the user reviews I/O and explicitly chooses Use this interface for this session. The review opens automatically. Binding uses driver/device names, not a hardware serial number.
- Missing physical routes remain unavailable; no silent reassignment. No input is explicitly selectable.
- Four-bar count-in now round-trips through the session decoder, matching the existing transport UI.

## Validation

- TypeScript check without emitting files passed.
- `work/prototype/check-io-source.mjs` imports the actual TypeScript path helpers and checks disabled holes, stereo pairs, missing routes, interface mismatch, mono outputs, and output pairs.
- Source UI preview with a simulated JUCE state bridge: correct command payloads for naming/enabling, main output selection, aux hardware routes, source destinations, sends, and interface confirmation; recording/playback locks; no browser exceptions; modal remains within a 1024×600 viewport and footer is visible.
- These UI checks do not validate native audio or persistence.
- Added `tests/IOSetupTests.h` to the native test entry point for physical input 32, null holes, main output remapping/disconnect, interface mismatch, v10 session validation, and 32 independent WAV writers. Updated previous compact-index input tests. These native tests passed, including all 32 WAV writers and actual automatic-punch boundaries inside one audio callback.

## Remaining real-hardware checks

1. Native build and automated tests passed; validate the following on the intended interface.
2. Record distinct signals into physical inputs 1/3/4 with input 2 disabled; verify WAV identities, meters, monitoring, and punch boundary segments.
3. Record 18/32 inputs together on capable ASIO hardware; check disk overruns, CPU load, and stop/finalization behavior.
4. Route Main to 3/4 and a separate cue aux to 5/6; verify master processing stays on Main and mute/solo/pre-fader behavior is predictable.
5. Disable one side of an assigned stereo path, reopen sessions with missing channels, and switch interfaces; verify explicit review and silence rather than fallback.
6. Restart and check interface channel names, active channels, main routing, missing paths, and four-bar count-in persistence. Artwork is embedded in the built native WebView assets; visually check it at application launch.
