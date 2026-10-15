# Tracksmith reliability — first automated pass

October 14, 2026. **PASS for this bounded automated run.** No production code fixes were required. The existing GGD MIDI Fix application matches the executable tested by source revision; the application binary hash also matches the delivered ZIP.

## Sustained workload

Two separate runs, approximately 180.2 seconds total wall time (60 seconds, then 120 seconds), rendering 178.3 seconds of paced synthetic audio. Saves, reopens and recovery checkpoints pause playback as intended, so rendered time is slightly shorter than elapsed time.

- 80 tracks: 64 resident audio tracks, 8 aux buses and 8 MIDI lanes.
- 16 real VST3 instances of the controlled delay-test effect, on source and aux channels. MIDI lanes exercise scheduling; they are not eight third-party virtual instruments.
- 48 kHz; varying 128, 256, 512 and 1024-frame calls through the production renderer, across 16 physical-output slots including separate cue routes.
- Looping, repeated seeks/reset, automation reads, pre/post sends and a synthetic monitor signal at the playback monitor summing point.
- Eight simultaneous mono recording queues/writers using four distinct synthetic input signals, with repeated two-second takes.
- Repeated audio splits/restoration, MIDI group deletion/restoration, VST3 replacement/reset, asynchronous session save/open, recovery snapshots and worker-overlap refusal.
- All fixture media, sessions and recovery data use isolated test folders. The running DAW and user session were not operated on.

## Results

| Check | Combined result |
| --- | --- |
| Audio blocks processed | 17,833 |
| Blocks exceeding their corresponding frame-time budget | 0 |
| Successful eight-track recording passes | 19 |
| Recorded WAV files checked | 152 |
| Repeated edit cycles | 90 |
| Successful session save/reopen cycles | 20 |
| Validated recovery snapshots | 20 |
| VST3 unload/reload cycles | 10 |

Every recording completed without a writer error or warning. Tests verified expected frame counts, WAV formats and assigned-input sample values. All rendered output samples were finite, and both main/cue paths produced a nonzero signal. Session reopens retained 80 tracks, referenced audio, MIDI notes, playlists and all 16 inserts. Recovery snapshots remained discoverable and readable.

The two-minute run used an average 13.9% of the available audio-frame time for the timed render/capture section. Its longest measured section was 7.53 ms; individual deadlines vary with the block size, and 0 sections exceeded their matching deadline. These are synthetic processing-budget measurements, not device callback/xrun measurements.

Process memory monitoring sampled once per second. Peak private memory: 49.25 MiB. Peak working set: 76.49 MiB. The minimum private-memory floor changed by 1.24 MiB between the 20–40-second window and the final window (100 seconds onward). Short runs and allocator behavior cannot establish the absence of a memory leak; raw samples are included.

## Other checks run

Passed: native VST3/NAM/MIDI host tests, all 225 saved GGD imports, session/recovery checks, recording queue/input/WAV writer tests, stereo export, clip/fade edits, routing, automation, standalone monitoring and playback suites. Frontend TypeScript, global/MIDI keyboard guards, note-edit/selection math and MIDI-drop transfer checks also passed.

## What this establishes

The current engine passed an initial sustained synthetic workload and repeated file/plugin/edit lifecycles. No test failures or production crashes were observed. The executable remains the working GGD MIDI Fix build; this work adds a reusable test harness and report.

This is a first reliability pass, not final installer readiness. It does not measure your ASIO/device callback, microphone timing, audible glitches, UI mouse interactions, multi-hour behavior or CPU-heavy third-party instruments. The 16 plugins are controlled delay fixtures; this is not a 128-heavy-plugin performance claim. Next is a longer hardware session with representative plugins and normal UI use, followed by fixes if needed and the installer when those checks are satisfactory.

Application SHA-256: `dbc64f42086e517148e6fa4f421c84a85e58885ff03d3868a799337bff5ae62b`

The accompanying results archive contains raw run metrics, memory samples, logs and harness source. No GitHub update was made during this test.
