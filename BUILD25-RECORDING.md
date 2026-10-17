# Build 25 - recording reliability

Based on Build 24 (`04d7a0f`). Normal GGD routing remains: assign outputs inside GGD/Kontakt, select the matching audio-track Mix INPUT, arm and record. No new printing workflow or Library sidebar.

## Fix

Repeated Stop presses during recording latency drain could move the capture endpoint forward again. A native regression reproduced this against the unchanged Build 24 engine. The first Stop now fixes the endpoint; later presses leave it unchanged until recording finishes. The guard resets for the next take and when recording stops or cancels. Session format remains 12.

## Validation

See [full validation](BUILD25-VALIDATION.txt). Native tests cover repeated routed drum recording, exact mono/stereo WAV content and duration, long-latency repeated Stop, punch completion/cancellation, waveform decoding, immediate printed playback, and on-disk session serialization for routes, MIDI, media references and plugin-state bytes. UI build, all eight UI suites, browser workflow and native Windows Release build pass.

GGD/Kontakt and a physical ASIO interface were not used in these tests. Plugin-state serialization is tested; actual kit reload and real-device performance need hands-on verification. Screenshots use mocked browser state.

## Run and check

Close other Tracksmith instances, then run `portable/Tracksmith.exe` with its supplied folders beside it. Save a copy of your session before comparing builds.

1. Load your GGD kit and MIDI groove. Assign kick/snare outputs in its mixer.
2. Choose those matching outputs in the receiving audio tracks' Mix INPUT menus; arm and record.
3. Press Stop (including several presses), then disarm/turn off input monitoring and play the printed clips.
4. Check each waveform, timing and channel isolation. Save a new session, close and reopen; verify MIDI, plugin kit and output assignments, audio tracks and recorded clips.
5. Try punch recording and Stop before/during a punch. Report any hang, missing output, timing shift or recall failure.

The portable executable is local; no installer or GitHub release assets were published for Build 25. Build 24 remains the published main until a subsequent GitHub update is requested.
