// The installer's checks before any write.
//
// Unlike the original (FUN_1001d6f0), nothing is written until a preflight
// has checked the code at every core site: a half-installed stereo hand-off
// leaves the game running its own LibOVR init on our manager.

#include "game/preflight.h"

#include "core/diag/logger.h" // R: game (A2): log through an injected Log&
#include "game/patcher.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <string_view>

namespace omvr::game {

namespace {

namespace diag = core::diag;
using Game = GameImage;

// ---- preflight --------------------------------------------------------------

// Records which core sites do not hold what the patches expect. Each failure
// is logged with the site's RVA and what was found there, so one run names
// every mismatch.
class Preflight {
public:
    explicit Preflight(const Game& g) noexcept : g_{g} {}

    // `site` holds what `what` describes.
    void expect(bool ok, const std::uint8_t* site, std::string_view what) noexcept {
        if (ok) return;
        ++failures_;
        char hex[3 * kShownBytes];
        diag::logf("[hook] check failed at RVA {:08X}: expected {}; found {}", rva(site), what,
                   bytes_at(site, hex));
    }

    // The vtable slot (or code pointer) at `slot` holds `expected`.
    void expect_slot(const std::uint8_t* slot, const std::uint8_t* expected, std::string_view what) noexcept {
        expect_slot_value(slot, read_u32(slot) == address_of(expected), what);
    }

    // The vtable slot at `slot` holds a code address inside the image: for
    // a slot whose function this build's table does not give.
    void expect_slot_in_image(const std::uint8_t* slot, std::string_view what) noexcept {
        expect_slot_value(slot, in_image(read_u32(slot)), what);
    }

    // An address the checks would read lies outside the image: itself a
    // failed check, and nothing is read there.
    void expect_rva_in_image(std::uint32_t site_rva, std::string_view what) noexcept {
        if (site_rva < g_.image_size) return;
        ++failures_;
        diag::logf("[hook] check failed: {} at RVA {:08X} lies outside AI.exe (size {:08X})", what, site_rva,
                   g_.image_size);
    }

    bool in_image(std::uint32_t va) const noexcept {
        return va - address_of(g_.base) < g_.image_size;
    }

    // `*p` points at a NUL-terminated `s` inside the image.
    bool points_to(const std::uint8_t* p, std::string_view s) const noexcept {
        const std::uint32_t va = read_u32(p);
        if (!in_image(va) || !in_image(va + static_cast<std::uint32_t>(s.size()))) return false;
        const auto* str = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(va));
        return _strnicmp(str, s.data(), s.size()) == 0 && str[s.size()] == '\0';
    }

    int failures() const noexcept { return failures_; }

private:
    static constexpr std::size_t kShownBytes = 8;

    std::uint32_t rva(const std::uint8_t* p) const noexcept { return address_of(p) - address_of(g_.base); }

    void expect_slot_value(const std::uint8_t* slot, bool ok, std::string_view what) noexcept {
        if (ok) return;
        ++failures_;
        const std::uint32_t value = read_u32(slot);
        if (in_image(value)) {
            diag::logf("[hook] check failed at RVA {:08X}: expected {}; found RVA {:08X}", rva(slot), what,
                       value - address_of(g_.base));
        } else {
            diag::logf("[hook] check failed at RVA {:08X}: expected {}; found {:08X}, outside AI.exe", rva(slot),
                       what, value);
        }
    }

    // Up to kShownBytes bytes at `p` as hex, never past the image's end.
    std::string_view bytes_at(const std::uint8_t* p, char (&out)[3 * kShownBytes]) const noexcept {
        static constexpr char kHex[] = "0123456789abcdef";
        const std::uint32_t at = rva(p);
        const std::size_t n = at < g_.image_size ? std::min<std::size_t>(kShownBytes, g_.image_size - at) : 0;
        std::size_t k = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (i) out[k++] = ' ';
            out[k++] = kHex[p[i] >> 4];
            out[k++] = kHex[p[i] & 0xF];
        }
        return {out, k};
    }

    const Game& g_;
    int failures_ = 0;
};

// The UI set (01 section 2.3): the archive path, the intro push, the UI's
// external-call slot and the two menu Open slots. Checked for every run.
void check_ui_set(Preflight& pf) noexcept {
    auto at = [](AiAddr a) { return ai_address(a); };
    pf.expect(pf.points_to(at(AiAddr::UiPakPathPtr), kStockUiPak), at(AiAddr::UiPakPathPtr),
              "a pointer to \"data\\ui.pak\" (UiPakPathPtr)");
    {
        const std::uint8_t* p = at(AiAddr::BootupMoviePush);
        pf.expect(p[0] == 0x68 && pf.points_to(p + 1, kShowBootup), p, "push \"ShowBootup\" (ShowBootupPush)");
    }
    pf.expect_slot(at(AiAddr::CmdDispatchSlot), at(AiAddr::OriginalCmdHandler),
                   "the UI's external-call handler (UiExternalCallSlot)");
    pf.expect_slot(at(AiAddr::MenuVtable), at(AiAddr::MenuOpen), "the front-end menu's open (slot 0)");
    pf.expect_slot(at(AiAddr::PauseMenuVtable), at(AiAddr::PauseMenuOpen), "the pause menu's open (slot 0)");
}

} // namespace

bool preflight(const Game& g, const AddressMap& addresses, bool full_set) noexcept {
    if (!addresses.all_within(g.image_size)) {
        diag::log_line("[hook] check failed: the address table points outside AI.exe; nothing patched, "
                       "game runs as shipped");
        return false;
    }

    Preflight pf{g};
    check_ui_set(pf);
    if (full_set) {
        // Inline sites first: the checks below read them.
        for (std::uint32_t rva : g.sites.stereo_mode_tests) pf.expect_rva_in_image(rva, "a Rift test");
        pf.expect_rva_in_image(g.sites.left_stick_prompt_call_2, "the second left-stick prompt call");
    }
    if (full_set && pf.failures() == 0) {
        auto at = [](AiAddr a) { return ai_address(a); };
        const std::uint8_t* push_mode = at(AiAddr::GamePushMode);
        const std::uint8_t* pop_mode  = at(AiAddr::GamePopMode);

        // 6: `cmp reg, eax`.
        for (std::uint32_t rva : g.sites.stereo_mode_tests) {
            const std::uint8_t* p = g.base + rva;
            pf.expect(p[0] == 0x3B && (p[1] & 0xC7) == 0xC0, p, "cmp reg, eax (a Rift test)");
        }

        // 7: `push 0xb0` before new(stereo manager), and the store of the
        // result 0x24 bytes on that the hand-off's `jmp short +0x1D` lands on:
        // `mov [reg+disp32], eax`.
        {
            const std::uint8_t* p = at(AiAddr::StereoManagerAlloc);
            pf.expect(p[0] == 0x68 && read_u32(p + 1) == 0xB0 && p[0x24] == 0x89 && (p[0x25] & 0xF8) == 0x80, p,
                      "push 0xB0, and mov [reg+disp32], eax at +0x24 (StereoObjectAlloc)");
        }
        for (AiAddr a : {AiAddr::HeadPoseCallA, AiAddr::HeadPoseCallB}) {
            const std::uint8_t* p = at(a);
            pf.expect(p[0] == 0x8B && (p[1] & 0xC0) == 0x40 && p[2] == 0x2C, p, "mov eax, [reg+0x2C] (a recenter call)");
        }
        // The table does not name these callees: the opcode only.
        pf.expect(*at(AiAddr::CreateDeviceCall) == kCall, at(AiAddr::CreateDeviceCall), "call StereoInit");
        pf.expect(*at(AiAddr::HmdResolutionCall) == kCall, at(AiAddr::HmdResolutionCall), "call the display size");
        pf.expect(*at(AiAddr::RenderTargetSizeCall) == kCall, at(AiAddr::RenderTargetSizeCall),
                  "call the render-target size");

        // 8: call sites and vtable slots.
        pf.expect(*at(AiAddr::StereoUpdateCall) == kCall, at(AiAddr::StereoUpdateCall), "call StereoUpdate");
        pf.expect(calls(at(AiAddr::VirtualScreenCall), at(AiAddr::BuildUiQuad)), at(AiAddr::VirtualScreenCall),
                  "call BuildUiQuad (the virtual screen)");
        pf.expect(calls(at(AiAddr::HudPanelCall), at(AiAddr::BuildUiQuad)), at(AiAddr::HudPanelCall),
                  "call BuildUiQuad (the HUD panel)");
        for (AiAddr a : {AiAddr::PushModeCallA, AiAddr::PushModeCallB, AiAddr::PushModeCallC}) {
            pf.expect(calls(at(a), push_mode), at(a), "call StereoPushMode");
        }
        for (AiAddr a : {AiAddr::PopModeCallA, AiAddr::PopModeCallB, AiAddr::PopModeCallC}) {
            pf.expect(calls(at(a), pop_mode), at(a), "call StereoPopMode");
        }
        {
            // The table does not name the callee: both calls reach the same
            // function inside AI.exe.
            const std::uint8_t* first  = at(AiAddr::LeftStickPromptCall);
            const std::uint8_t* second = g.base + g.sites.left_stick_prompt_call_2;
            pf.expect(*first == kCall && pf.in_image(call_target(first)), first,
                      "call into AI.exe (the left-stick prompt)");
            pf.expect(*first == kCall && *second == kCall && call_target(first) == call_target(second), second,
                      "the same call as LeftStickPromptCall (the second left-stick prompt)");
        }
        const std::uint8_t* follow = at(AiAddr::FollowVtable);
        pf.expect_slot(follow, at(AiAddr::FollowOnActivatePatch), "FollowBehavior::OnActivate (slot 0)");
        pf.expect_slot(follow + 4, at(AiAddr::FollowOnDeactivate), "FollowBehavior::OnDeactivate (slot 1)");
        pf.expect_slot(at(AiAddr::UiEntityMessageSlot), at(AiAddr::UiEntityOnMessage),
                       "the UI entity's message handler");
        pf.expect_slot(at(AiAddr::MenuVtable) + 8, at(AiAddr::MenuClose), "the front-end menu's close (slot 2)");
        pf.expect_slot(at(AiAddr::PauseMenuVtable) + 8, at(AiAddr::PauseMenuClose),
                       "the pause menu's close (slot 2)");
        pf.expect_slot(at(AiAddr::LoadingScreenVtable), at(AiAddr::LoadingScreenOpen),
                       "the loading screen's open (slot 0)");
        pf.expect_slot(at(AiAddr::LoadingScreenVtable) + 8, at(AiAddr::LoadingScreenClose),
                       "the loading screen's close (slot 2)");
        pf.expect_slot(at(AiAddr::AttractScreenVtable), at(AiAddr::AttractOpen),
                       "the attract screen's open (slot 0)");
        // A shared empty method the table does not name.
        pf.expect_slot_in_image(at(AiAddr::AttractScreenVtable) + 8, "code in AI.exe (attract screen slot 2)");

        // 9: byte patches.
        pf.expect(*at(AiAddr::SsaoSettingBranch) == 0x75, at(AiAddr::SsaoSettingBranch), "jne (the SSAO branch)");
        {
            const std::uint8_t* p = at(AiAddr::HeadClipCompare);  // comiss xmm, [disp32]
            pf.expect(p[0] == 0x0F && p[1] == 0x2F && (p[2] & 0xC7) == 0x05, p, "comiss xmm, [disp32] (head-in-wall)");
        }
        {
            const std::uint8_t* p = at(AiAddr::CameraNoiseCall);  // mov ecx, reg; call
            pf.expect(p[0] == 0x8B && (p[1] & 0xC0) == 0xC0 && p[2] == kCall, p, "mov ecx, reg; call (camera noise)");
        }
        {
            const std::uint8_t* p = at(AiAddr::FollowLookState);  // mov cl, [esp+disp8]
            pf.expect(p[0] == 0x8A && p[1] == 0x4C && p[2] == 0x24, p, "mov cl, [esp+disp8] (FollowLookState)");
        }
        {
            // `jbe rel32`; the replacement `jmp` hard-codes the same target.
            constexpr std::uint8_t kJbe[6] = {0x0F, 0x86, 0x2B, 0x03, 0x00, 0x00};
            const std::uint8_t* p = at(AiAddr::CameraCorrectionBranch);
            pf.expect(std::memcmp(p, kJbe, sizeof(kJbe)) == 0, p, "jbe +0x32B (the camera correction)");
        }
        {
            const std::uint8_t* p = at(AiAddr::FollowOnActivatePatch) + 8;  // mov [reg+0x264], al
            pf.expect(p[0] == 0x88 && (p[1] & 0xF8) == 0x80 && read_u32(p + 2) == 0x264, p,
                      "mov [reg+0x264], al (FollowBehavior::OnActivate)");
        }
        {
            // movss xmm, [disp32], whose low displacement byte the patch
            // moves on by 0x10: it must be the back-buffer size's.
            const std::uint8_t* w = at(AiAddr::UiMouseWidthLoad);
            const std::uint8_t* h = at(AiAddr::UiMouseHeightLoad);
            auto load = [](const std::uint8_t* p, std::uint8_t lo) {
                return p[0] == 0xF3 && p[1] == 0x0F && p[2] == 0x10 && (p[3] & 0xC7) == 0x05 && p[4] == lo;
            };
            pf.expect(load(w, g.sites.ui_mouse_width_disp_lo), w, "movss xmm, [back-buffer width] (UI mouse)");
            pf.expect(load(h, g.sites.ui_mouse_height_disp_lo), h, "movss xmm, [back-buffer height] (UI mouse)");
        }
        pf.expect(*at(AiAddr::UiInputModeBranch) == 0x74, at(AiAddr::UiInputModeBranch), "je (the UI input mode)");
        {
            const std::uint8_t* p = at(AiAddr::ResumeGamePopMode);  // mov ecx, [abs]; call PopMode
            pf.expect(p[0] == 0x8B && p[1] == 0x0D && calls(p + 6, pop_mode), p,
                      "mov ecx, [abs]; call StereoPopMode (Resume_game)");
        }
        {
            const std::uint8_t* p = at(AiAddr::PauseMenuPushMode);  // mov ecx, [abs]; push 1; call PushMode
            pf.expect(p[0] == 0x8B && p[1] == 0x0D && p[6] == 0x6A && p[7] == 0x01 && calls(p + 8, push_mode), p,
                      "mov ecx, [abs]; push 1; call StereoPushMode (Pause)");
        }
        pf.expect(*at(AiAddr::LoaderSleepPush) == 0x51, at(AiAddr::LoaderSleepPush), "push ecx (the loader sleep)");
        {
            const std::uint8_t* p = at(AiAddr::PadConnectedSet);  // setne cl
            pf.expect(p[0] == 0x0F && p[1] == 0x95 && p[2] == 0xC1, p, "setne cl (the pad connected flag)");
        }
        {
            // Pad slots 1, 2, 3, 5 and 11, relative to slot 1.
            const std::uint8_t* pad = at(AiAddr::PadVtable);
            pf.expect_slot(pad + 0x00, at(AiAddr::PadIsDown), "the pad's IsDown (slot 1)");
            pf.expect_slot(pad + 0x04, at(AiAddr::PadGetValue), "the pad's GetValue (slot 2)");
            pf.expect_slot(pad + 0x08, at(AiAddr::PadGetAxis), "the pad's GetAxis (slot 3)");
            pf.expect_slot(pad + 0x10, at(AiAddr::PadControlToName), "the pad's ControlToName (slot 5)");
            pf.expect_slot(pad + 0x28, at(AiAddr::PadHasActivity), "the pad's HasActivity (slot 11)");
        }
    }

    if (pf.failures() != 0) {
        diag::logf("[hook] {} core check(s) failed: nothing patched, game runs as shipped", pf.failures());
        return false;
    }
    return true;
}

} // namespace omvr::game
