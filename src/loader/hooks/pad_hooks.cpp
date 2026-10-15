// The game pad's replaced methods (all __thiscall via __fastcall with an
// unused EDX). From: FUN_10008d50 (IsDown), FUN_10008f00 (GetValue),
// FUN_10008e20 (GetAxis), FUN_10009030 (ControlToName), FUN_10008fa0
// (HasActivity). Analysis: dumps/06d_vr_input.md.

#include "loader/hooks/pad_hooks.h"

#include "game/input/input_manager.h"
#include "loader/context.h"
#include "loader/hooks/stereo_update_hooks.h"

#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// bool IsDown(int code). From: FUN_10008d50 (ret 4).
bool __fastcall pad_is_down(void* self, void* /*edx*/, int code) noexcept { return context().pad.is_down(self, code); }

// float GetValue(int code). From: FUN_10008f00 (ret 4).
float __fastcall pad_value(void* self, void* /*edx*/, int code) noexcept { return context().pad.value(self, code); }

// float GetAxis(int code, bool negate). From: FUN_10008e20 (ret 8).
float __fastcall pad_axis(void* self, void* /*edx*/, int code, int negate) noexcept {
    return context().pad.axis(self, code, negate);
}

// const char* ControlToName(uint16 code). From: FUN_10009030 (ret 4).
const char* __fastcall pad_control_name(void* self, void* /*edx*/, std::uint32_t raw) noexcept {
    return context().pad.control_name(self, raw);
}

// bool HasActivity(). From: FUN_10008fa0 (ret 0).
bool __fastcall pad_has_activity(void* self, void* /*edx*/) noexcept { return context().pad.has_activity(self); }

static_assert(std::is_convertible_v<decltype(&pad_is_down), game::input::IsDownFn>);
static_assert(std::is_convertible_v<decltype(&pad_value), game::input::ValueFn>);
static_assert(std::is_convertible_v<decltype(&pad_axis), game::input::AxisFn>);
static_assert(std::is_convertible_v<decltype(&pad_control_name), game::input::NameFn>);
static_assert(std::is_convertible_v<decltype(&pad_has_activity), game::input::ActivityFn>);

} // namespace

game::sites::PadReplacements pad_replacements() noexcept {
    return {
        reinterpret_cast<const void*>(&pad_is_down),
        reinterpret_cast<const void*>(&pad_value),
        reinterpret_cast<const void*>(&pad_axis),
        reinterpret_cast<const void*>(&pad_control_name),
        reinterpret_cast<const void*>(&pad_has_activity),
        reinterpret_cast<const void*>(&is_move_on_left_stick),
    };
}

} // namespace omvr::loader::hooks
