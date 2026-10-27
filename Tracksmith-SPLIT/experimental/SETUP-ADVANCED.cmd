@echo off
setlocal
set "BASE=%USERPROFILE%\Downloads\Tracksmith-SPLIT-Build1\Tracksmith-SPLIT-Build1\.venv\Scripts\python.exe"
set "ENV=%~dp0.advanced-env"
if not exist "%BASE%" (echo Build 1 Python environment not found.& pause & exit /b 1)
echo Creating a separate advanced-model environment. This may take several minutes and several GB.
"%BASE%" -m venv "%ENV%"
if errorlevel 1 (echo Could not create environment.& pause & exit /b 1)
"%ENV%\Scripts\python.exe" -m pip install "sound-razor[guitars] @ git+https://github.com/pedromtcosta/sound-razor.git" "mdxnet-infer @ git+https://github.com/openmirlab/mdxnet-infer.git"
if errorlevel 1 (echo Setup failed. Check internet, Git installation, and errors above.& pause & exit /b 1)
echo Setup complete. Models download on first use. Start SPLIT and load six-stem results.
pause

