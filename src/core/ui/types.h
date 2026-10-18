// OpenMotherVR-Provenance: clean
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Values the HUD options (spec 23 §7, 22 §8.3) share between the game's
// accessors and the mod's logic.
namespace omvr::core::ui {

// The two HUD movies the player's "HUD hidden" state covers (22 §8.3):
// `weaponstuff` (health, ammunition, crosshair, prompts) and
// `pickupOverlay` (pickup and interaction prompts, tutorial messages).
enum class HudMovie : std::uint8_t { Weapon = 0, Pickup = 1 };
inline constexpr std::size_t kHudMovieCount = 2;

// One HUD object as the game update finds it.
struct HudMovieState {
    bool exists        = false; // the object exists (pickupOverlay: and its movie is loaded)
    bool forced_hidden = false; // the game's "HUD forced hidden" byte (+0x40, 22 §2.4)
};

using HudMovieStates = std::array<HudMovieState, kHudMovieCount>;

// What the mod does to one HUD object in an update.
enum class HudAction : std::uint8_t { None, Hide, Show };

} // namespace omvr::core::ui
