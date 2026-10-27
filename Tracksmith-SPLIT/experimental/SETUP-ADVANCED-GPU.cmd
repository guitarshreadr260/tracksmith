@echo off
setlocal
set "ENV=%~dp0.advanced-env"
if not exist "%ENV%\Scripts\python.exe" (
  echo Run SETUP-ADVANCED.cmd first to install the advanced models.
  pause
  exit /b 1
)
echo Installing NVIDIA CUDA-enabled PyTorch into the advanced environment.
"%ENV%\Scripts\python.exe" -m pip install --upgrade --force-reinstall "torch==2.8.0+cu128" --index-url https://download.pytorch.org/whl/cu128
if errorlevel 1 (echo GPU setup failed. See the error above.& pause & exit /b 1)
"%ENV%\Scripts\python.exe" -c "import torch; print('PyTorch:',torch.__version__,'CUDA:',torch.version.cuda); print('GPU:', torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'NOT AVAILABLE'); raise SystemExit(0 if torch.cuda.is_available() else 1)"
if errorlevel 1 (echo GPU check failed. Please share this window.& pause & exit /b 1)
echo GPU setup complete.
pause

