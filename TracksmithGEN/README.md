# Tracksmith GEN standalone 0.6

Windows music-generation prototype with a native C# / WinForms interface and a
separate Python CUDA worker using an existing ACE-Step 1.5 installation.
This standalone project has its own version numbering, independent of the
JUCE/React DAW in `../Tracksmith/`.

## Features

- Prompt-first Part / instrumental Song workflow and **Multi Platinum** button.
- Four generation approaches: Balanced, Direct, Larger planner, Extended.
- Local rule-based idea expansion, editable prompts and exact-prompt preview.
- GPU stages, elapsed-time feedback and cancellation.
- Persistent takes, search, favorites, rename and original stereo WAV export.
- Remembered draft and generation settings; Ctrl+Enter and Space shortcuts.
- Earlier chord-based MIDI sketch tools remain available.

## Build and run

On Windows with .NET Framework 4.x and its C# compiler:

```powershell
.\build.ps1
.\TracksmithGEN.exe
```

Keep `ace_worker.py` beside the executable. Use a writable app folder so GEN
can save its library and preferences. The GUI build does not install models.

## GPU environment

Reuse an existing ACE-Step 1.5 installation with its `.venv`, `acestep` source
and cached `checkpoints`. Select that repository folder with Engine settings.
The default lookup is
`~/Downloads/TracksmithGEN-ACE-Step-GPU-Test-Kit/ACE-Step-1.5`.

For a fresh test environment, `setup/SETUP-GPU.cmd` wraps the upstream source
installation in a separate folder and checks actual CUDA computation. It
downloads substantial dependencies/model assets on first use. After setup,
select `setup/ACE-Step-1.5` from GEN's Engine settings. The browser launcher
is optional; standalone GEN launches its own worker and writes WAV directly.

Tested Windows stack: RTX 4070 Super 12GB, PyTorch `2.7.1+cu128`, ACE SFT
audio checkpoint, 0.6B/1.7B planners. The worker uses cached assets with offline
loading; it does not silently fall back to CPU-only generation.

| Approach | SFT steps | Guidance | Musical planner |
|---|---:|---:|---|
| Balanced | 50 | 7 | 0.6B |
| Direct | 50 | 10 | None |
| Larger planner | 50 | 7 | 1.7B |
| Extended | 100 | 7 | 1.7B |

All approaches use SDPA, CPU offload, no quantization, no caption/metadata
rewriting and WAV output. Planner-based modes keep semantic-code planning.
The larger planner is not a different audio checkpoint or a proven quality
upgrade. Missing cached planners produce an error rather than a download.

## Validation

```powershell
Start-Process .\TracksmithGEN.exe -ArgumentList '--self-test' -Wait
Get-Content .\test-results.txt

# Requires cached models and the default installed ACE path.
Start-Process .\TracksmithGEN.exe -ArgumentList '--gpu-smoke-test','direct' -Wait
Get-Content .\gpu-smoke-result.txt
```

See [validation](tests/VALIDATION.md) and
[measured configuration comparisons](tests/QUALITY-FINDINGS.md). The quality
trial script accepts `ACE_REPO OUTPUT_DIRECTORY` arguments and runs with the
ACE environment's Python. Generated audio and local settings are excluded
from source control.

## Current limits

Instrumental text-to-music only, 10-120 seconds, 1-3 sequential takes. Part mode
does not guarantee isolation. Tempo/key guidance needs musical verification.
Imported WAVs are for audition/organization, not accompaniment conditioning.
Fallback preview playback is mono; original stereo export is preserved.
The idea expander is rule-based, not another language model. Interactive
listening quality is not established by successful automated generation.

## Updating an existing installation

After building, `UPDATE-GEN.cmd` copies the executable and worker to the
existing GEN folder and keeps a backup executable. It preserves saved takes
and the separate ACE installation. See [usage](START-HERE-0.6.txt).
