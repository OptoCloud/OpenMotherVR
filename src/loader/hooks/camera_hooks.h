// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"

// Interaction.CameraTakeovers' hooks (spec 23 §4): the game's scripted
// turns of the first-person camera, kept or skipped as core::ui's rule
// says. Their sites are checked and written only once the key asks for
// "Reduced" (with "Game", the default, nothing is patched: 23 §4.3).
namespace omvr::loader::hooks {

// At attach (full patch set): installs the sites if the key asks for them
// now. Game thread, before the game's threads run.
void prepare_camera_takeovers(const game::GameImage& g) noexcept;

// Each stereo update, on the game thread: follows the interaction in
// progress for its rule and installs the sites once the key changed in the
// menu asks for them.
void camera_takeovers_on_stereo_update() noexcept;

} // namespace omvr::loader::hooks
