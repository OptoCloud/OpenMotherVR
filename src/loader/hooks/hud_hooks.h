// OpenMotherVR-Provenance: clean
#pragma once

#include "core/stereo/types.h"
#include "game/game_image.h"

#include <optional>

// The interaction and HUD options' hooks (spec 23 §5 to §7, 22 §8.3). Each
// reads the game into values, asks the mod's logic in core::ui, and writes
// the result back. Their sites are checked and written only once a key
// asks for them (with every key at its default nothing is patched).
namespace omvr::loader::hooks {

// At attach (full patch set): logs each feature's state (23 §10), starts
// the player's HUD state from Hud.Hidden and installs the sites the keys
// ask for now. Game thread, before the game's threads run.
void prepare_hud(const game::GameImage& g) noexcept;

// Each stereo update, on the game thread: installs the sites a key changed
// in the menu now asks for, and applies the player's HUD state. `replace`:
// a snap turn or a recenter, which re-places a Follow HUD at once.
void hud_on_stereo_update(bool replace) noexcept;

// The HUD panel for this frame (05 §4.3, 23 §7.4). The thread that draws
// the overlay.
core::stereo::Matrix44 hud_panel_for_frame(const std::optional<core::stereo::Matrix44>& view,
                                           float inv_aspect) noexcept;

} // namespace omvr::loader::hooks
