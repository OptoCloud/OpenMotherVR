#pragma once

#include "core/input/action_tiers.h"
#include "game/constants.h"
#include "core/input/tuning.h"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <type_traits>

// Rebinding the game's action maps to virtual pad codes (04 §3.2).
// Templates over the map types, constrained by the field names of the
// game's layout (04 §2.4), so core needs no game header and tests can use
// fake structs of the same shape.
namespace omvr::core::input {

using game::kMaxBindings;

// A binding record: a 16-bit control code, a device byte and a flags byte.
template <class B>
concept PadBinding = requires(B b) {
    { b.code } -> std::convertible_to<std::int16_t>;
    { b.device } -> std::convertible_to<std::uint8_t>;
    { b.flags } -> std::convertible_to<std::uint8_t>;
};

// An action entry: its binding records and how many are in use.
template <class A>
concept BoundAction = requires(A a) {
    requires PadBinding<std::remove_cvref_t<decltype(a.bindings[0])>>;
    { a.count } -> std::convertible_to<int>;
};

// A composite entry: control code dwords, their count and the keep byte.
template <class C>
concept CompositeEntry = requires(C c) {
    { c.codes[0] } -> std::convertible_to<std::uint32_t>;
    { c.count } -> std::convertible_to<int>;
    { c.keep } -> std::convertible_to<bool>;
};

// An action map: digital and analog actions with their counts in use, and
// the composite list.
template <class M>
concept ActionMap = requires(M m) {
    requires BoundAction<std::remove_cvref_t<decltype(m.digital[0])>>;
    requires BoundAction<std::remove_cvref_t<decltype(m.analog[0])>>;
    requires CompositeEntry<std::remove_cvref_t<decltype(m.composite[0])>>;
    { m.num_digital } -> std::convertible_to<int>;
    { m.num_analog } -> std::convertible_to<int>;
    { m.composite_count } -> std::convertible_to<int>;
};

// How one kind of action (digital or analog) is virtualised.
struct RemapKind {
    bool clear_composite_flag;   // 04 §3.2 item 5
    std::uint8_t appended_flags; // 04 §3.2 item 3
};
inline constexpr RemapKind kRemapDigital{true, kAppendedFlagsDigital};
inline constexpr RemapKind kRemapAnalog{false, kAppendedFlagsAnalog};

// "No control" as a binding record's 16-bit code.
inline constexpr auto kNoPadControl16 = static_cast<std::int16_t>(game::kNoPadControl);

// Binds one action to its virtual code: the first pad binding is rewritten
// (its pad code kept as the stock code), extra pad bindings are disabled.
// Returns false when the action has no pad binding among its first
// min(count, 6) records.
template <BoundAction Action>
bool virtualise_pad_bindings(ActionTier& t, Action& act, int n, std::int16_t virtual_code,
                             RemapKind kind) noexcept {
    bool found = false;
    for (int b = 0; b < act.count && b < kMaxBindings; ++b) {
        auto& bind = act.bindings[b];
        if (bind.device != game::kBindingDevicePad) continue;
        if (found) {
            bind.code = kNoPadControl16;
        } else {
            found = true;
            if (!is_virtual_code(bind.code)) { // signed: -1 counts as not yet virtual
                t.stock_code[static_cast<std::size_t>(n)] = bind.code;
                bind.code                                 = virtual_code;
            }
        }
        if (kind.clear_composite_flag)
            bind.flags = static_cast<std::uint8_t>(bind.flags & ~game::kBindingCompositeRef);
    }
    return found;
}

// An action without a pad binding: "none" whatever the record count, so a
// full action (six records, none for the pad) does not keep a pad control
// remembered from before the game rebuilt its bindings (spec 04 section 3.2
// item 3); a record carrying the virtual code is appended where one fits.
template <BoundAction Action>
void append_pad_binding(ActionTier& t, Action& act, int n, std::int16_t virtual_code,
                        RemapKind kind) noexcept {
    t.stock_code[static_cast<std::size_t>(n)] = kNoPadControl16;
    if (act.count >= kMaxBindings) return; // a seventh record would overwrite the entry (04 §2.4)
    auto& bind  = act.bindings[act.count++];
    bind.code   = virtual_code;
    bind.device = game::kBindingDevicePad;
    bind.flags  = kind.appended_flags;
}

// Binds every action of `actions` to a virtual code on the pad. Already-
// virtual bindings are left alone, so this is idempotent.
//
// From: FUN_10003200 (thiscall on the tier, ret 4).
template <BoundAction Action>
void remap_actions(ActionTier& t, Action* actions, int count, int index_offset, RemapKind kind) noexcept {
    for (int i = 0; i < count; ++i) {
        const int n             = index_offset + i;
        const auto virtual_code = static_cast<std::int16_t>(t.base + n);
        if (!virtualise_pad_bindings(t, actions[i], n, virtual_code, kind)) {
            append_pad_binding(t, actions[i], n, virtual_code, kind);
        }
    }
}

// The composite entries that would deliver raw pad codes deliver none. The
// codes are dwords; -1 in the whole dword holds whichever width the game
// reads (spec 04 section 3.2 item 4).
template <ActionMap Map>
void blank_composites(Map& map) noexcept {
    const int composites = std::clamp(map.composite_count, 0, game::kMaxComposites);
    for (int c = 0; c < composites; ++c) {
        auto& comp = map.composite[c];
        if (comp.keep) continue;
        for (int i = 0; i < comp.count && i < game::kCompositeCodes; ++i)
            comp.codes[i] = game::kNoCompositeCode;
    }
}

// One virtualisation pass over one action map (04 §3.2). Game thread.
template <ActionMap Map>
void remap(ActionTier& t, Map& map) noexcept {
    t.num_digital = std::clamp(map.num_digital, 0, game::kMaxDigitalActions);
    remap_actions(t, map.digital, t.num_digital, 0, kRemapDigital);

    t.num_analog = std::clamp(map.num_analog, 0, game::kMaxAnalogActions);
    remap_actions(t, map.analog, t.num_analog, t.num_digital, kRemapAnalog);

    // 04 §3.2 item 7: actions beyond the counts in use remember "none".
    std::fill(t.stock_code.begin() + t.num_digital + t.num_analog, t.stock_code.end(), kNoPadControl16);

    blank_composites(map);
}

} // namespace omvr::core::input
