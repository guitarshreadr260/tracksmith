# Tracksmith — monitoring and routing milestone

## Multiple-track monitoring

In the Edit/Mixer Inspector, Recording input → Monitor armed tracks enables live input on every armed audio track. Each track uses its assigned mono input or stereo pair. The selected-track monitor toggle remains independent; enabling both does not double that track. Disarming removes the track from the monitoring group.

The monitor-level slider is shared; track faders, pan, mute/solo, inserts and sends remain independent. Existing clips and live input go through a track's insert chain together, and through shared aux/master processors once. Recording remains dry and retains each track's assigned input. Playback, stopped transport, count-in and punch boundaries use the same live-input paths.

Up to four enabled input channels are supported, with up to eight simultaneously recorded tracks. Multiple tracks can intentionally share an input. Input choices use enabled-input order; missing device channels are normalized before count-in/punch splitting. Armed monitoring is a temporary device-facing control, not saved session state. It starts off when opening a session. Plugin latency and actual interface timing still require hardware verification.

## Pre-fader sends

Each source track still has two sends to aux returns. The new Tap selector offers Post-fader (the existing default) or Pre-fader. Pre-fader sends are after source inserts but before source volume and pan, allowing an independent headphone mix. Both tap modes follow source mute and solo. Aux returns do not send to other buses; nested bus routing and independent send-pan controls remain future work.

## Hardware cue outputs

Audio settings now allow more output channels. The main mix remains on physical outputs 1–2. An aux return's Hardware output selector can route it to enabled stereo pairs 3–4 through 15–16. Only fully enabled pairs are listed, with the device's own channel names. Routing uses physical output indices and preserves disabled-channel holes.

To make a headphone mix:

1. Enable an additional stereo output pair in Audio settings.
2. Create an Aux / Bus track named Headphones and choose that Hardware output pair.
3. On source tracks, choose Headphones as a send destination, choose Pre-fader and raise the send level.
4. Adjust the Headphones aux fader for that output's overall level. Source faders can now change the main mix independently.

A hardware-routed aux leaves the main mix: it uses its own inserts, fader and pan, then goes straight to that physical output pair. It bypasses master fader/inserts and has its own output ceiling. Master volume therefore does not change a separate cue output. Main and cue paths can have different latency after master plugins. Output clipping status includes cue outputs.

Unavailable saved outputs remain unavailable rather than falling back to the main mix. Outputs beyond physical channel 16 are cleared but not routable. Only aux returns expose separate hardware destinations; use source outputs/sends to feed those auxes. Moving the master itself, arbitrary mono output mappings and per-output calibration remain future work.

Stereo WAV export prints the main mix only. Separate hardware auxes and every live input are excluded, even while monitoring is enabled. A source routed exclusively to a hardware aux is therefore excluded from the main-mix export by design.

## Sessions and preservation

New saves use version 9 for pre/post send modes and hardware routes. Versions 1–9 are readable. Earlier versions default to post-fader sends and main outputs. Keep older session copies if you need earlier builds to open them. Plugin/track automation, recording alignment, playlists, Edit beat/bar grid and the silent-meter fix are retained.

This is a preserved internal milestone; no build review is requested yet. Tracksmith Groove stays a separate optional VST3. Plugin scanning and custom UI redesign remain deferred.

## Validation

Automated checks cover two independent monitored inputs, disarming, overlap with selected monitoring, simultaneous file playback, real VST3 track/aux/master processing, pre-fader independence, source mute/pan, separate cue samples, disabled-channel holes, count-in split routing, dry two-track recording, live-input export exclusion, version 9 roundtrips and invalid routes. Existing automation, aligned recording, session, editing, monitoring and export checks remain covered.

Physical interface/output listening, native visual checks and large-session stress tests remain pending. GitHub reference archives are unchanged; they are not an automatic mirror of local milestones.
