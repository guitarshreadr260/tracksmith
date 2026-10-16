juce_add_plugin(TracksmithPlate COMPANY_NAME "Tracksmith" BUNDLE_ID "com.tracksmith.plate"
    PLUGIN_MANUFACTURER_CODE Trsm PLUGIN_CODE TsPl FORMATS VST3 PRODUCT_NAME "Tracksmith Plate"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE PRODUCES_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(TracksmithPlate PRIVATE PlateProcessor.cpp PlateDsp.h)
target_compile_definitions(TracksmithPlate PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TracksmithPlate PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
add_custom_command(TARGET TracksmithPlate_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:TracksmithPlate_VST3>/../.."
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Tracksmith Plate.vst3")
