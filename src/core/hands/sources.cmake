# OpenMotherVR-Provenance: clean
# Sources of src/core/hands/; each directory lists its own files.
target_sources(omvr_core PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/activity.cpp
    ${CMAKE_CURRENT_LIST_DIR}/activity.h
    ${CMAKE_CURRENT_LIST_DIR}/aim.cpp
    ${CMAKE_CURRENT_LIST_DIR}/aim.h
    ${CMAKE_CURRENT_LIST_DIR}/hand_mapping.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hand_mapping.h
    ${CMAKE_CURRENT_LIST_DIR}/hand_track.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hand_track.h
    ${CMAKE_CURRENT_LIST_DIR}/hands.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hands.h
    ${CMAKE_CURRENT_LIST_DIR}/pose_filter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pose_filter.h
    ${CMAKE_CURRENT_LIST_DIR}/tracker_facing.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tracker_facing.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
    ${CMAKE_CURRENT_LIST_DIR}/types.h
)
