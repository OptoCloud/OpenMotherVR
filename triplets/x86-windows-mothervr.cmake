# OpenMotherVR-Provenance: clean
# 32-bit, static libraries, static CRT: matches the DLL's /MT so compiled
# dependencies (openxr-loader, jsoncpp) link in without CRT mismatches and
# the deployed d3d11.dll needs no runtime DLLs.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# The openvr port only builds as a DLL (dynamic CRT). We use its headers only
# and load openvr_api.dll at runtime, so its linkage doesn't affect us.
if(PORT STREQUAL "openvr")
    set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
