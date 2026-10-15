#pragma once

#include "core/input/action_tiers.h"
#include "game/constants.h"

#include <algorithm>
#include <cstdint>

// Rebinding the game's action maps to virtual pad codes. Templates over the
// map types, so they need only the field names of the game's layout.
namespace omvr::core::input {

using game::kMaxBindings;

// Binds every action of `map` to a virtual code on the pad: the first pad
// binding is rewritten (its pad code kept in the tier), extra pad bindings
// are disabled, and actions without one get one appended. Already-virtual
// bindings are left alone, so this is idempotent.
//
// From: FUN_10003200 (thiscall on the tier, ret 4).
template <class Action>
void remap_actions(ActionTier& t, Action* actions, int count, int index_offset,
                   bool clear_flag_40, std::uint8_t appended_flags) noexcept {
    for (int i = 0; i < count; ++i) {
        Action& act = actions[i];
        const int n = index_offset + i;
        const auto virtual_code = static_cast<std::int16_t>(t.base + n);
        bool found = false;
        for (int b = 0; b < act.count && b < kMaxBindings; ++b) {
            auto& bind = act.bindings[b];
            if (bind.device != 0) continue;
            if (found) {
                bind.code = -1;
            } else {
                found = true;
                if (bind.code < 0x100) {   // signed: -1 counts as not yet virtual
                    t.original_code[n] = bind.code;
                    bind.code = virtual_code;
                }
            }
            if (clear_flag_40) bind.flags &= ~0x40;
        }
        if (!found) {
            // "None" whatever the record count: a full action (six records,
            // none for the pad) must not keep a pad control remembered from
            // before the game rebuilt its bindings (spec 04 section 3.2
            // item 3).
            t.original_code[n] = -1;
            if (act.count < kMaxBindings) { // the original does not bounds-check
                auto& bind = act.bindings[act.count++];
                bind.code   = virtual_code;
                bind.device = 0;
                bind.flags  = appended_flags;
            }
        }
    }
}

template <class Map>
void remap(ActionTier& t, Map& map) noexcept {
    t.num_digital = std::clamp(map.num_digital, 0, 0x29);
    remap_actions(t, map.digital, t.num_digital, 0, true, 0);

    t.num_analog = std::clamp(map.num_analog, 0, 0x10);
    remap_actions(t, map.analog, t.num_analog, t.num_digital, false, 1);

    std::fill(t.original_code.begin() + t.num_digital + t.num_analog,
              t.original_code.end(), std::int16_t{-1});

    // The codes are dwords. The original writes a 16-bit -1 into each,
    // which a 32-bit reader sees as 0x0000FFFF, neither "no control" nor a
    // pad code; -1 in the whole dword holds whichever width the game reads
    // (spec 04 section 3.2 item 4).
    const int composites = std::clamp(map.composite_count, 0, 12);
    for (int c = 0; c < composites; ++c) {
        auto& comp = map.composite[c];
        if (comp.keep) continue;
        for (int i = 0; i < comp.count && i < 7; ++i) {
            comp.codes[i] = 0xFFFFFFFFu;
        }
    }
}

} // namespace omvr::core::input
