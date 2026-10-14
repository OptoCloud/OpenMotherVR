#pragma once

// OpenXR with the Win32 + D3D11 platform bindings. Include this instead of
// <openxr/openxr.h> so every translation unit sees the same configuration.
//
// Only D3D11 *interfaces* are used (methods via vtables). Never link
// d3d11.lib: this DLL is itself named d3d11.dll and would import from itself.

#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace mothervr::runtime::openxr {

struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };

struct Pose {
    Quat orientation{};
    Vec3 position{};
};

inline Pose from_xr(const XrPosef& p) noexcept {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

inline XrPosef to_xr(const Pose& p) noexcept {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

} // namespace mothervr::runtime::openxr
