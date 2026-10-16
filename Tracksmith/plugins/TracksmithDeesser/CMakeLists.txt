juce_add_plugin(TracksmithDeesser COMPANY_NAME "Tracksmith" BUNDLE_ID "com.tracksmith.deesser"
    PLUGIN_MANUFACTURER_CODE Trsm PLUGIN_CODE TsDe FORMATS VST3 PRODUCT_NAME "Tracksmith De-Esser"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE PRODUCES_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(TracksmithDeesser PRIVATE DeesserProcessor.cpp DeesserDsp.h)
target_compile_definitions(TracksmithDeesser PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TracksmithDeesser PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
add_custom_command(TARGET TracksmithDeesser_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:TracksmithDeesser_VST3>/../.."
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Tracksmith De-Esser.vst3")
