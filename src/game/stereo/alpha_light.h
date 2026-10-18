// OpenMotherVR-Provenance: clean
#pragma once

#include "util/math/matrix.h"
#include "util/math/vec.h"

#include <cstdint>
#include <optional>

// The stereo device query the game's particle lighting lookup makes
// (08 §8.1), and the two cameras the lookup's diagnostic compares (08 §8.5).
// The correction is core/render/alpha_light.h.
namespace omvr::game::stereo {

// The game's Stereo::EyeNdc wrapper, found by the installer and set once
// at attach before its call is redirected; and, for the diagnostic of
// 08 §8.5, the `LightObject` and `CameraConstants` objects (24 §1.1), null
// where their addresses are not known (the diagnostic is then off).
struct AlphaLightTargets {
    void* eye_ndc;                          // float* __thiscall (manager, float out[4], int eye), ret 8
    const void* light_object     = nullptr; // the atlas camera, written once a frame (08 §8.1)
    const void* camera_constants = nullptr; // the drawn POV's camera (24 §14.2)
};

// float* Stereo::EyeNdc(float out[4], int eye), the game's own: writes
// (0, 0, 1, 1), or the device's slot 14 answer for `eye` while the device
// runs; returns `out`. Render thread.
float* call_eye_ndc(const AlphaLightTargets& targets, void* manager, float* out, std::int32_t eye) noexcept;

// The atlas camera's forward axis and position in world space (08 §8.1:
// `LightObject` +0x110 and +0x120), or nothing when its address is unknown.
struct AtlasCamera {
    util::Vec3 forward;
    util::Vec3 position;
};
std::optional<AtlasCamera> read_atlas_camera(const AlphaLightTargets& targets) noexcept;

// The view of the POV being drawn, as its begin-render stored it
// (`CameraConstants` +0x280, 24 §14.2), or nothing when the address is
// unknown. Render thread.
std::optional<util::Matrix44> read_drawn_view(const AlphaLightTargets& targets) noexcept;

} // namespace omvr::game::stereo
