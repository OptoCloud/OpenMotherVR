// The game-side hooks and byte patches of the hook installer
// (FUN_1001d6f0 steps 8 and 9).

#include "game/sites/game_behaviour.h"

#include "game/addresses.h"
#include "game/code_address.h"
#include "game/layout.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace omvr::game::sites {

namespace {

constexpr std::string_view kBytePatchesLog = "[hook] camera, UI and loader patches applied";
constexpr std::string_view kNoAppendLog =
    "[hook] mode stack: this build has no address for the game's "
    "append; the count is raised only into existing storage";
constexpr std::string_view kAppendCodeLog =
    "[hook] mode stack: StereoModeStackAppend's call (RVA {:08X}) is not the expected code; the count is "
    "raised only into existing storage";
constexpr std::string_view kAppendFoundLog =
    "[hook] mode stack: count raised through the game's append (RVA {:08X})";

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

// 05 §4.4: `lea ecx, [ebx+0x68]`, the manager's mode stack, before the
// append's call.
constexpr std::uint8_t kModRmEcxEbxDisp8 =
    x86::modrm(x86::kModDisp8, std::to_underlying(x86::Reg::Ecx), std::to_underlying(x86::Reg::Ebx));
static_assert(kModRmEcxEbxDisp8 == 0x4B);
constexpr std::array<std::uint8_t, 3> kLeaEcxModeStack = {
    x86::kLea, kModRmEcxEbxDisp8, static_cast<std::uint8_t>(offsetof(StereoManager, stack_data))};

// ---- 8: vtable slots and calls -------------------------------------------------

GameSideOriginals save_originals(const GameImage& g) noexcept {
    GameSideOriginals saved;
    for (const GameSideSlot& s : kGameSideSlots) {
        if (s.original) saved.*s.original = pointer_at(s.site.at(g));
    }
    return saved;
}

bool replace_slots(const GameImage& g, Patcher& p, const GameSideReplacements& r) noexcept {
    for (const GameSideSlot& s : kGameSideSlots) {
        if (!p.write_pointer(s.site.at(g), r.*s.replacement)) return false;
    }
    return true;
}

bool redirect_calls(const GameImage& g, Patcher& p, const GameSideReplacements& r) noexcept {
    for (const GameSideCall& c : kGameSideCalls) {
        if (!p.redirect_call(g.at(c.site), r.*c.replacement, c.redirected_as)) return false;
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

void find_mode_stack_append(const GameImage& g, core::diag::ports::Log& log,
                            stereo::ModeStackTargets& out) noexcept {
    const ReconRvas& r = kReconRvas[g.version];
    // Both rows: the check reads the bytes before the call site.
    if (r.mode_stack_append == 0 || r.mode_stack_append_call == 0) {
        log.line(kNoAppendLog);
        return;
    }
    const std::uint8_t* site = g.at_rva(r.mode_stack_append_call);
    if (!x86::matches(site - kLeaEcxModeStack.size(), kLeaEcxModeStack) ||
        !calls(site, g.at_rva(r.mode_stack_append))) {
        core::diag::ports::logf(log, kAppendCodeLog, r.mode_stack_append_call);
        return;
    }
    out.append = g.at_rva(r.mode_stack_append);
    core::diag::ports::logf(log, kAppendFoundLog, r.mode_stack_append);
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
