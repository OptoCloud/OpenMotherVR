// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/types.h"
#include "game/constants.h"

#include <optional>

// Aiming from the hand (spec 21 §5): which weapons aim from the hand, and
// the aim ray that replaces the game's head ray. Game thread.
namespace omvr::core::hands {

// The class of 21 §5.2 for the current item's weapon type; nullopt is no
// weapon object (throwables, flare, medikit).
WeaponClass weapon_class(std::optional<game::WeaponType> type) noexcept;

// The class aims from the hand: ranged weapons and no weapon; contact
// weapons and unknown types keep the game's ray (21 §5.2).
bool aims_from_hand(WeaponClass c) noexcept;

// From the hand's aim pose to the muzzle or nozzle along the aim
// direction, in game world metres (21 §5.2, §22 item 5).
float muzzle_offset(WeaponClass c) noexcept;

// The aim ray of 21 §5.2 from the hand's world aim pose `aim`: it starts
// `offset` along the aim direction, points along it, and ends at the range
// the game used (the length of the game's ray). `weight` (21 §3.3) blends
// from the game's ray (0) to the hand's (1), so a hand that is lost hands
// the aim back to the head smoothly. The direction is unit length.
AimRay hand_aim_ray(const AimRay& game, const WorldPose& aim, float weight, float offset) noexcept;

// The threshold the fire alignment test compares with (21 §2.5, §5.3):
// while the hand aims, one every length passes; otherwise the game's own.
float fire_gate_threshold(bool hand_aiming, float game_threshold) noexcept;

} // namespace omvr::core::hands
