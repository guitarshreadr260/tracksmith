# GEN 0.7 candidate: Stable Audio 3 guitar trial

Experimental test kit, not a released GEN 0.7 build. Existing GEN 0.6 is unchanged.

## Windows steps

1. Download this branch as a ZIP and extract it. Open TracksmithGEN/experiments/sa3-guitar.
2. Install Python 3.11 if the Python launcher does not already have it.
3. Review and accept the model's access conditions at https://huggingface.co/stabilityai/stable-audio-3-small-music using your own account.
4. Double-click SETUP.cmd. It creates its own .venv and installs CUDA 12.8 PyTorch and a pinned official Stable Audio 3 source snapshot. First install/download is several GB.
5. Double-click LOGIN.cmd and use a read-access Hugging Face token for that account. Do not send the token to anyone. Login uses the standard local Hugging Face credential cache.
6. Close other model processes to free VRAM. Double-click RUN-GUITAR-TEST.cmd.
7. Open the listening page that appears. Results live under runs/<timestamp>/LISTEN.html. Failed cases are displayed with their errors.

The trial requires working NVIDIA CUDA; it stops rather than falling back to CPU.
All prompts run sequentially on the small-music checkpoint, 8 steps, guidance 1,
20 seconds, seeds 31415 / 27182 / 16180. Three prompts produce nine WAVs.
The lead prompt uses GEN 0.6's original lead request with an explicit 100 BPM suffix. The other two add clean
and palm-muted guitar checks. All request D minor, 100 BPM through text only:
key, tempo and isolation are listening checks, not guarantees.

Listen for guitar identity, clear notes/attacks, unwanted backing instruments,
timing, and whether you'd use the riff. The listening page offers local scores
and an Export scores button. No scores or quality winner are preassigned.
Stereo PCM24 originals and a separate peak-scaled audition copy are saved.
Audition scaling is not loudness matching; level-match manually when comparing
the old ACE clips. Small alone is a screening test, not a full model ranking.

## What is verified

The inference call was checked against upstream model.py and cli.py at commit
77e0398c26f7d22d801ec1bb036307e8b231f741.
No execution environment was available when this kit was prepared.
Setup, syntax execution, CUDA compatibility, generation, playback and sound
quality remain UNTESTED. A successful WAV is not proof of musical quality.
Model assets are downloaded using upstream defaults, which may change; results
record the inference code revision, package versions and generation settings,
but do not lock remote weight revisions. Keep downloaded cache for comparisons.

If the new clips still sound artificial, don't integrate it into GEN.
Foundation-1 is another candidate for isolated loops, but remains untested.
Medium is a separate possible follow-up; its official path requires Flash
Attention 2, so this kit does not install or claim Windows support for it.

Sources:
- https://github.com/Stability-AI/stable-audio-3
- https://huggingface.co/stabilityai/stable-audio-3-small-music
- https://huggingface.co/RoyalCities/Foundation-1

Model access is gated by Stability AI's model license and Gemma terms.
Accept these yourself before downloading. This kit does not redistribute weights.

A branch CI check parses the Python and PowerShell source. Passing it verifies syntax only, not inference or installation.
