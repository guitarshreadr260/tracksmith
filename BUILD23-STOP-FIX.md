# Tracksmith Build 23 — Stop fix for normal routed recording

Local branch `build-23-record-stop`, based on Build 22 `e6fe99a`. Earlier builds remain unchanged. Nothing was pushed or merged on GitHub.

## Use the same GGD routing

The selected GGD/Kontakt output feeds the receiving audio track's Mix INPUT selector. Arm the receiving audio tracks and record normally. Build 23 retains the Build 22 routing, audio-track capture, mono/stereo formats and aligned Mix controls. No new printing workflow or routing workaround was added.

Configure the drum outputs in GGD/Kontakt, choose the matching **Mix → INPUT → Plug-in → instrument/output** on each receiver, and record. Mute the instrument strip's own main output if it would double the receivers; that mute does not mute their raw output feeds. Stop should now finish recording normally. Turn off input monitoring/disarm after recording to hear the printed clips.

## Reproduced fault and correction

The recording drain code could receive a Stop request with zero samples of recording correction. This made the capture-end boundary equal the current recording position. The next callback calculated zero frames and exited its loop before closing the recorder. Recording therefore remained active and repeated Stop requests could leave it in the same state. This is consistent with the reported inability to stop; a controlled native test reproduced it before the fix.

The engine now finalizes an already-reached capture boundary before calculating the next block. Manual Stop halts transport at that boundary, without waiting for the disk-writer completion callback. Stop before a pending punch start also halts transport. Natural punch completion retains its existing playback-continuation behavior.

The Stop button remains available while WAVs finalize. Space also sends Stop while recording finalizes instead of starting playback, and repeated Stop requests during finalization avoid resetting the plugin graph repeatedly.

## Validation

- New native regression failed against the unmodified Build 22 recording code with `Stop closes recorder at zero/nonzero latency boundary`.
- After the fix it passed at recording corrections of 0, 1 and 128 samples: recorder closes, playback stops before writer finalization, the WAV finishes and its captured length remains exactly 997 frames after the latency drain.
- The same native integration retained its passing output checks: selected kick and snare paths print into their respective mono/stereo WAVs, without channel contamination; the receiving tracks precede their instrument in the track list. Plugin latency alignment, 997-frame callback boundaries, physical-input recording, routed offline mix, session routing persistence and track-deletion remapping also pass.
- All eight UI suites passed, including the new Stop-availability check. Headless Edge passed the recording-finalization Stop-button and Space checks along with normal Mix input selection, plugin routing payloads, mixer alignment at two window heights, MIDI editing and existing stem controls. Browser state is mocked; no browser errors occurred.
- Production TypeScript/Vite and Windows native Release build with ASIO enabled passed. Existing compiler warnings remain.
- A loaded GGD kit and live ASIO device were not used for the reproduction. The fixed condition is demonstrated by the native test; the user's GGD record/Stop workflow still needs a hands-on check.

Changed: source/AudioEngine.h, source/Main.cpp, ui/src/Studio.tsx, native PluginRoutingTests.cpp, command tests and browser checks. Session format remains 12, as in Build 22. Use a new session filename when comparing with builds older than 22.

Run `portable/Tracksmith.exe` with its supplied instrument/plugin folders beside it.
