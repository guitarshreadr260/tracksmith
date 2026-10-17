# Build 32 - local instrumental generation prototype

Based on local Build 31 (5d87afc), retaining Build 30's Windows functionality and the QR-source dependency correction. Earlier builds are unchanged. No GitHub push.

## Try it on this PC

Close the previous Tracksmith window, then run portable/Start-Tracksmith-Generation.cmd. This starts or reuses the local generation engine and opens Build 32. Select Generate beside EZ Mode (or View > Generate instrumental).

Enter a music description, optional key/scale and 10-60 second duration. The request uses the session BPM (30-300) and 4/4 meter. Stop the transport before generating or inserting. Generate another variation to compare up to four results. Audition with the audio player, then insert the selected result at the current selection start or playhead on a new audio track. Undo removes the inserted clip; redo restores it. The new empty track remains after undo.

The preview player uses the computer's default audio output. Timeline playback uses Tracksmith's selected interface. Stop waiting cancels this client's wait; it does not kill the engine or its queued GPU job. Closing the dialog does not cancel generation.

Open generated files reveals persistent WAVs and JSON manifests (prompt, requested BPM/key/duration, actual duration and returned seed) under the per-user Tracksmith/Generated Audio folder. Session saving references the generated WAV through the existing media mechanism. The settings manifest remains beside the cache WAV; it is not embedded in or copied into a portable session. Keep these files when moving sessions.

## Actual engine

ACE-Step 1.5 is running locally on this PC's NVIDIA RTX 4070 SUPER (12 GB VRAM), with about 32 GB system RAM. The first prototype uses the 2B turbo model, 8 inference steps, batch size 1, no language-model planning and instrumental prompts. Model timing, key, instrument isolation and seamless loops are not guaranteed. This is generated stereo audio, not MIDI or independently generated stems.

The engine lives outside the DAW checkout at the workspace's work/acestep-runtime, with an isolated Python environment and separate downloaded model/cache files. It binds only to 127.0.0.1:8001. The native worker uses that fixed local endpoint; prompts/audio are not sent to a hosted generation API. The initial dependency/model downloads used PyPI, PyTorch and Hugging Face. There is no generation API key or per-request service charge in this local setup.

Official source: https://github.com/ace-step/ACE-Step-1.5
Pinned checkout: ca1e85fe9430179831e6bc6be790c332190a3866
MIT license retained at portable/Generation/ACE-Step-LICENSE.txt. Model provenance/license materials remain with the downloaded checkpoint folders. No claim of exclusive output ownership or Suno-equivalent quality is made.

## Implementation

A Generate dialog sends validated requests through the existing native command bridge. GenerationWorker submits an asynchronous ACE-Step task, polls status, validates the local result URL, downloads a bounded WAV, decodes it off the audio thread and rejects non-finite samples. Generation never calls the model on Tracksmith's audio callback. Existing Stop/playback commands remain independent. Insertion is explicit and requires a stopped transport, uses an existing audio-media install path and records normal clip undo history.

Previously generated results remain available on failure/cancel. Up to four decoded results are retained for this app run; generated files remain on disk. A 20-minute client timeout accommodates initial model loading. Missing engine, invalid requests and failed tasks show errors rather than returning mock audio. The local engine uses GPU/CPU/RAM, so real recording performance while it runs still needs user evaluation.

## Recreating the engine elsewhere

This is a local development package, not a redistributable AI installer. portable/Generation/engine-path.txt and cache-path.txt are local machine paths and ignored by Git. They point at the engine already installed and tested on this machine.

For another Windows CUDA machine, clone the pinned official ACE-Step checkout, use Python 3.11-3.12 and uv sync per its installation guide, and run python -m acestep.api_server --host 127.0.0.1 --port 8001 with ACESTEP_INIT_LLM=false, ACESTEP_DEVICE=cuda and ACESTEP_CONFIG_PATH=acestep-v15-turbo. Set the local launcher engine-path.txt to that checkout. Models auto-download on the first request. The environment and model files are several GB and are not committed or included in the portable app folder.

This engine environment currently depends on the existing local Python runtime. Do not move/delete the workspace runtime folders without recreating it. The current native TONE3000 publishable configuration is preserved locally in Build 32; no secret key is added.

## Validation and limits

See BUILD32-VALIDATION.txt and regression reports. A real 16-second generated demo WAV is available locally as BUILD32-generated-demo.wav. Its measured format is stereo, 48 kHz, 768000 frames, with finite nonzero samples.

Not implemented: vocals UI, reference-audio conditioning, automatic accompaniment, MIDI generation/transcription, multi-stem generation, automatic BPM/key verification, seamless loop repair, model management UI or public installer. Mac generation remains untested/deferred. Tone3000 online login and live-interface listening still need user confirmation as before.
