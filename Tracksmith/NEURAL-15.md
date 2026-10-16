# Tracksmith Neural Trial — Prototype 15

Select an audio clip in Edit/EZ, choose your MIDI groove library and set the session tempo.
Leave **Neural / GrooVAE (experimental)** checked, start at 30% reference influence,
and press **Generate from my audio**. Editable MIDI appears on a new native drum track.
Open Groove to change sounds. Uncheck Neural to compare with the library baseline.

This build runs Google's published GrooVAE tap2drum 2-bar checkpoint using
Magenta.js 1.23.1 and TensorFlow.js CPU inference in a separate local Node process.
The runtime and model weights are included. No account, subscription, audio upload,
or runtime download is required. A loopback-only server briefly serves the bundled
weights to TensorFlow and then closes.

The selected audio supplies a beat-grid energy/accent rhythm. Library matching
selects reference bars from the chosen folder, whose kick/snare rhythms supply
a second encoded input. The influence control blends the two encoded representations
before neural decoding. No unconditional model sampling is called. The library guides
inference; this version does not retrain or fine-tune the model on your library.

Generated velocities and timing offsets are retained in song MIDI. The native grid
shows quantized steps for preview/editing. Editing grid steps and sending them to the
song again replaces the performance with the edited grid. Model-specific tom classes
are explicitly mapped to high/mid/floor pads rather than assuming GM output mapping.

Scope: two-bar windows, up to 16 bars, current 4/4 session support. This first model
uses a rhythm representation, not end-to-end raw audio understanding. It does not
interpret chord functions or full song structure. Two-bar CPU inference was measured
around 0.3–0.5 seconds here; complete jobs also include analysis, library scanning and
process startup. The RTX 4070 Super is not used by this initial backend.

Model failures are reported explicitly. Baseline generation only runs when Neural is
unchecked. Automated synthetic/reference tests pass; musical quality still needs
listening tests with your own performances. Prototype 14 fixes are retained.

Inference source and pinned dependencies are in tools/neural. Use npm ci --ignore-scripts,
then bundle runner.cjs with esbuild (platform=node, format=cjs, target=node20) to
Native Instruments/Neural/neural-runner.cjs. Runtime licenses, third-party notices and
checkpoint checksums are alongside it.

Sources:

- https://magenta.tensorflow.org/groovae
- https://github.com/magenta/magenta-js/tree/master/music
- https://github.com/magenta/magenta-js/blob/master/music/demos/groovae.ts
- https://github.com/magenta/magenta-js/blob/master/music/checkpoints/README.md
- https://magenta.tensorflow.org/datasets/groove

The preceding research comparison is retained in RESEARCH-14.md.
