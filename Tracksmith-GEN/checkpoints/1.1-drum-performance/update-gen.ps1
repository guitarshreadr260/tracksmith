$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Windows.Forms
    $names = @('ANALYZE-RIFF.cmd', 'ANALYZE-RIFF.ps1', 'DrumModel\riff_analysis.py', 'DrumModel\riff_core.js', 'DrumModel\riff_template.html', 'RIFF-LAB-START-HERE.md', 'TracksmithGEN.exe', 'ace_worker.py', 'reference_worker.py', 'stable_worker.py', 'ALIGN-DRUMS.cmd', 'ALIGN-DRUMS.ps1', 'DrumModel\align_drums.py', 'DrumModel\drum_baseline.py', 'DrumModel\drum_maps.py', 'DrumModel\trained\drum_model.json', 'DrumModel\DATASET-LICENSE.txt', 'GGD-MIDI-Mapper.exe', 'REMAP-GGD.cmd', 'GGD-Factory-Map-Test.mid', 'SIMPLE-GRID-START-HERE.md', 'GGD-Simple-140-Bar2.mid', 'DrumModel\guitar_accents.py', 'EDITABLE-BEAT-START-HERE.md', 'GGD-Swing-58-Full-Kit-140-Bar2.mid')
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
        if (Test-Path -LiteralPath $oldFile) { New-Item -ItemType Directory -Path (Split-Path -Parent (Join-Path $backup $name)) -Force | Out-Null; Copy-Item -LiteralPath $oldFile -Destination (Join-Path $backup $name) }
    }
    try {
        foreach ($name in $names) {
            New-Item -ItemType Directory -Path (Split-Path -Parent (Join-Path $destinationFolder $name)) -Force | Out-Null
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
