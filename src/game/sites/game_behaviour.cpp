// The game-side hooks and byte patches of the hook installer
// (FUN_1001d6f0 steps 8 and 9).

#include "game/sites/game_behaviour.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/addresses.h"
#include "game/patcher.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace omvr::game::sites {

namespace {

namespace diag = core::diag;

// The UI mouse patch moves the load 0x10 bytes on, from the back-buffer size
// to the UI reference size in the same display info struct.
constexpr std::uint8_t kUiReferenceSizeOffset = 0x10;

} // namespace

bool install_game_side_hooks(const GameSideReplacements& r, GameSideOriginals& originals) noexcept {
    auto slot = [](AiAddr a, int index) {
        return *reinterpret_cast<void**>(ai_address(a) + 4 * index);
    };
    // Save the originals before their slots are replaced.
    originals = {
        .follow_on_activate   = slot(AiAddr::FollowVtable, 0),
        .follow_on_deactivate = slot(AiAddr::FollowVtable, 1),
        .ui_on_message        = slot(AiAddr::UiEntityMessageSlot, 0),
        .menu_close           = slot(AiAddr::MenuVtable, 2),
        .pause_open           = slot(AiAddr::PauseMenuVtable, 0),
        .pause_close          = slot(AiAddr::PauseMenuVtable, 2),
        .loading_open         = slot(AiAddr::LoadingScreenVtable, 0),
        .loading_close        = slot(AiAddr::LoadingScreenVtable, 2),
        .attract_open         = slot(AiAddr::AttractScreenVtable, 0),
    };

    const bool slots_ok =
        // Track the first-person camera; watch cutscenes and level fades.
        write_fn(ai_address(AiAddr::FollowVtable) + 0, r.follow_on_activate) &&
        write_fn(ai_address(AiAddr::FollowVtable) + 4, r.follow_on_deactivate) &&
        write_fn(ai_address(AiAddr::UiEntityMessageSlot), r.ui_on_message) &&
        // Menus, loading and the title video show on the virtual screen.
        write_fn(ai_address(AiAddr::MenuVtable) + 8, r.menu_close) &&
        write_fn(ai_address(AiAddr::PauseMenuVtable) + 0, r.pause_open) &&
        write_fn(ai_address(AiAddr::PauseMenuVtable) + 8, r.pause_close) &&
        write_fn(ai_address(AiAddr::LoadingScreenVtable) + 0, r.loading_open) &&
        write_fn(ai_address(AiAddr::LoadingScreenVtable) + 8, r.loading_close) &&
        write_fn(ai_address(AiAddr::AttractScreenVtable) + 0, r.attract_open) &&
        write_fn(ai_address(AiAddr::AttractScreenVtable) + 8, r.attract_close);
    if (!slots_ok) return false;

    if (!redirect_call(ai_address(AiAddr::StereoUpdateCall), r.stereo_update, "Stereo::Update") ||
        !redirect_call(ai_address(AiAddr::VirtualScreenCall), r.place_virtual_screen, "virtual screen") ||
        !redirect_call(ai_address(AiAddr::HudPanelCall), r.place_hud_panel, "HUD panel")) {
        return false;
    }
    for (AiAddr a : {AiAddr::PushModeCallA, AiAddr::PushModeCallB, AiAddr::PushModeCallC}) {
        if (!redirect_call(ai_address(a), r.push_mode, "PushMode")) return false;
    }
    for (AiAddr a : {AiAddr::PopModeCallA, AiAddr::PopModeCallB, AiAddr::PopModeCallC}) {
        if (!redirect_call(ai_address(a), r.pop_mode, "PopMode")) return false;
    }
    return true;
}

bool apply_byte_patches(const GameImage& g, const float* head_clip_threshold) noexcept {
    auto at = [](AiAddr a, std::ptrdiff_t offset = 0) { return ai_address(a) + offset; };
    const bool ok =
        // SSAO never runs: it bleeds across the side-by-side eyes.
        patch(at(AiAddr::SsaoSettingBranch), {0xEB, 0x29}) &&
        // Head-in-wall blackout threshold: ours, so it follows the clipping
        // blinders option.
        write_dword(at(AiAddr::HeadClipCompare, 3), address_of(head_clip_threshold)) &&
        // No procedural camera sway or breathing.
        patch(at(AiAddr::CameraNoiseCall), {0xEB, 0x05}) &&
        // Keep the second look-integration branch of FollowBehavior on.
        patch(at(AiAddr::FollowLookState), {0xB1, 0x01, 0xEB, 0x00}) &&
        // No DK2-era camera correction for head displacement.
        patch(at(AiAddr::CameraCorrectionBranch), {0xE9, 0x2C, 0x03, 0x00, 0x00, kNop}) &&
        // FollowBehavior::OnActivate leaves game look off; the stereo update
        // turns it on when snap rotation is off or keyboard/mouse is used.
        patch(at(AiAddr::FollowOnActivatePatch, 8), {kNop, kNop, kNop, kNop, kNop, kNop}) &&
        // The mouse cursor maps into the 1280x720 UI space, not the
        // side-by-side back buffer: the loads move on to the UI reference
        // size (the preflight checked the bytes the offset is added to).
        patch(at(AiAddr::UiMouseWidthLoad, 4),
              {static_cast<std::uint8_t>(g.sites.ui_mouse_width_disp_lo + kUiReferenceSizeOffset)}) &&
        patch(at(AiAddr::UiMouseHeightLoad, 4),
              {static_cast<std::uint8_t>(g.sites.ui_mouse_height_disp_lo + kUiReferenceSizeOffset)}) &&
        // Menus always use focus (pad) navigation.
        patch(at(AiAddr::UiInputModeBranch), {0xEB}) &&
        // The menu wrappers own the stereo mode for these two screens.
        patch(at(AiAddr::ResumeGamePopMode), {0xEB, 0x09, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop}) &&
        patch(at(AiAddr::PauseMenuPushMode),
              {0xEB, 0x0B, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop}) &&
        // No 30 Hz throttle in the loading loop: the headset needs frames.
        patch(at(AiAddr::LoaderSleepPush), {0x53});
    if (ok) diag::log_line("[hook] camera, UI and loader patches applied");
    return ok;
}

} // namespace omvr::game::sites
