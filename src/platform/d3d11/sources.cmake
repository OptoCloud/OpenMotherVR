# OpenMotherVR-Provenance: clean
# Sources of src/platform/d3d11/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/constants.h
    ${CMAKE_CURRENT_LIST_DIR}/handles.h
    ${CMAKE_CURRENT_LIST_DIR}/swapchain.cpp
    ${CMAKE_CURRENT_LIST_DIR}/swapchain.h
    ${CMAKE_CURRENT_LIST_DIR}/system_d3d11.cpp
    ${CMAKE_CURRENT_LIST_DIR}/system_d3d11.h
)
