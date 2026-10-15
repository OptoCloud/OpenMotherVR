// VR input through AI.exe's XInput gamepad.
//
// From: FUN_10008d50 (IsDown), FUN_10008f00 (GetValue), FUN_10008e20
// (GetAxis), FUN_10009030 (ControlToName), FUN_10008fa0 (HasActivity),
// FUN_100031b0 / FUN_10003200 (action-map remap), FUN_10003130 (code
// lookup), FUN_10001090 (glyph names). Analysis: dumps/06d_vr_input.md.

#include "core/input/virtual_pad.h"

#include "core/diag/logger.h"
#include "core/input/remap.h"

#include <cmath>
#include <cstddef>
#include <utility>

namespace omvr::core::input {

using game::input::action_map;
using game::input::kDeviceActive;

// From: FUN_100031b0.
void VirtualPad::remap_all() noexcept {
    std::uint8_t* mgr = manager_.manager();
    if (!mgr) return;
    for (int i = 0; i < 3; ++i) {
        remap(g_tiers[i], action_map(mgr, i));
    }
    if (!std::exchange(s_remap_logged, true)) {
        diag::logf("[input] action maps virtualised: digital/analog {}/{} {}/{} {}/{}",
                   g_tiers[0].num_digital, g_tiers[0].num_analog,
                   g_tiers[1].num_digital, g_tiers[1].num_analog,
                   g_tiers[2].num_digital, g_tiers[2].num_analog);
    }
}

// bool IsDown(int code). A raw pad code means the maps were (re)loaded and
// need virtualising; the game then sees nothing for it.
// From: FUN_10008d50 (ret 4).
bool VirtualPad::is_down(void* self, int code) noexcept {
    if (code < 0x100) {
        if (code != -1) remap_all();
        return false;
    }
    const ActionTier& t = tier_for(g_tiers, code);
    const int o = original_code(g_tiers, code);
    bool down = o != -1 && g_game.is_down(self, nullptr, o);
    const int k = code - t.base;
    if (k >= 0 && k < t.num_digital) down |= t.digital[k] != 0;
    return down;
}

// float GetValue(int code): non-negative, the larger of pad and VR.
// From: FUN_10008f00 (ret 4).
float VirtualPad::value(void* self, int code) noexcept {
    if (code < 0x100) return 0.0f;
    const float game = g_game.value(self, nullptr, original_code(g_tiers, code));
    const float vr   = vr_analog(tier_for(g_tiers, code), code);
    return vr > game ? vr : game;
}

// float GetAxis(int code, bool negate): signed, whichever has the larger
// magnitude. The VR value is not negated: the runtimes write signed values.
// From: FUN_10008e20 (ret 8).
float VirtualPad::axis(void* self, int code, int negate) noexcept {
    if (code < 0x100) return 0.0f;
    const float game = g_game.axis(self, nullptr, original_code(g_tiers, code), negate);
    const float vr   = vr_analog(tier_for(g_tiers, code), code);
    return std::fabs(vr) > std::fabs(game) ? vr : game;
}

// const char* ControlToName(uint16 code): the glyph id for button prompts.
// From: FUN_10009030 (ret 4; the compare is on the signed 16-bit code).
const char* VirtualPad::control_name(void* self, std::uint32_t raw) noexcept {
    const auto code = static_cast<std::int16_t>(raw);
    if (code < 0x100) return nullptr;
    const int o = original_code(g_tiers, code);
    if (!g_vr_glyphs || !g_vr_glyphs_allowed.load(std::memory_order_relaxed)) {
        return g_game.name(self, nullptr, static_cast<std::uint16_t>(o));
    }
    if (o < 0 || static_cast<std::size_t>(o) >= glyphs_.size()) return "VR_MENU"; // original throws past the end
    return glyphs_.name(static_cast<std::size_t>(o));
}

// bool HasActivity(): the game uses it to pick the active input device.
// VR activity counts as pad activity. When the source flips between VR and
// the pad, the pad is marked inactive for one call so the game runs its
// device-changed path and refreshes the button prompts.
// From: FUN_10008fa0 (ret 0).
bool VirtualPad::has_activity(void* self) noexcept {
    const bool game = g_game.activity(self, nullptr);
    const bool vr   = g_vr_activity.exchange(false);
    const bool any  = game || vr;
    // Without the mod's UI archive the prompts stay in stock-glyph mode.
    const bool want_vr = vr && g_vr_glyphs_allowed.load(std::memory_order_relaxed);

    std::uint8_t* mgr = manager_.manager();
    if (!mgr) return any;
    // The player map's pad and keyboard devices (device_by_type[0], [1]).
    // An entry stays null until the game has registered a device of that
    // type, so neither is written through unless it exists; without both,
    // the mode still changes and the prompts pick it up at the game's next
    // device change (spec 04 section 4.2).
    void* const* devices = action_map(mgr, 0).device_by_type;
    auto* const pad      = static_cast<std::uint8_t*>(devices[0]);
    auto* const keyboard = static_cast<std::uint8_t*>(devices[1]);
    if (any && g_vr_glyphs != want_vr) {
        g_vr_glyphs = want_vr;
        if (pad && keyboard) {
            g_device_pulse          = true;
            pad[kDeviceActive]      = 0;
            keyboard[kDeviceActive] = 1;
        }
    } else if (g_device_pulse) {
        g_device_pulse = false;
        if (pad) pad[kDeviceActive] = 1;
        if (keyboard) keyboard[kDeviceActive] = 0;
    }
    return any;
}

void VirtualPad::use_stock_glyphs_only() noexcept {
    if (g_vr_glyphs_allowed.exchange(false)) {
        diag::log_line("[input] no mod UI archive: button prompts keep the game's pad glyphs");
    }
}

bool VirtualPad::pad_button(void* pad, int pad_code) const noexcept {
    return g_game.is_down && g_game.is_down(pad, nullptr, pad_code);
}

// Without the replacements in place, VR has no effect on the pad: activity
// is the stock pad's own and there is no merged turn value.
bool VirtualPad::pad_activity(void* pad) noexcept {
    if (!g_game.activity) return false;
    return g_installed.load() ? has_activity(pad) : g_game.activity(pad, nullptr);
}

float VirtualPad::pad_axis_value(void* pad, int virtual_code) noexcept {
    return g_game.axis && g_installed.load() ? axis(pad, virtual_code, 0) : 0.0f;
}

int VirtualPad::analog_virtual_code(int map, int index) const noexcept {
    return input::analog_virtual_code(g_tiers, map, index);
}

bool VirtualPad::move_on_left_stick() const noexcept { return input::move_on_left_stick(g_tiers); }

} // namespace omvr::core::input
