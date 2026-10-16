# Tracksmith accompaniment research and Prototype 14

The governing requirement is user-driven generation: selected audio, a chosen groove library,
and an explicit session tempo. Generate is an explicit action. No background/cloud generation.

## Comparison with the previous engine

| Approach | Conditioning | Output | Difference from Tracksmith 13 |
| --- | --- | --- | --- |
| Tracksmith 13 | RMS rises / spectral flux, autocorrelation tempo, low/high band heuristics | Three drum roles folded into one repeated bar | Never consulted the included groove library; melodic attacks could be assigned drum roles |
| Magenta GrooVAE / Drumify | Rhythmic timing sequences; learned performance representations | Expressive drum MIDI | Learned generation rather than hand-written instrument classification; the Studio Drumify input is MIDI, not a turnkey raw-audio analyzer |
| JukeDrummer | Drumless audio representations plus beat embeddings | Drum audio | Directly addresses audio-conditioned accompaniment; audio output is less convenient than editable MIDI for our kit workflow |
| Coherent Drum Accompaniment with Fills | Melodic accompaniment and predicted improvisation locations | Drum patterns with fills | Supports phrase-aware generation rather than a single repeatedly folded bar |
| Tracksmith 14 local prototype | Selected audio energy/dynamics, low-band energy, phrase-level energy changes, selected MIDI library, user tempo | Editable multibar MIDI for the native kit | Retrieves ranked library bars and varies dynamics/ornaments using empirical hit probabilities; no trained neural model |

## Implemented local generation

- The user must select an audio clip and explicitly press Generate.
- GEN shows the groove library and session tempo; Choose library selects a local MIDI folder.
- Source offset, duration and timeline placement are retained.
- The worker scans up to 2000 MIDI files using their source mapping metadata (GM/GGD), accepts compatible 4/4 material, and ranks library bars against the audio.
- Dynamics and low-band energy condition the matching; phrase energy changes influence fill preference. Onset detection is no longer a validity gate.
- Generation preserves library drum relationships, adapts velocities, and makes limited probability-based variations. Identical inputs reproduce identical output.
- Silent audio or a missing/incompatible library reports failure. The generator does not substitute unrelated built-in patterns.
- First 16 bars maximum, 4/4, up to 2000 generated notes. Dense source detail and sub-sixteenth microtiming are reduced by this first grid-based prototype.
- Original audio is retained. The generated MIDI can be edited and the native Groove kit can be changed.

This is a retrieval/statistical generation prototype, not a newly trained neural model. It does
not understand chord functions, instrument identities or musical intent from raw audio. It
also does not use text prompts. Its bass/energy measurements describe the source; they are
not source separation or drum transcription.

## Neural engine direction

Keep an interchangeable offline inference stage between audio features and kit MIDI. Use
beat-aligned audio embeddings plus user controls as conditions, and normalized drum-role
sequences from the user's chosen library for retrieval/adaptation. Keep the user's library
separate from output kit note numbers. A learned model should preserve timing/velocity,
model several bars, and use section boundaries for fills. Compare it against this local
baseline with held-out user recordings and listening tests before replacing the engine.
Loading a library is not equivalent to training a neural model. Model weights, suitable
paired audio/drum training data, inference runtime, and actual performance evaluation are
still needed for that stage. No model weights were downloaded or trained in this build.

## Other fixes

GM alternate articulations now trigger the nearest supported native pad under the default
GM map. Custom maps remain exact; mapping changes republish to the host UI. GM tambourine
is no longer incorrectly treated as a hi-hat. A nine-pad kit still collapses some GM
articulations, so it cannot preserve every specialized third-party articulation.

Stereo monitor activation enables a missing hardware input when the device provides it,
preserving existing compact input indices. Unavailable pairs produce an explanatory status.
The input selector shows both channel names for stereo. Real interface/driver listening is
still required; automated tests exercise independent left/right buffers and multitrack routes.

Waveforms now use independent signed min/max envelopes per source channel, continuous
filled shapes, subtle center lines, a dedicated name strip and dark clip backgrounds.
Reopen existing sessions to regenerate the waveform cache.

## Primary sources reviewed

- https://magenta.tensorflow.org/groovae
- https://magenta.tensorflow.org/studio/standalone
- https://arxiv.org/abs/2210.06007
- https://legoodmanner.github.io/jukedrummer-demo/
- https://arxiv.org/abs/2209.00291
- https://arxiv.org/abs/2202.04464
- https://arxiv.org/abs/2601.02357
- https://midi.org/general-midi

Reviewed alongside the local C++ source and included groove-library manifests on 2026-10-15.
