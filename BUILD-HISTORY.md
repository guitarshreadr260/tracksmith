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
