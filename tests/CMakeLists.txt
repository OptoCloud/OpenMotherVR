# OpenMotherVR-Provenance: clean
# Unit tests of util and core: plain C++ that runs without the game, a
# headset or the DLL (STYLEGUIDE 2.1). Links omvr_core and omvr_util only,
# never d3d11.dll.
find_package(doctest CONFIG REQUIRED)

add_executable(omvr_tests
    main.cpp
    util/util_test.cpp
    core/comfort_test.cpp
    core/config_test.cpp
    core/crash_log_test.cpp
    core/diag_test.cpp
    core/engine_settings_test.cpp
    core/dpad_test.cpp
    core/hands_test.cpp
    core/haptics_test.cpp
    core/input_test.cpp
    core/render_test.cpp
    core/stereo_test.cpp
    core/play_space_test.cpp
    core/presence_test.cpp
    core/ui_test.cpp
    core/vr_off_test.cpp
    core/vr_test.cpp
    game/address_table_test.cpp
)
target_include_directories(omvr_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(omvr_tests PRIVATE omvr_options omvr_core omvr_util doctest::doctest)

add_test(NAME omvr_tests COMMAND omvr_tests)
