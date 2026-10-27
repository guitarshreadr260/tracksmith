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
$mapDialog = New-Object System.Windows.Forms.Form
$mapDialog.Text = 'Tracksmith GEN - basic beat and feel'
$mapDialog.ClientSize = New-Object System.Drawing.Size(530,415)
$mapDialog.StartPosition = 'CenterScreen'
$mapDialog.FormBorderStyle = 'FixedDialog'
$mapDialog.MaximizeBox = $false
function Add-Label($text,$y) {
 $label = New-Object System.Windows.Forms.Label
 $label.Text=$text; $label.SetBounds(18,$y,200,26); $mapDialog.Controls.Add($label)
}
function Add-Choice($items,$y) {
 $choice=New-Object System.Windows.Forms.ComboBox
 $choice.SetBounds(225,$y,285,30);$choice.DropDownStyle='DropDownList'
 foreach($item in $items){[void]$choice.Items.Add($item)}
 $choice.SelectedIndex=0;$mapDialog.Controls.Add($choice);return $choice
}
Add-Label 'Drum mapping' 20
$maps=Add-Choice @('GGD OKW Metal - factory','General MIDI') 18
Add-Label 'Beat complexity' 65
$complexity=Add-Choice @('Basic / editable foundation','Developed / extra kick','Fuller / more kick variation') 63
Add-Label 'Swing: 50 = straight' 110
$swing=New-Object System.Windows.Forms.NumericUpDown
$swing.SetBounds(225,108,285,30);$swing.Minimum=50;$swing.Maximum=75;$swing.DecimalPlaces=1;$swing.Value=50
$mapDialog.Controls.Add($swing)
Add-Label 'Timekeeping cymbal' 155
$keeper=Add-Choice @('Hi-hat','Ride') 153
Add-Label 'Short tom fill' 200
$fills=Add-Choice @('Off','Every 4 complete bars','Every 8 complete bars') 198
$follow=New-Object System.Windows.Forms.CheckBox
$follow.Text='Follow strong guitar attacks / max 1 extra kick per bar'
$follow.SetBounds(18,250,490,30);$follow.Checked=$true;$mapDialog.Controls.Add($follow)
$crash=New-Object System.Windows.Forms.CheckBox
$crash.Text='Crash at the start and every 4 bars'
$crash.SetBounds(18,290,490,30);$crash.Checked=$false;$mapDialog.Controls.Add($crash)
$ok=New-Object System.Windows.Forms.Button
$ok.Text='Continue';$ok.SetBounds(350,355,160,36);$ok.DialogResult='OK';$mapDialog.Controls.Add($ok);$mapDialog.AcceptButton=$ok
try {
 if($mapDialog.ShowDialog() -ne 'OK'){exit}
 $drumMap=if($maps.SelectedIndex -eq 0){'ggd-okw-metal'}else{'gm'}
 $swingValue=$swing.Value.ToString([Globalization.CultureInfo]::InvariantCulture)
 $density=$complexity.SelectedIndex
 $fillEvery=@(0,4,8)[$fills.SelectedIndex]
 $timekeeper=if($keeper.SelectedIndex -eq 0){'hihat'}else{'ride'}
 $optional=@()
 if($follow.Checked){$optional+='--follow-accents'}
 if($crash.Checked){$optional+='--crashes'}
} finally {$mapDialog.Dispose()}

$bpm = [Microsoft.VisualBasic.Interaction]::InputBox('Recording tempo in BPM', 'Align drums', '140')
if (-not $bpm) { exit }
$startBar = [Microsoft.VisualBasic.Interaction]::InputBox('First drum bar. Bar 1 stays empty when you choose 2. Import MIDI at bar 1.', 'Basic beat / song start', '2')
if (-not $startBar) { exit }
$save = New-Object System.Windows.Forms.SaveFileDialog
$save.Filter = 'MIDI|*.mid'
$save.FileName = [System.IO.Path]::GetFileNameWithoutExtension($source) + $(if ($drumMap -eq 'ggd-okw-metal') {'-GGD-Editable-Beat.mid'} else {'-GM-Editable-Beat.mid'})
if ($save.ShowDialog() -ne 'OK') { exit }
& $python -B (Join-Path $PSScriptRoot 'DrumModel\align_drums.py') $source $save.FileName --bpm $bpm --start-bar $startBar --drum-map $drumMap --pattern groove --swing $swingValue --density $density --fill-every $fillEvery --timekeeper $timekeeper @optional
if ($LASTEXITCODE -ne 0) { throw 'Alignment failed; see the diagnostic above.' }
[System.Windows.Forms.MessageBox]::Show('Import this MIDI at bar 1. Its first hits start on your selected bar (default 2). Keep your original guitar at its correct song position. Set the DAW to the recording tempo. Load your matching drum instrument. GGD Metal output targets its factory Invasion map.', 'Aligned MIDI ready') | Out-Null
