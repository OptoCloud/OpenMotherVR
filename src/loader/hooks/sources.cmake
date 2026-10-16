# OpenMotherVR-Provenance: clean
# Sources of src/loader/hooks/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/audio_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/audio_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/crash_hook.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crash_hook.h
    ${CMAKE_CURRENT_LIST_DIR}/d3d11_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/d3d11_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/event_tap_hook.cpp
    ${CMAKE_CURRENT_LIST_DIR}/event_tap_hook.h
    ${CMAKE_CURRENT_LIST_DIR}/file_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/file_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/hand_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hand_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/guard.h
    ${CMAKE_CURRENT_LIST_DIR}/menu_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/menu_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/pad_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pad_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/render_fix_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/render_fix_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/stereo_device_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/stereo_device_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/stereo_update_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/stereo_update_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/ui_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_hooks.h
    ${CMAKE_CURRENT_LIST_DIR}/window_hooks.cpp
    ${CMAKE_CURRENT_LIST_DIR}/window_hooks.h
)
