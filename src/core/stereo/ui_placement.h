#pragma once

namespace omvr::core::stereo {

// The menu screen: level, facing where the camera looks, 1.5 m ahead at eye
// height. The game's own placement follows the full head orientation.
// Writes the 4x4 into `out` and returns it.
// From: FUN_1001fc20 (yaw from the view through a quaternion; same result).
float* place_virtual_screen(float* out, float size, float inv_aspect, float dist) noexcept;

// The HUD in 3D: a panel 0.55 m wide, 1 m in front of the eyes, locked to
// the head. From: FUN_1001ffd0.
float* place_hud_panel(float* out, float size, float inv_aspect, float dist) noexcept;

} // namespace omvr::core::stereo
