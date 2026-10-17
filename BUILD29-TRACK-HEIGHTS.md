# Build 29 - expandable tracks

Based on Build 28 (4a042c2). Earlier build folders remain unchanged. This build is local; nothing was pushed to GitHub.

## Use

Drag the bottom edge of an Edit track up or down. Double-click the edge to reset its default height. The track header also offers Compact, Medium, Large, Jumbo and Maximum presets. A focused resize edge supports Up/Down and Home; Escape cancels a drag.

Audio waveforms fill the expanded row. Instrument piano rolls reveal more pitches while keeping the keyboard and note grid aligned, with existing pitch scrolling preserved. Click tracks remain without a piano roll. Mix strip geometry is unchanged.

## Implementation

Reusable TrackResize control and trackHeights helpers provide local drag previews and one native metadata update on release. Height presets share the same bounds. Audio rows support 126-900 pixels; instrument rows support 216-900 pixels and 12-88 visible pitches. Session format remains 13 with an optional per-track height field; older files default to their previous heights. Track add/delete/reorder and session recall preserve the metadata. Resizing does not reset the audio engine or resend waveforms.

## Validation

- Windows Release build with ASIO: passed.
- TypeScript/Vite production build and all ten UI suites: passed.
- Headless Edge workflow: passed, including audio drag, Escape cancellation, height presets, expanded MIDI key/grid alignment, drawing notes in newly exposed pitches and existing workflow regressions.
- Native session tests: passed, including height round-trip, invalid height rejection and older version-13 files without heights.
- Native application workflow regression: passed, including height recall and deletion/remapping.
- Expanded audio and MIDI screenshots inspected. Real-device use of this build remains for user confirmation.

Run portable/Tracksmith.exe, keeping the accompanying portable assets together.
