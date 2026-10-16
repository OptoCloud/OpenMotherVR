// OpenMotherVR-Provenance: clean
#pragma once

#include <cstdint>

// The stereo device query the game's particle lighting lookup makes
// (dumps/13_particle_lighting.md §4). The correction is
// core/render/alpha_light.h.
namespace omvr::game::stereo {

// The game's Stereo::EyeNdc wrapper, found by the installer and set once
// at attach before its call is redirected.
struct AlphaLightTargets {
    void* eye_ndc; // float* __thiscall (manager, float out[4], int eye), ret 8
};

// float* Stereo::EyeNdc(float out[4], int eye), the game's own: writes
// (0, 0, 1, 1), or the device's slot 14 answer for `eye` while the device
// runs; returns `out`. Render thread.
float* call_eye_ndc(const AlphaLightTargets& targets, void* manager, float* out, std::int32_t eye) noexcept;

} // namespace omvr::game::stereo
