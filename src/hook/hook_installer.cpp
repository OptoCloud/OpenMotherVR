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
#include "game/game_events.h"

#include <cstring>
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

// ---- preflight --------------------------------------------------------------

// Records which core sites do not hold what the patches expect.
class Preflight {
public:
    explicit Preflight(const Game& g) noexcept : g_{g} {}

    void expect(bool ok, std::string_view what) noexcept {
        if (!ok) {
            diag::logf("[hook] preflight: unexpected code or data at {}", what);
            ++failures_;
        }
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

    // Vtable slots `indices` of the vtable at `vt` hold code addresses.
    bool slots_in_image(const std::uint8_t* vt, std::initializer_list<int> indices) const noexcept {
        for (int i : indices) {
            if (!in_image(read_u32(vt + 4 * i))) return false;
        }
        return true;
    }

    bool passed() const noexcept { return failures_ == 0; }

private:
    const Game& g_;
    int failures_ = 0;
};

// Check every site the core steps write, before any of them is written.
// Opcodes and operands that do not depend on the build are checked exactly;
// call targets where the table names the callee or two sites share one.
bool preflight(const Game& g) noexcept {
    if (!ai_addresses_within(g.image_size)) {
        diag::log_line("[hook] preflight: address table points outside AI.exe");
        return false;
    }
    for (std::uint32_t rva : g.sites.stereo_mode_tests) {
        if (rva >= g.image_size) return false;
    }
    if (g.sites.left_stick_prompt_call_2 >= g.image_size) return false;

    Preflight pf{g};
    auto at = [](AiAddr a) { return ai_address(a); };
    const std::uint8_t* push_mode = at(AiAddr::GamePushMode);
    const std::uint8_t* pop_mode  = at(AiAddr::GamePopMode);

    // 2, 4, 5: data pointers and the console command slot.
    pf.expect(pf.points_to(at(AiAddr::UiPakPathPtr), kStockUiPak), "the UI.PAK path pointer");
    {
        const std::uint8_t* p = at(AiAddr::BootupMoviePush);
        pf.expect(p[0] == 0x68 && pf.points_to(p + 1, kShowBootup), "push \"ShowBootup\"");
    }
    pf.expect(read_u32(at(AiAddr::CmdDispatchSlot)) == address_of(at(AiAddr::OriginalCmdHandler)),
              "the console command slot");
    pf.expect(read_u32(at(AiAddr::MenuVtable)) == address_of(at(AiAddr::MenuOpen)), "the menu vtable");

    // 6: `cmp reg, eax`.
    pf.expect(stereo_mode_tests_match(g.base, g.version), "the stereo mode tests");

    // 7: `push 0xb0` before new(stereo manager); `mov eax, [reg+0x2c]`.
    {
        const std::uint8_t* p = at(AiAddr::StereoManagerAlloc);
        pf.expect(p[0] == 0x68 && read_u32(p + 1) == 0xB0, "the stereo manager allocation");
    }
    for (AiAddr a : {AiAddr::HeadPoseCallA, AiAddr::HeadPoseCallB}) {
        const std::uint8_t* p = at(a);
        pf.expect(p[0] == 0x8B && (p[1] & 0xC0) == 0x40 && p[2] == 0x2C, "a recenter call");
    }
    pf.expect(*at(AiAddr::CreateDeviceCall) == kCall, "call Stereo::Init");
    pf.expect(*at(AiAddr::HmdResolutionCall) == kCall, "call Stereo::GetDisplaySize");
    pf.expect(*at(AiAddr::RenderTargetSizeCall) == kCall, "call Stereo::GetRenderTargetSize");

    // 8: call sites and vtable slots.
    pf.expect(*at(AiAddr::StereoUpdateCall) == kCall, "call Stereo::Update");
    {
        // Both build the UI quad with the same function.
        const std::uint8_t* screen = at(AiAddr::VirtualScreenCall);
        const std::uint8_t* hud    = at(AiAddr::HudPanelCall);
        pf.expect(*screen == kCall && *hud == kCall && call_target(screen) == call_target(hud),
                  "the UI quad calls");
    }
    for (AiAddr a : {AiAddr::PushModeCallA, AiAddr::PushModeCallB, AiAddr::PushModeCallC}) {
        pf.expect(calls(at(a), push_mode), "call Stereo::PushMode");
    }
    for (AiAddr a : {AiAddr::PopModeCallA, AiAddr::PopModeCallB, AiAddr::PopModeCallC}) {
        pf.expect(calls(at(a), pop_mode), "call Stereo::PopMode");
    }
    {
        const std::uint8_t* first  = at(AiAddr::LeftStickPromptCall);
        const std::uint8_t* second = g.base + g.sites.left_stick_prompt_call_2;
        pf.expect(*first == kCall && *second == kCall && call_target(first) == call_target(second),
                  "the IsMoveOnLeftStick calls");
    }
    pf.expect(read_u32(at(AiAddr::FollowVtable)) == address_of(at(AiAddr::FollowOnActivatePatch)) &&
                  pf.slots_in_image(at(AiAddr::FollowVtable), {1}),
              "the FollowBehavior vtable");
    pf.expect(pf.slots_in_image(at(AiAddr::UiEntityMessageSlot), {0}), "the UI entity vtable");
    pf.expect(pf.slots_in_image(at(AiAddr::MenuVtable), {2}) &&
                  pf.slots_in_image(at(AiAddr::PauseMenuVtable), {0, 2}) &&
                  pf.slots_in_image(at(AiAddr::LoadingScreenVtable), {0, 2}) &&
                  pf.slots_in_image(at(AiAddr::AttractScreenVtable), {0, 2}),
              "the menu and loading screen vtables");

    // 9: byte patches.
    pf.expect(*at(AiAddr::SsaoSettingBranch) == 0x75, "the SSAO branch (jne)");
    {
        const std::uint8_t* p = at(AiAddr::HeadClipCompare);  // comiss xmm, [disp32]
        pf.expect(p[0] == 0x0F && p[1] == 0x2F && (p[2] & 0xC7) == 0x05, "the head-in-wall compare");
    }
    {
        const std::uint8_t* p = at(AiAddr::CameraNoiseCall);  // mov ecx, reg; call
        pf.expect(p[0] == 0x8B && (p[1] & 0xC0) == 0xC0 && p[2] == kCall, "the camera noise call");
    }
    {
        const std::uint8_t* p = at(AiAddr::FollowLookState);  // mov cl, [esp+disp8]
        pf.expect(p[0] == 0x8A && p[1] == 0x4C && p[2] == 0x24, "the FollowBehavior look state");
    }
    {
        // `jbe rel32`; the replacement `jmp` hard-codes the same target.
        constexpr std::uint8_t kJbe[6] = {0x0F, 0x86, 0x2B, 0x03, 0x00, 0x00};
        pf.expect(std::memcmp(at(AiAddr::CameraCorrectionBranch), kJbe, sizeof(kJbe)) == 0,
                  "the camera correction branch");
    }
    {
        const std::uint8_t* p = at(AiAddr::FollowOnActivatePatch) + 8;  // mov [reg+0x264], al
        pf.expect(p[0] == 0x88 && (p[1] & 0xF8) == 0x80 && read_u32(p + 2) == 0x264,
                  "FollowBehavior::OnActivate");
    }
    for (AiAddr a : {AiAddr::UiMouseWidthLoad, AiAddr::UiMouseHeightLoad}) {
        const std::uint8_t* p = at(a);  // movss xmm, [disp32]
        pf.expect(p[0] == 0xF3 && p[1] == 0x0F && p[2] == 0x10 && (p[3] & 0xC7) == 0x05,
                  "a UI mouse size load");
    }
    pf.expect(*at(AiAddr::UiInputModeBranch) == 0x74, "the UI input mode branch (je)");
    {
        const std::uint8_t* p = at(AiAddr::ResumeGamePopMode);  // mov ecx, [abs]; call PopMode
        pf.expect(p[0] == 0x8B && p[1] == 0x0D && calls(p + 6, pop_mode), "PopMode in Resume_game");
    }
    {
        const std::uint8_t* p = at(AiAddr::PauseMenuPushMode);  // mov ecx, [abs]; push 1; call PushMode
        pf.expect(p[0] == 0x8B && p[1] == 0x0D && p[6] == 0x6A && p[7] == 0x01 && calls(p + 8, push_mode),
                  "PushMode in Pause");
    }
    pf.expect(*at(AiAddr::LoaderSleepPush) == 0x51, "the loader sleep (push ecx)");
    {
        const std::uint8_t* p = at(AiAddr::PadConnectedSet);  // setne cl
        pf.expect(p[0] == 0x0F && p[1] == 0x95 && p[2] == 0xC1, "the pad connected flag");
    }
    pf.expect(pf.slots_in_image(at(AiAddr::PadVtable), {0, 1, 2, 4, 10}), "the pad vtable");

    return pf.passed();
}

// ---- steps --------------------------------------------------------------------

// 1: the original refuses to touch an unknown build ("Exe check failed!"):
// every address is build-specific.
std::optional<AIVersion> select_address_table(std::uint8_t* ai_base) noexcept {
    const auto version = integration::identify_ai_exe();
    if (!version) {
        diag::logf("[hook] unknown AI.exe build (timestamp=0x{:08X}), not patching",
                   integration::ai_exe_timestamp_raw());
        return std::nullopt;
    }
    init_ai_addresses(ai_base, *version);
    diag::logf("[hook] using {} address table", *version == AIVersion::Patched_2020 ? "2020" : "2015");
    return version;
}

// 2: the options page lives in the mod's UI archive. The original aborts
// without it; we carry on, since VR works without the menu.
void redirect_ui_archive(HMODULE self, const std::filesystem::path& game_dir, std::int64_t stamp) noexcept {
    if (integration::deploy_mother_pak(self, game_dir, stamp) &&
        write_dword(ai_address(AiAddr::UiPakPathPtr), address_of(kMotherPakPath))) {
        diag::log_line("[hook] UI archive redirected to data\\mother.pak");
    } else {
        diag::log_line("[hook] no mod UI archive: options page unavailable, continuing");
    }
}

// 4: the site is `push offset "ShowBootup"`; replace the pushed string.
void skip_intro() noexcept {
    if (write_dword(ai_address(AiAddr::BootupMoviePush) + 1, address_of(kSkipBootup))) {
        diag::log_line("[hook] startup intro skipped");
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

// 6
bool force_stereo_mode(const Game& g) noexcept {
    if (!force_stereo_mode_tests(g.base, g.version)) return false;
    diag::log_line("[hook] stereo/render patches applied");
    return true;
}

// 7: the game's hidden LibOVR stereo path gets our manager and device.
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
bool install_game_side_hooks(const Game& g) noexcept {
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
    return redirect_call(ai_address(AiAddr::LeftStickPromptCall), &is_move_on_left_stick, "left stick prompt") &&
           redirect_call(g.base + g.sites.left_stick_prompt_call_2, &is_move_on_left_stick,
                         "left stick prompt");
}

// 8b: planar reflections render per eye.
void fix_reflections(const Game& g, const ReconRvas& r) noexcept {
    std::uint8_t* site = g.base + r.reflection_render_call;
    if (!calls(site, g.base + r.reflection_render)) {
        diag::log_line("[hook] reflections: unexpected code at the call, not patched");
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
        diag::log_line("[hook] planar reflections render per eye in stereo");
    }
}

// 8b: anti-aliasing in stereo. Both sites or neither: without the SMAA one,
// T2x would run temporally on the side-by-side image.
void fix_anti_aliasing(const Game& g, const ReconRvas& r) noexcept {
    std::uint8_t* draw_site = g.base + r.stereo_output_draw_call;
    std::uint8_t* smaa_site = g.base + r.smaa_call;
    const std::uint8_t* getter = g.base + r.postfx_instance;  // mov eax, imm32; ret
    constexpr std::uint8_t kAaPrologue[6] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0};
    if (!calls(draw_site, g.base + r.fullscreen_draw) || !calls(smaa_site, g.base + r.smaa) ||
        getter[0] != 0xB8 || getter[5] != 0xC3 ||
        std::memcmp(g.base + r.aa_pass, kAaPrologue, sizeof(kAaPrologue)) != 0) {
        diag::log_line("[hook] anti-aliasing in stereo: unexpected code at the call sites, not patched");
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
        diag::log_line("[hook] anti-aliasing runs in stereo (SMAA T2x as T1x)");
    }
}

// 8b: object culling. In stereo the distance/LOD test passes in every POV
// group and the main POV's centre-view occlusion test passes; the frustum
// and the shadow lights' view-volume test stay the game's.
void fix_culling(const Game& g, const ReconRvas& r) noexcept {
    // mov eax,[esi+0x5C]; test eax,eax; je +0xF; push eax; push esi; push ebx:
    // the occlusion test takes the context's occlusion buffer, which only
    // the main POV has, so the replacement needs no phase check.
    constexpr std::uint8_t kOcclusionArgs[] = {0x8B, 0x46, 0x5C, 0x85, 0xC0, 0x74, 0x0F, 0x50, 0x56, 0x53};
    const std::uint8_t* occlusion_args = g.base + r.cull_occlusion_call - sizeof(kOcclusionArgs);
    if (!calls(g.base + r.cull_distance_call, g.base + r.cull_distance) ||
        !calls(g.base + r.cull_occlusion_call, g.base + r.cull_occlusion) ||
        std::memcmp(occlusion_args, kOcclusionArgs, sizeof(kOcclusionArgs)) != 0) {
        diag::log_line("[hook] culling: unexpected code in the visibility loop, not patched");
        return;
    }
    stereo::install_cull_fix({
        .distance  = g.base + r.cull_distance,
        .occlusion = g.base + r.cull_occlusion,
    });
    if (redirect_call(g.base + r.cull_distance_call, &stereo::cull_distance, "distance cull") &&
        redirect_call(g.base + r.cull_occlusion_call, &stereo::cull_occlusion, "occlusion cull")) {
        diag::logf("[hook] culling: calls at RVA {:08X}, {:08X} redirected; in stereo the distance "
                   "test passes and the main view's occlusion test passes",
                   r.cull_distance_call, r.cull_occlusion_call);
    }
}

// 8b: head-in-wall blackout. In stereo a fade waits 0.6 s before it shows.
void fix_head_clip_fade(const Game& g, const ReconRvas& r) noexcept {
    if (!calls(g.base + r.head_clip_fade_call, g.base + r.head_clip_fade)) {
        diag::log_line("[hook] head-in-wall fade: unexpected code at the call, not patched");
        return;
    }
    stereo::install_head_clip_delay(g.base + r.head_clip_fade);
    if (redirect_call(g.base + r.head_clip_fade_call, &stereo::head_clip_fade, "HeadClipFade")) {
        diag::logf("[hook] head-in-wall fade: call at RVA {:08X} redirected; fades wait 0.6 s in stereo",
                   r.head_clip_fade_call);
    }
}

// 8b: interaction and hiding state from the scripts' event strings, for the
// features that behave differently during interactions.
void tap_game_events(const Game& g, const ReconRvas& r) noexcept {
    if (game::install_event_tap(g.base + r.event_tap))
        diag::log_line("[hook] game events: interaction and hiding state tracked");
}

// 8b: each fix checks its own sites and stays off on its own.
void install_reconstruction_fixes(const Game& g) noexcept {
    const ReconRvas& r = recon_rvas(g.version);
    if (r.reflection_render_call != 0) fix_reflections(g, r);
    if (r.stereo_output_draw_call != 0) fix_anti_aliasing(g, r);
    if (r.cull_distance_call != 0) fix_culling(g, r);
    if (r.head_clip_fade_call != 0) fix_head_clip_fade(g, r);
    if (r.event_tap != 0) tap_game_events(g, r);
}

// 9: camera, UI and loader byte patches.
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
        // side-by-side back buffer.
        patch(at(AiAddr::UiMouseWidthLoad, 4), {g.sites.ui_mouse_width_disp_lo}) &&
        patch(at(AiAddr::UiMouseHeightLoad, 4), {g.sites.ui_mouse_height_disp_lo}) &&
        // Menus always use focus (pad) navigation.
        patch(at(AiAddr::UiInputModeBranch), {0xEB}) &&
        // The menu wrappers own the stereo mode for these two screens.
        patch(at(AiAddr::ResumeGamePopMode), {0xEB, 0x09, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop}) &&
        patch(at(AiAddr::PauseMenuPushMode),
              {0xEB, 0x0B, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop, kNop}) &&
        // No 30 Hz throttle in the loading loop: the headset needs frames.
        patch(at(AiAddr::LoaderSleepPush), {0x53}) &&
        // The pad always counts as connected: VR controllers come through it.
        patch(at(AiAddr::PadConnectedSet), {0xB1, 0x01, kNop});
    if (ok) diag::log_line("[hook] camera, UI and loader patches applied");
    return ok;
}

bool failed(const char* step) noexcept {
    diag::logf("[hook] {}: a write failed after the preflight; AI.exe is partly patched", step);
    return false;
}

} // namespace

bool install(HMODULE self, const std::filesystem::path& game_dir) noexcept {
    auto* ai_base = reinterpret_cast<std::uint8_t*>(::GetModuleHandleW(nullptr));
    diag::logf("[hook] AI.exe base: {:08X}", reinterpret_cast<std::uintptr_t>(ai_base));

    // 1
    const auto version = select_address_table(ai_base);
    if (!version) return false;
    const Game g{ai_base, image_size_of(ai_base), *version, inline_sites(*version)};
    if (!preflight(g)) {
        diag::log_line("[hook] AI.exe does not hold the expected code, not patching");
        return false;
    }

    // 2 (3, openvr_api.dll, is in openvr::init)
    const auto stamp = integration::this_module_build_filetime();
    redirect_ui_archive(self, game_dir, stamp);
    const auto& cfg = config::current();

    // 4
    if (cfg.debug.skip_startup_intro) skip_intro();

    // 5..8
    if (!hook_script_bridge()) return failed("script bridge");
    if (!force_stereo_mode(g)) return failed("stereo mode tests");
    if (!install_stereo_device(g)) return failed("stereo device");
    if (!install_game_side_hooks(g)) return failed("game-side stereo hooks");
    install_reconstruction_fixes(g);

    // 9
    if (!apply_byte_patches(g)) return failed("byte patches");
    // VR input goes through the game's gamepad: five of its vtable slots.
    input::install(ai_address(AiAddr::PadVtable),
                   reinterpret_cast<void**>(ai_address(AiAddr::InputManagerPtr)));

    // Settings reach the menu through on_menu_open, as in the original.
    return true;
}

} // namespace mothervr::hook
