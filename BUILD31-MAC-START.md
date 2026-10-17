# Build 31 - macOS preparation for Apple Silicon

Development branch: build-31-macos, based on Build 30 source checkpoint 28efa1d. Prepared on Windows for the user's M2 Mac. This is source preparation, not a verified working Mac binary. Build 30 remains unchanged.

## Inspection and changes

The audio, recording, edit/MIDI models and native TONE3000 processor use JUCE. A Mac port is feasible. These concrete build blockers were found and addressed:

- Windows crypto libraries and WebView2 settings were unconditional. They are now Windows-only; macOS uses JUCE's default WKWebView backend with the same embedded UI/resource bridge.
- The native TONE3000 target omitted macOS Objective-C++ implementations. It now uses its AudioPermissions, WindowKeyEvents and StandaloneStateAutosave .mm files, with Objective-C++20 and AVFoundation/AppKit links.
- NAM's architecture registrations could be stripped on Mac. Both native cores and the legacy stock NAM core now use Apple's force_load linker support.
- The app now has a stable bundle identifier and microphone permission description. Physical-input monitoring requests macOS microphone permission when needed.
- The VST3 picker uses the platform's plugin location and a platform-neutral title.
- Build-Mac.sh builds the UI, native DAW and stock VST3 plugins for the local Mac architecture, runs session/clip/mixer tests, copies license notices and makes a development ZIP.

Runtime assets are copied beside the executable inside Contents/MacOS so the existing relative asset lookup remains valid. Stock plugins are rebuilt as native Mac VST3 bundles; Windows VST3 binaries cannot be reused.

## Run on your M2 Mac

Requirements: macOS 12 or newer, Xcode Command Line Tools, CMake 3.22 or newer, Node.js 22.12 or newer, Git and an internet connection for frontend/JUCE dependencies. Install tools from Apple, cmake.org and nodejs.org, or your existing package manager.

1. Download and extract Tracksmith-Build-31-Mac-Source.zip on the Mac.
2. In Terminal, enter `cd `, drag the extracted Tracksmith-Build-31-Mac-Source folder into the Terminal window, and press Return.
3. Run `bash Tracksmith/Build-Mac.sh`.
4. The finished app is Tracksmith/build-mac-arm64/Tracksmith_artefacts/Release/Tracksmith.app. The development ZIP is Tracksmith/build-mac-arm64/Tracksmith-Mac-arm64.zip.

If Xcode tools are missing, run `xcode-select --install` and let installation finish before building. The script stops with a clear message when a required tool is missing. It does not install packages or change system settings for you.

For online TONE3000 access, configure the same publishable key locally in Tracksmith/t3k/.env.local as T3K_PUBLISHABLE_KEY=... before running the script. The source ZIP excludes that local configuration. Local A2 model/IR loading works without it. API distribution conditions from Build 30 still apply.

## First Mac checks

Launch the app locally. Allow microphone access when prompted and select the audio interface in Audio settings. Confirm playback/Stop, a recorded audio take, GGD MIDI playback and printing, MIDI drag/drop in WKWebView, native TONE3000 processing/sign-in, saving/reopening a session and stock/external VST3 editors.

Only load Mac arm64 or universal VST3 plugins. Install the Mac editions of GGD/Kontakt and other third-party plugins separately. Existing Windows session plugin paths may require manual reassignment; automatic cross-platform path remapping is not implemented.

## Known limits

No Mac is attached to this Windows session: Apple compilation, CoreAudio, WKWebView behavior, microphone permissions, actual M2 performance and third-party plugin compatibility remain unverified until the script runs on the Mac. Compilation may reveal further Apple-specific changes.

The first script builds arm64 for M-series Macs. Intel and universal binaries are not validated. The minimum deployment target is 12.0, not a tested compatibility claim. This is not a signed/notarized public release or an installer.

The repository contains groove MIDI but not the large locally bundled drum sample packs, neural runtime or default guitar model from the Windows portable folder. Those assets need a separate licensed transfer or Mac-native installation. The source build still has the built-in procedural Groove kit and can load user models/IRs. The older Groove guitar browser's secure Windows account-store path remains Windows-only; native TONE3000 is the intended new effect.

Keyboard shortcut labels and platform conventions retain their current Windows-oriented mappings for this first port; Command-key polish and WKWebView shortcut testing remain follow-up work.

See BUILD31-VALIDATION.txt for checks actually performed here. No GitHub update is part of this local preparation.
