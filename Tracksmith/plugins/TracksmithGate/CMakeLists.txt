juce_add_plugin(TracksmithGate COMPANY_NAME "Tracksmith" BUNDLE_ID "com.tracksmith.gate"
    PLUGIN_MANUFACTURER_CODE Trsm PLUGIN_CODE TsGt FORMATS VST3 PRODUCT_NAME "Tracksmith Gate"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE PRODUCES_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(TracksmithGate PRIVATE GateProcessor.cpp GateDsp.h)
target_compile_definitions(TracksmithGate PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TracksmithGate PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
add_custom_command(TARGET TracksmithGate_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:TracksmithGate_VST3>/../.."
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Tracksmith Gate.vst3")
