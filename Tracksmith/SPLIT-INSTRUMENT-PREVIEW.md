# SPLIT and instrument integration preview

This branch saves the newer SPLIT-integrated DAW source and transport layout together with the Tracksmith Piano and Bass preview instruments. The instruments are limited previews; full standalone presets, effects, UI and articulations are not yet integrated. The included sfizz runtime and compact sound libraries are under Native Instruments/Tracksmith Sounds with their license notices.

On Windows SPLIT currently requires the existing GPU-enabled environment at Downloads/Tracksmith-SPLIT-Build1/Tracksmith-SPLIT-Build1/.venv. This is a machine-specific dependency and must be made configurable before distribution. Full end-to-end separation and audio-device playback have not been verified in the combined build.

Build the web UI before configuring CMake. Vite is configured to deduplicate React to prevent the blank startup screen. The combined Windows DAW compiled and its startup screen and SPLIT button were inspected; the individual preview instrument MIDI rendering and state restoration passed earlier tests. NativeSfizzTests.cpp is the source of the standalone instrument test and is not yet registered in this branch's CMake configuration.
