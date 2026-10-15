#pragma once

#include "core/ui/ports/script_variables.h"

#include <cstdint>

namespace omvr::game::ui {

// Signatures of the AI.exe entry points MotherVR calls into. The game's UI
// layer uses the Fire* methods to set script variables by name. All are
// resolved by the hook installer from the per-build AI.exe address table
// (DAT_10066bf0..bfc and DAT_10066c20 in the original).
//
// They are all __thiscall methods: `this` in ECX, arguments on the stack,
// callee pops them. A plain function pointer can't be __thiscall, so they are
// declared __fastcall with an unused EDX parameter, which has the same ABI.
// The original does the same (`xor edx, edx` before every call).

// Registers a view-control item (menu row) with two display strings.
// From: DAT_10066bf0. Used for resolution-picker rows in the VR options menu.
using FireStr2Fn = void(__fastcall*)(void* ui, void* edx, const char* name, const char* label,
                                     const char* value);

// Sets a boolean script variable by name.
// From: DAT_10066bf4.
using FireBoolFn = void(__fastcall*)(void* ui, void* edx, const char* name, int bool_val);

// Sets an integer script variable by name.
// From: DAT_10066bf8.
using FireIntFn = void(__fastcall*)(void* ui, void* edx, const char* name, int value);

// Sets a float script variable by name.
// From: DAT_10066bfc.
using FireFloatFn = void(__fastcall*)(void* ui, void* edx, const char* name, float value);

// The game's UI script-variable setters (the Fire* methods), resolved from
// the address table by the hook installer. Game thread.
class ScriptVars final : public core::ui::ports::ScriptVariables {
public:
    // Takes the setters. All null before the hook installer runs.
    void bind(FireStr2Fn str2, FireBoolFn b, FireIntFn i, FireFloatFn f) noexcept;

    // True when the setters every push needs have been installed by the
    // hook installer, so early or late calls are safe. The row setter (the
    // controls help rows) is checked where it is used.
    bool ready() const noexcept override;
    void set_bool(core::ui::ports::UiHandle ui, const char* name, bool value) noexcept override;
    void set_int(core::ui::ports::UiHandle ui, const char* name, int value) noexcept override;
    void set_float(core::ui::ports::UiHandle ui, const char* name, float value) noexcept override;
    bool add_row(core::ui::ports::UiHandle ui, const char* name, const char* a,
                 const char* b) noexcept override;

    // The setters, for the installer's log line.
    FireBoolFn fire_bool() const noexcept { return fire_bool_; }
    FireIntFn fire_int() const noexcept { return fire_int_; }
    FireFloatFn fire_float() const noexcept { return fire_float_; }

private:
    FireStr2Fn fire_str2_   = nullptr;
    FireBoolFn fire_bool_   = nullptr;
    FireIntFn fire_int_     = nullptr;
    FireFloatFn fire_float_ = nullptr;
};

} // namespace omvr::game::ui
