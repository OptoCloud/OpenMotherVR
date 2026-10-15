// The game pad's replaced methods (all __thiscall via __fastcall with an
// unused EDX). From: FUN_10008d50 (IsDown), FUN_10008f00 (GetValue),
// FUN_10008e20 (GetAxis), FUN_10009030 (ControlToName), FUN_10008fa0
// (HasActivity). Analysis: dumps/06d_vr_input.md.
//
// Each thunk reads what it needs from the game (the stock method's answer,
// the action maps, map 0's devices), asks core::input::VirtualPad for the
// result and writes back what it asks for. Game thread.

#include "loader/hooks/pad_hooks.h"

#include "core/input/virtual_pad.h"
#include "game/code_address.h"
#include "game/input/input_manager.h"
#include "loader/context.h"
#include "loader/hooks/stereo_update_hooks.h"

#include <cstdint>
#include <optional>
#include <type_traits>

namespace omvr::loader::hooks {

namespace {

using core::input::VirtualPad;

// Virtualises the game's action maps, once the game has created them.
void remap_action_maps(Context& ctx) noexcept {
    if (const auto maps = ctx.input_manager.action_maps()) ctx.pad.remap(*maps);
}

core::input::PlayerDevices presence(const std::optional<game::input::PlayerDevices>& devices) noexcept {
    if (!devices) return core::input::PlayerDevices::NoManager;
    return devices->pad && devices->keyboard ? core::input::PlayerDevices::Both
                                             : core::input::PlayerDevices::Missing;
}

// Makes the game's device selection redraw the prompts (04 §4.2). A null
// device is never written through.
void apply_device_switch(core::input::DeviceSwitch change,
                         const std::optional<game::input::PlayerDevices>& devices) noexcept {
    if (!devices) return;
    switch (change) {
        case core::input::DeviceSwitch::None: return;
        case core::input::DeviceSwitch::PulseKeyboard:
            // Only asked for when both exist.
            game::input::set_device_active(devices->pad, false);
            game::input::set_device_active(devices->keyboard, true);
            return;
        case core::input::DeviceSwitch::RestorePad:
            if (devices->pad) game::input::set_device_active(devices->pad, true);
            if (devices->keyboard) game::input::set_device_active(devices->keyboard, false);
            return;
    }
}

// HasActivity with VR merged in, and the prompt switch it drives.
bool merged_activity(Context& ctx, void* self) noexcept {
    const bool stock   = ctx.targets.stock_pad.activity(self, nullptr);
    const auto devices = ctx.input_manager.player_devices();
    const auto result  = ctx.pad.on_activity(stock, presence(devices));
    apply_device_switch(result.devices, devices);
    return result.any;
}

// GetAxis with VR merged in.
float merged_axis(Context& ctx, void* self, int code, int negate) noexcept {
    if (!core::input::is_virtual_code(code)) return 0.0f;
    const float stock = ctx.targets.stock_pad.axis(self, nullptr, ctx.pad.stock_code(code), negate);
    return ctx.pad.merge_axis(code, stock);
}

// bool IsDown(int code). A raw pad code means the maps were (re)loaded and
// need virtualising; the game then sees nothing for it.
// From: FUN_10008d50 (ret 4).
bool __fastcall pad_is_down(void* self, void* /*edx*/, int code) noexcept {
    auto& ctx = context();
    if (!core::input::is_virtual_code(code)) {
        if (VirtualPad::asks_for_remap(code)) remap_action_maps(ctx);
        return false;
    }
    const int stock = ctx.pad.stock_code(code);
    const bool stock_down =
        stock != game::kNoPadControl && ctx.targets.stock_pad.is_down(self, nullptr, stock);
    return ctx.pad.merge_down(code, stock_down);
}

// float GetValue(int code). From: FUN_10008f00 (ret 4).
float __fastcall pad_value(void* self, void* /*edx*/, int code) noexcept {
    auto& ctx = context();
    if (!core::input::is_virtual_code(code)) return 0.0f;
    const float stock = ctx.targets.stock_pad.value(self, nullptr, ctx.pad.stock_code(code));
    return ctx.pad.merge_value(code, stock);
}

// float GetAxis(int code, bool negate). From: FUN_10008e20 (ret 8).
float __fastcall pad_axis(void* self, void* /*edx*/, int code, int negate) noexcept {
    return merged_axis(context(), self, code, negate);
}

// const char* ControlToName(uint16 code): the glyph id for button prompts.
// Only the low 16 bits are meaningful (04 §2.3); the compare is on the
// signed 16-bit code. From: FUN_10009030 (ret 4).
const char* __fastcall pad_control_name(void* self, void* /*edx*/, std::uint32_t raw) noexcept {
    auto& ctx       = context();
    const auto code = static_cast<std::int16_t>(raw);
    if (!core::input::is_virtual_code(code)) return nullptr;
    const int stock = ctx.pad.stock_code(code);
    if (!ctx.pad.shows_vr_glyphs())
        return ctx.targets.stock_pad.name(self, nullptr, static_cast<std::uint16_t>(stock));
    return ctx.pad.vr_glyph(stock);
}

// bool HasActivity(): the game uses it to pick the active input device.
// From: FUN_10008fa0 (ret 0).
bool __fastcall pad_has_activity(void* self, void* /*edx*/) noexcept {
    return merged_activity(context(), self);
}

static_assert(std::is_convertible_v<decltype(&pad_is_down), game::input::IsDownFn>);
static_assert(std::is_convertible_v<decltype(&pad_value), game::input::ValueFn>);
static_assert(std::is_convertible_v<decltype(&pad_axis), game::input::AxisFn>);
static_assert(std::is_convertible_v<decltype(&pad_control_name), game::input::NameFn>);
static_assert(std::is_convertible_v<decltype(&pad_has_activity), game::input::ActivityFn>);

} // namespace

game::sites::PadReplacements pad_replacements() noexcept {
    return {
        game::code_pointer(&pad_is_down),      game::code_pointer(&pad_value),
        game::code_pointer(&pad_axis),         game::code_pointer(&pad_control_name),
        game::code_pointer(&pad_has_activity), game::code_pointer(&is_move_on_left_stick),
    };
}

bool stock_pad_button(void* pad, int pad_code) noexcept {
    const auto& stock = context().targets.stock_pad;
    return stock.is_down && stock.is_down(pad, nullptr, pad_code);
}

// Without the replacements in place, VR has no effect on the pad: activity
// is the stock pad's own and there is no merged turn value.
bool merged_pad_activity(void* pad) noexcept {
    auto& ctx = context();
    if (!ctx.targets.stock_pad.activity) return false;
    return ctx.pad.installed() ? merged_activity(ctx, pad) : ctx.targets.stock_pad.activity(pad, nullptr);
}

float merged_pad_axis(void* pad, int virtual_code) noexcept {
    auto& ctx = context();
    return ctx.targets.stock_pad.axis && ctx.pad.installed() ? merged_axis(ctx, pad, virtual_code, 0) : 0.0f;
}

} // namespace omvr::loader::hooks
