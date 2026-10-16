// Hook installer: patches AI.exe in-process at DLL_PROCESS_ATTACH.
//
// Reconstruction of: FUN_1001d6f0 @ 0x1001d6f0 (the original 4142-byte
// monolithic installer). Every AI.exe address comes from the per-build table
// in game/addresses.h; the original keeps one table for the 2015 launch build
// and one for the 2020 patch and picks by AI.exe's PE timestamp
// (FUN_10002250 -> FUN_10001760).
//
// Unlike the original, nothing is written until a preflight has checked the
// code at every core site: a half-installed stereo hand-off leaves the game
// running its own LibOVR init on our manager.
//
// Also unlike the original, runtime None patches only the UI set (the
// options page and its settings push): flat play is the unmodded game.

#include "loader/install.h"

#include "core/config/ports/launch_switches.h"
#include "core/config/settings_table.h"
#include "core/stereo/blinders.h"
#include "game/build.h"
#include "game/code_address.h"
#include "game/game_image.h"
#include "game/patcher.h"
#include "game/preflight.h"
#include "game/sites/controls.h"
#include "game/sites/game_behaviour.h"
#include "game/sites/render_fixes.h"
#include "game/sites/stereo_path.h"
#include "game/sites/ui_set.h"
#include "loader/context.h"
#include "loader/hooks/event_tap_hook.h"
#include "loader/hooks/file_hooks.h"
#include "loader/hooks/hand_hooks.h"
#include "loader/hooks/menu_hooks.h"
#include "loader/hooks/pad_hooks.h"
#include "loader/hooks/render_fix_hooks.h"
#include "loader/hooks/stereo_device_hooks.h"
#include "loader/hooks/stereo_update_hooks.h"
#include "loader/hooks/ui_hooks.h"
#include "platform/win32/build_stamp.h"
#include "platform/win32/module.h"
#include "platform/win32/patching/pe_image.h"

#include <array>
#include <string_view>

namespace omvr::loader {

namespace {

namespace config = core::config;
namespace ports  = core::diag::ports;
namespace sites  = game::sites;
using game::code_pointer;
using game::GameImage;
using game::Patcher;

// ---- Areas and log templates (11) -------------------------------------------

constexpr std::string_view kOptionsUi     = "options UI";
constexpr std::string_view kStereoPath    = "stereo path";
constexpr std::string_view kGameBehaviour = "game behaviour";
constexpr std::string_view kControls      = "controls";

// The areas only the full patch set has, in the order install() reports them.
constexpr std::array<std::string_view, 8> kFullSetAreas = {
    kStereoPath,
    kGameBehaviour,
    kControls,
    sites::kReflectionsArea,
    sites::kStereoAaArea,
    sites::kCullingArea,
    sites::kHeadClipArea,
    sites::kEventTapArea,
};

constexpr std::string_view kNotInUiSetWhy = "not in the patch set for runtime None";
constexpr std::string_view kPadStockWhy = "the pad's vtable slots could not be written; the pad stays stock";

constexpr std::string_view kPatchSetLog = "[hook] patch set {} for runtime {} (from {})";
constexpr std::string_view kWriteFailedLog =
    "[hook] {}: a write failed after the preflight; earlier patches stay, later core patches skipped: AI.exe "
    "is "
    "partly patched";

// ---- Helpers ----------------------------------------------------------------

// The runtime the patch set is decided for: a -vrforce* switch, else the
// config file's VRRuntime (01 §5.1). The runtime detector applies the same
// switches when the game creates its stereo device; it cannot run here (it
// probes the runtimes, which is not allowed under the loader lock), but
// reading the switches alone can.
struct RunRuntime {
    config::VRRuntime runtime;
    bool forced; // from the command line
};

RunRuntime runtime_for_this_run(const config::Settings& cfg,
                                const config::ports::LaunchSwitches& switches) noexcept {
    const auto chosen = switches.forced_runtime();
    return {chosen.value_or(cfg.vr_runtime), chosen.has_value()};
}

bool failed(ports::Log& log, std::string_view step) noexcept {
    ports::logf(log, kWriteFailedLog, step);
    return false;
}

int ui_set_sites(bool mod_archive) noexcept {
    return mod_archive ? sites::kUiSetSitesWithArchive : sites::kUiSetSitesWithoutArchive;
}

// ---- Steps ------------------------------------------------------------------

// 2, 4, 5: the UI set. Sets `mod_archive` when the game now opens the
// mod's archive. (3, openvr_api.dll, is in the OpenVR runtime's start.)
bool install_ui_set(Context& ctx, const GameImage& g, Patcher& p, const std::filesystem::path& game_dir,
                    bool& mod_archive) noexcept {
    const auto stamp = platform::win32::this_module_build_filetime();
    mod_archive      = sites::redirect_ui_archive(g, p, ctx.ui_archive.deploy(game_dir, stamp));
    if (!mod_archive) {
        // The stock movies register no VR glyph ids: keep the game's pad glyphs.
        ctx.pad.use_stock_glyphs_only();
    }
    if (ctx.settings.current().debug.skip_startup_intro) sites::skip_intro(g, p, mod_archive);
    return sites::hook_script_bridge(
        g, p, ctx.script_vars, ctx.console,
        {code_pointer(&hooks::handle_mvr_command), code_pointer(&hooks::on_menu_open)});
}

// Runtime None: the pause menu's settings push completes the UI set.
bool finish_ui_only(Context& ctx, const GameImage& g, Patcher& p, bool mod_archive) noexcept {
    if (!sites::hook_pause_settings_push(g, p, code_pointer(&hooks::pause_open_settings_only),
                                         ctx.targets.pause_open_settings_only)) {
        return failed(ctx.log, kOptionsUi);
    }
    sites::area_on(ctx.log, kOptionsUi, ui_set_sites(mod_archive));
    for (std::string_view area : kFullSetAreas) sites::area_off(ctx.log, area, kNotInUiSetWhy);
    return true;
}

// 6, 7: the stereo path.
bool install_stereo_path(Context& ctx, const GameImage& g, Patcher& p) noexcept {
    if (!sites::force_stereo_mode(g, p)) return failed(ctx.log, kStereoPath);
    const sites::StereoDeviceHooks device_hooks{
        hooks::device_slots(), hooks::manager_slots(), code_pointer(&hooks::create_device),
        code_pointer(&hooks::get_hmd_resolution), code_pointer(&hooks::get_render_target_size)};
    if (!sites::install_stereo_device(g, p, ctx.manager, device_hooks)) return failed(ctx.log, kStereoPath);
    sites::area_on(ctx.log, kStereoPath,
                   static_cast<int>(g.sites.stereo_mode_tests.size()) + sites::kStereoDeviceSites);
    return true;
}

sites::GameSideReplacements game_side_replacements() noexcept {
    return {code_pointer(&hooks::follow_on_activate),
            code_pointer(&hooks::follow_on_deactivate),
            code_pointer(&hooks::ui_on_message),
            code_pointer(&hooks::menu_close),
            code_pointer(&hooks::pause_open),
            code_pointer(&hooks::pause_close),
            code_pointer(&hooks::loading_open),
            code_pointer(&hooks::loading_close),
            code_pointer(&hooks::attract_open),
            code_pointer(&hooks::attract_close),
            code_pointer(&hooks::stereo_update),
            code_pointer(&hooks::place_virtual_screen),
            code_pointer(&hooks::place_hud_panel),
            code_pointer(&hooks::push_mode),
            code_pointer(&hooks::pop_mode)};
}

sites::RenderFixHooks render_fix_hooks(Context& ctx) noexcept {
    return {code_pointer(&hooks::render_reflection),
            code_pointer(&hooks::stereo_final_draw),
            code_pointer(&hooks::run_smaa),
            code_pointer(&hooks::cull_distance),
            code_pointer(&hooks::cull_occlusion),
            code_pointer(&hooks::head_clip_fade),
            hooks::event_tap_replacement(),
            &ctx.targets.event_tap,
            &ctx.targets.render_fixes,
            &hooks::install_reflections};
}

// 8, 8b: the game-side hooks, the mode-stack append and the render fixes.
bool install_game_side(Context& ctx, const GameImage& g, Patcher& p, bool mod_archive) noexcept {
    core::stereo::set_clipping_blinders(ctx.settings.current().rendering.clipping_blinders);
    if (!sites::install_game_side_hooks(g, p, game_side_replacements(), ctx.targets.game_side)) {
        return failed(ctx.log, kGameBehaviour);
    }
    sites::find_mode_stack_append(g, ctx.log, ctx.targets.mode_stack);
    // The pause menu's open slot is written with the game-side hooks; its
    // wrapper also pushes the settings.
    sites::area_on(ctx.log, kOptionsUi, ui_set_sites(mod_archive));
    sites::install_render_fixes(g, p, render_fix_hooks(ctx));
    return true;
}

// 9: the byte patches and the controls.
bool install_patches_and_controls(Context& ctx, const GameImage& g, Patcher& p) noexcept {
    if (!sites::apply_byte_patches(g, p, &core::stereo::head_clip_threshold()))
        return failed(ctx.log, kGameBehaviour);
    sites::area_on(ctx.log, kGameBehaviour, sites::kGameSideSites + sites::kBytePatchSites);
    ctx.input_manager.bind(game::CodeAddress{g.at(game::AiAddr::InputManagerPtr)});
    const auto outcome = sites::install_controls(g, p, hooks::pad_replacements(), ctx.targets.stock_pad);
    if (outcome != sites::ControlsOutcome::SlotsNotWritten) ctx.pad.mark_installed();
    switch (outcome) {
        case sites::ControlsOutcome::SlotsNotWritten:
            sites::area_off(ctx.log, kControls, kPadStockWhy);
            break;
        case sites::ControlsOutcome::LaterWriteFailed: return failed(ctx.log, kControls);
        case sites::ControlsOutcome::Installed:
            sites::area_on(ctx.log, kControls, sites::kControlSites);
            break;
    }
    return true;
}

} // namespace

bool install(Context& ctx, const std::filesystem::path& game_dir) noexcept {
    // The patch set follows the runtime for this run (01 §2.3): runtime None
    // is the unmodded game plus the options page.
    const auto [runtime, forced] = runtime_for_this_run(ctx.settings.current(), ctx.launch);
    const bool full_set          = runtime != config::VRRuntime::None;
    ctx.ui_set_only.store(!full_set, std::memory_order_release);

    auto* ai_base = static_cast<std::uint8_t*>(platform::win32::module_handle(nullptr));

    // 1
    const auto version = game::select_address_table(ai_base, ctx.addresses, ctx.log);
    if (!version) return false;
    const GameImage g{ai_base, platform::win32::image_size_of(ai_base), *version,
                      game::inline_sites(*version), ctx.addresses};
    ports::logf(ctx.log, kPatchSetLog, full_set ? "full" : "UI only", config::to_string(runtime),
                forced ? "command line" : "config");
    if (!game::preflight(g, full_set, ctx.log)) return false;

    Patcher patcher{ctx.log};
    bool mod_archive = false;
    if (!install_ui_set(ctx, g, patcher, game_dir, mod_archive)) return failed(ctx.log, kOptionsUi);
    if (!full_set) return finish_ui_only(ctx, g, patcher, mod_archive);

    // Settings reach the menu through on_menu_open (5).
    const bool installed = install_stereo_path(ctx, g, patcher) &&
                           install_game_side(ctx, g, patcher, mod_archive) &&
                           install_patches_and_controls(ctx, g, patcher);
    // v2 hands (21): their sites only once Hands.Enabled is on.
    if (installed) hooks::prepare_hands(g);
    // The level for the Discord activity; runtime None leaves the game's
    // imports alone like everything else outside the UI set.
    if (installed && ctx.settings.current().discord_rich_presence) hooks::watch_game_file_opens(ai_base);
    return installed;
}

} // namespace omvr::loader
