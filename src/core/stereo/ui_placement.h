#pragma once

#include "core/stereo/types.h"

#include <optional>

// Where the game's UI quads go in stereo (05 §4). Pure: the thunks read the
// main view from the game and copy the quad back into the game's matrix.
namespace omvr::core::stereo {

// The menu screen (mode 1, 05 §4.2): level, facing where the camera looks,
// kMenuScreenDistance ahead at eye height. The game's own placement follows
// the full head orientation. `view` is the main view's world-to-view matrix,
// nullopt before the game has one (the quad is then the identity).
// From: FUN_1001fc20 (yaw from the view through a quaternion; same result).
Matrix44 virtual_screen_quad(const std::optional<Matrix44>& view, float inv_aspect) noexcept;

// The HUD in 3D (mode 0, 05 §4.3): a panel kHudPanelWidth wide, one unit in
// front of the eyes, locked to the head. From: FUN_1001ffd0.
Matrix44 hud_panel_quad(const std::optional<Matrix44>& view, float inv_aspect) noexcept;

} // namespace omvr::core::stereo
