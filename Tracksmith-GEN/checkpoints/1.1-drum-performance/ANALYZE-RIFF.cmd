@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ANALYZE-RIFF.ps1"
if errorlevel 1 pause
