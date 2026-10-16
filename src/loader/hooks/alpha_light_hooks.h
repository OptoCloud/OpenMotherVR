// OpenMotherVR-Provenance: clean
#pragma once

#include <cstdint>

// The particle lighting fix (dumps/13_particle_lighting.md): the call of
// Stereo::EyeNdc in the game's lighting atlas lookup.
namespace omvr::loader::hooks {

// Replaces the call of Stereo::EyeNdc at the atlas lookup.
// float* __thiscall (manager, float out[4], int eye), ret 8, returns `out`.
// Render thread.
float* __fastcall alpha_light_eye_ndc(void* manager, void* edx, float* out, std::int32_t eye) noexcept;

} // namespace omvr::loader::hooks
