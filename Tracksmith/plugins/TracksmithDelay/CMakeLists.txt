juce_add_plugin(TracksmithDelay COMPANY_NAME "Tracksmith" BUNDLE_ID "com.tracksmith.delay"
    PLUGIN_MANUFACTURER_CODE Trsm PLUGIN_CODE TsDd FORMATS VST3 PRODUCT_NAME "Tracksmith Digital Delay"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE PRODUCES_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(TracksmithDelay PRIVATE DelayProcessor.cpp DelayDsp.h)
target_compile_definitions(TracksmithDelay PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TracksmithDelay PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
add_custom_command(TARGET TracksmithDelay_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:TracksmithDelay_VST3>/../.."
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Tracksmith Digital Delay.vst3")
