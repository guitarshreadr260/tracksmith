# Build 29 - October 17, 2026

Local build: drag the bottom edge of Edit tracks to resize, choose height presets, and double-click to reset. Audio waveforms grow; instrument piano rolls show more pitches with aligned keys. Heights save in sessions as optional version-13 metadata. [Notes and validation](BUILD29-TRACK-HEIGHTS.md). Published main remains Build 24.

---

# Build 28 - October 17, 2026

Local build: opaque track-colored clips, mini-keyboard/note-grid pitch scrolling, expanded editable transport counters and selection fields, independent nudge, selection playback pre/post-roll and Big Counter/display presets. Session format remains 13. [Notes and validation](BUILD28-TRANSPORT-PIANO.md). Published main remains Build 24.

---

# Build 27 - October 17, 2026

Local edit-workflow build: selectable tools; R/T zoom; 256-bar arrangement space with a wider initial ruler; saved track display order/colors; insert-style send slots and controls; no piano roll on click tracks. Session format 13 reads 1-12. [Build notes and validation](BUILD27-EDIT-WORKFLOW.md). Published main remains Build 24.

---

# Build 26 - October 17, 2026

Local mixer-response and shortcut fix. User confirmed drums printed in Build 25. Lightweight fader feedback, Ctrl+= Edit/Mix switching, native browser zoom suppression and fader-focused Space transport are implemented. [Details and validation](BUILD26-MIXER-FIX.md). Real-device confirmation remains pending; published main remains Build 24.

---

# Build 25 - October 17, 2026

Local recording reliability build based on published Build 24 main. Fixes repeated Stop extending latency drain; expands recording, punch, printed playback and session recall regression coverage; supplies a fresh local Windows portable application. [Notes](BUILD25-RECORDING.md) | [Validation](BUILD25-VALIDATION.txt). Live GGD/ASIO testing remains pending.

---

# Build 24 - October 17, 2026

Current main checkpoint: menus/shortcuts (17), editing (18), Time Warp (19-20), inline MIDI and clipboard (20), drum stems (21), plugin-output recording and aligned mixer inputs (22), and recording Stop fix (23). Build 24 updates release identity and documentation, retaining the Build 23 engine. Build 16 is preserved by tag; history is not rewritten.

# October 15, 2026 — UI Prototype 16

Modern UI/startup art; native EZ instruments; editing/monitoring and scanner improvements; 32-input/16-output I/O setup; session 10; musical grid and exact Dynamic punch. Release build, four native suites, UI checks and packaged editing regression passed. Elastic Audio and real-hardware validation remain pending. [Details and source restoration](prototype-16/BUILD.md).

# Build history

| Stage | Main change | Current status |
|---|---|---|
| WAV export | Full/selection stereo WAV with tails and cancellation | Automated checks passed |
| Mixer routing | Aux/bus returns, outputs and two post-fader sends | Delivered; user approved |
| Recording/monitoring | Multi-track dry recording with existing audio playback during monitoring | Delivered; user approved |
| Takes/playlists | Recorded-range replacement, fades/crossfades, alternate audio/MIDI playlists | Delivered; user approved |
| Plugin browser / New Tracks | Saved plugin browser, isolated discovery, interrupted-load recovery; Ctrl+Shift+N opens track choices | Scanner improvements deferred by user |
| Automation | Track volume/pan Off/Read/Write, points, undo and export | Automated checks passed; listening feedback pending |
| Groove VST3 0.18 | AVRX Groove renamed, instrument wrapper, host sync and full state recall | Automated checks passed; user test pending |

Versioned archives remain saved on the development PC. This repository starts with the latest reference snapshots; it does not reconstruct every earlier source commit.


## Updates after the initial reference upload — October 14, 2026

| Build stage | Main additions | Status |
| --- | --- | --- |
| Plugin automation | Plugin curves and Read/Write/Touch/Latch | Automated checks passed; hardware/native review remains |
| Recording and monitoring | Reported-latency alignment, armed-track monitoring, pre/post sends and hardware cues | Delivered; automated checks passed |
| Playlist/group editing | Playlist management, alternate lanes, range promotion and explicit group operations | Delivered; automated checks passed |
| Review build | Autosave/recovery, rotating snapshots and corrupt-snapshot fallback | User said so far so good; broad reliability/installer still pending |
| Instrument piano roll | Instrument track type, inline draw/move/resize, velocity/channel and MIDI groove drops | Delivered; user then reported selection/deletion limitations |
| Selection and Shortcuts — current | Box/Shift note selection, group delete/Undo, clipboard and more shortcuts; track-name context menu | Automated checks passed; latest manual review pending |

The latest source/build links are in README. Session format is now 9; older Automation archive documentation describes that older build's format 7. Earlier archives and Groove downloads remain preserved.


## GGD fix and first reliability checkpoint — October 14, 2026

| Stage | Result |
| --- | --- |
| GGD MIDI fix | Duplicate/zero-gate normalization, preserved distinct attacks, explicit drop results and automatic note-register view. All 225 saved grooves import; user confirmed working. |
| Reliability first pass | 60-second and 120-second paced synthetic runs: 80 tracks, 16 controlled VST3 effects, 152 WAVs, 20 session reopens, 20 recovery snapshots, 90 edits and 10 plugin reloads. Passed; no production change required. |
| Next | Longer hardware/interface, representative-plugin and normal UI tests remain; then installer and custom UI. |

README now points to GGD MIDI Fix and a newer source checkpoint containing the test harness. The report distinguishes synthetic processing-budget results from device/xrun performance. Previous source/build archives and the optional Groove VST3 release remain preserved.


## Windows installer and stock plugin suite — October 15, 2026

The user reported that the first 0.1.0 Windows installer trial went well. That installer predates the current stock suite and remains an earlier milestone.

The new portable test build packages nine VST3 plugins: EQ 7, Gate, Bus Compressor, FET Compressor, Limiter, De-Esser, Plate, Digital Delay and NAM. Reverb is plate-style with stereo diffusion; delay is clean and unmodulated. NAM was renamed from AVRX NAM, gained post-model EQ and preset file save/load, and retained its original VST3 identities and parameter IDs for compatibility.

DSP, VST3 loading/state, native editor rendering, NAM preset compatibility and existing DAW regression suites passed. Package hashes and nine included bundles verified. The current stock suite has not yet received the user's listening pass. Earlier source/build snapshots and the separate Groove release are preserved.

[Current Windows test build](Tracksmith-Windows-Stock-Plugins-Test.zip) · [Current source](Tracksmith-Source-Stock-Plugins.zip) · [Guide](Tracksmith-Stock-Plugins-Guide.md) · [Test results](Tracksmith-Stock-Plugin-Test-Results.json)
