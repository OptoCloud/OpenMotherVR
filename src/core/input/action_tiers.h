#pragma once

#include "game/constants.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

// VR input through AI.exe's XInput gamepad (04 §3).
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
    // Written and read across threads only through vr_digital / vr_analog_at
    // and their setters below. Mutable so a reader of a const tier can still
    // make an atomic_ref (which needs a non-const object) for its load.
    mutable std::array<std::uint8_t, game::kMaxDigitalActions> digital{}; // VR state of digital action k
    mutable std::array<float, game::kMaxAnalogActions> analog{};          // VR value of analog action j
    int base        = 0;
    int num_digital = 0;
    int num_analog  = 0;
    // [k] / [num_digital + j]: the pad control action k (j) had before the
    // remap; kNoPadControl = none.
    std::array<std::int16_t, game::kMaxDigitalActions + game::kMaxAnalogActions> stock_code{};
};

// The three tiers: [0] player map, [1] UI map, [2] a third map VR never
// writes (its pad bindings are still virtualised and pass through).
inline constexpr std::size_t kTierCount = game::kActionMapCount;
using ActionTiers                       = std::array<ActionTier, kTierCount>;

// A code the replaced pad methods treat as a virtual code: at or above the
// first tier's base, outside the game's pad codes (04 §3.1).
[[nodiscard]] constexpr bool is_virtual_code(int code) noexcept { return code >= game::kTierBase[0]; }

// The VR state of digital action `k` / analog action `j` of a tier. The
// render thread writes it (ControlMapper) and the game thread reads it (the
// pad methods, the aim and haptic hooks), so every element goes through
// atomic_ref (AGENTS.md: data crossing threads goes through atomics).
// Relaxed: each value stands alone and one frame stale is harmless. The
// game thread's remap changes only the counts and pad codes, never these.
// `k` and `j` are in range (callers check against the counts or use the
// game's fixed action indices).
static_assert(std::atomic_ref<std::uint8_t>::is_always_lock_free);
static_assert(std::atomic_ref<float>::is_always_lock_free);

[[nodiscard]] inline bool vr_digital(const ActionTier& t, std::size_t k) noexcept {
    return std::atomic_ref(t.digital[k]).load(std::memory_order_relaxed) != 0;
}

inline void set_vr_digital(ActionTier& t, std::size_t k, bool down) noexcept {
    std::atomic_ref(t.digital[k]).store(down ? 1 : 0, std::memory_order_relaxed);
}

[[nodiscard]] inline float vr_analog_at(const ActionTier& t, std::size_t j) noexcept {
    return std::atomic_ref(t.analog[j]).load(std::memory_order_relaxed);
}

inline void set_vr_analog(ActionTier& t, std::size_t j, float v) noexcept {
    std::atomic_ref(t.analog[j]).store(v, std::memory_order_relaxed);
}

// The tiers with their bases and no recorded pad codes.
[[nodiscard]] ActionTiers make_tiers() noexcept;

// The tier a virtual code belongs to.
[[nodiscard]] ActionTier& tier_for(ActionTiers& tiers, int code) noexcept;
[[nodiscard]] const ActionTier& tier_for(const ActionTiers& tiers, int code) noexcept;

// The pad code a virtual code stood for, kNoPadControl for none. Bounds
// checked: codes we did not create (a stale map, 0x300..) would otherwise
// read past the tier (04 §3.1). From: FUN_10003130.
[[nodiscard]] int stock_code(const ActionTiers& tiers, int code) noexcept;

// Analog VR value for a virtual code, or 0 when the code is a digital
// action. Bounds checked.
[[nodiscard]] float vr_analog(const ActionTier& t, int code) noexcept;

// The virtual code of analog action `index` of action map `map`, derived
// from the counts read at the last virtualisation (spec 04 section 3.1: the
// turn is map 0's analog action 3 whatever the digital count is), or -1
// before the maps are virtualised or when the map has no such action.
// Game thread.
[[nodiscard]] int analog_virtual_code(const ActionTiers& tiers, int map, int index) noexcept;

// The stock left-stick prompt check, answered for virtualised bindings:
// true when map 0's analog action 0 (move) was on LSTICK_X, or has no pad
// control at all (before the first virtualisation every action has none,
// and the stock check answers true without a pad binding). Spec 04
// section 4.3. Game thread.
[[nodiscard]] bool move_on_left_stick(const ActionTiers& tiers) noexcept;

} // namespace omvr::core::input
