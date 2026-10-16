# Stereo WAV export

1. Open a session and check the mix. Track levels, pan, mute, solo, master level, and active track/master VST3 inserts are included. MIDI lanes play through their loaded instrument.
2. Click **Export WAV** at the top of the window.
3. Choose **Full session** or **Highlighted selection**. For a selection, first drag across the timeline ruler. The selection must be inside the session and have a positive length.
4. Choose **24-bit PCM** (default) or **16-bit PCM**. This build exports stereo at the current session/device sample rate; sample-rate conversion is pending.
5. Set the **effects tail**, from 0 to 30 seconds. Sources stop at the range end; inserts keep processing their remaining sound during the tail.
6. Click **Choose file & export**, then choose the output filename. A progress bar and **Cancel export** appear.

Playback stops during export. Track editing, recording and device changes wait until export finishes. Plugin editor windows close for the export; reopen them afterward. The edit position, monitoring preferences, loop and metronome settings are preserved. The metronome and live audio input are excluded from the exported mix. Sources before a selected range are rendered and discarded so held notes and effects can enter the selection naturally; a late selection can take longer to export.

The exporter uses the native mixer and existing plugin instances on a background thread. Track and master plugin latency is removed from the start and flushed from the end. A plugin latency change aborts the export and keeps the previous destination. Restore or bypass missing plugins first. A clipping message means the mix reached Tracksmith's normal output ceiling; lower levels and export again if needed.

Output is written to a temporary file and replaces the destination only after successful completion. Cancellation preserves an existing destination. Session source WAVs cannot be selected as export destinations. Limits: a range ending within one hour, a 0–30 second tail, and a standard WAV size under 4 GiB. This version has no export normalization, dithering option, MP3 export or sample-rate conversion. Hardware-only or external real-time plugin workflows are not supported by this offline export.

Validation: Release build, UI type/build checks, native WAV tests (stereo samples, 16/24-bit format, frame-exact range/tail, gain/pan/mute/solo, latency, cancellation, write errors and destination preservation), actual track/master VST3 export, existing session/plugin-host and shortcut tests passed. Listening with your own session and third-party plugins remains to be checked locally.
