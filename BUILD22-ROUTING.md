# Tracksmith Build 22 — Mix inputs and live plugin-output capture

Separate local branch `build-22-plugin-routing`, based on Build 21 `046c9ed`. Earlier builds stay unchanged. No GitHub push or merge was performed.

## Where the input control is

Each audio track strip in **Mix** now has an **INPUT** selector above Inserts, as well as the existing **OUTPUT** selector below Sends. INPUT chooses what feeds and records into the audio track. OUTPUT chooses where that track is mixed.

The INPUT list contains **Audio interface** and **Plug-in → instrument track / plugin name** groups. Select the required plugin output, such as Kontakt st.1 (1–2), st.2 (3–4), or a named GGD bus. Mono/stereo input format follows that output automatically; a stereo pair can also be captured as its left mono channel using the format control. The same input choices are available in Edit view.

Instrument strips show their insert-A instrument as the source; aux strips show their assigned bus sends. These keep the same layout space as audio input controls. The transport is now visible in Mix as well as Edit.

## Capture GGD outputs onto audio tracks

1. Open the session in this Build 22 executable. Its instrument host enables the available output buses when loading GGD/Kontakt, up to 64 channels.
2. Configure the drum or microphone outputs inside GGD/Kontakt. Tracksmith lists the exposed plugin bus names and channel numbers; it cannot infer which drum you assigned to a generically named Kontakt output.
3. Create audio tracks for the required drum channels. In each Mix strip, choose **INPUT → Plug-in → your GGD/Kontakt track → the required output**. The receiving track can be before or after the instrument in the track list.
4. Turn on Monitor to hear each receiving track, or arm the receiving audio tracks for recording. Armed plugin-input tracks monitor automatically unless explicitly switched off. Leave their OUTPUT at Main stereo, or route them to an aux.
5. Mute the instrument track's own main output if it would double the sound through the receivers. This does not mute its raw plugin-output feeds. Arm only the receiving audio tracks, position the transport and press Record; Stop finishes and loads their separate WAV takes using the existing recording workflow.
6. After printing, switch off monitoring/disarm the receivers to play their recorded clips, or remove their live input assignments. While live plugin monitoring is on, the receiver monitors the input instead of simultaneously playing its recorded clip.

This follows the input-selector routing concept in [Avid’s multi-output instrument guide](https://apps.avid.com/protoolsfirsthelp/version12.3/enu/Pro%20Tools%20First%20Help/mix1.basic.54.46.html).

## Mixer alignment

Audio, instrument and aux strips reserve identical space for Input controls. Insert, send, pan, arm/solo/mute, fader and meter groups align vertically. Track-strip scrolling is shared so one strip cannot scroll out of alignment independently. Meter panels use a shared 230-pixel height, including the master; track fader sections use a shared 240-pixel height. Mono and stereo pan groups keep the same height. The master retains its distinct control layout.

## Engine and session changes

- Native instrument instances process their enabled output buses into preallocated channel buffers. The ordinary instrument strip uses the first output pair; receiving audio tracks tap the selected raw instrument output before DAW faders/inserts. Each receiver can then use its own normal inserts and mixer controls.
- The graph renders instruments first, then audio receivers. Monitoring, metering, real-time recording and offline mixing use the same current-block output feeds. The callback does not allocate route or audio buffers.
- The recorder resolves hardware or plugin sources per graph block, preserving physical input indices. Plugin takes use the source instrument's reported latency for alignment instead of hardware input latency. Record up to 32 receiving tracks from one instrument in a pass. Mixing hardware inputs with plugin recording, or recording multiple different instruments in one pass, is rejected with a message; record those separately.
- Raw input printing excludes the receiver's DAW inserts and fader processing. GGD/Kontakt's internal mixer processing is part of its output.
- Output changes are handled while the callback is detached; mismatched layouts are silenced until rebuilt. Missing/bypassed outputs are unavailable, rather than falling back to a hardware input. Track deletion disconnects references to the deleted instrument and remaps remaining references.
- Session format is now **12** to save plugin-input assignments. Builds 1–11 sessions remain readable. Save to a new filename when comparing with older builds, which cannot read format 12.

## Validation

- Production TypeScript/Vite build and Windows Release native build with ASIO enabled passed. Existing compiler warnings remain; no audio-interface hardware test was performed.
- Eight UI suites passed. Headless Edge verified the actual Mix INPUT and OUTPUT controls, plugin groups, source-token command payload, automatic format display and the existing editing/stem controls, with no browser errors.
- Layout checks compared control positions and heights across stereo audio, mono audio, instrument and aux strips at 1600×1100 and 1600×800; all matched. Every meter panel measured 230 pixels. Screenshots use mocked bridge state.
- Native integration passed with a deterministic multi-output instrument: receiver tracks preceding their instrument, mono/stereo monitoring, real-time separate WAV recording without physical inputs, exact one-second takes, 128-sample plugin-latency alignment, and 997-frame device callbacks crossing internal graph blocks.
- The same integration passed offline mix capture of routed plugin audio, normal physical-input capture after the recorder change, session routing encode/decode, unavailable output checks, mono bus boundaries and deletion remapping.
- Existing mixer, session-format and recording tests passed.
- A loaded GGD kit and real-device recording were not tested. Your actual GGD routing, playback and recording remain the required hands-on check.

Core files changed: PluginInput.h, PluginRack.h, PlaybackCore.h, AudioEngine.h, RecordGroup.h, SessionFile.h, TrackEditing.h and Main.cpp. UI: Studio.tsx, bridge.ts and mixer-alignment.css. Added native PluginRoutingTests plus shared multi-output fixture and extended browser checks.

Run `portable/Tracksmith.exe` alongside its supplied Native Instruments, Stock Plugins and ThirdPartyNotices folders.
