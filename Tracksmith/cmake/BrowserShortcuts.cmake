# JUCE 8.0.10 exposes no zoom/accelerator options. Patch a build-local copy of
# gui_extra, leaving the supplied JUCE checkout and other builds unchanged.
if(WIN32)
    set(_tracksmith_gui_source "${JUCE_SOURCE_DIR}/modules/juce_gui_extra")
    set(_tracksmith_gui_copy "${CMAKE_CURRENT_BINARY_DIR}/tracksmith-juce/juce_gui_extra")
    file(COPY "${_tracksmith_gui_source}/" DESTINATION "${_tracksmith_gui_copy}")
    set(_tracksmith_browser "${_tracksmith_gui_copy}/native/juce_WebBrowserComponent_windows.cpp")
    file(READ "${_tracksmith_browser}" _tracksmith_native)
    set(_tracksmith_anchor "            settings->put_IsStatusBarEnabled (! preferences.getWinWebView2BackendOptions().getIsStatusBarDisabled());")
    string(FIND "${_tracksmith_native}" "${_tracksmith_anchor}" _tracksmith_anchor_position)
    if(_tracksmith_anchor_position EQUAL -1)
        message(FATAL_ERROR "Tracksmith browser shortcut patch requires the pinned JUCE 8.0.10 source.")
    endif()
    string(REPLACE "${_tracksmith_anchor}" "${_tracksmith_anchor}
            // Tracksmith owns DAW shortcuts; browser zoom must never consume Ctrl+=.
            settings->put_IsZoomControlEnabled (false);
            webViewController->put_ZoomFactor (1.0);
            ComSmartPtr<ICoreWebView2Settings3> dawSettings;
            settings.QueryInterface (dawSettings);
            if (dawSettings != nullptr)
                dawSettings->put_AreBrowserAcceleratorKeysEnabled (false);" _tracksmith_native "${_tracksmith_native}")
    file(WRITE "${_tracksmith_browser}" "${_tracksmith_native}")
    get_target_property(_tracksmith_gui_sources juce_gui_extra INTERFACE_SOURCES)
    list(REMOVE_ITEM _tracksmith_gui_sources "${_tracksmith_gui_source}/juce_gui_extra.cpp")
    list(APPEND _tracksmith_gui_sources "${_tracksmith_gui_copy}/juce_gui_extra.cpp")
    set_property(TARGET juce_gui_extra PROPERTY INTERFACE_SOURCES "${_tracksmith_gui_sources}")
endif()
