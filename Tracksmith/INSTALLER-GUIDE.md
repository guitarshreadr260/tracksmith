# Tracksmith Windows installer preview

Version 0.1.0, Windows x64 — October 14, 2026. This installs the working GGD MIDI Fix application, with the earlier MIDI selection/shortcut and DAW features. The application code has not changed for this installer.

## Try it

1. Save your work and close the older Tracksmith.
2. Download Tracksmith-Setup-0.1.0-x64.msi and double-click it.
3. Choose Install, then Finish.
4. Open Tracksmith from the Start menu.
5. Open a copy of an existing session. Try playback, GGD groove dragging, note selection/deletion and Save/Open.

The installer is for your Windows account. The normal destination is `%LOCALAPPDATA%\Programs\Tracksmith`. It adds a Start menu shortcut and a Windows Installed Apps entry. It does not replace your portable folder or change file associations. The stock NAM VST3 and its notices stay in Stock Plugins beside Tracksmith.exe. Tracksmith Groove and third-party plugins are not installed by this package.

Existing sessions, recordings, recovery files, preferences and third-party plugin folders are outside this package's removal list. Keep the portable build available while checking this first installer. Sessions still use format 9.

## Microsoft prerequisites

Use 64-bit Windows 10 or Windows 11 with Microsoft Edge WebView2 Runtime and the current Microsoft Visual C++ v14 Redistributable (x64). The MSI checks for those runtimes and reports if they are missing; it does not download or install them. The existing development PC passed the prerequisite searches.

- [Microsoft WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)
- [Microsoft Visual C++ Redistributable downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)

## Remove or repair

To remove the installed application, use Windows Settings > Apps > Installed apps > Tracksmith > Uninstall. User-created sessions and media are intended to remain intact. The package only owns the files listed in its manifest and its own installer registry entries/shortcut.

For repair, run `msiexec /fa "Tracksmith-Setup-0.1.0-x64.msi"`. This preview does not provide a full maintenance/custom-feature wizard or an automatic updater. Uninstall it before trying a future installer with a different version until upgrade testing is completed.

## Verification and limits

Passed: Windows Installer package verification, opening the database/package, AppSearch runtime detection, launch conditions, CostInitialize/FileCost/CostFinalize, missing-prerequisite condition evaluation, extraction of the embedded cabinet and SHA-256 matches for all 26 payload files. This includes the exact working Tracksmith and NAM binaries, stock notices and guides. No AVRX Drums payload is included.

The automated full-install attempt could not connect to the Windows Installer service in the restricted test environment (1601, access denied 0x80070005). It stopped before installing the product. Actual installation, repair, shortcut behavior and removal are therefore **pending manual review**. That result does not establish a fault in the user's Windows Installer service. The package's read-only preflight checks pass.

This is an unsigned installer preview; a signed release and clean-machine/upgrade testing remain future work. The prior synthetic engine reliability results still apply to the unchanged application, but do not replace this installer trial or hardware listening checks.
