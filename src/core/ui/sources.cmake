# Sources of src/core/ui/; each directory lists its own files.
target_sources(mothervr PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
    ${CMAKE_CURRENT_LIST_DIR}/commands.h
    ${CMAKE_CURRENT_LIST_DIR}/compat_messages.cpp
    ${CMAKE_CURRENT_LIST_DIR}/compat_messages.h
    ${CMAKE_CURRENT_LIST_DIR}/display_reset.cpp
    ${CMAKE_CURRENT_LIST_DIR}/display_reset.h
    ${CMAKE_CURRENT_LIST_DIR}/notices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/notices.h
    ${CMAKE_CURRENT_LIST_DIR}/pak2.cpp
    ${CMAKE_CURRENT_LIST_DIR}/pak2.h
    ${CMAKE_CURRENT_LIST_DIR}/runtime_menu.cpp
    ${CMAKE_CURRENT_LIST_DIR}/runtime_menu.h
    ${CMAKE_CURRENT_LIST_DIR}/settings_push.cpp
    ${CMAKE_CURRENT_LIST_DIR}/settings_push.h
    ${CMAKE_CURRENT_LIST_DIR}/tuning.h
    ${CMAKE_CURRENT_LIST_DIR}/ui_archive.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_archive.h
    ${CMAKE_CURRENT_LIST_DIR}/ui_contract.h
    ${CMAKE_CURRENT_LIST_DIR}/ui_patch.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_patch.h
)
