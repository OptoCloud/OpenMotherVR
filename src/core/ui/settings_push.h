#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/input/control_scheme.h"
#include "core/ui/display_reset.h"
#include "core/ui/ports/play_space_control.h"
#include "core/ui/ports/script_variables.h"
#include "core/ui/ports/runtime_state.h"
#include "util/sync/once_latch.h"

#include <string_view>

namespace omvr::core::ui {

// The game's UI script variables and the services a settings push reads.
struct SettingsPushDeps {
    ports::ScriptVariables& vars;
    DisplayReset& display_reset;
    const input::ControlSchemes& schemes;
    const config::Settings& settings;
    ports::RuntimeState& runtime;
    ports::PlaySpaceControl& play_space;
    diag::ports::Log& log;
};

// Pushes the settings into the game's UI script variables, so the in-game
// options page shows them. Game thread, from the menu hooks.
class SettingsPush {
public:
    explicit SettingsPush(const SettingsPushDeps& deps) noexcept : deps_(deps) {}

    // Pushes the entire live config into the game's script-variable layer, on
    // the menu object `ui`. The original only ever calls this from its menu
    // hooks, with their `this`.
    //
    // From: FUN_10020870 @ 0x10020870 (size 996 bytes).
    void push(ports::UiHandle ui) noexcept;

private:
    void push_runtime_and_debug(ports::UiHandle ui) noexcept;
    void push_rendering(ports::UiHandle ui) noexcept;
    void push_input(ports::UiHandle ui) noexcept;
    // The v2 pages (spec 22 §12, 21 §15.3): every settings-table row with a
    // page function, then what the pages show that is not a setting.
    void push_pages(ports::UiHandle ui) noexcept;
    // The controls help on the options page: one row per "Action\tControl"
    // line of the layout in use. Nothing when the game's row function is
    // unknown.
    void push_controls_help(ports::UiHandle ui) noexcept;

    SettingsPushDeps deps_;
    util::OnceLatch not_ready_logged_;
};

} // namespace omvr::core::ui
