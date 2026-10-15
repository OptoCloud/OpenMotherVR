// Hook installer: patches AI.exe in-process at DLL_PROCESS_ATTACH.
//
// Reconstruction of: FUN_1001d6f0 @ 0x1001d6f0 (the original 4142-byte
// monolithic installer). Every AI.exe address comes from the per-build table
// in ai_addresses.h; the original keeps one table for the 2015 launch build
// and one for the 2020 patch and picks by AI.exe's PE timestamp
// (FUN_10002250 -> FUN_10001760).
//
// Unlike the original, nothing is written until a preflight has checked the
// code at every core site: a half-installed stereo hand-off leaves the game
// running its own LibOVR init on our manager.
//
// Also unlike the original, runtime None patches only the UI set (the
// options page and its settings push): flat play is the unmodded game.

#include "hook_installer.h"

#include "ai_addresses.h"
#include "ai_patches.h"
#include "recon_addresses.h"
#include "memory_patch.h"
#include "config/config.h"
#include "diag/log.h"
#include "integration/exe_check.h"
#include "integration/payloads.h"
#include "integration/script_bridge.h"
#include "stereo/game_hooks.h"
#include "stereo/stereo_device.h"
#include "input/gamepad.h"
#include "runtime/vr_detect.h"
#include "game/game_events.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string_view>

namespace mothervr::hook {

namespace {

using integration::AIVersion;

// Strings AI.exe is pointed at. They must outlive the process's use of them,
// hence static storage. Originally std::string members at +0x00 and +0x18 of
// the singleton built in FUN_10002090.
constexpr char kMotherPakPath[] = "data\\mother.pak";
constexpr char kSkipBootup[]    = "SkipBootup";
// What the game's own pointers lead to before we replace them.
constexpr std::string_view kStockUiPak   = "data\\ui.pak";
constexpr std::string_view kShowBootup   = "ShowBootup";

constexpr std::uint8_t kCall = 0xE8;  // call rel32
constexpr std::uint8_t kNop  = 0x90;

// The UI mouse patch moves the load 0x10 bytes on, from the back-buffer size
// to the UI reference size in the same display info struct.
constexpr std::uint8_t kUiReferenceSizeOffset = 0x10;

// Set by install() before anything else; read by the D3D11 proxy later.
std::atomic<bool> g_ui_set_only{false};

std::uint32_t address_of(const void* p) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

std::uint32_t read_u32(const std::uint8_t* p) noexcept {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

template <class Fn>
bool write_fn(std::uint8_t* slot, Fn* fn) noexcept {
    return write_dword(slot, address_of(reinterpret_cast<const void*>(fn)));
}

bool patch(std::uint8_t* site, std::initializer_list<std::uint8_t> bytes) noexcept {
    return write_bytes(site, std::data(bytes), bytes.size());
}

// Target of the `call rel32` at `site`.
std::uint32_t call_target(const std::uint8_t* site) noexcept {
    return address_of(site + 5) + read_u32(site + 1);
}

// True if `site` holds `call rel32` to `target`: the code is what a fix
// that redirects the call was written for.
bool calls(const std::uint8_t* site, const std::uint8_t* target) noexcept {
    return *site == kCall && call_target(site) == address_of(target);
}

// Point the `call rel32` at `site` to `fn`. The replacement must have the
// original callee's ABI.
template <class Fn>
bool redirect_call(std::uint8_t* site, Fn* fn, const char* what) noexcept {
    if (*site != kCall) {
        diag::logf("[hook] {}: no call at {}, not patched", what, static_cast<void*>(site));
        return false;
    }
    const auto target = address_of(reinterpret_cast<const void*>(fn));
    if (!write_dword(site + 1, target - address_of(site + 5))) {
        diag::logf("[hook] {}: call at {} not redirected", what, static_cast<void*>(site));
        return false;
    }
    return true;
}

struct Game {
    std::uint8_t*      base;
    std::uint32_t      image_size;
    AIVersion          version;
    const InlineSites& sites;
};

std::uint32_t image_size_of(const std::uint8_t* base) noexcept {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->OptionalHeader.SizeOfImage;
}

// ---- feature areas ----------------------------------------------------------

// One outcome line per feature area at attach, so a log shows which parts of
// the mod are in place and, for those that are not, why.
void area_on(std::string_view area, int sites) noexcept {
    diag::logf("[hook] {}: in place on {} sites", area, sites);
}

void area_off(std::string_view area, std::string_view why) noexcept {
    diag::logf("[hook] {}: off; {}", area, why);
}

// The areas only the full patch set has, in the order install() reports them.
constexpr std::string_view kFullSetAreas[] = {
    "stereo path", "game behaviour", "controls", "per-eye reflections",
    "stereo anti-aliasing", "culling", "head-in-wall delay", "event tap",
};

// ---- runtime for this run ---------------------------------------------------

// The runtime the patch set is decided for: a -vrforce* switch, else the
// config file's VRRuntime. runtime::detect() applies the same switches when
// the game creates its stereo device; it cannot run here (it probes the
// runtimes, which is not allowed under the loader lock), but reading the
// switches alone can.
config::VRRuntime runtime_for_this_run(bool& forced) noexcept {
    const auto chosen = runtime::forced_runtime();
    forced = chosen.has_value();
    return chosen.value_or(config::current().vr_runtime);
}

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

// Check every site the core steps write, before any of them is written.
// Opcodes and operands that do not depend on the build are checked exactly;
// call targets where the table names the callee or two sites share one;
// vtable slots against the stock method the table names.
bool preflight(const Game& g, bool full_set) noexcept {
    if (!ai_addresses_within(g.image_size)) {
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

// ---- steps --------------------------------------------------------------------

// 1: the original refuses to touch an unknown build ("Exe check failed!"):
// every address is build-specific.
std::optional<AIVersion> select_address_table(std::uint8_t* ai_base) noexcept {
    const auto version = integration::identify_ai_exe();
    if (!version) {
        diag::logf("[hook] unknown AI.exe build (timestamp=0x{:08X}): no patches, game runs as shipped",
                   integration::ai_exe_timestamp_raw());
        return std::nullopt;
    }
    init_ai_addresses(ai_base, *version);
    diag::logf("[hook] AI.exe build {} recognised; image base {:08X}",
               *version == AIVersion::Patched_2020 ? "2020" : "2015", reinterpret_cast<std::uintptr_t>(ai_base));
    return version;
}

// 2: the options page lives in the mod's UI archive. The original aborts
// without it; we carry on, since VR works without the menu. True when the
// game now opens the mod's archive.
bool redirect_ui_archive(HMODULE self, const std::filesystem::path& game_dir, std::int64_t stamp) noexcept {
    if (integration::deploy_mother_pak(self, game_dir, stamp) &&
        write_dword(ai_address(AiAddr::UiPakPathPtr), address_of(kMotherPakPath))) {
        diag::log_line("[hook] UI archive redirected to data\\mother.pak");
        return true;
    }
    diag::log_line("[hook] no mod UI archive: options page unavailable, continuing");
    // The stock movies register no VR glyph ids: keep the game's pad glyphs.
    input::use_stock_glyphs_only();
    return false;
}

// 4: the site is `push offset "ShowBootup"`; replace the pushed string. The
// skip-intro function is defined by the mod's UI movie only: with the stock
// archive the boot sequence would call a name its movie lacks, so the intro
// plays then.
void skip_intro(bool mod_archive) noexcept {
    if (!mod_archive) {
        diag::log_line("[hook] startup intro: plays; the options archive is not in use");
    } else if (write_dword(ai_address(AiAddr::BootupMoviePush) + 1, address_of(kSkipBootup))) {
        diag::log_line("[hook] startup intro: skipped");
    } else {
        diag::log_line("[hook] startup intro: plays; the operand could not be written");
    }
}

// 5: the game functions the script bridge calls, and the console command
// dispatch through handle_mvr_command.
bool hook_script_bridge() noexcept {
    using namespace integration;
    g_fire_str2    = reinterpret_cast<PFN_FireStr2  >(ai_address(AiAddr::FireStr2));
    g_fire_bool    = reinterpret_cast<PFN_FireBool  >(ai_address(AiAddr::FireBool));
    g_fire_int     = reinterpret_cast<PFN_FireInt   >(ai_address(AiAddr::FireInt));
    g_fire_float   = reinterpret_cast<PFN_FireFloat >(ai_address(AiAddr::FireFloat));
    g_original_cmd = reinterpret_cast<PFN_CmdHandler>(ai_address(AiAddr::OriginalCmdHandler));
    g_original_menu_open = reinterpret_cast<PFN_MenuMethod>(ai_address(AiAddr::MenuOpen));
    g_exit_request       = ai_address(AiAddr::ExitRequest);
    g_display_reset_flag = ai_address(AiAddr::DisplayResetFlag);
    diag::logf("[hook] script callbacks: fire_bool={:08X} fire_int={:08X} fire_float={:08X}",
               reinterpret_cast<std::uintptr_t>(g_fire_bool),
               reinterpret_cast<std::uintptr_t>(g_fire_int),
               reinterpret_cast<std::uintptr_t>(g_fire_float));

    // All console commands (the UI's ExternalInterface.call("mvr_...")) now
    // pass through handle_mvr_command first.
    if (!write_fn(ai_address(AiAddr::CmdDispatchSlot), &handle_mvr_command)) return false;
    diag::logf("[hook] cmd dispatch slot {:08X} -> handle_mvr_command",
               reinterpret_cast<std::uintptr_t>(ai_address(AiAddr::CmdDispatchSlot)));

    // The Fire* methods need the game's menu object, so settings are pushed
    // from a hook on it rather than from DLL code with no object at hand.
    return write_fn(ai_address(AiAddr::MenuVtable), &on_menu_open);
}

// Runtime None: the pause menu's open runs the game's method and pushes the
// settings, so the options page reached from the pause menu shows them. No
// stereo mode change: there is no stereo device.
integration::PFN_MenuMethod g_pause_open = nullptr;

// From: FUN_10020ca0, without its stereo mode push.
void __fastcall pause_open_settings_only(void* self, void* /*edx*/) noexcept {
    if (g_pause_open) g_pause_open(self, nullptr);
    integration::push_settings_to_game(self);
}

bool hook_pause_settings_push() noexcept {
    std::uint8_t* slot = ai_address(AiAddr::PauseMenuVtable);
    g_pause_open = *reinterpret_cast<integration::PFN_MenuMethod*>(slot);
    return write_fn(slot, &pause_open_settings_only);
}

// 6
bool force_stereo_mode(const Game& g) noexcept {
    if (!force_stereo_mode_tests(g.base, g.version)) return false;
    diag::log_line("[hook] stereo/render patches applied");
    return true;
}

// 7: the game's hidden LibOVR stereo path gets our manager and device.
constexpr int kStereoDeviceSites = 6;   // hand-off, two recenter sites, three calls
bool install_stereo_device(const Game& /*g*/) noexcept {
    stereo::initialize();

    // `push 0xb0; ...; call new; ...; call ctor` becomes
    // `mov eax, &manager; jmp short` to the store of the result.
    std::uint8_t* alloc = ai_address(AiAddr::StereoManagerAlloc);
    // Two recenter sites call device slot 11 (+0x2C); make them call
    // slot 21 (+0x54), the yaw-only reference pose.
    constexpr std::uint8_t kSlot21 = 0x54;
    const bool ok =
        patch(alloc, {0xB8}) && write_dword(alloc + 1, address_of(&stereo::manager())) &&
        patch(alloc + 5, {0xEB, 0x1D}) &&
        patch(ai_address(AiAddr::HeadPoseCallA) + 2, {kSlot21}) &&
        patch(ai_address(AiAddr::HeadPoseCallB) + 2, {kSlot21}) &&
        redirect_call(ai_address(AiAddr::CreateDeviceCall), &stereo::create_device, "CreateDevice") &&
        redirect_call(ai_address(AiAddr::HmdResolutionCall), &stereo::get_hmd_resolution, "GetHmdResolution") &&
        redirect_call(ai_address(AiAddr::RenderTargetSizeCall), &stereo::get_render_target_size,
                      "GetRenderTargetSize");
    if (ok) {
        diag::logf("[hook] stereo manager at {} handed to the game", static_cast<void*>(&stereo::manager()));
    }
    return ok;
}

// 8
constexpr int kGameSideSites = 19;   // 10 vtable slots, 9 calls
bool install_game_side_hooks() noexcept {
    using namespace stereo;
    auto slot = [](AiAddr a, int index) {
        return *reinterpret_cast<void**>(ai_address(a) + 4 * index);
    };
    // Save the originals before their slots are replaced.
    install_game_hooks({
        .follow_on_activate   = slot(AiAddr::FollowVtable, 0),
        .follow_on_deactivate = slot(AiAddr::FollowVtable, 1),
        .ui_on_message        = slot(AiAddr::UiEntityMessageSlot, 0),
        .menu_close           = slot(AiAddr::MenuVtable, 2),
        .pause_open           = slot(AiAddr::PauseMenuVtable, 0),
        .pause_close          = slot(AiAddr::PauseMenuVtable, 2),
        .loading_open         = slot(AiAddr::LoadingScreenVtable, 0),
        .loading_close        = slot(AiAddr::LoadingScreenVtable, 2),
        .attract_open         = slot(AiAddr::AttractScreenVtable, 0),
    });

    const bool slots_ok =
        // Track the first-person camera; watch cutscenes and level fades.
        write_fn(ai_address(AiAddr::FollowVtable) + 0, &follow_on_activate) &&
        write_fn(ai_address(AiAddr::FollowVtable) + 4, &follow_on_deactivate) &&
        write_fn(ai_address(AiAddr::UiEntityMessageSlot), &ui_on_message) &&
        // Menus, loading and the title video show on the virtual screen.
        write_fn(ai_address(AiAddr::MenuVtable) + 8, &menu_close) &&
        write_fn(ai_address(AiAddr::PauseMenuVtable) + 0, &pause_open) &&
        write_fn(ai_address(AiAddr::PauseMenuVtable) + 8, &pause_close) &&
        write_fn(ai_address(AiAddr::LoadingScreenVtable) + 0, &loading_open) &&
        write_fn(ai_address(AiAddr::LoadingScreenVtable) + 8, &loading_close) &&
        write_fn(ai_address(AiAddr::AttractScreenVtable) + 0, &attract_open) &&
        write_fn(ai_address(AiAddr::AttractScreenVtable) + 8, &attract_close);
    if (!slots_ok) return false;

    if (!redirect_call(ai_address(AiAddr::StereoUpdateCall), &stereo_update, "Stereo::Update") ||
        !redirect_call(ai_address(AiAddr::VirtualScreenCall), &place_virtual_screen, "virtual screen") ||
        !redirect_call(ai_address(AiAddr::HudPanelCall), &place_hud_panel, "HUD panel")) {
        return false;
    }
    for (AiAddr a : {AiAddr::PushModeCallA, AiAddr::PushModeCallB, AiAddr::PushModeCallC}) {
        if (!redirect_call(ai_address(a), &push_mode, "PushMode")) return false;
    }
    for (AiAddr a : {AiAddr::PopModeCallA, AiAddr::PopModeCallB, AiAddr::PopModeCallC}) {
        if (!redirect_call(ai_address(a), &pop_mode, "PopMode")) return false;
    }
    return true;
}

// 8b: planar reflections render per eye.
constexpr std::string_view kReflections = "per-eye reflections";
void fix_reflections(const Game& g, const ReconRvas& r) noexcept {
    std::uint8_t* site = g.base + r.reflection_render_call;
    if (!calls(site, g.base + r.reflection_render)) {
        diag::logf("[hook] {}: off; ReflectionRenderCall (RVA {:08X}) is not the expected call",
                   kReflections, r.reflection_render_call);
        return;
    }
    if (!redirect_call(site, &stereo::render_reflection, "reflections")) return;
    // render_reflection renders flat until the clear gate is in, so the
    // gate goes in only once the call reaches it.
    if (stereo::install_reflection_fix({
            .render              = g.base + r.reflection_render,
            .apply_eye_transform = g.base + r.apply_eye_transform,
            .update_frustum      = g.base + r.pov_update_frustum,
            .renderer_eye_owner  = reinterpret_cast<void**>(g.base + r.renderer_eye_owner),
            .clear               = g.base + r.clear_render_target,
        })) {
        area_on(kReflections, 2);
    } else {
        diag::logf("[hook] {}: off; the clear gate at RendererClear (RVA {:08X}) does not hold the expected "
                   "prologue, reflections stay mono", kReflections, r.clear_render_target);
    }
}

// 8b: anti-aliasing in stereo. Both sites or neither: without the SMAA one,
// T2x would run temporally on the side-by-side image.
constexpr std::string_view kStereoAa = "stereo anti-aliasing";
void fix_anti_aliasing(const Game& g, const ReconRvas& r) noexcept {
    std::uint8_t* draw_site = g.base + r.stereo_output_draw_call;
    std::uint8_t* smaa_site = g.base + r.smaa_call;
    const std::uint8_t* getter = g.base + r.postfx_instance;  // mov eax, imm32; ret
    constexpr std::uint8_t kAaPrologue[6] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0};
    if (!calls(draw_site, g.base + r.fullscreen_draw) || !calls(smaa_site, g.base + r.smaa) ||
        getter[0] != 0xB8 || getter[5] != 0xC3 ||
        std::memcmp(g.base + r.aa_pass, kAaPrologue, sizeof(kAaPrologue)) != 0) {
        diag::logf("[hook] {}: off; unexpected code at StereoOutputDrawCall (RVA {:08X}), SmaaCall (RVA {:08X}), "
                   "PostFxInstance or AaPass", kStereoAa, r.stereo_output_draw_call, r.smaa_call);
        return;
    }
    stereo::install_stereo_aa({
        .fullscreen_draw = g.base + r.fullscreen_draw,
        .postfx          = g.base + r.postfx_instance,
        .aa_pass         = g.base + r.aa_pass,
        .smaa            = g.base + r.smaa,
    });
    if (redirect_call(smaa_site, &stereo::run_smaa, "SMAA") &&
        redirect_call(draw_site, &stereo::stereo_final_draw, "stereo output draw")) {
        area_on(kStereoAa, 2);
    }
}

// 8b: object culling. In stereo the distance/LOD test passes in every POV
// group and the main POV's centre-view occlusion test passes; the frustum
// and the shadow lights' view-volume test stay the game's.
constexpr std::string_view kCulling = "culling";
void fix_culling(const Game& g, const ReconRvas& r) noexcept {
    // mov eax,[esi+0x5C]; test eax,eax; je +0xF; push eax; push esi; push ebx:
    // the occlusion test takes the context's occlusion buffer, which only
    // the main POV has, so the replacement needs no phase check.
    constexpr std::uint8_t kOcclusionArgs[] = {0x8B, 0x46, 0x5C, 0x85, 0xC0, 0x74, 0x0F, 0x50, 0x56, 0x53};
    const std::uint8_t* occlusion_args = g.base + r.cull_occlusion_call - sizeof(kOcclusionArgs);
    if (!calls(g.base + r.cull_distance_call, g.base + r.cull_distance) ||
        !calls(g.base + r.cull_occlusion_call, g.base + r.cull_occlusion) ||
        std::memcmp(occlusion_args, kOcclusionArgs, sizeof(kOcclusionArgs)) != 0) {
        diag::logf("[hook] {}: off; unexpected code at CullDistanceCall (RVA {:08X}) or CullPlaneTestACall "
                   "(RVA {:08X})", kCulling, r.cull_distance_call, r.cull_occlusion_call);
        return;
    }
    stereo::install_cull_fix({
        .distance  = g.base + r.cull_distance,
        .occlusion = g.base + r.cull_occlusion,
    });
    if (redirect_call(g.base + r.cull_distance_call, &stereo::cull_distance, "distance cull") &&
        redirect_call(g.base + r.cull_occlusion_call, &stereo::cull_occlusion, "occlusion cull")) {
        area_on(kCulling, 2);
        diag::log_line("[hook] culling: in stereo the distance test passes and the main view's occlusion test "
                       "passes");
    }
}

// 8b: head-in-wall blackout. In stereo a fade waits 0.6 s before it shows.
constexpr std::string_view kHeadClip = "head-in-wall delay";
void fix_head_clip_fade(const Game& g, const ReconRvas& r) noexcept {
    if (!calls(g.base + r.head_clip_fade_call, g.base + r.head_clip_fade)) {
        diag::logf("[hook] {}: off; HeadClipFadeCall (RVA {:08X}) is not the expected call", kHeadClip,
                   r.head_clip_fade_call);
        return;
    }
    stereo::install_head_clip_delay(g.base + r.head_clip_fade);
    if (redirect_call(g.base + r.head_clip_fade_call, &stereo::head_clip_fade, "HeadClipFade")) {
        area_on(kHeadClip, 1);
    }
}

// 8b: interaction and hiding state from the scripts' event strings, for the
// features that behave differently during interactions.
constexpr std::string_view kEventTap = "event tap";
void tap_game_events(const Game& g, const ReconRvas& r) noexcept {
    if (game::install_event_tap(g.base + r.event_tap)) {
        area_on(kEventTap, 1);
    } else {
        diag::logf("[hook] {}: off; ScriptEventTap (RVA {:08X}) does not hold the expected no-op or could not "
                   "be detoured", kEventTap, r.event_tap);
    }
}

// 8b: each fix checks its own sites and stays off on its own. A fix whose
// addresses are unknown for this build (0) is reported off for that reason.
void install_reconstruction_fixes(const Game& g) noexcept {
    constexpr std::string_view kNoAddress = "this build has no address for it";
    const ReconRvas& r = recon_rvas(g.version);
    if (r.reflection_render_call != 0) fix_reflections(g, r); else area_off(kReflections, kNoAddress);
    if (r.stereo_output_draw_call != 0) fix_anti_aliasing(g, r); else area_off(kStereoAa, kNoAddress);
    if (r.cull_distance_call != 0) fix_culling(g, r); else area_off(kCulling, kNoAddress);
    if (r.head_clip_fade_call != 0) fix_head_clip_fade(g, r); else area_off(kHeadClip, kNoAddress);
    if (r.event_tap != 0) tap_game_events(g, r); else area_off(kEventTap, kNoAddress);
}

// 9: camera, UI and loader byte patches.
constexpr int kBytePatchSites = 12;
bool apply_byte_patches(const Game& g) noexcept {
    auto at = [](AiAddr a, std::ptrdiff_t offset = 0) { return ai_address(a) + offset; };
    const bool ok =
        // SSAO never runs: it bleeds across the side-by-side eyes.
        patch(at(AiAddr::SsaoSettingBranch), {0xEB, 0x29}) &&
        // Head-in-wall blackout threshold: ours, so it follows the clipping
        // blinders option.
        write_dword(at(AiAddr::HeadClipCompare, 3), address_of(&stereo::head_clip_threshold())) &&
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

// 9, controls: VR input goes through the game's gamepad, five of its vtable
// slots replaced as one patch. Only with those in place does the pad count
// as always connected and do the left-stick prompts ask the remapped
// bindings; without them the real pad keeps the game's own behaviour
// (gamepad.h, spec 04 section 3.3). 0 when the slots were not written, -1
// when a later write failed.
constexpr int kControlSites = 8;   // five pad slots, connected flag, two prompt calls
int install_controls(const Game& g) noexcept {
    if (!input::install(ai_address(AiAddr::PadVtable),
                        reinterpret_cast<void**>(ai_address(AiAddr::InputManagerPtr)))) {
        return 0;
    }
    const bool ok =
        // The pad always counts as connected: VR controllers come through it.
        patch(ai_address(AiAddr::PadConnectedSet), {0xB1, 0x01, kNop}) &&
        redirect_call(ai_address(AiAddr::LeftStickPromptCall), &stereo::is_move_on_left_stick,
                      "left stick prompt") &&
        redirect_call(g.base + g.sites.left_stick_prompt_call_2, &stereo::is_move_on_left_stick,
                      "left stick prompt");
    return ok ? kControlSites : -1;
}

bool failed(const char* step) noexcept {
    diag::logf("[hook] {}: a write failed after the preflight; earlier patches stay, later core patches "
               "skipped: AI.exe is partly patched", step);
    return false;
}

} // namespace

bool ui_set_only() noexcept { return g_ui_set_only.load(std::memory_order_acquire); }

bool install(HMODULE self, const std::filesystem::path& game_dir) noexcept {
    // The patch set follows the runtime for this run (01 section 2.3):
    // runtime None is the unmodded game plus the options page.
    bool forced = false;
    const auto runtime = runtime_for_this_run(forced);
    const bool full_set = runtime != config::VRRuntime::None;
    g_ui_set_only.store(!full_set, std::memory_order_release);

    auto* ai_base = reinterpret_cast<std::uint8_t*>(::GetModuleHandleW(nullptr));

    // 1
    const auto version = select_address_table(ai_base);
    if (!version) return false;
    const Game g{ai_base, image_size_of(ai_base), *version, inline_sites(*version)};
    diag::logf("[hook] patch set {} for runtime {} (from {})", full_set ? "full" : "UI only",
               config::to_string(runtime), forced ? "command line" : "config");
    if (!preflight(g, full_set)) return false;

    // 2 (3, openvr_api.dll, is in openvr::init)
    const auto stamp = integration::this_module_build_filetime();
    const bool mod_archive = redirect_ui_archive(self, game_dir, stamp);
    const auto& cfg = config::current();

    // 4
    if (cfg.debug.skip_startup_intro) skip_intro(mod_archive);

    // 5
    if (!hook_script_bridge()) return failed("options UI");

    if (!full_set) {
        if (!hook_pause_settings_push()) return failed("options UI");
        area_on("options UI", mod_archive ? 4 : 3);
        for (std::string_view area : kFullSetAreas) area_off(area, "not in the patch set for runtime None");
        return true;
    }

    // 6..8
    if (!force_stereo_mode(g)) return failed("stereo path");
    if (!install_stereo_device(g)) return failed("stereo path");
    area_on("stereo path", static_cast<int>(g.sites.stereo_mode_tests.size()) + kStereoDeviceSites);
    if (!install_game_side_hooks()) return failed("game behaviour");
    // The pause menu's open slot is written with the game-side hooks; its
    // wrapper also pushes the settings.
    area_on("options UI", mod_archive ? 4 : 3);
    install_reconstruction_fixes(g);

    // 9
    if (!apply_byte_patches(g)) return failed("game behaviour");
    area_on("game behaviour", kGameSideSites + kBytePatchSites);
    switch (const int sites = install_controls(g)) {
        case 0:  area_off("controls", "the pad's vtable slots could not be written; the pad stays stock"); break;
        case -1: return failed("controls");
        default: area_on("controls", sites); break;
    }

    // Settings reach the menu through on_menu_open, as in the original.
    return true;
}

} // namespace mothervr::hook
