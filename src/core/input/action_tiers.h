#pragma once

#include "game/constants.h"

#include <array>
#include <cstdint>

// VR input through AI.exe's XInput gamepad.
//
// MotherVR does not add an input device. It virtualises the gamepad: every
// gamepad binding in the game's three action maps is rebound to a virtual
// control code (base + action index), and five methods of the pad's vtable
// are replaced. When the game asks the pad about a virtual code, the hook
// asks the real pad about the code that was bound there originally and
// merges in the VR state the runtime wrote for that action.
//
// Analysis: dumps/06d_vr_input.md.
namespace omvr::core::input {

// Per action map: the VR state the backend writes, and the pad code each
// action was bound to before the remap.
//
// From: the function-local statics behind FUN_10002ef0 / FUN_10002fa0 /
// FUN_10003050 (layout of the original's is in the analysis doc; ours need
// not match).
struct ActionTier {
    std::array<std::uint8_t, 0x2C> digital{};   // VR state of digital action k
    std::array<float, 0x10>        analog{};    // VR value of analog action j
    int base        = 0;
    int num_digital = 0;
    int num_analog  = 0;
    std::array<std::int16_t, 0x39> original_code{}; // [k] / [num_digital + j]; -1 = none
};

// The three tiers: [0] player map, [1] UI map, [2] a third map VR never
// writes (its pad bindings are still virtualised and pass through).
using ActionTiers = std::array<ActionTier, 3>;

// The tiers with their bases and no recorded pad codes.
ActionTiers make_tiers() noexcept;

// The tier a virtual code belongs to.
ActionTier& tier_for(ActionTiers& tiers, int code) noexcept;
const ActionTier& tier_for(const ActionTiers& tiers, int code) noexcept;

// The pad code a virtual code stood for, -1 for none. The original indexes
// with code & 0xFF unchecked; codes we did not create (a stale map, 0x300..)
// would read past the tier. From: FUN_10003130.
int original_code(const ActionTiers& tiers, int code) noexcept;

// Analog VR value for a virtual code, or 0 when the code is a digital
// action. The original indexes without a bounds check.
float vr_analog(const ActionTier& t, int code) noexcept;

// The virtual code of analog action `index` of action map `map`, derived
// from the counts read at the last virtualisation (spec 04 section 3.1: the
// turn is map 0's analog action 3 whatever the digital count is), or -1
// before the maps are virtualised or when the map has no such action.
// Game thread.
int analog_virtual_code(const ActionTiers& tiers, int map, int index) noexcept;

// The stock left-stick prompt check, answered for virtualised bindings:
// true when map 0's analog action 0 (move) was on LSTICK_X, or has no pad
// control at all (before the first virtualisation every action has none,
// and the stock check answers true without a pad binding). Spec 04
// section 4.3. Game thread.
bool move_on_left_stick(const ActionTiers& tiers) noexcept;

} // namespace omvr::core::input
