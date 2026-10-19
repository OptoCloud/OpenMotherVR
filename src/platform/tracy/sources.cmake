# OpenMotherVR-Provenance: clean
# Sources of src/platform/tracy/; each directory lists its own files.
# Built only in the traced flavour (MOTHERVR_TRACY).
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/gpu_zones.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tracy_tracer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tracy_tracer.h
)
