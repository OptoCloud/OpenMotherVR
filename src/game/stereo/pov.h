#pragma once

#include "game/layout.h"

#include <cstddef>
#include <cstdint>

// The game's points of view (POV): the main view and the reflection view.
namespace omvr::game::stereo {

// POV fields.
inline constexpr std::ptrdiff_t kPovView     = 0x000;  // float[16]
inline constexpr std::ptrdiff_t kPovProj     = 0x040;  // float[16]
inline constexpr std::ptrdiff_t kPovProjCopy = 0x080;  // float[16], used for shader constants
inline constexpr std::ptrdiff_t kPovFov      = 0x0E0;  // vertical FOV, radians
inline constexpr std::ptrdiff_t kPovAspect   = 0x0E4;
inline constexpr std::ptrdiff_t kPovFlags    = 0x36B;  // bit 0 active, bit 4 renders
inline constexpr std::ptrdiff_t kPovEye      = 0x378;  // -1 none; selects the viewport half

struct ReflectionTargets {
    void*  render;               // ReflectionPOV render
    void*  apply_eye_transform;  // Stereo::ApplyEyeTransform
    void*  update_frustum;       // POV frustum update
    void** renderer_eye_owner;   // its +0x2C is the renderer's current eye
    std::uint8_t* clear;         // the game's clear of the bound target
};

// The main view's world-to-view matrix (row vectors, left-handed, metres):
// columns 0..2 are the camera's right, up and forward axes in world space.
// Null before the game has a POV manager.
const float* view_matrix() noexcept;

// Eye `eye`'s transform on the POV's centre view and projection, and the
// derived fields: what POV::SetEye does, without its lighting and
// reflection-plane side effects. `manager` is the stereo manager the game
// applies the eye transform with.
void setup_eye(const ReflectionTargets& g_reflection, StereoManager* manager, std::uint8_t* pov, int eye) noexcept;

} // namespace omvr::game::stereo
