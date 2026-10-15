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
using PFN_FireStr2   = void (__fastcall*)(void* ui, void* edx, const char* name,
                                          const char* label, const char* value);

// Sets a boolean script variable by name.
// From: DAT_10066bf4.
using PFN_FireBool   = void (__fastcall*)(void* ui, void* edx, const char* name, int bool_val);

// Sets an integer script variable by name.
// From: DAT_10066bf8.
using PFN_FireInt    = void (__fastcall*)(void* ui, void* edx, const char* name, int value);

// Sets a float script variable by name.
// From: DAT_10066bfc.
using PFN_FireFloat  = void (__fastcall*)(void* ui, void* edx, const char* name, float value);

// The game's UI script-variable setters (the Fire* methods), resolved from
// the address table by the hook installer.
class ScriptVars final : public core::ui::ports::ScriptVariables {
public:
    // Takes the setters. All null before the hook installer runs.
    void bind(PFN_FireStr2 str2, PFN_FireBool b, PFN_FireInt i, PFN_FireFloat f) noexcept;

    // True when the setters every push needs have been installed by the
    // hook installer, so early or late calls are safe. The row setter (the
    // controls help rows) is checked where it is used.
    bool ready() const noexcept override;
    void set_bool(core::ui::ports::UiHandle ui, const char* name, bool value) noexcept override;
    void set_int(core::ui::ports::UiHandle ui, const char* name, int value) noexcept override;
    void set_float(core::ui::ports::UiHandle ui, const char* name, float value) noexcept override;
    bool add_row(core::ui::ports::UiHandle ui, const char* name, const char* a, const char* b) noexcept override;

    PFN_FireBool  fire_bool() const noexcept { return g_fire_bool; }
    PFN_FireInt   fire_int() const noexcept { return g_fire_int; }
    PFN_FireFloat fire_float() const noexcept { return g_fire_float; }

private:
    PFN_FireStr2  g_fire_str2  = nullptr;
    PFN_FireBool  g_fire_bool  = nullptr;
    PFN_FireInt   g_fire_int   = nullptr;
    PFN_FireFloat g_fire_float = nullptr;
};

} // namespace omvr::game::ui
