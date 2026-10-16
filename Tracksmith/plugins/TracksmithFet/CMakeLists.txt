juce_add_plugin(TracksmithFet COMPANY_NAME "Tracksmith" BUNDLE_ID "com.tracksmith.fet"
    PLUGIN_MANUFACTURER_CODE Trsm PLUGIN_CODE TsFt FORMATS VST3 PRODUCT_NAME "Tracksmith FET Compressor"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE PRODUCES_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(TracksmithFet PRIVATE FetProcessor.cpp FetDsp.h)
target_compile_definitions(TracksmithFet PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TracksmithFet PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
add_custom_command(TARGET TracksmithFet_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:TracksmithFet_VST3>/../.."
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Tracksmith FET Compressor.vst3")
