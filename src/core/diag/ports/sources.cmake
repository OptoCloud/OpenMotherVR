# OpenMotherVR-Provenance: clean
# Sources of src/core/diag/ports/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/clock.h
    ${CMAKE_CURRENT_LIST_DIR}/log.h
    ${CMAKE_CURRENT_LIST_DIR}/log_flush.h
    ${CMAKE_CURRENT_LIST_DIR}/log_sink.h
)
