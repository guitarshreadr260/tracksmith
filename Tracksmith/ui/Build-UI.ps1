$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$npmCommand = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $npmCommand) { throw 'Install Node.js LTS (22.12 or newer), reopen PowerShell, and run this script again.' }
if (Test-Path package-lock.json) { & $npmCommand.Source ci } else { & $npmCommand.Source install }
if ($LASTEXITCODE -ne 0) { throw 'Installing frontend dependencies failed.' }
& $npmCommand.Source run build
if ($LASTEXITCODE -ne 0) { throw 'Frontend build failed.' }
Write-Host 'UI built. Reconfigure CMake if needed, then rebuild Tracksmith in Visual Studio.'
