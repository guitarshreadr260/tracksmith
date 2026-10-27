# GEN drum baseline - first experiment

This is a trained statistical baseline, not a neural audio model or a completed guitar-follow feature. GEN's recorder/NAM entry point was removed in the accompanying 0.8.1 executable. Existing audio-guided variations are unchanged. The baseline runs separately from the app for now.

Training: Google LLC Groove MIDI Dataset v1.0.0, 897 train files listed; 10 non-4/4 training files excluded. 17,841 full 4/4 bars used. Test and validation files never contribute to fitted parameters. Nine drum voices, sixteen steps per bar. Learn per-step hit frequency, mean velocity and timing offset, globally and by style. Generation samples independent events, so instrument coordination, fills, triplets, song structure and guitar conditioning are not learned. No claim of audible improvement yet.

Validation binary cross entropy: global 0.27155, style 0.27582. Test: global 0.25496, style 0.24647. Style conditioning does not consistently improve prediction, so global is the default. Lower scores are better, but do not establish musical quality. Held-out sets retain the dataset's original splits; no cherry-picking. See trained/metrics.json.

Requires Python 3.11 standard library only.

Train:
python drum_baseline.py train PATH_TO_GROOVE_FOLDER trained

Generate:
python drum_baseline.py generate trained/drum_model.json demo.mid --bpm 120 --bars 8 --seed 42

Optional --style rock (or another trained style) is experimental. rock-demo.mid is a generated 120 BPM eight-bar example, not matched to any guitar. Audition it using a drum instrument in Groove/your DAW.

Next: use Joe's original guitar WAV for beat/phase/tempo analysis, retain original audio, align a separate MIDI drum part, then compare to simpler groove retrieval. This data contains drums alone, not paired guitar/drums; it cannot teach guitar-to-drum relationships by itself.

Dataset attribution: Groove MIDI Dataset, Google LLC, https://magenta.tensorflow.org/datasets/groove, CC BY 4.0 https://creativecommons.org/licenses/by/4.0/. Dataset MIDI was mapped to nine drum categories and quantized to sixteenth notes to produce fitted statistics. Dataset license included in DATASET-LICENSE.txt.

Citation: Jon Gillick, Adam Roberts, Jesse Engel, Douglas Eck, and David Bamman. Learning to Groove with Inverse Sequence Transformations. ICML, 2019.
