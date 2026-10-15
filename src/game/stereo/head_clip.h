#pragma once

// The game's head-in-wall fade (08 §4.1). The delay the stereo device puts
// on it is core/stereo/head_clip_delay.h.
namespace omvr::game::stereo {

// The game's HeadClipFade, found by the installer and set once at attach
// before its call is redirected.
struct HeadClipTargets {
    void* fade; // float __stdcall (const vec3* eye, const vec3* pivot)
};

// float HeadClipFade(const vec3* eye, const vec3* pivot), the game's own:
// __stdcall, result in st(0). Game thread (camera update).
float call_head_clip_fade(const HeadClipTargets& targets, const float* eye, const float* pivot) noexcept;

} // namespace omvr::game::stereo
