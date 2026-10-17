# Build 26 - mixer response and Edit/Mix shortcut

Based on Build 25 (`54e9ff8`). User confirmed the drums printed, then reported a brief lockup while lowering an audio-track volume and Ctrl+= zooming instead of switching windows.

## Changes

- Track gain, pan, mute/solo and arm changes apply to audio atomics immediately; project-control feedback is coalesced into the regular UI update. They no longer synchronously publish every track's cached waveform and playlist data for every slider event. The next feedback snapshot includes project controls without waveform arrays; media/edit operations still publish full data.
- Master-volume changes also request lightweight project feedback.
- Ctrl+= toggles Edit/Mix, including when a fader or text field has focus. Ctrl+Shift+M remains an alias. Key repeat does not toggle repeatedly; dialogs retain their focus behavior.
- Space after adjusting a focused range/fader controls transport. Text-entry fields keep ordinary Space behavior. This was a separate focus-related reason Space could appear unable to stop.
- Stop clears the playing flag before resetting the plugin graph.
- The Windows build uses an isolated, build-local JUCE gui_extra copy with WebView2 browser zoom and browser accelerator keys disabled and zoom reset to 100%. Original JUCE sources and earlier builds remain unchanged. Normal text-editing shortcuts remain enabled by the WebView2 setting. [Microsoft documentation](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/winrt/microsoft_web_webview2_core/corewebview2settings?view=webview2-winrt-1.0.4022.49).

## Validation

- Native 48-track / 1000-fader-command test passed: no synchronous UI/waveform publication during the burst, final gain reaches audio atomics and UI feedback, and Stop halts playback. Full snapshot: 17,253,143 bytes; mixer feedback: 60,455 bytes. Measured command handling: 3.38 ms in a synthetic run with no device/plugin. This is not a measurement of the user's GGD/ASIO pause.
- Build 25 recording integration and full session suite still pass: eight repeated isolated kick/snare prints, repeated Stop, long latency drain, punch completion/cancellation and immediate printed playback.
- TypeScript/Vite production build and all eight UI suites pass. New keyboard tests cover Ctrl+= with focus, repeat suppression, fader-focused Space and text-field protection.
- Headless Edge workflow passes: Edit/Mix toggles, fader adjustment followed by Space sends Stop, viewport scale stays unchanged, prior routing/MIDI/stem controls pass, no browser errors. Browser state is mocked; this does not drive a real Kontakt instance or native WebView2 accelerator events.
- Windows Release application compiles with the isolated native browser patch and ASIO enabled. Existing compiler warnings remain.

## Try it

Close the other Tracksmith and run `portable/Tracksmith.exe` with its supplied folders. Open a session copy, play the printed drums, lower an audio-track fader, and press Space while the fader still has focus. Verify Ctrl+= switches both ways without resizing the interface. R/T still zoom the timeline.

Session format remains 12. Real GGD/ASIO validation of the reported pause and native keyboard behavior remains for the user. Build 26 is saved locally; GitHub main remains Build 24 until another update is requested.
