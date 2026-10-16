# OpenMotherVR-Provenance: clean
# Sources of src/game/stereo/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light.cpp
    ${CMAKE_CURRENT_LIST_DIR}/alpha_light.h
    ${CMAKE_CURRENT_LIST_DIR}/cull.cpp
    ${CMAKE_CURRENT_LIST_DIR}/cull.h
    ${CMAKE_CURRENT_LIST_DIR}/field.h
    ${CMAKE_CURRENT_LIST_DIR}/follow_camera.cpp
    ${CMAKE_CURRENT_LIST_DIR}/follow_camera.h
    ${CMAKE_CURRENT_LIST_DIR}/graphics_settings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/graphics_settings.h
    ${CMAKE_CURRENT_LIST_DIR}/head_clip.cpp
    ${CMAKE_CURRENT_LIST_DIR}/head_clip.h
    ${CMAKE_CURRENT_LIST_DIR}/mode_stack.cpp
    ${CMAKE_CURRENT_LIST_DIR}/mode_stack.h
    ${CMAKE_CURRENT_LIST_DIR}/postfx.cpp
    ${CMAKE_CURRENT_LIST_DIR}/postfx.h
    ${CMAKE_CURRENT_LIST_DIR}/pov.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pov.h
    ${CMAKE_CURRENT_LIST_DIR}/renderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/renderer.h
    ${CMAKE_CURRENT_LIST_DIR}/scene_messages.cpp
    ${CMAKE_CURRENT_LIST_DIR}/scene_messages.h
    ${CMAKE_CURRENT_LIST_DIR}/stereo_objects.cpp
    ${CMAKE_CURRENT_LIST_DIR}/stereo_objects.h
)
