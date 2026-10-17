param([switch]$OpenTracksmith)
$ErrorActionPreference = 'Stop'
$runtimeConfig = Join-Path $PSScriptRoot 'Generation/engine-path.txt'
if (-not (Test-Path -LiteralPath $runtimeConfig)) { throw 'Local engine path is missing. See BUILD32-GENERATION.md for setup.' }
$enginePath = (Get-Content -LiteralPath $runtimeConfig -Raw).Trim()
$pythonPath = Join-Path $enginePath '.venv/Scripts/python.exe'
if (-not (Test-Path -LiteralPath $pythonPath)) { throw 'The local generation environment is missing. See BUILD32-GENERATION.md.' }
$ready = $false
try { $health = Invoke-RestMethod -Uri 'http://127.0.0.1:8001/health' -TimeoutSec 2; $ready = $health.code -eq 200 } catch {}
if (-not $ready) {
    $logFolder = Join-Path $PSScriptRoot 'Generation'
    New-Item -ItemType Directory -Path $logFolder -Force | Out-Null
    $env:ACESTEP_INIT_LLM = 'false'
    $env:ACESTEP_DEVICE = 'cuda'
    $env:ACESTEP_CONFIG_PATH = 'acestep-v15-turbo'
    $env:ACESTEP_DOWNLOAD_SOURCE = 'huggingface'
    $cacheConfig = Join-Path $logFolder 'cache-path.txt'
    if (Test-Path -LiteralPath $cacheConfig) { $env:HF_HOME = (Get-Content -LiteralPath $cacheConfig -Raw).Trim() }
    $engineProcess = Start-Process -FilePath $pythonPath -ArgumentList @('-m','acestep.api_server','--host','127.0.0.1','--port','8001') -WorkingDirectory $enginePath -RedirectStandardOutput (Join-Path $logFolder 'engine.out.log') -RedirectStandardError (Join-Path $logFolder 'engine.err.log') -WindowStyle Hidden -PassThru
    $engineProcess.Id | Set-Content -LiteralPath (Join-Path $logFolder 'engine.pid')
    Write-Output 'Local generation engine is starting. First model loading can take a few minutes.'
} else { Write-Output 'Local generation engine is already responding.' }
if ($OpenTracksmith) { Start-Process -FilePath (Join-Path $PSScriptRoot 'Tracksmith.exe') }
