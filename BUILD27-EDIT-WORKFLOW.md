# Build 27 - edit tools, song timeline, organization and sends

Based on Build 26 (`a144552`). The user confirmed the Build 26 mixer and keyboard fixes work. R/T remain zoom out/in, following the user's final clarification. Left/Right arrows move the edit insertion cursor separately.

## Included

- Selectable Trim (F6), Selector (F7), Grabber (F8) and Smart tools; Ctrl+numeric-keypad-7 selects Smart. In audio clips Smart uses upper waveform for range selection, lower waveform for movement, edges for trims and corner handles for fades. Explicit tools restrict the gesture. Instrument notes support selection, move and right-edge trim according to tool; existing double-click drawing remains. These implement Tracksmith's core gestures, not every Pro Tools tool variant.
- A 256-bar timeline by default, with an initial visible ruler around bars 1-80 and labels every eight bars, following the supplied screenshot. At 120 BPM, 256 bars of 4/4 provide 8 minutes 32 seconds of arrangement space. The length selector offers 16, 80, 128, 256, 512 or 1024 bars and media can extend the display farther. This is arrangement/view space; playback/export duration still follows actual media. R/T and the zoom slider share the wider zoom range. The length/zoom choice is view state; it is not a fixed audio limit or saved song-end marker.
- Organization bars in Edit headers and Mix strips: drag the grip onto another organization bar (left half before, right half after), or use Up/Down buttons. Reordering changes saved display order, preserving physical audio/MIDI/insert/input/bus identities. Track deletion remaps display metadata with existing route remapping.
- Preset/custom track colors, with Auto reset. Assign the same color to related tracks for visual grouping. This does not automatically create linked editing/automation groups. Colors appear on headers, Mix strips and clips and save with the session.
- Compact send A/B slots open a control panel for Aux destination, level, pre/post-fader, mute and removal. Insert effects on the receiving Aux. Send level/mute updates are atomic and use lightweight UI feedback; mute preserves the stored level. There are still two sends per track, matching the existing engine; send pan and send automation are not newly implemented. Aux sends remain disabled to prevent feedback.
- Click tracks have no inline note grid, piano keyboard, note controls or lower piano-roll access; click settings and metering remain.
- Build 26 lightweight mixer changes, Ctrl+= Edit/Mix switching, browser zoom suppression and fader-focused Space are retained.

## Session compatibility

Session format is now 13 to persist colors, order and send mute. Formats 1-12 remain readable and default to original order, automatic colors and unmuted sends. Save a new filename when comparing with earlier builds; older builds cannot open format 13.

## References

The tool regions and combined-tool behavior follow [Avid's Smart Tool guide](https://www.avid.com/pro-tools/user-guide/smart-tool). Send-slot/panel structure and pre/post control follow [Avid's send/output window controls](https://apps.avid.com/proToolsFirstHelp/version12.0/enu/Pro%20Tools%20First%20Help/mix1.basic.54.42.html). The user's supplied ruler screenshot guided the initial bar scale.

## Validation

- TypeScript/Vite production build and all eight UI suites passed, including R/T bounds and retained clipboard/transport shortcuts.
- Headless Edge passed real pointer gestures for audio move, range selection and trim; F7 tool selection; 256-bar default and R/T zoom; stable-ID reorder buttons and drag; color assignment; send assignment, level, pre/post and mute payloads; click track piano-roll exclusion; prior mixer geometry, MIDI/drop/clipboard, Time Warp and transport checks. No browser errors. Browser native state is mocked.
- Native command regression passed: before/after reorder, unchanged audio/MIDI/plugin-input/Aux identities, color and order serialization, send level/pre/mute recall, invalid color rejection, deletion remapping.
- Native session tests passed, including duplicate display order rejection, invalid color rejection and legacy format-12 defaults.
- Native recording integration retains the eight routed drum passes, repeated Stop/latency drains, punch/cancel and printed playback checks. A DSP test verifies pre-fader send audio reaches Aux with the source fader down, mute silences it without losing level, and unmute restores it.
- Packaged executable passed native workflow, 48-track/1000-fader response and editing/undo regressions.
- Windows Release with ASIO builds. The portable executable is packaged locally with the prior instrument/plugin assets. Existing compiler warnings remain.

Live GGD/ASIO testing of these new controls remains with the user. Screenshots use mocked browser state. No GitHub push or new installer/release upload is part of this build.

Run `portable/Tracksmith.exe` after closing the previous instance, keeping its supplied folders alongside it.
