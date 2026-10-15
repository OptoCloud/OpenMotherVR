# Sources of src/game/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/addresses.cpp
    ${CMAKE_CURRENT_LIST_DIR}/addresses.h
    ${CMAKE_CURRENT_LIST_DIR}/build.cpp
    ${CMAKE_CURRENT_LIST_DIR}/build.h
    ${CMAKE_CURRENT_LIST_DIR}/clear_gate.cpp
    ${CMAKE_CURRENT_LIST_DIR}/clear_gate.h
    ${CMAKE_CURRENT_LIST_DIR}/constants.h
    ${CMAKE_CURRENT_LIST_DIR}/detour.cpp
    ${CMAKE_CURRENT_LIST_DIR}/detour.h
    ${CMAKE_CURRENT_LIST_DIR}/event_tap.cpp
    ${CMAKE_CURRENT_LIST_DIR}/event_tap.h
    ${CMAKE_CURRENT_LIST_DIR}/layout.h
    ${CMAKE_CURRENT_LIST_DIR}/patcher.cpp
    ${CMAKE_CURRENT_LIST_DIR}/patcher.h
    ${CMAKE_CURRENT_LIST_DIR}/preflight.cpp
    ${CMAKE_CURRENT_LIST_DIR}/preflight.h
    ${CMAKE_CURRENT_LIST_DIR}/stereo_extension.h
)
