#pragma once

#include "game/ui/console.h"
#include "game/ui/script_vars.h"

namespace omvr::game::sites {

// The UI set (01 section 2.3): the archive path, the intro push, the UI's
// external-call slot and the two menu Open slots. Patched for every run.

// 2: the options page lives in the mod's UI archive. The original aborts
// without it; we carry on, since VR works without the menu. With
// `archive_built` (MOTHER.PAK is in place) the game is pointed at it. True
// when the game now opens the mod's archive.
bool redirect_ui_archive(bool archive_built) noexcept;

// 4: the site is `push offset "ShowBootup"`; replace the pushed string. The
// skip-intro function is defined by the mod's UI movie only: with the stock
// archive the boot sequence would call a name its movie lacks, so the intro
// plays then.
void skip_intro(bool mod_archive) noexcept;

// The replacements step 5 installs.
struct UiReplacements {
    const void* handle_mvr_command;
    const void* on_menu_open;
};

// 5: the game functions the settings push calls, and the console command
// dispatch through handle_mvr_command.
bool hook_script_bridge(ui::ScriptVars& script_vars, ui::Console& console, const UiReplacements& r) noexcept;

// Runtime None: the pause menu's open slot to `replacement`; `original`
// receives the game's method.
bool hook_pause_settings_push(const void* replacement, void*& original) noexcept;

} // namespace omvr::game::sites
