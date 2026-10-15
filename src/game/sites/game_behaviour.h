#pragma once

#include "game/preflight.h"

namespace omvr::game::sites {

// The AI.exe methods behind the vtable slots step 8 replaces, which the
// replacements call or wrap.
struct GameSideOriginals {
    void* follow_on_activate   = nullptr;  // FollowBehavior vtable slot 0 (original)
    void* follow_on_deactivate = nullptr;  // slot 1
    void* ui_on_message        = nullptr;  // UI entity vtable slot 2 (original)
    void* menu_close           = nullptr;  // front-end menu slot 2
    void* pause_open           = nullptr;  // pause menu slot 0
    void* pause_close          = nullptr;  // pause menu slot 2
    void* loading_open         = nullptr;  // loading screen slot 0
    void* loading_close        = nullptr;  // loading screen slot 2
    void* attract_open         = nullptr;  // attract/title screen slot 0
};

// The replacements step 8 installs.
struct GameSideReplacements {
    const void* follow_on_activate;
    const void* follow_on_deactivate;
    const void* ui_on_message;
    const void* menu_close;
    const void* pause_open;
    const void* pause_close;
    const void* loading_open;
    const void* loading_close;
    const void* attract_open;
    const void* attract_close;
    const void* stereo_update;
    const void* place_virtual_screen;
    const void* place_hud_panel;
    const void* push_mode;
    const void* pop_mode;
};

// 8: game-side stereo hooks: call redirects into the stereo manager, the
// FollowBehavior, UI entity and menu/loading screen vtable slots. The
// originals are saved into `originals` before their slots are replaced.
bool install_game_side_hooks(const GameSideReplacements& r, GameSideOriginals& originals) noexcept;

// 9: camera, UI and loader byte patches. `head_clip_threshold` is the
// threshold the game's head-in-wall check is pointed at.
bool apply_byte_patches(const GameImage& g, const float* head_clip_threshold) noexcept;

// Sites of steps 8 (10 vtable slots, 9 calls) and 9.
inline constexpr int kGameSideSites   = 19;
inline constexpr int kBytePatchSites  = 12;

} // namespace omvr::game::sites
