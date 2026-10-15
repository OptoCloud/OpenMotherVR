#pragma once

#include "core/vr/ports/vr_runtime.h"
#include "game/layout.h" // R: stereo (A3): the size from value structs, not the game's device

namespace omvr::core::stereo {

// Both eyes side by side: left half, right half.
void set_target_size(game::StereoInterface& si, int width, int height) noexcept;

// GetRenderTargetSize(int* w, int* h): both eyes side by side, and the eye
// viewports.
//
// The original asks for half scale until stereo runs. The game sizes its
// targets from that first answer (the same call picks the display mode) and
// does not rebuild them when stereo starts, so the eyes stay at half
// resolution. We use the configured pixel scale from the start; the
// swapchain is kept windowed so the mode change cannot fail.
void render_target_size(game::StereoInterface& si, vr::ports::VrRuntime& backend, int* w, int* h);

} // namespace omvr::core::stereo
