$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Windows.Forms
    $destinationFolder = Join-Path $env:USERPROFILE 'Downloads\TracksmithGEN-Standalone-0.2-AI-Lab-Windows'
    if (-not (Test-Path -LiteralPath (Join-Path $destinationFolder 'TracksmithGEN.exe'))) {
        $picker = New-Object System.Windows.Forms.FolderBrowserDialog
        $picker.Description = 'Choose your existing GEN folder containing TracksmithGEN.exe'
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { exit }
        $destinationFolder = $picker.SelectedPath
        $picker.Dispose()
    }
    $targetExe = Join-Path $destinationFolder 'TracksmithGEN.exe'
    if (-not (Test-Path -LiteralPath $targetExe)) { throw 'That folder does not contain TracksmithGEN.exe.' }
    if ([IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\') -eq [IO.Path]::GetFullPath($destinationFolder).TrimEnd('\')) {
        Write-Host 'The update is already in your GEN folder. Open TracksmithGEN.exe.'
        exit
    }
    $backupExe = Join-Path $destinationFolder 'TracksmithGEN-before-ACE-GPU.exe'
    if (-not (Test-Path -LiteralPath $backupExe)) { Copy-Item -LiteralPath $targetExe -Destination $backupExe }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'TracksmithGEN.exe') -Destination $targetExe -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ace_worker.py') -Destination (Join-Path $destinationFolder 'ace_worker.py') -Force
    Write-Host ('Updated GEN: ' + $destinationFolder)
    Write-Host 'Your saved takes and model installations are preserved.'
    Write-Host 'Open TracksmithGEN.exe in that folder and use Multi Platinum.'
} catch {
    Write-Host ('Update stopped: ' + $_.Exception.Message) -ForegroundColor Red
    Write-Host 'Close GEN before updating, then run UPDATE-GEN.cmd again.'
    exit 1
}
