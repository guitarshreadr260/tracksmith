# Tracksmith Build 17

## Verified baseline

GitHub `guitarshreadr260/tracksmith` main/HEAD was `e8a426bcc2cdb15f93f352179add655789a859c2` when inspected on October 16, 2026. The remote did **not** contain the local Prototype 16 checkpoint `de07171`. That clean local checkpoint was found in the October 15 upload workspace and used to create the independent local branch `build-17-menus` and this worktree. Nothing was pushed.

Build 16's source archive SHA256 remains `2ae12e63f93eaa680ec2feca66270a1cd5fefabc66b156b0d764874f3766ca16`, matching its manifest. The original checkpoint, source workspace and supplied portable Build 16 folder were not edited. The referenced conversation exposed only content-reference markers for assistant responses, so no implementation claims were inferred from it.

## Changes

`Tracksmith/` restores the archived Build 16 source as ordinary files for continued development. Compared with the extracted archive, the only changed existing source file is `ui/src/Studio.tsx`:

- Replaced placeholder menu branches with File, Edit, View, Track, Clip, MIDI, Audio, Options, Window and Help.
- Registered existing session open/save/new/export, undo/redo, track creation/options/playlists, clip split/trim/delete, MIDI import/export/delete, audio import/device/I/O, snap, dynamic transport, metronome, backups, workspace/window and help actions.
- Added shared availability checks for native connection, busy/recording state, selected track kind, clip/note selection, time range and edit history. Undo/redo includes MIDI and batch history.
- Routed advertised session/edit/track/import shortcuts and several toolbar actions through the same registry; preserved range deletion and transport shortcuts.
- Removed the Library sidebar and its menu/toolbar controls as requested. The workspace uses the available width. The separate Groove library chooser in generation controls is existing functionality.

New source files: `ui/src/commands.ts` (registry/types/menu order), `ui/src/MenuBar.tsx` (reusable menu component), `ui/src/menus.css` (menu styling), and `ui/tests/commands.mjs` (command regression checks). The menu component provides arrows, Home/End, Escape with focus restoration, checked states, disabled items and outside-click/blur dismissal.

`ui/package-lock.json` records the exact npm installation used for validation; original dependency version declarations and pnpm lock remain unchanged. No native engine, DSP, plugin, session format or bridge source was changed. Existing unused Library component code remains available in StudioVisuals; it is no longer mounted by Studio.

## Validation

- PASS: TypeScript check and Vite production build (`npm run build`).
- PASS: `node tests/commands.mjs`: ten-menu coverage, unavailable/unknown command rejection, duplicate-ID detection, native command payloads, MIDI undo, audio split/import and range deletion.
- PASS: existing `midiEditing.mjs`, `midiDrop.mjs`, `midiKeyboard.mjs` suites.
- PASS: headless Edge interaction smoke checks: ten menus, disabled save in preview, arrow navigation, Escape focus restoration, End/Enter execution, snap toggle, outside-click dismissal, no Library sidebar/control and no page errors. These checks use browser UI preview, not real audio hardware.
- PASS: Build 16 archive hash and unchanged `prototype-16` checkpoint files.
- PASS: native MSVC Release build, including the stock plugin dependencies, with ASIO disabled. Initial compilation found a missing WebView2 SDK include path; configuring the SDK from the existing local project resolved it. Compiler warnings remain in unchanged native/dependency code. Final build log is saved beside this report.

The legacy `tests/shortcuts.mjs` targets an old handler in `main.tsx`; the current entry file only imports Studio. It was not counted as a passing Build 17 suite. New registry checks cover the changed command dispatch.

## Running and limits

UI: from `Tracksmith/ui`, install dependencies, then `npm run build` or `npm run dev`. Full source/runtime restoration instructions remain in `prototype-16/BUILD.md`.

`portable/Tracksmith.exe` is the new native Release executable. Its sibling Native Instruments and Stock Plugins folders are copies from the supplied Build 16 portable folder. This folder is separate from Build 16. The compiled executable was built successfully but was not launched or listening-tested in this task.

Native build validation uses MSVC/Visual Studio 2026, JUCE 8.0.10 from the existing local checkout, and the existing WebView2 SDK. ASIO was disabled for this validation build. Hardware recording, ASIO, listening, latency and complete native menu/session interaction still require manual verification. No native regression suite was rerun because native code was unchanged.
