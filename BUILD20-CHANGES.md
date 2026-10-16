# Tracksmith Build 20

Local branch `build-20-time-warp`, based on Build 19 commit `7739682`. Earlier builds were not changed. Nothing was pushed or merged on GitHub.

## Instrument tracks and GGD groove drops

- Instrument timeline lanes now accept one file-backed `.mid` or `.midi` groove up to 1 MiB. Drop location follows the project grid; Slip mode keeps the unsnapped location. Native MIDI parsing creates editable timeline notes, with import undo/redo.
- Every instrument lane now defaults to an in-track Notes view with a mini piano keyboard and 24 pitch rows, visible even when empty. Draw notes with a double-click, drag to move/transpose, drag the small right-edge handle to resize, and Alt-drag to copy. Octave controls and Fit notes change the visible register. The larger lower piano-roll editor remains optional through the Piano roll button; it no longer opens automatically.
- A timeline drop selects its instrument and displays the imported notes directly in that lane. The register centers around the imported pitches.
- A vertical insertion guide follows pointer movement and file drags, showing bar | beat | tick. Drag-over uses a distinct green Drop guide. Its position matches the drop calculation, follows the project grid, and supports Shift to bypass snapping. Use Grid 1 bar for whole-bar placement; the shared ruler and note rows use the same horizontal time scale.
- Both timeline and piano-roll drops share file reading, including a DataTransfer.items fallback. A plugin drop that exposes no readable file gets an explanation; export the groove as MIDI and drop the exported file.
- No claim of live GGD plugin drag compatibility: validation used a real bundled GGD MIDI file through the native command and a browser file-drop simulation. Test the live GGD window on your setup next. Plugin-specific virtual-file drags may still require export. Existing PPQ and 2048-note limits remain; overlapping imports are rejected without changing notes.

## Time Warp

Select an audio clip, then Clip → Time Warp timing. Use Suggest attacks to detect note onsets from the original source, adjust sensitivity, and review suggested markers. Analysis runs in the background and changes neither audio nor undo history. Suggestions replace unapplied marker drafts while retaining existing applied timing anchors.

Destination markers snap to the selected musical grid using the clip's project position; hold Shift while dragging for precise positioning. Numeric fields remain precise. Quantize anchors and the Strength slider interpolate toward grid positions from a stable starting map. Correction is automatically limited when necessary to keep the timing map valid.

Audition correction renders in the background and starts playback in session context. The persistent audition bar provides Play/Stop, Original/Corrected, Apply correction and Discard audition. Original means the clip timing before this audition, including any earlier applied corrections. Playback stops at the selected clip end. Saving and other edits are blocked during the staged audition. Apply commits one undo step; Discard restores prior audio, source references, undo history and loop setting. Cached render files may remain for reuse. Render timing edit still applies directly. Marker changes alone remain silent.

The Build 19 limits remain: source regions 0.5–30 seconds, at most 16 anchors, fixed outer edges, minimum 50 ms spacing and local stretch ratios 0.75–1.5. No automatic tempo-follow stretching or session-format change was added.

## Validation

- Production UI build passed: TypeScript and Vite.
- UI suites passed: commands, shortcuts, clipEditing, elasticEditing, midiEditing, midiDrop, midiKeyboard, inlineMidi. Added tests cover project-phase snapping, strength limits, immutable quantization and plugin file-item fallback.
- Headless Edge workflow passed: suggestion result, quantization, audition controls, lane file drop at beat 8, in-track keyboard, note movement/transposition, resizing, Alt-copy, drawing, bar/beat drop guide, no-file explanation and no browser errors. Reusable check: `Tracksmith/ui/tests/studioWorkflow.browser.cjs` (requires Playwright with Edge installed; PLAYWRIGHT_MODULE can specify its module path).
- Release native build passed with ASIO enabled. Compiler emitted warnings from existing code/dependencies. No ASIO hardware recording or listening test was performed.
- StudioElasticTests, StudioSessionTests and StudioClipTests passed. Added DSP checks cover separated attacks within 25 ms, silence, cancellation and inverse mapping. Existing checks cover pitch, stereo relationships, frame counts and timing.
- Native Time Warp diagnostic passed: analysis without mutation, A/B audition, discard rollback, apply once, undo, loop restoration, original PCM preservation, rendered WAV/session reopening, cache reuse, cancellation and reset.
- Native GGD diagnostic passed using an existing bundled GGD file: instrument notes at the drop offset, positive durations, undo and redo. Reports are beside this document.

## Files

Core: source/AttackDetection.h, ElasticMap.h, ElasticWorker.h and Main.cpp. UI: ElasticEditor.tsx, elasticEditing.ts, elastic.css, bridge.ts, Studio.tsx, InstrumentPianoRoll.tsx, InlineMidiNotes.tsx, inlineMidi.css and shared midiDrop.ts. Tests: tests/ElasticTests.cpp and the UI test files noted above. The executable is `portable/Tracksmith.exe`; keep it beside the supplied Native Instruments, Stock Plugins and ThirdPartyNotices folders.

## Notes view reference

The revised in-track layout follows the Notes view concept described in [Avid’s instrument-track documentation](https://apps.avid.com/protoolsfirsthelp/version12.3/enu/Pro%20Tools%20First%20Help/ed1.basics.23.16.html). Notes are edited inside each instrument lane, with its keyboard aligned beside the timeline. The implementation is Tracksmith code and retains its existing MIDI engine and limits. The in-track editor currently handles individual notes; use the optional larger editor for marquee and group editing.
