#include "game/ui/script_vars.h"

namespace omvr::game::ui {

namespace {

void* game_ui(core::ui::ports::UiHandle ui) noexcept {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(ui));
}

} // namespace

void ScriptVars::bind(PFN_FireStr2 str2, PFN_FireBool b, PFN_FireInt i, PFN_FireFloat f) noexcept {
    g_fire_str2  = str2;
    g_fire_bool  = b;
    g_fire_int   = i;
    g_fire_float = f;
}

bool ScriptVars::ready() const noexcept { return g_fire_bool && g_fire_int && g_fire_float; }

void ScriptVars::set_bool(core::ui::ports::UiHandle ui, const char* name, bool value) noexcept {
    g_fire_bool(game_ui(ui), nullptr, name, value ? 1 : 0);
}

void ScriptVars::set_int(core::ui::ports::UiHandle ui, const char* name, int value) noexcept {
    g_fire_int(game_ui(ui), nullptr, name, value);
}

void ScriptVars::set_float(core::ui::ports::UiHandle ui, const char* name, float value) noexcept {
    g_fire_float(game_ui(ui), nullptr, name, value);
}

bool ScriptVars::add_row(core::ui::ports::UiHandle ui, const char* name, const char* a, const char* b) noexcept {
    if (!g_fire_str2) return false;
    g_fire_str2(game_ui(ui), nullptr, name, a, b);
    return true;
}

} // namespace omvr::game::ui
