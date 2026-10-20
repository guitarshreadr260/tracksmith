$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /target:winexe /optimize+ /out:"$PSScriptRoot\TracksmithGEN.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Core.dll /reference:System.Xml.Linq.dll "$PSScriptRoot\TracksmithGEN.cs" "$PSScriptRoot\AudioLab.cs" "$PSScriptRoot\Studio.cs" "$PSScriptRoot\AceBridge.cs" "$PSScriptRoot\ReferenceStudio.cs"
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
Write-Output 'Built TracksmithGEN.exe'
