#include "game/ui/script_vars.h"

namespace omvr::game::ui {

namespace {

// The game's menu object behind a handle (its `this`).
void* game_ui(core::ui::ports::UiHandle ui) noexcept {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(ui));
}

// The script setter's boolean is an int.
int script_bool(bool value) noexcept { return value ? 1 : 0; }

} // namespace

void ScriptVars::bind(FireStr2Fn str2, FireBoolFn b, FireIntFn i, FireFloatFn f) noexcept {
    fire_str2_  = str2;
    fire_bool_  = b;
    fire_int_   = i;
    fire_float_ = f;
}

bool ScriptVars::ready() const noexcept { return fire_bool_ && fire_int_ && fire_float_; }

void ScriptVars::set_bool(core::ui::ports::UiHandle ui, const char* name, bool value) noexcept {
    fire_bool_(game_ui(ui), nullptr, name, script_bool(value));
}

void ScriptVars::set_int(core::ui::ports::UiHandle ui, const char* name, int value) noexcept {
    fire_int_(game_ui(ui), nullptr, name, value);
}

void ScriptVars::set_float(core::ui::ports::UiHandle ui, const char* name, float value) noexcept {
    fire_float_(game_ui(ui), nullptr, name, value);
}

bool ScriptVars::add_row(core::ui::ports::UiHandle ui, const char* name, const char* a, const char* b) noexcept {
    if (!fire_str2_) return false;
    fire_str2_(game_ui(ui), nullptr, name, a, b);
    return true;
}

} // namespace omvr::game::ui
