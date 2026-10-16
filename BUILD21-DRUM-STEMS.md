# Tracksmith Build 21 — drum stem rendering

Separate local branch `build-21-drum-stems`, based on clipboard checkpoint `b019adb`. Build 20 contains the finished keyboard/clipboard update and stays separate. No GitHub push or merge was performed.

## Editing shortcuts included

Ctrl+X Cut, Ctrl+C Copy, Ctrl+V Paste at the edit cursor, Ctrl+A Select all on the current track, Ctrl+B Clear, Ctrl+D Duplicate, Ctrl+Z Undo and Ctrl+Shift+Z Redo. Existing R/T zoom, Ctrl+E split, Ctrl+T trim and Alt-drag copying remain. The clipboard works for audio clips/ranges and MIDI notes across compatible tracks. Paste overwrites its destination span; Undo restores it. Text fields keep their normal editing shortcuts. Clipboard data is session-local. Reference: [Avid Windows shortcuts](https://resources.avid.com/SupportFiles/attach/Pro_Tools/12.6/Pro_Tools_Shortcuts.pdf).

## Render separate GGD drum WAVs

1. Import or record song MIDI onto the instrument track with GGD/Kontakt loaded and enabled in insert A.
2. Inside the instrument, route the drum or microphone channels to separate plugin outputs. For Kontakt, configure its Outputs mixer as well as the drum library routing. [Native Instruments output-routing guide](https://support.native-instruments.com/support/solutions/articles/69000880095-create-multiple-kontakt-output-channels-and-assign-them-in-a-daw-).
3. Select that instrument track and choose **File → Render GGD drum stems…**.
4. Tracksmith creates an independent instance using the instrument's saved state and exposes its native output buses for export. Select the outputs you want, uncheck unused buses or a full-kit mix, and name them to match your routing, such as Kick, Snare, Overheads and Room. Tracksmith cannot infer drum names from generic Kontakt output labels.
5. Set a decay tail (default 2 seconds), then choose a parent folder. A new timestamped **Tracksmith Drum Stems** folder contains one 24-bit WAV per selected output plus a channel/rate/peak manifest. All WAVs start at bar 1 and have identical frame counts, ending at the next bar boundary after the last MIDI note plus the tail. Import them at the same timeline position to keep them aligned.

## Behavior and limits

- Captures actual instrument output buses together in one pass. It does not isolate MIDI pitches or attempt to split a stereo mix into drums. Overhead/room bleed and instrument processing follow the kit's configured routing.
- Uses session sample rate, original mono/stereo bus widths, and instrument latency compensation. Supports 8–192 kHz, up to 64 output channels and a one-hour MIDI part, with a 0–30 second tail. Larger output buses are split into consecutive channel pairs.
- Uses song MIDI and a snapshot of current instrument settings. DAW insert effects, faders, sends, master processing and automation are excluded. The original instrument instance, MIDI and session media are retained. Rendering adds no session edits.
- Existing exports are preserved. A new folder is published only after every WAV and its manifest finish. Cancellation or errors remove staging files. Silent outputs and clipping are reported; no normalization changes relative levels.
- If only a stereo mix is exposed, export setup explains that separate GGD/Kontakt routing is required and provides **Open instrument routing**. This milestone adds offline multi-output export; normal live mixer routing remains stereo.
- Third-party sample-loading behavior, kit routing and licensing can affect results. No loaded GGD kit was available to validate its actual audio in this session. A real GGD render and listening test are still required on the user's setup. Rendered test audio came from a controlled multi-output test instrument, not GGD samples.

## Validation completed

- TypeScript/Vite production build and native Windows Release build with ASIO enabled passed. Compiler warnings remain in existing code and dependencies; no ASIO hardware test was performed.
- All eight UI suites passed: commands, shortcuts, clipEditing, elasticEditing, midiEditing, midiDrop, midiKeyboard and inlineMidi.
- Headless Edge workflow passed for clipboard keys and insertion-point paste, in-track MIDI edits and file drops, timing controls, stem menu availability, output selection/naming, decay tail, progress, cancellation and stereo-only routing guidance, with no browser errors. Screenshots use mocked bridge state.
- Native StudioStemTests passed with a four-output drum fixture: mono/stereo 24-bit files, identical lengths, exact MIDI attacks after 128-sample latency compensation, separated kick/snare buses, silent-output detection, manifest, cancellation cleanup, existing-destination preservation and invalid-layout rejection.
- The final Build 21 executable passed the native clipboard regression covering cross-track audio, offsets/fades, range copying, overwrite/undo, MIDI properties and incompatible paste protection.

## Main changes

Added `source/DrumStemExport.h`, `tests/DrumStemTests.cpp`, `ui/src/DrumStemDialog.tsx` and `ui/src/stems.css`. Updated native Main.cpp, CMake test registration, bridge types, Studio menu/modal integration and browser/shortcut tests. No session format change or new dependency was introduced. Native DSP tests live under `Tracksmith/tests`; the optional browser workflow is `Tracksmith/ui/tests/studioWorkflow.browser.cjs` and requires Playwright/Edge (PLAYWRIGHT_MODULE can specify its module path).

Run `portable/Tracksmith.exe` with its supplied Native Instruments, Stock Plugins and ThirdPartyNotices folders beside it.
