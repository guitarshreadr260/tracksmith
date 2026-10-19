# GEN generation-approach trial

Model: acestep-v15-sft; GPU: NVIDIA GeForce RTX 4070 SUPER. Completed 8/8 clips.

Every successful clip is stereo, 48 kHz, 20 seconds, SFT on CUDA, with offload, SDPA, no quantization, and seed 31415. Lead guitar uses 100 BPM; metalcore uses 140 BPM. Both request D minor. Metadata and caption rewriting are disabled.

| Approach | Steps | Guidance | Planner | Mean runtime | Peak CUDA allocation |
|---|---:|---:|---|---:|---:|
| A / Balanced (current) | 50 | 7.0 | acestep-5Hz-lm-0.6B | 14.22 s | 4.99 GB |
| B / Direct, no planner | 50 | 10.0 | None | 6.32 s | 4.70 GB |
| C / Larger musical planner | 50 | 7.0 | acestep-5Hz-lm-1.7B | 14.96 s | 5.00 GB |
| D / Extended render | 100 | 7.0 | acestep-5Hz-lm-1.7B | 17.87 s | 4.99 GB |

These timing measurements exclude model/planner initialization. CUDA allocation is PyTorch allocation, not total system RAM or all GPU processes. Instrument accuracy, isolation, genre adherence and quality are UNRATED. No winner is claimed.

Open LISTEN.html and compare A/B/C/D separately for guitar and the full band. If one approach seems useful, replicate it with more seeds before selecting a permanent default.

GEN 0.6 exposes these approaches and remembers your draft, mode, approach, duration, take count and tempo/key choices. It retains Balanced as the default for new installations. Existing saves/takes and the Multi Platinum button remain.

These are configuration comparisons using the same cached SFT audio checkpoint. The 1.7B option upgrades the musical planning model, not the audio decoder. No new audio checkpoint or new model weights were downloaded. It is not a demonstrated replacement for a higher-quality audio model.