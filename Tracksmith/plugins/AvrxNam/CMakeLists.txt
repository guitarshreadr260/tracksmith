file(GLOB_RECURSE NAM_SOURCES CONFIGURE_DEPENDS "ThirdParty/NAMCore/NAM/*.cpp")
add_library(AvrxNamCore STATIC ${NAM_SOURCES})
target_include_directories(AvrxNamCore SYSTEM PUBLIC
    ThirdParty/NAMCore ThirdParty/NAMCore/Dependencies/eigen ThirdParty/NAMCore/Dependencies/nlohmann)
target_compile_features(AvrxNamCore PUBLIC cxx_std_20)
target_compile_definitions(AvrxNamCore PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_link_options(AvrxNamCore INTERFACE "/WHOLEARCHIVE:$<TARGET_FILE:AvrxNamCore>")
endif()
juce_add_plugin(AvrxNam COMPANY_NAME "Tracksmith" BUNDLE_ID "com.avrx.nam"
    PLUGIN_MANUFACTURER_CODE Avrx PLUGIN_CODE AvN1 FORMATS VST3 PRODUCT_NAME "Tracksmith NAM"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE PRODUCES_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(AvrxNam PRIVATE Plugin.cpp NativeNam.cpp NativeNam.h)
target_compile_features(AvrxNam PRIVATE cxx_std_20)
target_compile_definitions(AvrxNam PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING)
target_link_libraries(AvrxNam PRIVATE AvrxNamCore juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
add_custom_command(TARGET AvrxNam_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:AvrxNam_VST3>/../.."
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Tracksmith NAM.vst3"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/NAMCore/LICENSE"
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/NAMCore-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/NAMCore/Dependencies/eigen/COPYING.MPL2"
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Eigen-COPYING.MPL2"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/NAMCore/Dependencies/eigen/COPYING.BSD"
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/Eigen-COPYING.BSD"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/NAMCore/Dependencies/nlohmann/LICENSE.MIT"
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/nlohmann-LICENSE.MIT"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/NOTICE.txt"
        "$<TARGET_FILE_DIR:Tracksmith>/Stock Plugins/NOTICE.txt")
