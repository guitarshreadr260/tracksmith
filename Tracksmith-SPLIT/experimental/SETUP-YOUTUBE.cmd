@echo off
set "PY=%USERPROFILE%\Downloads\Tracksmith-SPLIT-Build1\Tracksmith-SPLIT-Build1\.venv\Scripts\python.exe"
if not exist "%PY%" (
  echo Build 1 environment was not found in Downloads.
  pause
  exit /b 1
)
"%PY%" -m pip install -U "yt-dlp[default]" imageio-ffmpeg
if errorlevel 1 (
  echo Setup failed. Check your internet connection and the message above.
) else (
  echo YouTube support installed. You can now start SPLIT.
)
pause

