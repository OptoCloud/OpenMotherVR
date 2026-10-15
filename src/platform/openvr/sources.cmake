# Sources of src/platform/openvr/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/api_loader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/api_loader.h
    ${CMAKE_CURRENT_LIST_DIR}/binding_manifest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/binding_manifest.h
    ${CMAKE_CURRENT_LIST_DIR}/compat_names.h
    ${CMAKE_CURRENT_LIST_DIR}/constants.h
    ${CMAKE_CURRENT_LIST_DIR}/input_actions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/input_actions.h
    ${CMAKE_CURRENT_LIST_DIR}/openvr_runtime.cpp
    ${CMAKE_CURRENT_LIST_DIR}/openvr_runtime.h
    ${CMAKE_CURRENT_LIST_DIR}/runtime_probe.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_probe.h
)
