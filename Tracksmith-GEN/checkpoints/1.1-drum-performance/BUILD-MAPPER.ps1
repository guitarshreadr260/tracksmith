$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /target:winexe /optimize+ /out:"$PSScriptRoot\GGD-MIDI-Mapper.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll "$PSScriptRoot\GgdMidiMapper.cs"
if ($LASTEXITCODE -ne 0) {throw 'MIDI mapper build failed'}
