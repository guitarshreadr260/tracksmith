$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
function Run-Step {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $Program (exit $LASTEXITCODE)" }
}
if (-not (Test-Path '.venv\Scripts\python.exe')) {
    if (-not (Get-Command py -ErrorAction SilentlyContinue)) {
        throw 'Install Python 3.11 with the Python launcher, then run SETUP.cmd again.'
    }
    Run-Step 'py' @('-3.11', '-m', 'venv', '.venv')
}
$python = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
Run-Step $python @('-m', 'pip', 'install', '--upgrade', 'pip')
Run-Step $python @('-m', 'pip', 'install', 'torch==2.7.1', 'torchaudio==2.7.1', '--index-url', 'https://download.pytorch.org/whl/cu128')
Run-Step $python @('-m', 'pip', 'install', 'https://github.com/Stability-AI/stable-audio-3/archive/77e0398c26f7d22d801ec1bb036307e8b231f741.zip')
Run-Step $python @('-m', 'pip', 'check')
Run-Step $python @('-c', 'import torch; assert torch.cuda.is_available(), "CUDA is unavailable"; x=torch.ones(2,device="cuda"); print(torch.cuda.get_device_name(0), x.sum().item())')
Write-Host 'Setup complete. Accept model access terms, run LOGIN.cmd, then RUN-GUITAR-TEST.cmd.'
