cmake_minimum_required(VERSION 3.20)

# vcpkg: project triplet (static libs, static CRT; see triplets/). Must be set
# before project() so the vcpkg toolchain picks it up.
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "x86-windows-mothervr" CACHE STRING "")

project(mothervr LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "mothervr must be built as 32-bit (x86). Configure with: -A Win32")
endif()

# Warnings as errors. Off by default so a newer compiler's new warnings do
# not stop a local build; CI turns it on.
option(MOTHERVR_WERROR "Treat compiler warnings as errors" OFF)

# Release version, stamped into the DLL's version resource. CI passes the
# tag without its "v" (1.0.0, 1.0.0-rc.2); local builds are 0.0.0-dev.
set(MOTHERVR_VERSION "0.0.0-dev" CACHE STRING "Version stamped into d3d11.dll")
if(NOT MOTHERVR_VERSION MATCHES "^([0-9]+)[.]([0-9]+)[.]([0-9]+)([-+][0-9A-Za-z.+-]+)?$")
    message(FATAL_ERROR "MOTHERVR_VERSION '${MOTHERVR_VERSION}' is not MAJOR.MINOR.PATCH[-pre]")
endif()
set(MOTHERVR_VERSION_MAJOR ${CMAKE_MATCH_1})
set(MOTHERVR_VERSION_MINOR ${CMAKE_MATCH_2})
set(MOTHERVR_VERSION_PATCH ${CMAKE_MATCH_3})
configure_file(src/loader/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# compile_commands.json for clang-tidy (Ninja and Makefile generators).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static CRT: matches the original MotherVR and means the deployed
# d3d11.dll has no external runtime dependency.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenXR CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)

# openvr: headers only. Linking openvr_api would add a static import on
# openvr_api.dll, which lives at a path discovered at runtime; we instead
# LoadLibrary + GetProcAddress from platform/openvr/api_loader.cpp.
# The vcpkg openvr port installs only headers (no CMake config file), so we
# locate them with find_path rather than find_package. The vcpkg toolchain
# puts its include directory on the search path.
find_path(OPENVR_INCLUDE_DIR NAMES openvr.h REQUIRED)

# The DLL embeds Valve's OpenVR loader, the x86 openvr_api.dll of a pinned
# public SDK release. Whatever the file is gets embedded and written next to
# the game, so a wrong file (a Git LFS pointer in a checkout without LFS, a
# 64-bit build, another release) would make every OpenVR start fail on the
# player's machine: check that it is an x86 PE image with the recorded hash.
# Updating the loader changes the release and the hash together;
# scripts/fetch_openvr_api.ps1 reads both from here.
set(OPENVR_API_RELEASE "1.0.10")
set(OPENVR_API_SHA256 "9520d2b88a97f6390fb2d0447019fa0e85344fb1c4ae1aaa3f9d78fed0c8eda3")
set(OPENVR_API_DLL "${CMAKE_CURRENT_SOURCE_DIR}/res/openvr_api.dll")
set(_openvr_api_fix "Run `pwsh scripts/fetch_openvr_api.ps1` (or `git lfs pull`).")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OPENVR_API_DLL}")
if(NOT EXISTS "${OPENVR_API_DLL}")
    message(FATAL_ERROR "res/openvr_api.dll is missing. ${_openvr_api_fix}")
endif()
file(SIZE "${OPENVR_API_DLL}" OPENVR_API_SIZE)
if(OPENVR_API_SIZE LESS 4096)
    message(FATAL_ERROR "res/openvr_api.dll is not the real DLL (Git LFS pointer?). ${_openvr_api_fix}")
endif()
# PE check: "MZ", then the "PE\0\0" signature at e_lfanew (offset 0x3C,
# little endian) followed by the machine field, 0x014C for x86.
file(READ "${OPENVR_API_DLL}" _mz LIMIT 2 HEX)
file(READ "${OPENVR_API_DLL}" _lfanew OFFSET 60 LIMIT 4 HEX)
string(REGEX REPLACE "^(..)(..)(..)(..)$" "\\4\\3\\2\\1" _lfanew "${_lfanew}")
math(EXPR _lfanew "0x${_lfanew}")
file(READ "${OPENVR_API_DLL}" _pe OFFSET ${_lfanew} LIMIT 6 HEX)
if(NOT _mz STREQUAL "4d5a" OR NOT _pe STREQUAL "504500004c01")
    message(FATAL_ERROR "res/openvr_api.dll is not an x86 (32-bit) PE DLL. ${_openvr_api_fix}")
endif()
file(SHA256 "${OPENVR_API_DLL}" _openvr_api_hash)
if(NOT _openvr_api_hash STREQUAL OPENVR_API_SHA256)
    message(FATAL_ERROR "res/openvr_api.dll is not the x86 loader of OpenVR SDK ${OPENVR_API_RELEASE}: "
                        "SHA-256 ${_openvr_api_hash}, expected ${OPENVR_API_SHA256}. ${_openvr_api_fix}")
endif()

# The pass-through exports are listed twice: as thunks (d3d11_exports.inc)
# and as aliases in the module definition file (d3d11.def). Check that the
# two name the same functions; reconfigure when either changes.
set(MOTHERVR_EXPORTS_INC "${CMAKE_CURRENT_SOURCE_DIR}/src/loader/d3d11_exports.inc")
set(MOTHERVR_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/loader/d3d11.def")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MOTHERVR_EXPORTS_INC}" "${MOTHERVR_DEF}")
file(STRINGS "${MOTHERVR_EXPORTS_INC}" _inc_lines REGEX "^OMVR_D3D11_FORWARD\\(")
set(_inc_names "")
foreach(_line IN LISTS _inc_lines)
    string(REGEX REPLACE "^OMVR_D3D11_FORWARD\\([0-9]+, *([A-Za-z0-9_]+)\\).*$" "\\1" _name "${_line}")
    list(APPEND _inc_names "${_name}")
endforeach()
file(STRINGS "${MOTHERVR_DEF}" _def_lines REGEX "= *mvr_fwd_")
set(_def_names "")
foreach(_line IN LISTS _def_lines)
    string(REGEX REPLACE "^ *([A-Za-z0-9_]+) *= *mvr_fwd_([A-Za-z0-9_]+).*$" "\\1;\\2" _pair "${_line}")
    list(GET _pair 0 _export)
    list(GET _pair 1 _thunk)
    if(NOT _export STREQUAL _thunk)
        message(FATAL_ERROR "d3d11.def: ${_export} is aliased to mvr_fwd_${_thunk}")
    endif()
    list(APPEND _def_names "${_export}")
endforeach()
list(SORT _inc_names)
list(SORT _def_names)
if(NOT _inc_names STREQUAL _def_names)
    message(FATAL_ERROR "src/loader/d3d11.def and src/loader/d3d11_exports.inc list different pass-through "
                        "exports.\n.inc: ${_inc_names}\n.def: ${_def_names}")
endif()

# Build settings every project target shares (STYLEGUIDE 1): warnings,
# conformance, UTF-8 sources, the Windows header switches, and /WX with
# MOTHERVR_WERROR. Linked PRIVATE so they do not leak to consumers.
add_library(omvr_options INTERFACE)
target_include_directories(omvr_options INTERFACE src)
if(MSVC)
    target_compile_options(omvr_options INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<BOOL:${MOTHERVR_WERROR}>>:/WX>)
    target_compile_definitions(omvr_options INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    # Release builds keep debug info in the objects (/Z7), so parallel
    # compiles share no PDB and the DLL's d3d11.pdb covers the libraries too.
    target_compile_options(omvr_options INTERFACE "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Z7>")
endif()

# The layers below the composition root that need neither the game nor
# Windows (STYLEGUIDE 2.1) build as static libraries, so the unit tests
# link them without the DLL: util (standard library only) and core (logic
# behind its ports).
add_library(omvr_util STATIC)
add_library(omvr_core STATIC)
target_link_libraries(omvr_util PRIVATE omvr_options)
target_link_libraries(omvr_core PUBLIC omvr_util nlohmann_json::nlohmann_json PRIVATE omvr_options)

# The DLL: the platform adapters, the game interface and the loader. The
# forwarders and the exported entry are compiled into it (not a library),
# so nothing the .def names can be dropped by the linker.
add_library(mothervr SHARED "${CMAKE_CURRENT_BINARY_DIR}/version.rc")

# Each source directory lists its own files (src/**/sources.cmake), in the
# order of the layers: util, platform, game, core, loader. util and core
# list theirs into omvr_util and omvr_core, the rest into mothervr.
set(MOTHERVR_SOURCE_DIRS
    util/base util/binary util/hash util/math util/sync util/text
    platform/win32 platform/win32/input platform/win32/patching platform/win32/window
    platform/d3d11 platform/d3d11/render platform/openvr platform/openxr platform/audio platform/discord
    game game/sites game/stereo game/input game/ui game/audio game/hands
    core/diag core/diag/ports core/config core/config/ports core/ui core/ui/ports
    core/vr core/vr/ports core/input core/input/ports core/stereo core/stereo/ports core/render core/hands
    loader loader/hooks
)
foreach(_dir IN LISTS MOTHERVR_SOURCE_DIRS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/src/${_dir}/sources.cmake")
endforeach()

# resources.rc embeds these as BINARY resources. Ninja and Makefile builds
# track only the files an .rc #includes, so without this a regenerated
# payload would not rebuild resources.res and the DLL would keep the old one.
set_source_files_properties(src/loader/resources.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/res/openvr_api.dll;${CMAKE_CURRENT_SOURCE_DIR}/res/ui_patch.bin")

if(MSVC)
    # Release builds also write d3d11.pdb, kept with each release (not in
    # the player zip) so a crash offset inside the proxy from the log can be
    # mapped to source. /DEBUG turns off the linker's REF and ICF defaults;
    # turning them back on keeps the shipped code what it would be without
    # symbols. /PDBALTPATH names only the file in the DLL's debug directory,
    # not the build machine's path.
    target_link_options(mothervr PRIVATE "$<$<CONFIG:Release>:/DEBUG;/OPT:REF;/OPT:ICF;/PDBALTPATH:%_PDB%>")
endif()

target_include_directories(mothervr PRIVATE ${OPENVR_INCLUDE_DIR})

target_link_libraries(mothervr PRIVATE
    omvr_options
    omvr_core
    omvr_util
    nlohmann_json::nlohmann_json
    OpenXR::openxr_loader
    minhook::minhook
)

set_target_properties(mothervr PROPERTIES
    OUTPUT_NAME "d3d11"
    PREFIX ""
    SUFFIX ".dll"
)

# Unit tests of util and core (tests/): a separate executable that never
# links into d3d11.dll, run with ctest.
option(MOTHERVR_BUILD_TESTS "Build the unit tests (tests/)" ON)
if(MOTHERVR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
