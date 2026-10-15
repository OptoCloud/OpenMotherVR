#pragma once

#include "core/input/control_scheme.h"
#include "core/ui/display_reset.h"
#include "core/ui/ports/script_variables.h"

namespace omvr::core::ui {

// Pushes the settings into the game's UI script variables, so the in-game
// options page shows them. Game thread, from the menu hooks.
class SettingsPush {
public:
    SettingsPush(ports::ScriptVariables& vars, DisplayReset& display_reset,
                 const input::ControlSchemes& schemes) noexcept
        : vars_(vars), display_reset_(display_reset), schemes_(schemes) {}

    // Pushes the entire live config into the game's script-variable layer, on
    // the menu object `ui`. The original only ever calls this from its menu
    // hooks, with their `this`.
    //
    // From: FUN_10020870 @ 0x10020870 (size 996 bytes).
    void push(ports::UiHandle ui) noexcept;

private:
    ports::ScriptVariables& vars_;
    DisplayReset&           display_reset_;
    const input::ControlSchemes& schemes_;
    bool                    s_logged = false;
};

} // namespace omvr::core::ui
