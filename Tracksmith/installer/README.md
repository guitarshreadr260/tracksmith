# Installer recipe

Native per-user Windows x64 MSI, preview version 0.1.0. Uses Python, msi.dll and Windows makecab.exe; no downloaded build tools. Run on Windows:

`python Build-Windows-MSI.py --payload "C:\path\to\Tracksmith" --work "C:\path\to\installer-work" --output "C:\path\to\Tracksmith-Setup-0.1.0-x64.msi"`

Payload must contain the current Tracksmith.exe, Stock Plugins and guides from the portable build. Only root files and Stock Plugins files are packaged. The script refuses to overwrite an existing MSI. Keep the product/upgrade and component GUID design stable when adding properly tested upgrades; the current preview requires uninstalling before a different installer version.

Runtime preflight and cabinet hashes passed. Full install/repair/uninstall testing was blocked by Windows Installer service access in the restricted execution environment, so actual lifecycle testing remains pending. See INSTALLER-GUIDE.md.
