# Aux tracks, buses and sends

## Share an effect

1. In Edit, choose **New track → Aux / Bus** and click **Add track**. Rename it, for example, Shared Reverb.
2. Open Mixer. Load your reverb or delay VST3 in an insert on the aux return. For a parallel effect, set the plugin's own mix to **100% wet**.
3. On each source track, select the aux return in **Send A** or **Send B**, then raise that send's level. Sends begin at silence. Keep **Output → Master** for the source's normal dry sound.
4. The aux fader, pan and mute control the shared effect return. Source mute removes that source from its output and sends. Source solo keeps its shared returns audible. Aux solo auditions that return and its feeders, suppressing the other returns and direct source outputs.

Send levels are post-insert, post-fader and post-pan. They can be adjusted during playback and recording. Track fader changes therefore change both the direct sound and its effect feed. Aux and master inserts process the combined signal once, including when clips play alongside live input monitoring.

## Use a bus for a group

Create an Aux / Bus track and name it, for example, Guitar Bus. Change each guitar track's **Output** from Master to Guitar Bus. Their entire output now passes through that bus's inserts, fader and pan before reaching Master. The source is not duplicated directly to Master. Sends can still feed other aux returns independently.

A source can deliberately feed the same aux via its output and a send; those feeds add together. Normally choose one route unless you want the extra level.

## Save, reopen and export

Routing, send levels, aux track type and aux insert settings save with the session. New saves use **session version 5**. This build reads version 1–4 sessions with direct-to-Master routing defaults; older builds cannot open version 5 saves. Keep an older session copy if you need to return to an older build.

WAV export includes buses and shared effects, uses the same native graph as playback, and compensates the latency of source inserts, the aux stage and master inserts. Choose an effects tail long enough for the reverb or delay. Ordinary playback retains the existing session-end behavior; use the export tail to capture effects beyond the last clip.

## Current limits

- Up to 16 stereo aux returns within the 128-track session limit.
- Two post-fader sends per audio or MIDI/instrument source track.
- Aux returns feed Master only. Aux-to-aux routing, feedback, pre-fader sends and hardware output routing are not included in this build.
- Routing destination changes stop playback and are blocked during recording. Send levels, faders, mute and solo remain adjustable.
- Aux tracks receive bus audio; they cannot import WAVs, host MIDI instruments, arm for recording or receive a separate hardware-monitor assignment.
- Live monitoring remains on one selected audio track and follows that track's bus/sends. Additional effects and bus compensation can add monitoring latency. Hardware recording-latency alignment is still pending.
- Reverb/delay effects are supplied by the VST3 plugins you load; no new stock reverb/delay plugin is bundled.

Mixer columns now contain output and send controls. Scroll horizontally for more tracks and vertically for the faders and lower controls when needed.

## Verification

Native tests cover shared send sums, group output routing, one nonlinear aux/master effects pass, post-fader level changes, source/aux mute and solo, pan, graph validation, saved routing, older-session defaults, and actual VST3 aux/master latency compensation at the first and last samples of WAV export. Actual NAM VST3 tests cover live input plus clips through a shared aux and master, zero live-monitor gain with clips retained, and stopped live monitoring. Recording, eight-take imports, export, playback and keyboard regression checks are retained. User-interface click-through, hardware listening and third-party plugin combinations need local validation.
