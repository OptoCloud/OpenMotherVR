// From: the function-local statics behind FUN_10002ef0 / FUN_10002fa0 /
// FUN_10003050 (the tiers), FUN_10003130 (code lookup), FUN_10003100 (the
// left-stick prompt). Analysis: dumps/06d_vr_input.md.

#include "core/input/action_tiers.h"

namespace omvr::core::input {

namespace {

// The tier whose base is the highest one at or below `code`; codes below
// the first base (raw pad codes) fall in tier 0.
std::size_t tier_index(int code) noexcept {
    for (std::size_t i = kTierCount - 1; i > 0; --i) {
        if (code >= game::kTierBase[i]) return i;
    }
    return 0;
}

// `index` is a valid position in a container of `size` entries.
constexpr bool in_range(int index, int size) noexcept { return index >= 0 && index < size; }

} // namespace

ActionTiers make_tiers() noexcept {
    ActionTiers tiers{};
    for (std::size_t i = 0; i < kTierCount; ++i) {
        tiers[i].base = game::kTierBase[i];
        tiers[i].stock_code.fill(static_cast<std::int16_t>(game::kNoPadControl));
    }
    return tiers;
}

ActionTier& tier_for(ActionTiers& tiers, int code) noexcept { return tiers[tier_index(code)]; }

const ActionTier& tier_for(const ActionTiers& tiers, int code) noexcept { return tiers[tier_index(code)]; }

int stock_code(const ActionTiers& tiers, int code) noexcept {
    const ActionTier& t = tier_for(tiers, code);
    const int k         = code - t.base;
    return in_range(k, static_cast<int>(t.stock_code.size())) ? t.stock_code[static_cast<std::size_t>(k)]
                                                              : game::kNoPadControl;
}

float vr_analog(const ActionTier& t, int code) noexcept {
    const int j = (code - t.base) - t.num_digital;
    return in_range(j, t.num_analog) ? t.analog[static_cast<std::size_t>(j)] : 0.0f;
}

int analog_virtual_code(const ActionTiers& tiers, int map, int index) noexcept {
    if (!in_range(map, static_cast<int>(tiers.size()))) return game::kNoPadControl;
    const ActionTier& t = tiers[static_cast<std::size_t>(map)];
    return in_range(index, t.num_analog) ? t.base + t.num_digital + index : game::kNoPadControl;
}

bool move_on_left_stick(const ActionTiers& tiers) noexcept {
    const ActionTier& t = tiers[game::kPlayerMap];
    // Before the first virtualisation num_digital is 0 and every entry is
    // none, so this reads "none" and answers true, as the stock check does
    // for an action with no pad binding. From: FUN_10003100.
    const int move = t.stock_code[static_cast<std::size_t>(t.num_digital + game::player_action::kMoveX)];
    return move == game::kNoPadControl || move == game::kPadLStickX;
}

} // namespace omvr::core::input
