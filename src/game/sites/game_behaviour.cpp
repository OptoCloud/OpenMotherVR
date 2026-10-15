// The game-side hooks and byte patches of the hook installer
// (FUN_1001d6f0 steps 8 and 9).

#include "game/sites/game_behaviour.h"

#include "game/code_address.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace omvr::game::sites {

namespace {

constexpr std::string_view kBytePatchesLog = "[hook] camera, UI and loader patches applied";

// ---- 9: the bytes written (05 §7) --------------------------------------------

// 05 §7.1: the SSAO setting's `jne` becomes a `jmp` with the same
// displacement, so the branch that skips SSAO is always taken.
constexpr std::uint8_t kSsaoBranchRel                           = 0x29;
constexpr std::array<std::uint8_t, x86::kJmpRel8Size> kSkipSsao = {x86::kJmpRel8, kSsaoBranchRel};

// 05 §7.3: over the `mov ecx, reg` before the noise call, a jump past the
// call.
constexpr std::array<std::uint8_t, x86::kJmpRel8Size> kSkipCameraNoise = {
    x86::kJmpRel8, static_cast<std::uint8_t>(x86::kCallRel32Size)};

// 05 §7.4: `mov cl, [esp+disp8]` becomes `mov cl, 1` and a two-byte jump to
// the next instruction, which keeps the four bytes' length.
constexpr std::array<std::uint8_t, 4> kLookStateOn = {x86::kMovClImm8, 0x01, x86::kJmpRel8, 0x00};

// 05 §7.5: a `jmp rel32` to the `jbe`'s target. It is one byte shorter, so
// its displacement is one more and a `nop` fills the sixth byte.
constexpr auto kSkipCameraCorrection = [] {
    auto out       = x86::nops<x86::kJbeRel32Size>();
    const auto jmp = x86::jmp_rel32(kCameraCorrectionRel + (x86::kJbeRel32Size - x86::kJmpRel32Size));
    std::copy(jmp.begin(), jmp.end(), out.begin());
    return out;
}();
// The bytes this patch has always written.
static_assert(kSkipCameraCorrection == std::array<std::uint8_t, 6>{0xE9, 0x2C, 0x03, 0x00, 0x00, 0x90});

// 05 §7.6: the store of "game look enabled" in OnActivate goes.
constexpr auto kNoLookEnable = x86::nops<x86::kMovRm8Disp32Size>();

// 05 §7.8: the input-mode `je` becomes a `jmp`.
constexpr std::array<std::uint8_t, 1> kFocusNavigation = {x86::kJmpRel8};

// 05 §4.4: the menu wrappers own the stereo mode of these two screens, so
// the game's own `mov ecx, [abs]; (push 1;) call` goes.
constexpr std::size_t kResumePopSize = x86::kMovEcxAbsSize + x86::kCallRel32Size;
constexpr std::size_t kPausePushSize = x86::kMovEcxAbsSize + x86::kPushImm8Size + x86::kCallRel32Size;
constexpr auto kSkipResumePop        = x86::jump_over<kResumePopSize>();
constexpr auto kSkipPausePush        = x86::jump_over<kPausePushSize>();
// The jumps this patch has always written: `jmp +9` over 11 bytes, `jmp
// +0x0B` over 13.
static_assert(kSkipResumePop.size() == 11 && kSkipResumePop[1] == 0x09);
static_assert(kSkipPausePush.size() == 13 && kSkipPausePush[1] == 0x0B);

// 05 §7.9: `push ecx` (the remaining wait) becomes `push ebx`, so the
// loader loop does not sleep.
constexpr std::array<std::uint8_t, 1> kNoLoaderSleep = {x86::kPushEbx};

// The UI mouse loads move on from the back-buffer size to the UI reference
// size (05 §7.7); the preflight checked the byte the offset is added to.
std::array<std::uint8_t, 1> ui_reference_disp(std::uint8_t back_buffer_disp_lo) noexcept {
    return {static_cast<std::uint8_t>(back_buffer_disp_lo + kUiReferenceSizeOffset)};
}

// ---- 8: vtable slots -----------------------------------------------------------

std::uint8_t* slot_at(const GameImage& g, AiAddr vtable, std::uint32_t index) noexcept {
    return g.at(vtable) + slot_offset(index);
}

// The method in slot `index` of the vtable at `vtable`.
void* method_at(const GameImage& g, AiAddr vtable, std::uint32_t index) noexcept {
    return pointer_at(slot_at(g, vtable, index));
}

GameSideOriginals save_originals(const GameImage& g) noexcept {
    return {
        .follow_on_activate   = method_at(g, AiAddr::FollowVtable, kFollowOnActivateSlot),
        .follow_on_deactivate = method_at(g, AiAddr::FollowVtable, kFollowOnDeactivateSlot),
        .ui_on_message        = pointer_at(g.at(AiAddr::UiEntityMessageSlot)),
        .menu_close           = method_at(g, AiAddr::MenuVtable, kScreenCloseSlot),
        .pause_open           = method_at(g, AiAddr::PauseMenuVtable, kScreenOpenSlot),
        .pause_close          = method_at(g, AiAddr::PauseMenuVtable, kScreenCloseSlot),
        .loading_open         = method_at(g, AiAddr::LoadingScreenVtable, kScreenOpenSlot),
        .loading_close        = method_at(g, AiAddr::LoadingScreenVtable, kScreenCloseSlot),
        .attract_open         = method_at(g, AiAddr::AttractScreenVtable, kScreenOpenSlot),
    };
}

bool replace_slots(const GameImage& g, Patcher& p, const GameSideReplacements& r) noexcept {
    return
        // Track the first-person camera; watch cutscenes and level fades.
        p.write_pointer(slot_at(g, AiAddr::FollowVtable, kFollowOnActivateSlot), r.follow_on_activate) &&
        p.write_pointer(slot_at(g, AiAddr::FollowVtable, kFollowOnDeactivateSlot), r.follow_on_deactivate) &&
        p.write_pointer(g.at(AiAddr::UiEntityMessageSlot), r.ui_on_message) &&
        // Menus, loading and the title video show on the virtual screen.
        p.write_pointer(slot_at(g, AiAddr::MenuVtable, kScreenCloseSlot), r.menu_close) &&
        p.write_pointer(slot_at(g, AiAddr::PauseMenuVtable, kScreenOpenSlot), r.pause_open) &&
        p.write_pointer(slot_at(g, AiAddr::PauseMenuVtable, kScreenCloseSlot), r.pause_close) &&
        p.write_pointer(slot_at(g, AiAddr::LoadingScreenVtable, kScreenOpenSlot), r.loading_open) &&
        p.write_pointer(slot_at(g, AiAddr::LoadingScreenVtable, kScreenCloseSlot), r.loading_close) &&
        p.write_pointer(slot_at(g, AiAddr::AttractScreenVtable, kScreenOpenSlot), r.attract_open) &&
        p.write_pointer(slot_at(g, AiAddr::AttractScreenVtable, kScreenCloseSlot), r.attract_close);
}

bool redirect_calls(const GameImage& g, Patcher& p, const GameSideReplacements& r) noexcept {
    if (!p.redirect_call(g.at(AiAddr::StereoUpdateCall), r.stereo_update, "Stereo::Update") ||
        !p.redirect_call(g.at(AiAddr::VirtualScreenCall), r.place_virtual_screen, "virtual screen") ||
        !p.redirect_call(g.at(AiAddr::HudPanelCall), r.place_hud_panel, "HUD panel")) {
        return false;
    }
    for (AiAddr a : {AiAddr::PushModeCallA, AiAddr::PushModeCallB, AiAddr::PushModeCallC}) {
        if (!p.redirect_call(g.at(a), r.push_mode, "PushMode")) return false;
    }
    for (AiAddr a : {AiAddr::PopModeCallA, AiAddr::PopModeCallB, AiAddr::PopModeCallC}) {
        if (!p.redirect_call(g.at(a), r.pop_mode, "PopMode")) return false;
    }
    return true;
}

} // namespace

bool install_game_side_hooks(const GameImage& g, Patcher& patcher, const GameSideReplacements& r,
                             GameSideOriginals& originals) noexcept {
    // Save the game's methods before their slots are replaced.
    originals = save_originals(g);
    return replace_slots(g, patcher, r) && redirect_calls(g, patcher, r);
}

bool apply_byte_patches(const GameImage& g, Patcher& p, const float* head_clip_threshold) noexcept {
    const bool ok =
        // SSAO never runs: it bleeds across the side-by-side eyes.
        p.write(g.at(AiAddr::SsaoSettingBranch), kSkipSsao) &&
        // Head-in-wall blackout threshold: ours, so it follows the clipping
        // blinders option.
        p.write_pointer(g.at(AiAddr::HeadClipCompare) + x86::kComissDispOffset, head_clip_threshold) &&
        // No procedural camera sway or breathing.
        p.write(g.at(AiAddr::CameraNoiseCall), kSkipCameraNoise) &&
        // Keep the second look-integration branch of FollowBehavior on.
        p.write(g.at(AiAddr::FollowLookState), kLookStateOn) &&
        // No DK2-era camera correction for head displacement.
        p.write(g.at(AiAddr::CameraCorrectionBranch), kSkipCameraCorrection) &&
        // FollowBehavior::OnActivate leaves game look off; the stereo update
        // turns it on when snap rotation is off or keyboard/mouse is used.
        p.write(g.at(AiAddr::FollowOnActivatePatch) + kFollowLookStoreOffset, kNoLookEnable) &&
        // The mouse cursor maps into the 1280x720 UI space, not the
        // side-by-side back buffer.
        p.write(g.at(AiAddr::UiMouseWidthLoad) + x86::kMovssDispOffset,
                ui_reference_disp(g.sites.ui_mouse_width_disp_lo)) &&
        p.write(g.at(AiAddr::UiMouseHeightLoad) + x86::kMovssDispOffset,
                ui_reference_disp(g.sites.ui_mouse_height_disp_lo)) &&
        // Menus always use focus (pad) navigation.
        p.write(g.at(AiAddr::UiInputModeBranch), kFocusNavigation) &&
        // The menu wrappers own the stereo mode for these two screens.
        p.write(g.at(AiAddr::ResumeGamePopMode), kSkipResumePop) &&
        p.write(g.at(AiAddr::PauseMenuPushMode), kSkipPausePush) &&
        // No 30 Hz throttle in the loading loop: the headset needs frames.
        p.write(g.at(AiAddr::LoaderSleepPush), kNoLoaderSleep);
    if (ok) p.log().line(kBytePatchesLog);
    return ok;
}

} // namespace omvr::game::sites
