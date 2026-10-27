$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName Microsoft.VisualBasic
$python = $null
$config = Join-Path $PSScriptRoot 'gen-reference-python.txt'
if (Test-Path -LiteralPath $config) {
    $candidate = (Get-Content -LiteralPath $config -Raw).Trim()
    if (Test-Path -LiteralPath $candidate) { $python = $candidate }
}
if (-not $python) {
    $downloads = Join-Path $env:USERPROFILE 'Downloads'
    foreach ($outer in Get-ChildItem -LiteralPath $downloads -Directory -Filter 'tracksmith-gen-0.7-guitar-model-trial*') {
        $roots = @($outer.FullName) + @(Get-ChildItem -LiteralPath $outer.FullName -Directory -Filter 'tracksmith-gen-0.7-guitar-model-trial*' | ForEach-Object FullName)
        foreach ($root in $roots) {
            $candidate = Join-Path $root 'TracksmithGEN\experiments\sa3-guitar\.venv\Scripts\python.exe'
            if (Test-Path -LiteralPath $candidate) { $python = $candidate; break }
        }
        if ($python) { break }
    }
}
if (-not $python) {
    $pick = New-Object System.Windows.Forms.OpenFileDialog
    $pick.Title = 'Choose your working Python 3.11 executable'
    $pick.Filter = 'Python|python.exe'
    if ($pick.ShowDialog() -ne 'OK') { exit }
    $python = $pick.FileName
}
$pick = New-Object System.Windows.Forms.OpenFileDialog
$pick.Title = 'Choose the original guitar PCM WAV (leading silence is preserved)'
$pick.Filter = 'PCM WAV|*.wav'
if ($pick.ShowDialog() -ne 'OK') { exit }
$source = $pick.FileName
$bpm=[Microsoft.VisualBasic.Interaction]::InputBox('Recording tempo in BPM','Riff analysis','140')
if(-not $bpm){exit}
$startBar=[Microsoft.VisualBasic.Interaction]::InputBox('First drum bar. Import exported MIDI at bar 1.','Riff analysis','2')
if(-not $startBar){exit}
$save=New-Object System.Windows.Forms.SaveFileDialog
$save.Filter='Riff Lab HTML|*.html'
$save.FileName=[IO.Path]::GetFileNameWithoutExtension($source)+'-RiffLab.html'
if($save.ShowDialog() -ne 'OK'){exit}
Write-Host 'Analyzing attacks and rhythm groups. Your source WAV remains intact.'
& $python -B (Join-Path $PSScriptRoot 'DrumModel\riff_analysis.py') $source $save.FileName --bpm $bpm --start-bar $startBar
if($LASTEXITCODE -ne 0){throw 'Riff analysis failed; see the message above.'}
Invoke-Item -LiteralPath $save.FileName