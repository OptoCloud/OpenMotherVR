#pragma once

#include <cstddef>
#include <cstdint>

// Values that cross the VR runtime port.
namespace omvr::core::vr {

// Two eyes and two hands; index 0 is the left one throughout.
inline constexpr std::size_t kEyeCount  = 2;
inline constexpr std::size_t kHandCount = 2;

// Opaque GPU objects of the game's renderer (D3D11 interface pointers on
// the platform side, see platform/d3d11/handles.h).
enum class GpuDevice : std::uintptr_t {};
enum class GpuContext : std::uintptr_t {};
enum class GpuTexture : std::uintptr_t {};

// A per-eye render size in pixels.
struct TextureSize {
    std::int32_t w, h;
};

// Tangents of the half-angles of a field of view (up, down, left, right),
// all positive for a view that contains its axis (03 §3.1).
struct FovTangents {
    float up, down, left, right;
};

// The half-angles of a field of view in radians, in the OpenXR sign
// convention: left and down negative (09 §4.1).
struct FovAngles {
    float left, right, up, down;
};

// A region of a texture in normalised coordinates, v = 0 at the top
// (03 §3.3).
struct TextureBounds {
    float u_min, v_min, u_max, v_max;
};

// What the runtime says about the headset: each eye's real FOV, the
// symmetric FOV the game renders, and names for the log. The names point
// into storage the runtime keeps for the session.
struct HeadsetDesc {
    const char* product_name = nullptr;
    const char* manufacturer = nullptr;
    FovTangents default_eye_fov[kEyeCount]{};
    FovTangents max_eye_fov[kEyeCount]{};
};

} // namespace omvr::core::vr
