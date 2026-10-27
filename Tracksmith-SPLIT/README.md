# Tracksmith SPLIT source checkpoint — October 27, 2026

This checkpoint preserves SPLIT source alongside Tracksmith DAW Build 33. It does not update the DAW or integrate SPLIT into it.

- `standalone/`: playback-position preview. Joe reported that full-song separation and custom mix export worked. Includes local Demucs, GPU selection, YouTube import, live mix, WAV/FLAC/MP3 export and individual WAV stem export.
- `experimental/`: later advanced guitar/drum model and CUDA adapter experiments. Lead/rhythm quality was reported insufficient; GPU model inference has not been verified end to end. Model weights have uncertain redistribution terms and are not included.
- `vst3/`: JUCE VST3 source compiled on Windows. The VST3 launches file-based separation in a background process; it is not verified in a DAW and does not import DAW timeline audio or create tracks. The CMake path points at a local JUCE checkout and needs adjustment on another machine.
- Approved console design: https://www.figma.com/design/mEUVQQ7wL2laMDpMMEm3aA?node-id=4-2

No user audio, downloaded models, Python environments, installed binaries or private recordings are included. Startup scripts assume the Build 1 environment in Downloads; preserve that environment until an independent setup has been verified. The VST3 binary test package remains separate from this source checkpoint.

GEN 1.1 source and the GEN 1.2 visual prototype are in Tracksmith-GEN/checkpoints; demo audio and binaries are not included. This does not back up the Groove guitar preview source or executable, all older builds, local recordings, or other projects. Do not delete local files solely on the basis of this checkpoint.
