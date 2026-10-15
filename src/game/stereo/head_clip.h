#pragma once

// The game's head-in-wall fade.
namespace omvr::game::stereo {

// ---- Head-in-wall blackout delay (not in the original) ----------------------
//
// HeadClipFade (VRCameraBehavior) returns the scene visibility: below 1 when
// the head is away from the body or inside geometry. Walking through a door
// clips the head for a moment and flashes the view black. In stereo, a fade
// that starts is held at 1 for 0.6 s and only then let through.
// Analysis: dumps/06c_detours_and_patches.md bdc+3.
void install_head_clip_delay(void* head_clip_fade) noexcept;

// float HeadClipFade(const vec3* eye, const vec3* pivot), the game's own.
float call_head_clip_fade(const float* eye, const float* pivot) noexcept;

} // namespace omvr::game::stereo
