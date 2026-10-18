# OpenMotherVR-Provenance: clean
# Sources of src/core/vr/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/fov.cpp
    ${CMAKE_CURRENT_LIST_DIR}/fov.h
    ${CMAKE_CURRENT_LIST_DIR}/hand_offsets.h
    ${CMAKE_CURRENT_LIST_DIR}/heading.cpp
    ${CMAKE_CURRENT_LIST_DIR}/heading.h
    ${CMAKE_CURRENT_LIST_DIR}/pose_age.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pose_age.h
    ${CMAKE_CURRENT_LIST_DIR}/pose_convert.h
    ${CMAKE_CURRENT_LIST_DIR}/pose_hold.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pose_hold.h
    ${CMAKE_CURRENT_LIST_DIR}/runtime_choice.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_choice.h
    ${CMAKE_CURRENT_LIST_DIR}/runtime_detector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_detector.h
    ${CMAKE_CURRENT_LIST_DIR}/runtime_failure.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_failure.h
    ${CMAKE_CURRENT_LIST_DIR}/space_change.cpp
    ${CMAKE_CURRENT_LIST_DIR}/space_change.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
    ${CMAKE_CURRENT_LIST_DIR}/types.h
)
