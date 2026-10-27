cmake_minimum_required(VERSION 3.22)
project(TracksmithSplitVST VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory("C:/AVRX_GROOVE/JUCE-master/JUCE-master" JUCE)
juce_add_plugin(TracksmithSplit
    COMPANY_NAME "Tracksmith" PLUGIN_MANUFACTURER_CODE Trks PLUGIN_CODE Tspt
    FORMATS VST3 PRODUCT_NAME "Tracksmith SPLIT"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(TracksmithSplit)
target_sources(TracksmithSplit PRIVATE Plugin.cpp)
target_compile_definitions(TracksmithSplit PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TracksmithSplit PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

