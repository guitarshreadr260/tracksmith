# Installer validation

Package: Tracksmith-Setup-0.1.0-x64.msi
SHA-256: f2744abc7c41a7bbef3998379552ed5b9d70bf005de27730c9bc1c51ede2b959

- PASS: MSI package verification, database open and cabinet extraction.
- PASS: all 26 extracted files match the working payload hashes.
- PASS: package preflight AppSearch, launch conditions and directory/file costing.
- PASS: WebView2 detected; x64 Visual C++ runtime detected.
- PASS: missing-prerequisite conditions evaluate as rejected.
- BLOCKED: full install in restricted test environment, Windows Installer server connection 1601 / 0x80070005. No product registered or payload installed by that attempt.
- PENDING: actual install, repair, uninstall, Start menu launch, upgrade and clean-machine checks.

Per-user package. Registry key paths use HKCU. No elevated/custom actions or wildcard user-data removal rules are authored. Uninstall owns only the package's manifest files/installer registry values and Start menu shortcut. User-data preservation must still be verified in the actual install/uninstall trial.
