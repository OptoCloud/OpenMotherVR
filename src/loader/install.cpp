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

#include "core/config/launch_options.h"
#include "core/config/settings_table.h"
#include "core/stereo/blinders.h"
#include "game/build.h"
#include "game/preflight.h"
#include "game/sites/controls.h"
#include "game/sites/game_behaviour.h"
#include "game/sites/render_fixes.h"
#include "game/sites/stereo_path.h"
#include "game/sites/ui_set.h"
#include "loader/context.h"
#include "loader/hooks/event_tap_hook.h"
#include "loader/hooks/menu_hooks.h"
#include "loader/hooks/pad_hooks.h"
#include "loader/hooks/render_fix_hooks.h"
#include "loader/hooks/stereo_device_hooks.h"
#include "loader/hooks/stereo_update_hooks.h"
#include "loader/hooks/ui_hooks.h"
#include "platform/win32/build_stamp.h"
#include "platform/win32/module.h"
#include "platform/win32/patching/pe_image.h"

#include <string_view>

namespace omvr::loader {

namespace {

namespace diag = core::diag;
namespace config = core::config;
using game::sites::area_off;
using game::sites::area_on;

template <class Fn>
const void* fn(Fn* f) noexcept { return reinterpret_cast<const void*>(f); }

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
    const auto chosen = config::forced_runtime();
    forced = chosen.has_value();
    return chosen.value_or(config::current().vr_runtime);
}

bool failed(const char* step) noexcept {
    diag::logf("[hook] {}: a write failed after the preflight; earlier patches stay, later core patches "
               "skipped: AI.exe is partly patched", step);
    return false;
}

bool install_reflections(const game::stereo::ReflectionTargets& targets) noexcept {
    return context().reflections.install(targets);
}

} // namespace

bool install(Context& ctx, const std::filesystem::path& game_dir) noexcept {
    // The patch set follows the runtime for this run (01 section 2.3):
    // runtime None is the unmodded game plus the options page.
    bool forced = false;
    const auto runtime = runtime_for_this_run(forced);
    const bool full_set = runtime != config::VRRuntime::None;
    ctx.ui_set_only.store(!full_set, std::memory_order_release);

    auto* ai_base = static_cast<std::uint8_t*>(platform::win32::module_handle(nullptr));

    // 1
    const auto version = game::select_address_table(ai_base, ctx.addresses);
    if (!version) return false;
    const game::GameImage g{ai_base, platform::win32::image_size_of(ai_base), *version,
                            game::inline_sites(*version)};
    diag::logf("[hook] patch set {} for runtime {} (from {})", full_set ? "full" : "UI only",
               config::to_string(runtime), forced ? "command line" : "config");
    if (!game::preflight(g, ctx.addresses, full_set)) return false;

    // 2 (3, openvr_api.dll, is in openvr::init)
    const auto stamp = platform::win32::this_module_build_filetime();
    const bool mod_archive = game::sites::redirect_ui_archive(ctx.ui_archive.deploy(game_dir, stamp));
    if (!mod_archive) {
        // The stock movies register no VR glyph ids: keep the game's pad glyphs.
        ctx.pad.use_stock_glyphs_only();
    }
    const auto& cfg = config::current();

    // 4
    if (cfg.debug.skip_startup_intro) game::sites::skip_intro(mod_archive);

    // 5
    if (!game::sites::hook_script_bridge(ctx.script_vars, ctx.console,
                                         {fn(&hooks::handle_mvr_command), fn(&hooks::on_menu_open)})) {
        return failed("options UI");
    }

    if (!full_set) {
        if (!game::sites::hook_pause_settings_push(fn(&hooks::pause_open_settings_only),
                                                   ctx.originals.pause_open_settings_only)) {
            return failed("options UI");
        }
        area_on("options UI", mod_archive ? 4 : 3);
        for (std::string_view area : kFullSetAreas) area_off(area, "not in the patch set for runtime None");
        return true;
    }

    // 6..8
    if (!game::sites::force_stereo_mode(g)) return failed("stereo path");
    if (!game::sites::install_stereo_device(
            ctx.manager, {hooks::device_slots(), hooks::manager_slots(), fn(&hooks::create_device),
                          fn(&hooks::get_hmd_resolution), fn(&hooks::get_render_target_size)})) {
        return failed("stereo path");
    }
    area_on("stereo path", static_cast<int>(g.sites.stereo_mode_tests.size()) + game::sites::kStereoDeviceSites);
    core::stereo::set_clipping_blinders(cfg.rendering.clipping_blinders);
    if (!game::sites::install_game_side_hooks(
            {fn(&hooks::follow_on_activate), fn(&hooks::follow_on_deactivate), fn(&hooks::ui_on_message),
             fn(&hooks::menu_close), fn(&hooks::pause_open), fn(&hooks::pause_close), fn(&hooks::loading_open),
             fn(&hooks::loading_close), fn(&hooks::attract_open), fn(&hooks::attract_close),
             fn(&hooks::stereo_update), fn(&hooks::place_virtual_screen), fn(&hooks::place_hud_panel),
             fn(&hooks::push_mode), fn(&hooks::pop_mode)},
            ctx.originals.game_side)) {
        return failed("game behaviour");
    }
    game::sites::find_mode_stack_append(g);
    // The pause menu's open slot is written with the game-side hooks; its
    // wrapper also pushes the settings.
    area_on("options UI", mod_archive ? 4 : 3);
    game::sites::install_reconstruction_fixes(
        g, {fn(&hooks::render_reflection), fn(&hooks::stereo_final_draw), fn(&hooks::run_smaa),
            fn(&hooks::cull_distance), fn(&hooks::cull_occlusion), fn(&hooks::head_clip_fade),
            hooks::event_tap_replacement(), &ctx.originals.event_tap, &install_reflections});

    // 9
    if (!game::sites::apply_byte_patches(g, &core::stereo::head_clip_threshold())) return failed("game behaviour");
    area_on("game behaviour", game::sites::kGameSideSites + game::sites::kBytePatchSites);
    ctx.input_manager.bind(reinterpret_cast<void**>(game::ai_address(game::AiAddr::InputManagerPtr)));
    const int sites = game::sites::install_controls(g.base, g.sites, hooks::pad_replacements(), ctx.pad.stock_pad());
    // 0: the five slots were not written; -1: they were, a later write failed.
    if (sites != 0) ctx.pad.mark_installed();
    switch (sites) {
        case 0:  area_off("controls", "the pad's vtable slots could not be written; the pad stays stock"); break;
        case -1: return failed("controls");
        default: area_on("controls", sites); break;
    }

    // Settings reach the menu through on_menu_open, as in the original.
    return true;
}

} // namespace omvr::loader
