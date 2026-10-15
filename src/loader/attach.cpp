#include "loader/attach.h"

#include "core/config/settings_json.h"
#include "core/config/settings_table.h"
#include "core/ui/compat_messages.h"
#include "game/build.h"
#include "loader/context.h"
#include "loader/install.h"
#include "platform/win32/module.h"
#include "platform/win32/process_info.h"
#include "platform/win32/text.h"

#include <format>
#include <string>

namespace omvr::loader {

namespace {

namespace diag {
using core::diag::log_line;
using core::diag::logf;
using platform::win32::module_directory;
using platform::win32::utf8;
} // namespace diag

using core::ui::ports::DialogKind;
using platform::win32::process_kind;
using platform::win32::windows_build;

} // namespace

void attach(Context& ctx) {
    diag::logf("session: {} process; mod {}; Windows build {}", process_kind(),
               platform::win32::version_string(ctx.module), windows_build());

    const auto dir = diag::module_directory();
    diag::logf("install dir: {}", diag::utf8(dir));


    const bool first_launch = ctx.settings.initialize(dir);
    const auto& cfg = ctx.settings.current();
    diag::logf("config: vr_runtime={} pixel_scale={:.2f} snap_rot={} skip_intro={} dev_debug={}",
                         core::config::to_string(cfg.vr_runtime),
                         cfg.rendering.pixel_scale,
                         cfg.input.snap_rotation,
                         cfg.debug.skip_startup_intro,
                         cfg.debug.debug);
    diag::logf("config: settings in effect ({}): {}", ctx.settings.load_source(), core::config::describe(cfg));

    if (auto v = game::identify_ai_exe()) {
        diag::logf("ai.exe: recognized build (timestamp=0x{:08X})",
                             static_cast<unsigned>(*v));
    } else {
        const auto stamp = game::ai_exe_timestamp_raw();
        diag::logf("ai.exe: unknown build (timestamp=0x{:08X}), hooks will be skipped", stamp);
        ctx.dialogs.post(core::ui::exe_check_failed(stamp), DialogKind::Warning, nullptr, "unsupported build");
    }

    const bool installed = install(ctx, dir);
    if (!installed) {
        // Either nothing was patched (unknown build, a failed preflight) or
        // a write failed part way; the installer's own lines say which.
        diag::log_line("hooks not (fully) installed, see the [hook] lines above");
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
    if (first_launch && installed) {
        ctx.dialogs.post(std::string(core::ui::welcome()), DialogKind::Information,
                         [&ctx] { ctx.settings.save_current(); }, "welcome");
    } else if (first_launch) {
        diag::log_line("config: mod not active this run; welcome and mothervr.json left for a run where it is");
    }
}

} // namespace omvr::loader
