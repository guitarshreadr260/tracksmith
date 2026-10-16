# Tracksmith Build 19 — Time Warp preview

Implemented on the separate local `build-19-time-warp` branch from accepted Build 18 checkpoint `93ae9fe`. Builds 16, 17 and 18 are unchanged. No push or merge was performed.

## Using Time Warp

1. Select an audio clip. Open **Clip → Time Warp timing…**.
2. Add a timing anchor. Set its Original seconds to the note you want to move.
3. Drag the destination marker or enter Destination seconds. The source and current rendered waveforms are shown separately.
4. Choose **Render timing edit**, then play the result. Marker previews alone do not modify audio.
5. Use Undo/Redo, or Restore original timing. Restore is also undoable.

The two outer edges remain pinned. Total duration and nominal pitch are retained. Source regions are limited to 0.5–30 seconds, at most 16 anchors, a minimum 50 ms between anchors, and local output/input ratios of 0.75–1.5. These limits match this first manual-timing milestone. Sample-based clips do not automatically move or stretch when session tempo changes.

## Implementation

- A pinned Signalsmith Stretch/Linear engine, with original MIT licenses, renders stereo channels together on a background worker. Exact dual-mono inputs remain dual mono.
- Every edit reads the original PCM region, not a previously stretched buffer. The original source file and resident source samples are retained.
- Generated WAV media is keyed by source PCM, sample rate, anchor map and implementation/dependency version. Identical edits reuse the media and source slot. WAV length/sample-rate validation protects against incomplete cache media.
- The request stops playback; the audio callback only uses completed buffers. A failed/cancelled job leaves the previous clip intact. Project mutations and saving are blocked during the job; cancellation and application shutdown join the worker safely.
- Completed media is installed through the existing clip history and playlist system, with a new rendered waveform and a link to original source/offset/length/anchors. Split/trim slices retain that provenance; re-editing their map operates on the full underlying timing region while playback uses the selected slice.
- Session format **11** saves the original-source link and timing map for active and alternate playlists, as well as the rendered-media path. Versions 1–10 remain readable. Older Tracksmith builds do not read version 11: save Build 19 test sessions under a new name.
- The native source-media and in-memory session limits remain enforced. Each distinct applied render can consume another retained source slot; original and prior-render sources remain available for Undo and saved playlists.
- New UI: Time Warp editor, ordered/bounded anchor validation, mouse/keyboard destination-marker movement, numeric source/destination times, pinned-edge display, render/cancel/discard/reset actions and source/rendered waveform views.

Generated media lives under the current Windows user's `Tracksmith/Time Warp Media` application-data folder. It is **persistent session media**, not disposable temporary data. Keep referenced rendered WAVs and original files when transferring/backing up sessions. Automatic cache eviction and automatic regeneration during session open are not implemented; files are deliberately retained so Undo and session references stay valid. A render can be requested again from the original source when its provenance is available.

## Executed verification

- PASS: TypeScript/Vite production build.
- PASS: elasticEditing.mjs map bounds, ordering, fixed edges, invalid numbers and anchor insertion. Existing command, shortcut, clip-geometry and MIDI suites also pass.
- PASS: browser interaction tests for menu/editor opening, invalid-map blocking, render command payload, render busy/cancel state, restore/undo dispatch and source/rendered waveform display. Repeated state updates retain exactly one clip inspector; no React/page errors. These tests simulate native state.
- PASS: ASIO-enabled MSVC Release app build. Existing native/library compiler warnings remain. Stock plugin binaries are reused from Build 18.
- PASS: standalone DSP tests: identity-map bit equality; exact output frame count; 440 Hz tone preserved within the test's 6 Hz estimation tolerance; identical-channel equality; correlated-stereo channel relationship within 1e-7 relative error energy; finite samples; moved-note envelope within 65 ms of the requested anchor; bounded ratios; cancellation. The timing tolerance reflects the spectral analysis window rather than a sample-accurate transient guarantee.
- PASS: native clip and session regression suites, including version 11 timing metadata, version compatibility and invalid-map rejection.
- PASS: packaged-app asynchronous regression: the previous clip remains installed until success; original PCM unchanged; rendered media/provenance installed; WAV and session reopening reproduce the saved audio; Undo/Redo; identical-edit cache reuse; cancellation leaves previous audio; original restoration and its Undo.

The packaged-app result is in BUILD19-time-warp-regression.txt. The preview image uses a simulated session and illustrates the controls.

## Remaining scope and listening checks

This is a first **manual** Time Warp build. Automatic transient detection, quantization strength, tempo following, grouped multitrack warping, pitch/formant controls, real-time preview, automatic media eviction and cross-machine media collection remain future work.

Synthetic tests establish timing, pitch and state-management behavior; they do not establish listening quality on every guitar, drum or full mix. Audition sustained guitar notes, chords, transients, clip edges and stereo recordings on your interface before relying on a correction. Hardware ASIO recording/listening was not performed by these automated checks.

The backend is [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch), whose author recommends modest time changes and documents latency/seek/flush handling. This build uses its documented output-seek and flush path. It does not use Avid's processing algorithms.

## Package

Run `portable/Tracksmith.exe` with its sibling Native Instruments and Stock Plugins folders. Close earlier builds before a live audio test. The source and all new dependency headers/licenses are included in this worktree.
