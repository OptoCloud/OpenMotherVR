// The installer's checks before any write.
//
// Unlike the original (FUN_1001d6f0), nothing is written until a preflight
// has checked the code at every core site: a half-installed stereo hand-off
// leaves the game running its own LibOVR init on our manager.

#include "game/preflight.h"

#include "game/code_address.h"
#include "game/sites/controls.h"
#include "game/sites/game_behaviour.h"
#include "game/sites/ui_set.h"
#include "game/x86.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <string_view>

namespace omvr::game {

namespace {

namespace ports = core::diag::ports;

// ---- Log templates (01 §2.5) ------------------------------------------------

constexpr std::string_view kCheckFailedLog = "[hook] check failed at RVA {:08X}: expected {}; found {}";
constexpr std::string_view kOutsideImageLog =
    "[hook] check failed: {} at RVA {:08X} lies outside AI.exe (size {:08X})";
constexpr std::string_view kSlotFoundRvaLog =
    "[hook] check failed at RVA {:08X}: expected {}; found RVA {:08X}";
constexpr std::string_view kSlotFoundOutsideLog =
    "[hook] check failed at RVA {:08X}: expected {}; found {:08X}, outside AI.exe";
constexpr std::string_view kTableOutsideLog =
    "[hook] check failed: the address table points outside AI.exe; nothing patched, game runs as shipped";
constexpr std::string_view kChecksFailedLog =
    "[hook] {} core check(s) failed: nothing patched, game runs as shipped";

// ---- The code each core site must hold --------------------------------------

using x86::Pattern;

// 6 (05 §2): `cmp reg, eax`, the register the game compares chosen freely.
constexpr Pattern<2> kCmpRegEax{{x86::kCmpR32Rm32, x86::kModRmEaxEax}, {0xFF, x86::kModRegMask}};

// 7 (02 §3.1): the manager's allocation pushes its size; the hand-off jumps
// to the store of the result, `mov [reg+disp32], eax`.
constexpr Pattern<2> kStoreEaxDisp32{{x86::kMovRm32R32, x86::kModDisp32}, {0xFF, x86::kModRegFieldMask}};

// 7 (02 §6.4): `mov eax, [reg+0x2C]`, the head-pose slot of the device
// vtable.
constexpr Pattern<3> kLoadHeadPoseSlot{
    {x86::kMovR32Rm32, x86::kModDisp8, static_cast<std::uint8_t>(slot_offset(kDeviceHeadPoseSlot))},
    {0xFF, x86::kModMask, 0xFF}};

// 9 (05 §7.2): `comiss xmm, [disp32]`, the head-in-wall threshold.
constexpr Pattern<3> kComissDisp32{{x86::kTwoByte, x86::kComiss, x86::kModAbsolute},
                                   {0xFF, 0xFF, x86::kModRegMask}};

// 9 (05 §7.3): `mov ecx, reg; call`, the camera noise.
constexpr Pattern<3> kMovEcxRegCall{{x86::kMovR32Rm32, x86::kModRegister, x86::kCallRel32},
                                    {0xFF, x86::kModMask, 0xFF}};

// 9 (05 §7.4): `mov cl, [esp+disp8]`, the look state.
constexpr auto kMovClEspDisp8 = std::to_array<std::uint8_t>({x86::kMovR8Rm8, x86::kModRmClSib, x86::kSibEsp});

// 9 (05 §7.6): `mov [reg+disp32], al`, the look-enabled store.
constexpr Pattern<2> kStoreAlDisp32{{x86::kMovRm8R8, x86::kModDisp32}, {0xFF, x86::kModRegFieldMask}};

// 9 (05 §7.7): `movss xmm, [disp32]`, a back-buffer size load.
constexpr Pattern<4> kMovssDisp32{{x86::kRepPrefix, x86::kTwoByte, x86::kMovss, x86::kModAbsolute},
                                  {0xFF, 0xFF, 0xFF, x86::kModRegMask}};

// 9 (05 §4.4): `mov ecx, [abs]`, the stereo object, before a mode call;
// the pause menu pushes the screen mode.
constexpr auto kMovEcxAbs = std::to_array<std::uint8_t>({x86::kMovR32Rm32, x86::kModRmEcxAbs});
constexpr auto kPushScreenMode =
    std::to_array<std::uint8_t>({x86::kPushImm8, static_cast<std::uint8_t>(kStereoModeScreen)});

// 9 (05 §7.10): `setne cl`, the pad connected flag.
constexpr auto kSetneCl = std::to_array<std::uint8_t>({x86::kTwoByte, x86::kSetne, x86::kModRmCl});

// ---- The checker ------------------------------------------------------------

// Records which core sites do not hold what the patches expect. Each failure
// is logged with the site's RVA and what was found there, so one run names
// every mismatch.
class Preflight {
public:
    Preflight(const GameImage& g, ports::Log& log) noexcept : g_{g}, log_{log} {}

    const GameImage& game() const noexcept { return g_; }

    // `site` holds what `what` describes.
    void expect(bool ok, const std::uint8_t* site, std::string_view what) noexcept {
        if (ok) return;
        ++failures_;
        ShownBytes hex{};
        ports::logf(log_, kCheckFailedLog, g_.rva_of(site), what, bytes_at(site, hex));
    }

    // The vtable slot (or code pointer) at `slot` holds `expected`.
    void expect_slot(const std::uint8_t* slot, const std::uint8_t* expected, std::string_view what) noexcept {
        expect_slot_value(slot, holds_address(slot, expected), what);
    }

    // The vtable slot at `slot` holds a code address inside the image: for
    // a slot whose function this build's table does not give.
    void expect_slot_in_image(const std::uint8_t* slot, std::string_view what) noexcept {
        expect_slot_value(slot, g_.contains(read_u32(slot)), what);
    }

    // `site` holds its stock method, or code in the image when the table
    // does not name it.
    void expect_slot(const SlotSite& site) noexcept {
        if (site.stock) {
            expect_slot(site.at(g_), g_.at(*site.stock), site.what);
        } else {
            expect_slot_in_image(site.at(g_), site.what);
        }
    }

    // An address the checks would read lies outside the image: itself a
    // failed check, and nothing is read there.
    void expect_rva_in_image(std::uint32_t site_rva, std::string_view what) noexcept {
        if (site_rva < g_.image_size) return;
        ++failures_;
        ports::logf(log_, kOutsideImageLog, what, site_rva, g_.image_size);
    }

    // `*p` points at a NUL-terminated `s` inside the image (compared
    // without case, as the game's own path compare does).
    bool points_to(const std::uint8_t* p, std::string_view s) const noexcept {
        const std::uint32_t va = read_u32(p);
        if (!g_.contains(va) || !g_.contains(va + static_cast<std::uint32_t>(s.size()))) return false;
        const char* str = CodeAddress{va};
        return _strnicmp(str, s.data(), s.size()) == 0 && str[s.size()] == '\0';
    }

    int failures() const noexcept { return failures_; }

private:
    static constexpr std::size_t kShownBytes = 8;
    // "xx " per byte, the last one without its space.
    using ShownBytes = std::array<char, 3 * kShownBytes>;

    void expect_slot_value(const std::uint8_t* slot, bool ok, std::string_view what) noexcept {
        if (ok) return;
        ++failures_;
        const std::uint32_t value = read_u32(slot);
        if (g_.contains(value)) {
            ports::logf(log_, kSlotFoundRvaLog, g_.rva_of(slot), what, value - address_of(g_.base));
        } else {
            ports::logf(log_, kSlotFoundOutsideLog, g_.rva_of(slot), what, value);
        }
    }

    // Up to kShownBytes bytes at `p` as hex, never past the image's end.
    std::string_view bytes_at(const std::uint8_t* p, ShownBytes& out) const noexcept {
        static constexpr char kHex[]       = "0123456789abcdef";
        constexpr unsigned kNibbleBits     = 4;
        constexpr std::uint8_t kNibbleMask = 0xF;
        const std::uint32_t at             = g_.rva_of(p);
        const std::size_t n = at < g_.image_size ? std::min<std::size_t>(kShownBytes, g_.image_size - at) : 0;
        std::size_t k       = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (i) out[k++] = ' ';
            out[k++] = kHex[p[i] >> kNibbleBits];
            out[k++] = kHex[p[i] & kNibbleMask];
        }
        return {out.data(), k};
    }

    const GameImage& g_;
    ports::Log& log_;
    int failures_ = 0;
};

// ---- One checker per step ---------------------------------------------------

// The UI set (01 §2.3): the archive path, the intro push, the UI's
// external-call slot and the two menu Open slots. Checked for every run.
void check_ui_set(Preflight& pf) noexcept {
    const GameImage& g = pf.game();
    pf.expect(pf.points_to(g.at(AiAddr::UiPakPathPtr), kStockUiPak), g.at(AiAddr::UiPakPathPtr),
              "a pointer to \"data\\ui.pak\" (UiPakPathPtr)");
    const std::uint8_t* push = g.at(AiAddr::BootupMoviePush);
    pf.expect(push[0] == x86::kPushImm32 && pf.points_to(push + x86::kImm32Offset, kShowBootup), push,
              "push \"ShowBootup\" (ShowBootupPush)");
    pf.expect_slot(g.at(AiAddr::CmdDispatchSlot), g.at(AiAddr::OriginalCmdHandler),
                   "the UI's external-call handler (UiExternalCallSlot)");
    pf.expect_slot(sites::kMenuOpenSite);
    pf.expect_slot(sites::kPauseMenuOpenSite);
}

// The inline sites lie inside the image; the checks below read them.
void check_inline_sites_in_image(Preflight& pf) noexcept {
    const InlineSites& sites = pf.game().sites;
    for (std::uint32_t rva : sites.stereo_mode_tests) pf.expect_rva_in_image(rva, "a Rift test");
    pf.expect_rva_in_image(sites.left_stick_prompt_call_2, "the second left-stick prompt call");
}

// 6: the "Stereo Mode == Rift" tests (05 §2).
void check_stereo_mode_tests(Preflight& pf) noexcept {
    const GameImage& g = pf.game();
    for (std::uint32_t rva : g.sites.stereo_mode_tests) {
        const std::uint8_t* p = g.at_rva(rva);
        pf.expect(x86::matches(p, kCmpRegEax), p, "cmp reg, eax (a Rift test)");
    }
}

// 7: the hand-off of the stereo manager and device (02 §3.1, §6.1, §6.2,
// §6.4).
void check_stereo_hand_off(Preflight& pf) noexcept {
    const GameImage& g        = pf.game();
    const std::uint8_t* alloc = g.at(AiAddr::StereoManagerAlloc);
    pf.expect(alloc[0] == x86::kPushImm32 && read_u32(alloc + x86::kImm32Offset) == kGameStereoManagerSize &&
                  x86::matches(alloc + kStereoAllocStoreOffset, kStoreEaxDisp32),
              alloc, "push 0xB0, and mov [reg+disp32], eax at +0x24 (StereoObjectAlloc)");
    for (AiAddr a : {AiAddr::HeadPoseCallA, AiAddr::HeadPoseCallB}) {
        pf.expect(x86::matches(g.at(a), kLoadHeadPoseSlot), g.at(a), "mov eax, [reg+0x2C] (a recenter call)");
    }
    // The table does not name these callees: the opcode only.
    auto expect_call = [&](AiAddr a, std::string_view what) {
        pf.expect(*g.at(a) == x86::kCallRel32, g.at(a), what);
    };
    expect_call(AiAddr::CreateDeviceCall, "call StereoInit");
    expect_call(AiAddr::HmdResolutionCall, "call the display size");
    expect_call(AiAddr::RenderTargetSizeCall, "call the render-target size");
}

// 8: the call sites the game-side hooks redirect (05 §3, §4).
void check_game_side_calls(Preflight& pf) noexcept {
    const GameImage& g = pf.game();
    for (const sites::GameSideCall& c : sites::kGameSideCalls) {
        const std::uint8_t* site = g.at(c.site);
        pf.expect(c.callee ? calls(site, g.at(*c.callee)) : *site == x86::kCallRel32, site, c.checked_as);
    }
    // The table does not name the callee: both calls reach the same
    // function inside AI.exe.
    const std::uint8_t* first  = g.at(AiAddr::LeftStickPromptCall);
    const std::uint8_t* second = g.at_rva(g.sites.left_stick_prompt_call_2);
    pf.expect(*first == x86::kCallRel32 && g.contains(call_target(first)), first,
              "call into AI.exe (the left-stick prompt)");
    pf.expect(
        *first == x86::kCallRel32 && *second == x86::kCallRel32 && call_target(first) == call_target(second),
        second, "the same call as LeftStickPromptCall (the second left-stick prompt)");
}

// 8: the vtable slots the game-side hooks replace (05 §5, §6).
void check_game_side_slots(Preflight& pf) noexcept {
    for (const sites::GameSideSlot& s : sites::kGameSideSlots) {
        if (!s.checked_with_ui_set) pf.expect_slot(s.site);
    }
}

// 9: the camera and look byte patches (05 §7.1 to §7.6).
void check_camera_patch_sites(Preflight& pf) noexcept {
    const GameImage& g = pf.game();
    pf.expect(*g.at(AiAddr::SsaoSettingBranch) == x86::kJneRel8, g.at(AiAddr::SsaoSettingBranch),
              "jne (the SSAO branch)");
    pf.expect(x86::matches(g.at(AiAddr::HeadClipCompare), kComissDisp32), g.at(AiAddr::HeadClipCompare),
              "comiss xmm, [disp32] (head-in-wall)");
    pf.expect(x86::matches(g.at(AiAddr::CameraNoiseCall), kMovEcxRegCall), g.at(AiAddr::CameraNoiseCall),
              "mov ecx, reg; call (camera noise)");
    pf.expect(x86::matches(g.at(AiAddr::FollowLookState), kMovClEspDisp8), g.at(AiAddr::FollowLookState),
              "mov cl, [esp+disp8] (FollowLookState)");
    // `jbe rel32`; the replacement `jmp` hard-codes the same target.
    pf.expect(x86::matches(g.at(AiAddr::CameraCorrectionBranch), sites::kCameraCorrectionBranch),
              g.at(AiAddr::CameraCorrectionBranch), "jbe +0x32B (the camera correction)");
    const std::uint8_t* store = g.at(AiAddr::FollowOnActivatePatch) + kFollowLookStoreOffset;
    pf.expect(x86::matches(store, kStoreAlDisp32) &&
                  read_u32(store + x86::kMovRmDispOffset) == kFollowLookEnabledOffset,
              store, "mov [reg+0x264], al (FollowBehavior::OnActivate)");
}

// 9: the UI, menu, loader and pad byte patches (05 §4.4, §7.7 to §7.10).
void check_ui_patch_sites(Preflight& pf) noexcept {
    const GameImage& g = pf.game();
    // movss xmm, [disp32], whose low displacement byte the patch moves on by
    // 0x10: it must be the back-buffer size's.
    auto size_load = [](const std::uint8_t* p, std::uint8_t disp_lo) {
        return x86::matches(p, kMovssDisp32) && p[x86::kMovssDispOffset] == disp_lo;
    };
    const std::uint8_t* w = g.at(AiAddr::UiMouseWidthLoad);
    const std::uint8_t* h = g.at(AiAddr::UiMouseHeightLoad);
    pf.expect(size_load(w, g.sites.ui_mouse_width_disp_lo), w, "movss xmm, [back-buffer width] (UI mouse)");
    pf.expect(size_load(h, g.sites.ui_mouse_height_disp_lo), h, "movss xmm, [back-buffer height] (UI mouse)");
    pf.expect(*g.at(AiAddr::UiInputModeBranch) == x86::kJeRel8, g.at(AiAddr::UiInputModeBranch),
              "je (the UI input mode)");

    const std::uint8_t* resume = g.at(AiAddr::ResumeGamePopMode);
    pf.expect(
        x86::matches(resume, kMovEcxAbs) && calls(resume + x86::kMovEcxAbsSize, g.at(AiAddr::GamePopMode)),
        resume, "mov ecx, [abs]; call StereoPopMode (Resume_game)");
    const std::uint8_t* pause      = g.at(AiAddr::PauseMenuPushMode);
    const std::uint8_t* pause_push = pause + x86::kMovEcxAbsSize;
    pf.expect(x86::matches(pause, kMovEcxAbs) && x86::matches(pause_push, kPushScreenMode) &&
                  calls(pause_push + x86::kPushImm8Size, g.at(AiAddr::GamePushMode)),
              pause, "mov ecx, [abs]; push 1; call StereoPushMode (Pause)");

    pf.expect(*g.at(AiAddr::LoaderSleepPush) == x86::kPushEcx, g.at(AiAddr::LoaderSleepPush),
              "push ecx (the loader sleep)");
    pf.expect(x86::matches(g.at(AiAddr::PadConnectedSet), kSetneCl), g.at(AiAddr::PadConnectedSet),
              "setne cl (the pad connected flag)");
}

// 9: the pad slots the controls replace as one patch (04 §3.3).
void check_pad_slots(Preflight& pf) noexcept {
    for (const sites::PadSlot& s : sites::kPadSlots) pf.expect_slot(s.site);
}

} // namespace

bool preflight(const GameImage& g, bool full_set, ports::Log& log) noexcept {
    if (!g.addresses.all_within(g.image_size)) {
        log.line(kTableOutsideLog);
        return false;
    }

    Preflight pf{g, log};
    check_ui_set(pf);
    // The core checks read through the inline sites, so they run only when
    // those lie in the image; a UI-set mismatch does not hide them.
    const int before_inline = pf.failures();
    if (full_set) check_inline_sites_in_image(pf);
    if (full_set && pf.failures() == before_inline) {
        check_stereo_mode_tests(pf);
        check_stereo_hand_off(pf);
        check_game_side_calls(pf);
        check_game_side_slots(pf);
        check_camera_patch_sites(pf);
        check_ui_patch_sites(pf);
        check_pad_slots(pf);
    }

    if (pf.failures() != 0) {
        ports::logf(log, kChecksFailedLog, pf.failures());
        return false;
    }
    return true;
}

} // namespace omvr::game
