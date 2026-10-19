# OpenMotherVR-Provenance: clean
# Tracy's client library only, at the version of the profiler the project
# uses (the vcpkg baseline ships 0.13.1; Tracy refuses a connection between
# different protocol versions). The tools come from Tracy's own Windows
# release of the same version: upstream vcpkg builds none for x86.
#
# The options are fixed here, not features: the library is linked only into
# the traced flavour of d3d11.dll (MOTHERVR_TRACY), which loads inside the
# game, so:
# - TRACY_MANUAL_LIFETIME + TRACY_DELAYED_INIT: no profiler threads from a
#   static initialiser, which would run in DllMain under the loader lock;
#   the mod calls StartupProfiler once attach has finished.
# - TRACY_NO_CRASH_HANDLER: the mod's own unhandled-exception filter writes
#   the crash: record to mothervr.log.
# - TRACY_ONLY_LOCALHOST: no port reachable from the network in a game.
# - TRACY_ON_DEMAND: nothing is recorded until a profiler connects.
# TRACY_DELAYED_INIT is not an option upstream; the patch adds it so it is
# exported to consumers with the others (TracyProfiler.hpp reads it).
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO wolfpld/tracy
    REF "v${VERSION}"
    SHA512 53912d7563e595812b37bc55fd40508cfd8e5c42d48d957a73b6b7d18bf1287b3f795c10c9a986bf7b906d5b5bebe13b02216e563e794d0a82b2783e8ce5510b
    HEAD_REF master
    PATCHES
        delayed-init-option.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DTRACY_STATIC=ON
        -DTRACY_ENABLE=ON
        -DTRACY_ON_DEMAND=ON
        -DTRACY_MANUAL_LIFETIME=ON
        -DTRACY_DELAYED_INIT=ON
        -DTRACY_NO_CRASH_HANDLER=ON
        -DTRACY_ONLY_LOCALHOST=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_Git=ON
    MAYBE_UNUSED_VARIABLES
        CMAKE_DISABLE_FIND_PACKAGE_Git
)
vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME Tracy CONFIG_PATH "lib/cmake/Tracy")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
