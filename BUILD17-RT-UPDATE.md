# Build 17 continuation: R/T zoom and validation

## Changes

- R zooms the arrangement out; T zooms it in, in 0.25 steps between 0.75 and 3. Held keys repeat. Commands are disabled in the mixer, where no timeline is visible. View menu and shortcut help show the mappings.
- Shortcut dispatch now reads the command registry's shortcut metadata instead of maintaining a separate map. Modifier matching is exact. Fixed the previous plain-S/plain-O/plain-N behavior that could accidentally run Ctrl shortcuts.
- Text fields, menus, dialogs, startup screens, IME composition and already-handled keyboard events protect against global shortcuts. Piano-roll capture handlers now yield to menu/dialog focus as well.
- Play/stop, return to start, record (F12/Numpad3/Ctrl+Space), split, snap, loop, punch, metronome, workspace, session-open and audio-device toolbar buttons share registered commands. Split rejects MIDI-note selections. Parameterized controls such as per-track gain, plugin slots and zoom slider remain direct controls.
- Replaced the stale shortcut test targeting main.tsx with tests against the current Studio handler. Expanded command and piano-roll keyboard regression checks.

## Executed checks

- PASS: TypeScript and production Vite build.
- PASS: commands.mjs, shortcuts.mjs, midiEditing.mjs, midiDrop.mjs and midiKeyboard.mjs.
- PASS: headless Edge R/T direction and bounds, Ctrl/typing/dialog/menu guards, menu shortcut hints, simulated native bridge save/play/record dispatch and no Library sidebar. No browser page errors.
- PASS: native Release build with ASIO enabled using the available ASIO SDK; the initial Build 17 executable had ASIO disabled. Existing compiler warnings remain.
- PASS: StudioSessionTests (session serialization/validation), StudioRecordingTests (recording buffer, selected input and WAV writer), StudioMixerTests (routing/buses/sends/solo).
- PASS: app --editing-regression with a generated stereo WAV fixture: native audio/MIDI delete and undo/redo across track changes, scoped range deletion/undo, signed waveform extraction and neural groove generation. The report is BUILD17-editing-regression.txt.

Browser bridge tests simulate native state. Native regression tests exercise actual native commands and processing without a physical interface. Live ASIO device selection, recording/monitoring, round-trip latency, device unplug/reconnect and listening quality remain manual checks. No claim of hardware verification is made.

## Package and version control

The updated executable is `portable/Tracksmith-Build17-RT.exe`, beside the copied runtime asset folders. The earlier `portable/Tracksmith.exe` was open and was left untouched. Close the earlier app before opening the update for a manual audio test.

Build 16 is untouched. This continuation is saved locally on build-17-menus; it has not been pushed or merged. The earlier Build 17 commit a491ce3 is already on GitHub. BUILD17-REPORT.md describes that initial checkpoint; this document supersedes its testing limitations where explicitly listed above.
