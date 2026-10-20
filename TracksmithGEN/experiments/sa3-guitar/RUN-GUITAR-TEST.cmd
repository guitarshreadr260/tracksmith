@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" goto missing
".venv\Scripts\python.exe" trial.py
if errorlevel 1 echo Trial failed or incomplete. Read the errors above.
pause
exit /b
:missing
echo Run SETUP.cmd first.
pause
exit /b 1
