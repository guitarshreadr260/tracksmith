@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Generation-Engine.ps1" -OpenTracksmith
if errorlevel 1 pause
