# OpenMotherVR-Provenance: clean
# Sources of src/core/diag/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/crash_safe_sink.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crash_safe_sink.h
    ${CMAKE_CURRENT_LIST_DIR}/crash_state.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crash_state.h
    ${CMAKE_CURRENT_LIST_DIR}/flush_policy.cpp
    ${CMAKE_CURRENT_LIST_DIR}/flush_policy.h
    ${CMAKE_CURRENT_LIST_DIR}/flush_schedule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/flush_schedule.h
    ${CMAKE_CURRENT_LIST_DIR}/log_format.cpp
    ${CMAKE_CURRENT_LIST_DIR}/log_format.h
    ${CMAKE_CURRENT_LIST_DIR}/logger.cpp
    ${CMAKE_CURRENT_LIST_DIR}/logger.h
    ${CMAKE_CURRENT_LIST_DIR}/thread_roles.cpp
    ${CMAKE_CURRENT_LIST_DIR}/thread_roles.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
)
