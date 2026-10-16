# Tracksmith NAM

Renamed from AVRX NAM. Stable manufacturer/plugin codes, VST3 component/controller identities, original six parameter IDs/order and AVRX_NAM state tag are retained for compatibility. New EQ parameters are appended. The stock DAW redirects old AVRX NAM descriptions to the bundled Tracksmith NAM path.

Post-model EQ: bass shelf 120 Hz, sweepable mid bell (200–4000 Hz), treble shelf 6 kHz, and optional 12 dB/oct high pass (20 Hz = off). Gain range ±12 dB. Existing sessions restore flat EQ. Model, input/output gain and noise gate behavior remain; EQ operates when a model is ready.

Save preset and Load preset use .tsnampreset files. Presets store all parameters and the .nam model location. They do not embed model contents. Relative model lookup supports moving a preset with its model. Missing models and invalid files/parameter values are rejected before settings change. Saving uses temporary-file replacement.
