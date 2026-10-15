# Sources of src/platform/openxr/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/constants.h
    ${CMAKE_CURRENT_LIST_DIR}/controller_state.h
    ${CMAKE_CURRENT_LIST_DIR}/frame_loop.cpp
    ${CMAKE_CURRENT_LIST_DIR}/input.cpp
    ${CMAKE_CURRENT_LIST_DIR}/input.h
    ${CMAKE_CURRENT_LIST_DIR}/instance.cpp
    ${CMAKE_CURRENT_LIST_DIR}/openxr_runtime.cpp
    ${CMAKE_CURRENT_LIST_DIR}/openxr_runtime.h
    ${CMAKE_CURRENT_LIST_DIR}/result_names.cpp
    ${CMAKE_CURRENT_LIST_DIR}/result_names.h
    ${CMAKE_CURRENT_LIST_DIR}/runtime_probe.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_probe.h
    ${CMAKE_CURRENT_LIST_DIR}/session.cpp
    ${CMAKE_CURRENT_LIST_DIR}/session.h
    ${CMAKE_CURRENT_LIST_DIR}/session_impl.h
    ${CMAKE_CURRENT_LIST_DIR}/swapchain.cpp
    ${CMAKE_CURRENT_LIST_DIR}/swapchain.h
    ${CMAKE_CURRENT_LIST_DIR}/virtual_screen.cpp
    ${CMAKE_CURRENT_LIST_DIR}/virtual_screen.h
    ${CMAKE_CURRENT_LIST_DIR}/xr_convert.h
    ${CMAKE_CURRENT_LIST_DIR}/xr_handle.h
    ${CMAKE_CURRENT_LIST_DIR}/xr_headers.h
)
