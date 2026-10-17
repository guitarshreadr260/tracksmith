# Build 24 - current main checkpoint

Based on Build 23 commit `6eccf5d`, including all development through Build 23. Updates the About label and stale current-Build-16 claims in README and status/history pages. No new audio-engine behavior or session-format changes.

Main advances by fast-forward. Branch `build-24-main` identifies this checkpoint. Tag `build-16-checkpoint` preserves original main commit `de07171b91af5dd64ebab931eea8a1602214406f`. Existing Build 17-23 branches and Prototype 16 files remain unchanged. Current editable code lives in `Tracksmith/`; legacy ZIP/MSI downloads predate Build 24.

## Validation

Build 24 TypeScript/Vite production build passed. All eight UI suites passed: commands, shortcuts, clip editing, Time Warp editing, MIDI editing, MIDI drops, MIDI keyboard and inline MIDI. Native code is unchanged and was not rebuilt for this source/documentation checkpoint. Native Build 23 routing/Stop results are inherited; see BUILD23-STOP-FIX.md. Actual loaded GGD/ASIO recording still requires a hands-on check. No new Windows executable is packaged in this source checkpoint.
