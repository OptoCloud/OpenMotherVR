// From: the function-local statics behind FUN_10002ef0 / FUN_10002fa0 /
// FUN_10003050 (the tiers), FUN_10003130 (code lookup), FUN_10003100 (the
// left-stick prompt). Analysis: dumps/06d_vr_input.md.

#include "core/input/action_tiers.h"

namespace omvr::core::input {

ActionTiers make_tiers() noexcept {
    ActionTiers t{};
    for (int i = 0; i < 3; ++i) {
        t[i].base = game::kTierBase[i];
        t[i].original_code.fill(-1);
    }
    return t;
}

ActionTier& tier_for(ActionTiers& tiers, int code) noexcept {
    return tiers[code < 0x200 ? 0 : code < 0x400 ? 1 : 2];
}

const ActionTier& tier_for(const ActionTiers& tiers, int code) noexcept {
    return tiers[code < 0x200 ? 0 : code < 0x400 ? 1 : 2];
}

int original_code(const ActionTiers& tiers, int code) noexcept {
    const ActionTier& t = tier_for(tiers, code);
    const int k = code - t.base;
    return k >= 0 && k < static_cast<int>(t.original_code.size()) ? t.original_code[k] : -1;
}

float vr_analog(const ActionTier& t, int code) noexcept {
    const int j = (code - t.base) - t.num_digital;
    return j >= 0 && j < t.num_analog ? t.analog[j] : 0.0f;
}

int analog_virtual_code(const ActionTiers& tiers, int map, int index) noexcept {
    if (map < 0 || map >= static_cast<int>(tiers.size())) return -1;
    const ActionTier& t = tiers[static_cast<std::size_t>(map)];
    return index >= 0 && index < t.num_analog ? t.base + t.num_digital + index : -1;
}

bool move_on_left_stick(const ActionTiers& tiers) noexcept {
    const ActionTier& t = tiers[0];
    // Before the first virtualisation num_digital is 0 and every entry is
    // -1, so this reads "none" and answers true, as the stock check does
    // for an action with no pad binding. From: FUN_10003100.
    const std::int16_t o = t.original_code[static_cast<std::size_t>(t.num_digital)];
    return o == -1 || o == 0x12;   // none, or LSTICK_X
}

} // namespace omvr::core::input
