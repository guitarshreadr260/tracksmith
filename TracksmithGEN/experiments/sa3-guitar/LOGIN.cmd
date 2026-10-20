@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" goto missing
".venv\Scripts\python.exe" -c "from huggingface_hub import login; login(add_to_git_credential=False)"
pause
exit /b
:missing
echo Run SETUP.cmd first.
pause
exit /b 1
