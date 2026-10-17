# Build 30 - native TONE3000 preview

Based on Build 29 (0981976). Earlier builds are unchanged. Development preview. Source and validation notes are uploaded to GitHub main and build-30-tone3000; no executable release is published. The publishable integration key remains in the local ignored configuration.

## What changed

TONE3000 replaces Tracksmith NAM in the stock insert menu and new EZ Guitar tracks. It is a compiled native AudioProcessor and native JUCE editor inside Tracksmith, with no separate TONE3000 VST installation required. Add it from any track, aux or master insert menu: TONE3000 - Built in.

The integrated upstream processor includes A2 NAM models, WAV cabinet IRs, mono/stereo chains, block EQ/gain/mix, gate, global tone controls, pitch shift, tuner, presets and Tone3000 account/catalog UI. Use + to choose a tone online, or drop/load local A2 .nam and IR .wav files. A new instance starts with an empty chain; it does not preload catalog captures. Upstream currently restricts local NAM loading to A2 models.

Legacy Tracksmith/AVRX NAM identities and their original engines/state restore paths remain available for opening old sessions. They are hidden from the new-stock-insert choices. Existing inserts are not automatically converted, so their sound and automation retain their original meaning.

## Session and host integration

Tracksmith serializes the full processor state through its existing insert system; model/IR chain changes also notify session dirty tracking. Native identity: tracksmith-native-tone3000. Session format remains 13. The new engine's NAM namespace is isolated from the legacy NAM core.

Audio and MIDI device management stays in Tracksmith. Embedded effect settings cannot reconfigure the interface. Tone settings, presets, local model cache and logs use Tracksmith/TONE3000 in the per-user data/log locations, separated from the official standalone plugin. Keep downloaded/cache files when moving sessions; no portable catalog redistribution is implemented.

The publishable integration key is configured locally in an ignored .env.local file. No secret key is used. User sign-in uses upstream OAuth/PKCE and the system browser. Account sign-in and a real authenticated online download still need user confirmation.

## Source and licensing

Powered by [TONE3000](https://www.tone3000.com). The pinned upstream commit and dependency references are in Tracksmith/t3k/TRACKSMITH-UPSTREAM.md. Original licenses are retained. Local changes adapt the editor to JUCE 8, scope its theme to its own window, isolate settings/cache and NAM symbols, and add host dirty-state notifications and a hosted-device shim.

API development/evaluation is permitted, but this preview is not a commercial-release approval. Tone3000's API terms require a commercial agreement and review before a paid product is published. Their free publishing tier restricts catalog access to select/load flows and bounded lists; this upstream full-browser implementation requires the appropriate access arrangement before distribution. [API terms](https://www.tone3000.com/api/terms).

No catalog models or upstream factory capture presets are bundled with this preview. The legacy guitar assets remain solely for old-session compatibility.

## Validation

See BUILD30-VALIDATION.txt for actual completed checks and remaining user checks.

Run portable/Tracksmith.exe with the accompanying folders intact.
