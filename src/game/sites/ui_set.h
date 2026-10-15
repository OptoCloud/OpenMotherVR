#pragma once

#include "game/game_image.h"
#include "game/patcher.h"
#include "game/ui/console.h"
#include "game/ui/script_vars.h"

namespace omvr::game::sites {

// The UI set (01 §2.3): the archive path, the intro push, the UI's
// external-call slot and the two menu Open slots. Patched for every run.

// 2: the options page lives in the mod's UI archive. The original aborts
// without it; we carry on, since VR works without the menu. With
// `archive_built` (MOTHER.PAK is in place) the game is pointed at it. True
// when the game now opens the mod's archive.
bool redirect_ui_archive(const GameImage& g, Patcher& patcher, bool archive_built) noexcept;

// 4: the site is `push offset "ShowBootup"`; replace the pushed string. The
// skip-intro function is defined by the mod's UI movie only: with the stock
// archive the boot sequence would call a name its movie lacks, so the intro
// plays then (05 §11).
void skip_intro(const GameImage& g, Patcher& patcher, bool mod_archive) noexcept;

// The replacements step 5 installs.
struct UiReplacements {
    const void* handle_mvr_command;
    const void* on_menu_open;
};

// 5: the game functions the settings push calls, and the console command
// dispatch through handle_mvr_command.
bool hook_script_bridge(const GameImage& g, Patcher& patcher, ui::ScriptVars& script_vars, ui::Console& console,
                        const UiReplacements& r) noexcept;

// Runtime None: the pause menu's open slot to `replacement`; `stock`
// receives the game's method.
bool hook_pause_settings_push(const GameImage& g, Patcher& patcher, const void* replacement, void*& stock) noexcept;

// Sites of the UI set: the external-call slot and the menu's open slot,
// the archive path when the mod's archive is in use, and the pause menu's
// open slot, written with the game-side hooks or on its own for runtime
// None.
inline constexpr int kUiSetSitesWithArchive    = 4;
inline constexpr int kUiSetSitesWithoutArchive = 3;

} // namespace omvr::game::sites
