# Native TONE3000 processor. Keep its NAM namespace separate from legacy sessions.
set(T3K_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/t3k")
set(T3K_PLUGIN "${T3K_ROOT}/plugin")
file(GLOB T3K_DSP CONFIGURE_DEPENDS "${T3K_PLUGIN}/src/*.cpp")
file(GLOB_RECURSE T3K_NAM CONFIGURE_DEPENDS "${T3K_PLUGIN}/NeuralAmpModelerCore/NAM/*.cpp")
list(FILTER T3K_DSP EXCLUDE REGEX "StandaloneAudioSettings.cpp$")
if(APPLE)
    list(FILTER T3K_DSP EXCLUDE REGEX "(AudioPermissions|WindowKeyEvents|StandaloneStateAutosave).cpp$")
    list(APPEND T3K_DSP "${T3K_PLUGIN}/src/AudioPermissions.mm"
        "${T3K_PLUGIN}/src/WindowKeyEvents.mm" "${T3K_PLUGIN}/src/StandaloneStateAutosave.mm")
endif()
add_library(TracksmithTone3000 STATIC ${T3K_DSP}
    "${T3K_PLUGIN}/ui/backend/ProcessorBackend.cpp" "${T3K_PLUGIN}/ui/NativeEditor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Tone3000Native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/Tone3000HostedAudio.cpp")
include("${T3K_PLUGIN}/ui/NativeUi.cmake")
t3k_add_native_ui(TracksmithTone3000)
target_include_directories(TracksmithTone3000 PRIVATE
    "${T3K_PLUGIN}/include" "${T3K_PLUGIN}/ui"
    "${T3K_PLUGIN}/NeuralAmpModelerCore"
    "${T3K_PLUGIN}/NeuralAmpModelerCore/Dependencies/eigen"
    "${T3K_PLUGIN}/NeuralAmpModelerCore/Dependencies/nlohmann"
    "${T3K_PLUGIN}/AudioDSPTools"
    "${T3K_PLUGIN}/AudioDSPTools/dsp/ResamplingContainer/Dependencies")
add_library(TracksmithTone3000Nam STATIC ${T3K_NAM})
target_compile_features(TracksmithTone3000Nam PRIVATE cxx_std_20)
target_include_directories(TracksmithTone3000Nam PRIVATE
    "${T3K_PLUGIN}/NeuralAmpModelerCore" "${T3K_PLUGIN}/NeuralAmpModelerCore/Dependencies/eigen"
    "${T3K_PLUGIN}/NeuralAmpModelerCore/Dependencies/nlohmann")
target_compile_definitions(TracksmithTone3000Nam PRIVATE nam=tracksmith_t3k_nam NAM_ENABLE_A2_FAST NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(TracksmithTone3000 PRIVATE TracksmithTone3000Nam)
file(READ "${T3K_ROOT}/VERSION" T3K_VERSION)
string(STRIP "${T3K_VERSION}" T3K_VERSION)
target_compile_features(TracksmithTone3000 PRIVATE cxx_std_20)
set_target_properties(TracksmithTone3000 PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
target_compile_definitions(TracksmithTone3000 PRIVATE
    nam=tracksmith_t3k_nam createPluginFilter=createTone3000PluginFilter
    NAM_ENABLE_A2_FAST HEADLESS=0 JucePlugin_Build_Standalone=0 JucePlugin_IsSynth=0
    JucePlugin_Name="TONE3000" JucePlugin_VersionString="${T3K_VERSION}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_VST3_CAN_REPLACE_VST2=0
    NOMINMAX WIN32_LEAN_AND_MEAN _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING)
target_link_libraries(TracksmithTone3000 PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
    juce::juce_recommended_config_flags)
if(MSVC)
    target_compile_options(TracksmithTone3000 PRIVATE /bigobj)
    target_link_options(TracksmithTone3000 INTERFACE "/WHOLEARCHIVE:$<TARGET_FILE:TracksmithTone3000Nam>")
elseif(APPLE)
    target_link_options(TracksmithTone3000 INTERFACE "LINKER:-force_load,$<TARGET_FILE:TracksmithTone3000Nam>")
    target_link_libraries(TracksmithTone3000 PRIVATE "-framework AVFoundation" "-framework AppKit")
endif()
target_link_libraries(Tracksmith PRIVATE TracksmithTone3000)

option(TRACKSMITH_TONE3000_TESTS "Build native Tone3000 integration checks" ON)
if(TRACKSMITH_TONE3000_TESTS)
    juce_add_console_app(TracksmithTone3000Tests PRODUCT_NAME "Tracksmith Tone3000 tests")
    target_sources(TracksmithTone3000Tests PRIVATE tests/Tone3000Tests.cpp)
    target_include_directories(TracksmithTone3000Tests PRIVATE "${T3K_PLUGIN}/include" "${T3K_PLUGIN}/NeuralAmpModelerCore" "${T3K_PLUGIN}/NeuralAmpModelerCore/Dependencies/eigen" "${T3K_PLUGIN}/NeuralAmpModelerCore/Dependencies/nlohmann" "${T3K_PLUGIN}/AudioDSPTools" "${T3K_PLUGIN}/AudioDSPTools/dsp/ResamplingContainer/Dependencies")
    target_compile_definitions(TracksmithTone3000Tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_VST3_CAN_REPLACE_VST2=0 NAM_ENABLE_A2_FAST NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_features(TracksmithTone3000Tests PRIVATE cxx_std_20)
    target_link_libraries(TracksmithTone3000Tests PRIVATE TracksmithTone3000 juce::juce_audio_utils juce::juce_dsp juce::juce_animation juce::juce_cryptography)
endif()
