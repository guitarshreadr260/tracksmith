param([ValidateSet('Setup','Start')][string]$Action = 'Setup')
$ErrorActionPreference = 'Stop'
try {
    Set-Location -LiteralPath $PSScriptRoot
    $toolsDirectory = Join-Path $PSScriptRoot 'tools'
    $modelDirectory = Join-Path $PSScriptRoot 'ACE-Step-1.5'
    $uvPath = Join-Path $toolsDirectory 'uv.exe'
    $pythonPath = Join-Path $modelDirectory '.venv\Scripts\python.exe'
    $env:UV_CACHE_DIR = Join-Path $toolsDirectory 'cache'
    $env:UV_PYTHON_INSTALL_DIR = Join-Path $toolsDirectory 'python'
    $env:HF_HOME = Join-Path $PSScriptRoot 'model-cache'
    $env:ACESTEP_LM_BACKEND = 'pt'
    $env:ACESTEP_CONFIG_PATH = 'acestep-v15-sft'
    $env:ACESTEP_LM_MODEL_PATH = 'acestep-5Hz-lm-0.6B'
    if ($Action -eq 'Setup') {
        Write-Host 'Tracksmith GEN / separate NVIDIA GPU model test'
        Write-Host 'Candidate: ACE-Step 1.5, 2B SFT audio model with a 0.6B language model.'
        Write-Host 'Large first-use downloads: allow 20 GB or more of free space.'
        Write-Host 'Your existing GEN installation and model environment are not modified.'
        & nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader
        if ($LASTEXITCODE -ne 0) { throw 'NVIDIA driver/GPU detection failed.' }
        New-Item -ItemType Directory -Force -Path $toolsDirectory | Out-Null
        if (-not (Test-Path -LiteralPath $uvPath)) {
            $uvArchive = Join-Path $toolsDirectory 'uv.zip'
            Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/astral-sh/uv/releases/latest/download/uv-x86_64-pc-windows-msvc.zip' -OutFile $uvArchive
            Expand-Archive -LiteralPath $uvArchive -DestinationPath $toolsDirectory -Force
            if (-not (Test-Path -LiteralPath $uvPath)) { throw 'uv extraction failed.' }
        }
        if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) { throw 'Git is required. Install Git for Windows, then rerun this setup.' }
        if (-not (Test-Path -LiteralPath (Join-Path $modelDirectory 'pyproject.toml'))) {
            if (Test-Path -LiteralPath $modelDirectory) { throw 'An incomplete ACE-Step folder already exists. Rename that folder, then rerun setup.' }
            & git.exe clone --depth 1 'https://github.com/ace-step/ACE-Step-1.5.git' $modelDirectory
            if ($LASTEXITCODE -ne 0) { throw 'Model source download failed.' }
        }
        Push-Location -LiteralPath $modelDirectory
        try {
            & $uvPath sync --python 3.11
            if ($LASTEXITCODE -ne 0) { throw 'Dependency installation failed. The error above identifies the failed dependency.' }
        } finally { Pop-Location }
        & $pythonPath -B (Join-Path $PSScriptRoot 'gpu-check.py')
        if ($LASTEXITCODE -ne 0) { throw 'CUDA computation did not pass. This setup will not call CPU-only generation a GPU upgrade.' }
        Write-Host ''
        Write-Host 'CUDA check passed. Next: double-click START-GPU-MODEL.cmd.'
        Write-Host 'The model weights download on first start. Setup is not a listening-quality test.'
    } else {
        if (-not (Test-Path -LiteralPath $pythonPath)) { throw 'Run SETUP-GPU.cmd first.' }
        & $pythonPath -B (Join-Path $PSScriptRoot 'gpu-check.py')
        if ($LASTEXITCODE -ne 0) { throw 'CUDA check failed. Launch stopped.' }
        Push-Location -LiteralPath $modelDirectory
        try {
            Write-Host 'Loading ACE-Step 1.5 SFT with batch size 1 and adaptive CPU offload.'
            Write-Host 'First launch downloads model weights. Keep this terminal open.'
            Write-Host 'When ready, open http://127.0.0.1:7860 in your browser.'
            & $uvPath run --no-sync acestep --config_path acestep-v15-sft --lm_model_path acestep-5Hz-lm-0.6B --backend pt --batch_size 1 --init_service true --server-name 127.0.0.1 --port 7860
            if ($LASTEXITCODE -ne 0) { throw 'Model launch failed. Copy the final error lines for diagnosis.' }
        } finally { Pop-Location }
    }
} catch {
    Write-Host ''
    Write-Host ('Stopped: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
