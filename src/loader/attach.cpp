#include "loader/attach.h"

#include "core/config/settings_json.h"
#include "core/config/settings_table.h"
#include "core/ui/compat_messages.h"
#include "core/ui/presence_text.h"
#include "game/build.h"
#include "loader/context.h"
#include "loader/install.h"
#include "platform/discord/constants.h"
#include "platform/win32/module.h"
#include "platform/win32/process_info.h"
#include "platform/win32/text.h"

#include <chrono>
#include <format>
#include <string>

namespace omvr::loader {

namespace {

using core::diag::ports::logf;
using core::ui::ports::DialogKind;
using platform::win32::utf8;

void log_session(Context& ctx, const std::filesystem::path& dir) {
    logf(ctx.log, "session: {} process; mod {}; Windows build {}", platform::win32::process_kind(),
         platform::win32::version_string(ctx.module), platform::win32::windows_build());
    logf(ctx.log, "install dir: {}", utf8(dir));
}

void log_settings(Context& ctx) {
    const auto& cfg = ctx.settings.current();
    logf(ctx.log, "config: vr_runtime={} pixel_scale={:.2f} snap_rot={} skip_intro={} dev_debug={}",
         core::config::to_string(cfg.vr_runtime), cfg.rendering.pixel_scale, cfg.input.snap_rotation,
         cfg.debug.skip_startup_intro, cfg.debug.debug);
    logf(ctx.log, "config: settings in effect ({}): {}", ctx.settings.load_source(),
         core::config::describe(cfg));
}

// The exe check: an unknown AI.exe gets a message, and the installer
// patches nothing on it.
void check_game_build(Context& ctx) {
    if (auto v = game::identify_ai_exe()) {
        logf(ctx.log, "ai.exe: recognized build (timestamp=0x{:08X})", static_cast<unsigned>(*v));
        return;
    }
    const auto stamp = game::ai_exe_timestamp_raw();
    logf(ctx.log, "ai.exe: unknown build (timestamp=0x{:08X}), hooks will be skipped", stamp);
    ctx.dialogs.post(core::ui::exe_check_failed(stamp), DialogKind::Warning, nullptr, "unsupported build");
}

// The original's welcome. It defaulted to VR off; this build starts with
// SteamVR, so the text says where to change that instead. The template
// mothervr.json is written once it is closed, so a process that exits
// first (a Steam relaunch) leaves the welcome to the next one.
// Only on a run where the mod is active: on an unknown build or a failed
// check it would promise VR and an options page that are not there, and
// writing the file would use up the welcome before the first run that
// has them.
// From: FUN_1000ce80 (first-run branch).
void welcome_first_launch(Context& ctx, bool installed) {
    if (installed) {
        ctx.dialogs.post(
            std::string(core::ui::welcome()), DialogKind::Information,
            [&ctx] { ctx.settings.save_current(); }, "welcome");
    } else {
        ctx.log.line("config: mod not active this run; welcome and mothervr.json left for a run where it is");
    }
}

// The runtime's name as players know it.
std::string_view runtime_label(core::config::VRRuntime r) noexcept {
    switch (r) {
        case core::config::VRRuntime::OpenVR: return "SteamVR";
        case core::config::VRRuntime::Oculus:
        case core::config::VRRuntime::OpenXR: return "OpenXR";
        case core::config::VRRuntime::None: break;
    }
    return "";
}

// Shows on the player's Discord profile where they are and what they are
// doing. The worker polls only atomics (the play state, the interaction
// state, the stereo device's flags); the runtime is fixed for the run, so
// it is read here.
void start_discord(Context& ctx) {
    if (!ctx.settings.current().discord_rich_presence) {
        ctx.log.line("discord: Rich Presence off in mothervr.json");
        return;
    }
    const auto runtime = ctx.launch.forced_runtime().value_or(ctx.settings.current().vr_runtime);
    const std::string label{runtime_label(runtime)};
    const std::int64_t start =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    ctx.discord.start(
        std::string(platform::discord::kApplicationId),
        [&ctx, label, start] {
            const bool tracked = !ctx.ui_set_only.load(std::memory_order_acquire);
            const auto text    = core::ui::describe({.tracked     = tracked,
                                                     .level       = ctx.play.level(),
                                                     .screen      = ctx.play.screen(),
                                                     .cutscene    = ctx.play.cutscene(),
                                                     .interaction = ctx.interactions.interaction(),
                                                     .hiding      = ctx.interactions.hiding()});
            platform::discord::Activity a{
                .details = std::string(text.details), .state = std::string(text.state), .start_unix = start};
            if (ctx.stereo.vr_session_active())
                a.large_text = std::format("In VR with {}", label);
            else if (label.empty() || ctx.stereo.runtime_failed())
                a.large_text = "VR off";
            else
                a.large_text = std::format("Starting {}", label);
            return a;
        },
        ctx.log);
}

} // namespace

void attach(Context& ctx) {
    const auto dir = platform::win32::module_directory();
    log_session(ctx, dir);

    const bool first_launch = ctx.settings.initialize(dir);
    log_settings(ctx);
    check_game_build(ctx);

    const bool installed = install(ctx, dir);
    if (!installed) {
        // Either nothing was patched (unknown build, a failed preflight) or
        // a write failed part way; the installer's own lines say which.
        ctx.log.line("hooks not (fully) installed, see the [hook] lines above");
    }
    if (first_launch) welcome_first_launch(ctx, installed);
    start_discord(ctx);
}

} // namespace omvr::loader
