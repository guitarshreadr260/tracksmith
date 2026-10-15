# Recording and playback while monitoring

## Listen to audio files and live input together

Import or record an audio file, enable **Input monitor** on its track, then press Play. The existing clips and live input are combined before that track's inserts. The combined track then passes through its volume, pan, mute/solo and master inserts. Other playback tracks remain audible. Track and master inserts process the combined sound once.

**Monitor level** controls only the live input, before track inserts. Setting it to zero leaves the existing clips audible. Track volume affects both clips and live input. Recordings remain dry; monitoring and effects are not printed into their WAVs. Stopping playback stops the clips while live monitoring remains available. Input monitoring currently selects one live track at a time.

In **Selection punch**, existing clips play outside the selected range; live input replaces the armed tracks inside it. Normal recording keeps existing audio clips playing for overdubbing.

## Record multiple audio tracks

1. In **Audio settings**, enable the inputs you need. The current device setup supports up to four enabled input channels.
2. Select each audio track and use its inspector's **Capture** and **Input source** controls to choose mono or a stereo pair. These choices belong to that track and are saved with the session. Input numbers refer to the enabled channels on the current device; check them after changing the device or enabled channels.
3. Arm each destination with **R**. Arming another track keeps earlier tracks armed. Up to eight audio tracks can record together, including tracks sharing the same input.
4. Enable input monitoring on the track you want to hear live. This uses that track's input assignment independently of the other recording routes.
5. Press Record or **Numpad 3**. All armed audio tracks start from the same timeline position and share the count-in and punch boundaries. Each writes its own dry WAV.
6. Press Stop and wait for all takes to finish loading. Each take appears on its destination track at the same recording origin. Punch takes preserve the existing audio before and after the selected range.

Track input and mono/stereo assignments remain fixed during a take. MIDI lanes must be disarmed for audio recording; hardware MIDI recording is still pending. Missing armed inputs are rejected before capture. If a writer or input stops during capture, the recording group stops and valid partial files are retained with a message. Shared free-session memory is divided across the armed tracks, so the maximum take length can decrease when more tracks are armed.

Automatic hardware recording-latency alignment remains pending. The tracks in a capture group share sample boundaries, but device and plugin monitoring latency still affect alignment with existing playback. WAV mix export remains available from the previous build.

## Verification

Release and UI builds passed. Native tests cover clips plus live input through actual track/master NAM VST3s, one effects pass, live-input gain zero with clips still audible, mute/solo/pan, raw recording, two independent mono inputs, mixed stereo/mono capture, eight-track capture, matching punch boundaries, missing input, source limits, saved per-track routes and older-session defaults. Existing WAV export, session, playback and keyboard checks also pass. Listening and multi-input recording with your interface still need local testing.
