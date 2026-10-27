@echo off
set "PY=%USERPROFILE%\Downloads\Tracksmith-SPLIT-Build1\Tracksmith-SPLIT-Build1\.venv\Scripts\python.exe"
if not exist "%PY%" (
  echo Build 1 environment not found. Keep Build 1 in its original Downloads folder.
  pause
  exit /b 1
)
"%PY%" "%~dp0split_qt.py"
if errorlevel 1 pause

