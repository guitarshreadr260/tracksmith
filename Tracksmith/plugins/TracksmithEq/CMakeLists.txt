juce_add_plugin(TracksmithEq COMPANY_NAME "Tracksmith" BUNDLE_ID "com.tracksmith.eq7"
    PLUGIN_MANUFACTURER_CODE Trsm PLUGIN_CODE TsE7 FORMATS VST3 PRODUCT_NAME "Tracksmith EQ 7"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE PRODUCES_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(TracksmithEq PRIVATE EqProcessor.cpp EqEditor.cpp EqProcessor.h EqDsp.h)
target_compile_definitions(TracksmithEq PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TracksmithEq PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
add_custom_command(TARGET TracksmithEq_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:TracksmithEq_VST3>/../.."
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Tracksmith EQ 7.vst3")
