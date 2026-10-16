// OpenMotherVR-Provenance: clean
#pragma once

#include "game/game_image.h"

// The haptics' hooks (spec 21 §14): the game's rumble output, and the
// health, fire control, tracker scan and hacking events read where they
// happen. Each reads the game into values and hands them to
// core::hands::Haptics; the runtime plays them on the render thread.
namespace omvr::loader::hooks {

// At attach (full patch set): with Hands.Haptics.Enabled on, checks and
// installs the haptics' game sources. With it off nothing is checked or
// written (21 §14.3). Game thread, before the game's threads run.
void prepare_haptics(const game::GameImage& g) noexcept;

// The stereo device handed the game a head pose: the player's health and
// the fire control for this frame. Game thread.
void haptics_on_head_pose() noexcept;

// The script event tap changed (or kept) the interaction state: the start
// and end of hacking and cutting. Game thread.
void haptics_on_interaction() noexcept;

// TrackerScan ran (the hook of hand_hooks.cpp, shared with the tracker in
// hand). The scan's thread.
void haptics_on_tracker_scan() noexcept;

// The haptics' game sources are in place: the runtime is handed the
// haptic output (21 §14.3). Read when the runtime starts.
bool haptics_installed() noexcept;

} // namespace omvr::loader::hooks
