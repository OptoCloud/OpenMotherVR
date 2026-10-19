# OpenMotherVR-Provenance: clean
# Sources of src/loader/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/attach.cpp
    ${CMAKE_CURRENT_LIST_DIR}/attach.h
    ${CMAKE_CURRENT_LIST_DIR}/context.cpp
    ${CMAKE_CURRENT_LIST_DIR}/context.h
    ${CMAKE_CURRENT_LIST_DIR}/d3d11.def
    ${CMAKE_CURRENT_LIST_DIR}/d3d11_entry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/d3d11_exports.inc
    ${CMAKE_CURRENT_LIST_DIR}/d3d11_forwarders.cpp
    ${CMAKE_CURRENT_LIST_DIR}/d3d11_forwarders.h
    ${CMAKE_CURRENT_LIST_DIR}/dll_main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gesture_context.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gesture_context.h
    ${CMAKE_CURRENT_LIST_DIR}/headset_audio.cpp
    ${CMAKE_CURRENT_LIST_DIR}/headset_audio.h
    ${CMAKE_CURRENT_LIST_DIR}/hook_targets.h
    ${CMAKE_CURRENT_LIST_DIR}/install.cpp
    ${CMAKE_CURRENT_LIST_DIR}/install.h
    ${CMAKE_CURRENT_LIST_DIR}/launch_options.cpp
    ${CMAKE_CURRENT_LIST_DIR}/launch_options.h
    ${CMAKE_CURRENT_LIST_DIR}/play_space_control.cpp
    ${CMAKE_CURRENT_LIST_DIR}/play_space_control.h
    ${CMAKE_CURRENT_LIST_DIR}/resource_ids.h
    ${CMAKE_CURRENT_LIST_DIR}/resources.rc
    ${CMAKE_CURRENT_LIST_DIR}/runtime_state.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_state.h
    ${CMAKE_CURRENT_LIST_DIR}/stick_context.cpp
    ${CMAKE_CURRENT_LIST_DIR}/stick_context.h
    ${CMAKE_CURRENT_LIST_DIR}/tracing.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tracing.h
)
