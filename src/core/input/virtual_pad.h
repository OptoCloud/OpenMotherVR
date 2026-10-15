#pragma once

#include "core/input/action_tiers.h"
#include "core/input/glyphs.h"
#include "game/input/input_manager.h" // R: input (A5): the thunks read the game's pad into value structs

#include <atomic>
#include <cstdint>

namespace omvr::core::input {

// The virtualised gamepad: the VR state of every action, the merge of the
// game's pad with it behind the five replaced pad methods, and the remap of
// the game's action maps (see action_tiers.h).
//
// From: FUN_10008d50 (IsDown), FUN_10008f00 (GetValue), FUN_10008e20
// (GetAxis), FUN_10009030 (ControlToName), FUN_10008fa0 (HasActivity),
// FUN_100031b0 / FUN_10003200 (action-map remap).
class VirtualPad {
public:
    VirtualPad(game::input::InputManager& manager, Glyphs& glyphs) noexcept
        : manager_(manager), glyphs_(glyphs) {}

    // The three tiers the runtimes write.
    ActionTiers& tiers() noexcept { return g_tiers; }

    // Set by the runtime whenever it reports a pressed digital action or an
    // analog value above 0.85. Consumed by the pad's HasActivity hook, which
    // makes the pad the active device and switches the button glyphs to VR.
    void note_vr_activity() noexcept { g_vr_activity.store(true); }

    // The mod's UI archive (MOTHER.PAK) is not in use this run, so the movies
    // register no VR glyph ids: prompts stay in stock-glyph mode for the rest
    // of the process (spec 04 section 4.2). Call once, from attach, when the
    // archive redirect was not made.
    void use_stock_glyphs_only() noexcept;

    // The game's five pad methods, read before their slots are replaced.
    // The installer fills it before the pad's slots point at the thunks.
    game::input::StockPad& stock_pad() noexcept { return g_game; }
    // All five pad slots were replaced. The helpers for the stereo update
    // answer from the stock pad alone when they were not.
    void mark_installed() noexcept { g_installed.store(true); }

    // For the stereo update (FUN_1001f8a0), on the game's active pad device:
    // a real pad button (pad code, no VR), the HasActivity hook, and the VR-
    // merged value of a virtual analog code (e.g. 0x12C, the player map's
    // turn).
    bool  pad_button(void* pad, int pad_code) const noexcept;
    bool  pad_activity(void* pad) noexcept;
    float pad_axis_value(void* pad, int virtual_code) noexcept;
    int   analog_virtual_code(int map, int index) const noexcept;
    bool  move_on_left_stick() const noexcept;

    // The replaced pad methods (see the thunks in loader/hooks/pad_hooks.cpp).
    bool        is_down(void* self, int code) noexcept;
    float       value(void* self, int code) noexcept;
    float       axis(void* self, int code, int negate) noexcept;
    const char* control_name(void* self, std::uint32_t raw) noexcept;
    bool        has_activity(void* self) noexcept;

private:
    void remap_all() noexcept;

    game::input::InputManager& manager_;
    Glyphs&                    glyphs_;
    game::input::StockPad      g_game{};
    ActionTiers                g_tiers = make_tiers();
    // Original: DAT_10066bc0 (initially 1), DAT_10068639, DAT_1006af7c.
    bool                       g_vr_glyphs = true;
    std::atomic<bool>          g_vr_activity{false};
    bool                       g_device_pulse = false;
    // False when the mod's UI archive is not in use this run: the stock
    // movies register no VR glyph ids, so a VR name would show no button at
    // all (spec 04 section 4.2). Set at attach, read on the game thread.
    std::atomic<bool>          g_vr_glyphs_allowed{true};
    std::atomic<bool>          g_installed{false};
    bool                       s_remap_logged = false;
};

} // namespace omvr::core::input
