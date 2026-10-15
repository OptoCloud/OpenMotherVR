# OpenMotherVR-Provenance: clean
# Sources of src/platform/win32/patching/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/inline_hook.cpp
    ${CMAKE_CURRENT_LIST_DIR}/inline_hook.h
    ${CMAKE_CURRENT_LIST_DIR}/memory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/memory.h
    ${CMAKE_CURRENT_LIST_DIR}/pe_image.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pe_image.h
)
