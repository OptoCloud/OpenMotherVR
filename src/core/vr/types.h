#pragma once

#include <cstdint>

// Values that cross the VR runtime port.
namespace omvr::core::vr {

// Opaque GPU objects of the game's renderer (D3D11 interface pointers on
// the platform side, see platform/d3d11/handles.h).
enum class GpuDevice : std::uintptr_t {};
enum class GpuContext : std::uintptr_t {};
enum class GpuTexture : std::uintptr_t {};

// A per-eye render size in pixels.
struct TextureSize {
    std::int32_t w, h;
};

// Tangents of the half-angles of a field of view (up, down, left, right).
struct FovTangents {
    float up, down, left, right;
};

// What the runtime says about the headset: each eye's real FOV, the
// symmetric FOV the game renders, and names for the log. The names point
// into storage the runtime keeps for the session.
struct HeadsetDesc {
    const char*  product_name = nullptr;
    const char*  manufacturer = nullptr;
    FovTangents  default_eye_fov[2]{};
    FovTangents  max_eye_fov[2]{};
};

} // namespace omvr::core::vr
