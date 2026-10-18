#pragma once

#include "game/game_image.h"
#include "game/patcher.h"
#include "game/stereo/mode_stack.h"
#include "game/sites/ui_set.h"
#include "game/x86.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace omvr::game::sites {

// The AI.exe methods behind the vtable slots step 8 replaces, which the
// replacements call or wrap.
struct GameSideOriginals {
    void* follow_on_activate   = nullptr; // FollowBehavior vtable slot 0 (original)
    void* follow_on_deactivate = nullptr; // slot 1
    void* ui_on_message        = nullptr; // UI entity vtable slot 2 (original)
    void* menu_close           = nullptr; // front-end menu slot 2
    void* pause_open           = nullptr; // pause menu slot 0
    void* pause_close          = nullptr; // pause menu slot 2
    void* loading_open         = nullptr; // loading screen slot 0
    void* loading_close        = nullptr; // loading screen slot 2
    void* attract_open         = nullptr; // attract/title screen slot 0
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

// A vtable slot step 8 replaces: where the game's method is saved (null
// when nothing calls it) and the replacement written there. The pause
// menu's open slot is checked with the UI set (01 §2.3).
struct GameSideSlot {
    SlotSite site;
    void* GameSideOriginals::* original;
    const void* GameSideReplacements::* replacement;
    bool checked_with_ui_set = false;
};

// The slots in the order they are written.
inline constexpr std::array kGameSideSlots = {
    // Track the first-person camera; watch cutscenes and level fades.
    GameSideSlot{{AiAddr::FollowVtable, kFollowOnActivateSlot, AiAddr::FollowOnActivatePatch,
                  "FollowBehavior::OnActivate (slot 0)"},
                 &GameSideOriginals::follow_on_activate,
                 &GameSideReplacements::follow_on_activate},
    GameSideSlot{{AiAddr::FollowVtable, kFollowOnDeactivateSlot, AiAddr::FollowOnDeactivate,
                  "FollowBehavior::OnDeactivate (slot 1)"},
                 &GameSideOriginals::follow_on_deactivate,
                 &GameSideReplacements::follow_on_deactivate},
    // The table names this slot itself.
    GameSideSlot{
        {AiAddr::UiEntityMessageSlot, 0, AiAddr::UiEntityOnMessage, "the UI entity's message handler"},
        &GameSideOriginals::ui_on_message,
        &GameSideReplacements::ui_on_message},
    // Menus, loading and the title video show on the virtual screen.
    GameSideSlot{
        {AiAddr::MenuVtable, kScreenCloseSlot, AiAddr::MenuClose, "the front-end menu's close (slot 2)"},
        &GameSideOriginals::menu_close,
        &GameSideReplacements::menu_close},
    GameSideSlot{kPauseMenuOpenSite, &GameSideOriginals::pause_open, &GameSideReplacements::pause_open, true},
    GameSideSlot{{AiAddr::PauseMenuVtable, kScreenCloseSlot, AiAddr::PauseMenuClose,
                  "the pause menu's close (slot 2)"},
                 &GameSideOriginals::pause_close,
                 &GameSideReplacements::pause_close},
    GameSideSlot{{AiAddr::LoadingScreenVtable, kScreenOpenSlot, AiAddr::LoadingScreenOpen,
                  "the loading screen's open (slot 0)"},
                 &GameSideOriginals::loading_open,
                 &GameSideReplacements::loading_open},
    GameSideSlot{{AiAddr::LoadingScreenVtable, kScreenCloseSlot, AiAddr::LoadingScreenClose,
                  "the loading screen's close (slot 2)"},
                 &GameSideOriginals::loading_close,
                 &GameSideReplacements::loading_close},
    GameSideSlot{{AiAddr::AttractScreenVtable, kScreenOpenSlot, AiAddr::AttractOpen,
                  "the attract screen's open (slot 0)"},
                 &GameSideOriginals::attract_open,
                 &GameSideReplacements::attract_open},
    // A shared empty method the table does not name.
    GameSideSlot{{AiAddr::AttractScreenVtable, kScreenCloseSlot, std::nullopt,
                  "code in AI.exe (attract screen slot 2)"},
                 nullptr,
                 &GameSideReplacements::attract_close},
};

// A call step 8 redirects into the stereo manager (05 §3, §4): the callee
// the preflight expects (none: the table does not name it, so only the
// opcode is checked), the names the preflight's and the patcher's log
// lines give it, and the replacement.
struct GameSideCall {
    AiAddr site;
    std::optional<AiAddr> callee;
    std::string_view checked_as;
    std::string_view redirected_as;
    const void* GameSideReplacements::* replacement;
};

// The calls in the order they are redirected.
inline constexpr std::array kGameSideCalls = {
    GameSideCall{AiAddr::StereoUpdateCall, std::nullopt, "call StereoUpdate", "Stereo::Update",
                 &GameSideReplacements::stereo_update},
    GameSideCall{AiAddr::VirtualScreenCall, AiAddr::BuildUiQuad, "call BuildUiQuad (the virtual screen)",
                 "virtual screen", &GameSideReplacements::place_virtual_screen},
    GameSideCall{AiAddr::HudPanelCall, AiAddr::BuildUiQuad, "call BuildUiQuad (the HUD panel)", "HUD panel",
                 &GameSideReplacements::place_hud_panel},
    GameSideCall{AiAddr::PushModeCallA, AiAddr::GamePushMode, "call StereoPushMode", "PushMode",
                 &GameSideReplacements::push_mode},
    GameSideCall{AiAddr::PushModeCallB, AiAddr::GamePushMode, "call StereoPushMode", "PushMode",
                 &GameSideReplacements::push_mode},
    GameSideCall{AiAddr::PushModeCallC, AiAddr::GamePushMode, "call StereoPushMode", "PushMode",
                 &GameSideReplacements::push_mode},
    GameSideCall{AiAddr::PopModeCallA, AiAddr::GamePopMode, "call StereoPopMode", "PopMode",
                 &GameSideReplacements::pop_mode},
    GameSideCall{AiAddr::PopModeCallB, AiAddr::GamePopMode, "call StereoPopMode", "PopMode",
                 &GameSideReplacements::pop_mode},
    GameSideCall{AiAddr::PopModeCallC, AiAddr::GamePopMode, "call StereoPopMode", "PopMode",
                 &GameSideReplacements::pop_mode},
};

// 8: game-side stereo hooks: call redirects into the stereo manager, the
// FollowBehavior, UI entity and menu/loading screen vtable slots. The
// originals are saved into `originals` before their slots are replaced.
bool install_game_side_hooks(const GameImage& g, Patcher& patcher, const GameSideReplacements& r,
                             GameSideOriginals& originals) noexcept;

// 8: the game's mode-stack append, so the stereo update can raise the stack
// count through the game's allocator (05 §4.4). Nothing is written to the
// game; on a build without the address, or other code at its call, the
// update keeps to the storage the stack already has.
// The append found goes to `out`.
void find_mode_stack_append(const GameImage& g, core::diag::ports::Log& log,
                            stereo::ModeStackTargets& out) noexcept;

// 9: camera, UI and loader byte patches. `head_clip_threshold` is the
// threshold the game's head-in-wall check is pointed at.
bool apply_byte_patches(const GameImage& g, Patcher& patcher, const float* head_clip_threshold) noexcept;

// Sites of steps 8 (10 vtable slots, 9 calls) and 9.
inline constexpr int kGameSideSites = static_cast<int>(kGameSideSlots.size() + kGameSideCalls.size());
static_assert(kGameSideSites == 19);
inline constexpr int kBytePatchSites = 12;

// 05 §7.5: `jbe rel32` into the DK2-era camera correction, as the game has
// it; the preflight checks it, and the patch's `jmp` keeps its target.
inline constexpr std::uint32_t kCameraCorrectionRel = 0x32B;
inline constexpr auto kCameraCorrectionBranch       = x86::jbe_rel32(kCameraCorrectionRel);

} // namespace omvr::game::sites
