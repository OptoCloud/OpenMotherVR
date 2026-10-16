# OpenMotherVR-Provenance: clean
# Sources of src/platform/d3d11/render/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dds.h
    ${CMAKE_CURRENT_LIST_DIR}/mirror_renderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mirror_renderer.h
    ${CMAKE_CURRENT_LIST_DIR}/texture_dump.cpp
    ${CMAKE_CURRENT_LIST_DIR}/texture_dump.h
    ${CMAKE_CURRENT_LIST_DIR}/vignette_renderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/vignette_renderer.h
)
