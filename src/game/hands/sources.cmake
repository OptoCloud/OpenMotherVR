# OpenMotherVR-Provenance: clean
# Sources of src/game/hands/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/body_objects.cpp
    ${CMAKE_CURRENT_LIST_DIR}/body_objects.h
    ${CMAKE_CURRENT_LIST_DIR}/body_sites.cpp
    ${CMAKE_CURRENT_LIST_DIR}/body_sites.h
    ${CMAKE_CURRENT_LIST_DIR}/hand_objects.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hand_objects.h
    ${CMAKE_CURRENT_LIST_DIR}/hand_sites.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hand_sites.h
    ${CMAKE_CURRENT_LIST_DIR}/haptic_sites.cpp
    ${CMAKE_CURRENT_LIST_DIR}/haptic_sites.h
)
