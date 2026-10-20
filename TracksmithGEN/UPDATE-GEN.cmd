@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update-gen.ps1"
if errorlevel 1 echo Update failed. Read the error above.
pause
