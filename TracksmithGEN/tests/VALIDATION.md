# GEN 0.6 validation - October 18, 2026

Completed before the source checkpoint:

- Native Windows executable compiled with the .NET Framework C# compiler.
- Sketch instruments, WAV read/write, MIDI exports and invalid-chord handling.
- Windows subprocess argument escaping, including quotes and trailing slashes.
- Word/sentence expansion and preservation of detailed requests.
- Prompt composition without default instrument injection.
- Saved library rename/favorite roundtrip and original WAV byte preservation.
- Preferences save/load: draft, scope, approach, duration and take count.
- Playback timing/seek/volume API using the fallback available in the test
  environment. This is not an audible-playback assessment.
- Real Direct-profile integration: native C# request -> installed Python
  CUDA worker -> 10-second WAV -> saved take -> library reopen.
- Eight actual 20-second GPU generations: lead guitar/full-band metalcore
  across four configurations. All produced finite stereo 48 kHz output.

Measured generation times exclude model loading. Direct averaged 6.32 seconds
per 20-second clip; Balanced 14.22, larger planner 14.96, extended 17.87.
Peak per-case PyTorch CUDA allocation was 4.70-5.00 GB. These are short tests,
not universal performance guarantees.

Instrument accuracy, isolation and musical usefulness remain unrated in the
automated results. No subjective winner is declared. Caption rewriting tests,
earlier Small-model trials and the current comparison are distinct experiments.
The standalone app is not the repository's existing DAW generation pipeline.

See QUALITY-FINDINGS.md and quality_trial.py for reproducible settings. Model
weights, audio samples, runtime environments, tokens, user preferences and
absolute local diagnostic paths are not part of this source checkpoint.
