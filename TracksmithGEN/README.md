# Tracksmith GEN standalone 0.7.2

Native Windows music-generation prototype. Multi Platinum now selects between Stable Audio 3 Small Music and the original
ACE-Step worker. Stable Audio is the default for older preferences without an engine choice. The new audio-guided workflow uses the separate
Stable Audio 3 Small Music environment installed during the guitar-model trial.

## Multi Platinum engines

Choose Stable Audio 3 / new engine or ACE-Step / original engine. Stable uses
its own cached Python environment and text-only worker, with no reference WAV
and no ACE dependency. ACE approach settings are disabled for Stable. The engine
choice persists; saved takes show the engine used. See START-HERE-0.7.2.txt.

## Prompt-first controls

Key and tempo are specified only in the main generation prompt. The selectors
and their hidden preference defaults were removed. GEN has no 512-character
app limit in either generation workflow; full prompts are passed and stored.
Model token contexts remain finite, so very long prompts can still be truncated
internally by a model. See START-HERE-0.7.1.txt for update details.

## Audio-guided workflow

- Import mono/stereo WAV or MP3, select a 5-30 second section and preview it.
- Describe a variation and adjust the input noise amount from 5-95%.
- Generate 1-3 sequential CUDA variations using the actual selected audio.
- Switch original/generated playback at the current position, save takes with
  source/seed/amount/reference linkage, and export the generated stereo WAV.
- Original recordings stay intact; the prepared reference section is a copy.

This is variation, not stem isolation or accompaniment generation. It does not
guarantee note, key or tempo preservation. A/B is not loudness-matched. B plays
the last variation; the library contains every completed take.

See [START-HERE-0.7.txt](START-HERE-0.7.txt) for usage, environment selection,
model limitations and validation. The original 0.6 guide and trial files are
retained for history.

## Build

Run `build.ps1` with the Windows .NET Framework C# compiler installed. Keep both
Python workers beside the executable. The build does not install dependencies
or download models. Use a writable program folder for preferences and library.

The reference worker reuses the trial's `.venv/Scripts/python.exe` and cached
Small Music weights. It works offline and requires CUDA. Model access/terms
are handled by the existing trial setup; credentials are not part of this repo.

## Validation

The native build and legacy regression suite passed. Seven worker tests cover
MP3 decoding, section boundaries, source preservation, and audio-conditioning
parameter/seed propagation. A real 5-second CUDA WAV-guided bridge test passed,
including saved-reference linkage and byte-preserving generated-WAV storage.
A separate real MP3 dialog test passed generation, save, playback-length and
seek checks. These use sample audio; audible quality remains unrated.

Run `python -m unittest -v test_reference_worker.py` using the Stable Audio
environment. Native developer checks: `--self-test`, `--reference-smoke-test
SOURCE PYTHON`, and `--reference-ui-test SOURCE PYTHON`. Quote paths with spaces.
The latter two require cached model assets and the reference environment.

GEN is independent of the JUCE/React Tracksmith DAW. This source branch does not
modify the DAW or redistribute model weights, credentials or recordings.

A real Multi Platinum-handler CUDA test also passed for Stable Audio text-only
generation with ACE unavailable, including saved/reopened output and engine
identity. This does not assess subjective music quality.
