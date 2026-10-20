$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Windows.Forms
    $names = @('TracksmithGEN.exe', 'ace_worker.py', 'reference_worker.py', 'stable_worker.py')
    foreach ($name in $names) {
        if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $name))) {
            throw "Package file missing: $name"
        }
    }
    $picker = New-Object System.Windows.Forms.FolderBrowserDialog
    $picker.Description = 'Choose your existing GEN folder containing TracksmithGEN.exe. Saved takes will be preserved.'
    $picker.SelectedPath = Join-Path $env:USERPROFILE 'Downloads\TracksmithGEN-Standalone-0.2-AI-Lab-Windows'
    try {
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { exit }
        $destinationFolder = $picker.SelectedPath
    } finally { $picker.Dispose() }
    $targetExe = Join-Path $destinationFolder 'TracksmithGEN.exe'
    if (-not (Test-Path -LiteralPath $targetExe)) { throw 'That folder does not contain TracksmithGEN.exe.' }
    if ([IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\') -eq [IO.Path]::GetFullPath($destinationFolder).TrimEnd('\')) {
        Write-Host 'Already in this program folder. Open TracksmithGEN.exe.'
        exit
    }
    $backup = Join-Path $destinationFolder ('GEN-program-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    foreach ($name in $names) {
        $oldFile = Join-Path $destinationFolder $name
        if (Test-Path -LiteralPath $oldFile) { Copy-Item -LiteralPath $oldFile -Destination (Join-Path $backup $name) }
    }
    try {
        foreach ($name in $names) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $destinationFolder $name) -Force
        }
    } catch {
        foreach ($name in $names) {
            $prior = Join-Path $backup $name
            if (Test-Path -LiteralPath $prior) {
                Copy-Item -LiteralPath $prior -Destination (Join-Path $destinationFolder $name) -Force
            }
        }
        throw
    }
    Write-Host ('Updated GEN: ' + $destinationFolder)
    Write-Host ('Previous program files: ' + $backup)
    Write-Host 'Saved takes, settings and models were kept. Open TracksmithGEN.exe.'
} catch {
    Write-Host ('Update stopped: ' + $_.Exception.Message) -ForegroundColor Red
    Write-Host 'Close GEN before updating, then run UPDATE-GEN.cmd again.'
    exit 1
}
