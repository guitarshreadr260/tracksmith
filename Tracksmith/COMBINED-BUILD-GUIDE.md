# Tracksmith — combined automation, recording and Edit grid build

## Included

- Touch and Latch for track volume/pan and selected VST3 controls.
- Plugin Write capture, in addition to the existing drawn plugin curves.
- A global automation compare/bypass button.
- Recording alignment from reported device input/output latency plus the plugin playback path.
- An Edit window beat/bar grid, bar-number ruler and Grid ON/OFF button.

## Automation

Select a track and scroll the left Inspector to Automation. Volume/Pan lanes and mixer strips offer Off, Read, Write, Touch and Latch. For a plugin choose its insert under Plugin curves, then choose a Control and mode. Open the plugin's editor to move that control.

- Off: manual controls. Read: follows saved curves.
- Write: captures throughout playback and returns to Read on Stop. Track Write captures both faders; plugin Write captures each control explicitly armed for Write.
- Touch: follows Read until a control is held, records its movement, then returns to the saved curve on release. Separate gestures preserve automation between them.
- Latch: follows Read until a control is touched, then keeps writing that control's last value until Stop. Untouched controls keep reading. Touch/Latch remain armed for another pass; gestures reset each time playback starts.

Turn Loop off before arming Write/Touch/Latch. Modes can be changed while stopped. Audio recording and automation writing use separate passes: put armed automation in Read/Off before recording audio. Read automation still works during recording and monitoring. Saving/exporting stops and commits pending automation.

Use Undo automation for track passes and Undo plugin curve for a plugin insert. Buffers and histories are bounded. Failed or oversized passes keep earlier curves, with a status message. Capture runs at the UI's 20 Hz control rate; plugin playback applies changes per block (up to 256 samples). Very fast gestures have limited timing resolution. Touch/Latch for native plugin windows require the plugin to report gesture start/end; Write remains available for controls without gesture reporting.

Automation ACTIVE/BYPASSED in the Inspector suspends track and plugin curves without deleting them. Toggle while stopped, then play to compare. Bypass uses track manual values and holds current plugin values. It also suspends automation capture. This is a temporary comparison setting, not a saved mix state; opening a session resets it. Modes and gestures cannot be used to write while looping.

Session format stays version 8. Curves save by stable plugin parameter IDs. Write/Touch/Latch arming saves as Read, so opening a session never starts overwriting curves. Automation undo is temporary, not session history.

## Recording alignment

Recording input in the Inspector has Latency alignment ON/OFF. It defaults ON and displays the correction used for the last take in samples. The correction sums the driver's reported input latency, output latency and the playback plugin path. All armed tracks share the same timing correction while retaining independent input routes.

Capture skips the delayed prefix and continues through an equal tail, so new audio begins at the intended timeline point. Punch recordings keep the exact selection length. Live monitoring and existing-take playback switch at the musical punch boundaries rather than at the delayed disk-capture boundaries. Pressing Stop finishes the short capture tail before the WAV is finalized. Count-in and cancellation before punch retain their existing behavior. The resulting WAV remains dry; no effects are printed.

This uses driver reports, not a measured hardware loopback calibration. Driver accuracy, interface loopback and manual/per-input offsets still need hardware verification and remain follow-ups. Changing the alignment toggle affects future takes, not existing recordings. Disabling alignment restores the previous capture timing.

## Edit grid

The Edit window has lighter beat divisions and stronger four-beat bar lines, aligned to beat zero and clip positions. The ruler shows bar numbers. Zoom changes spacing while preserving beat positions. Grid ON/OFF controls visibility independently of the existing beat-snap control. Time signature remains 4/4. Fractional-beat snap choices and other time signatures remain future work.

## Delivery and validation

Extract the whole Windows ZIP, keep Stock Plugins beside Tracksmith.exe, and close the older build after saving your work before opening it. Older builds cannot read version 8 sessions, so keep session copies.

Automated tests cover Touch/Latch playback, preserved intervals, actual VST3 gesture behavior and bypass, plugin export, aligned punch length/impulse placement, Stop-tail capture and existing recording/monitoring/session/routing/edit/export behavior. Native visual review, physical fader gestures and interface loopback listening remain unverified. Browser preview was unavailable; the local-file preview was blocked by browser policy and was not bypassed.

Per your preference, this build is preserved as an internal milestone; no review is needed before development continues. Next planned group: multiple-track monitoring, pre-fader sends and hardware output routing. Plugin scanning and custom UI redesign remain deferred.
