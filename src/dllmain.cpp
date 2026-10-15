// MotherVR DLL entry. Thin orchestrator: each subsystem owns its own state
// and exposes init/shutdown.
//
// DllMain runs under the loader lock, so attach does only what has to be in
// place before game code runs, in this order:
//   1. log: nothing else can report problems until this is up.
//   2. config: read mothervr.json; later steps branch on its contents.
//   3. exe check, VR runtime probe, address table, MOTHER.PAK and the code
//      and vtable patches (hook::install). MOTHER.PAK stays here: the game
//      opens its UI archive during startup, through the path patched here.
//   4. the dialog thread for the message boxes queued above (dialogs.h);
//      it starts once the loader releases its lock.
// The real d3d11.dll is loaded on the first call of any of our exports
// (proxy::d3d11::ensure_initialized), and the stereo device and VR runtime
// start when the game creates its stereo device.

#include "config/config.h"
#include "diag/crash_log.h"
#include "diag/log.h"
#include "hook/hook_installer.h"
#include "integration/dialogs.h"
#include "integration/exe_check.h"
#include "proxy/d3d11_proxy.h"

#include <exception>
#include <filesystem>
#include <format>
#include <windows.h>

namespace {

void write_config_template() { mothervr::config::save_current(); }

void attach(HINSTANCE self) {
    const auto dir = mothervr::diag::module_directory();
    mothervr::diag::logf("install dir: {}", mothervr::diag::utf8(dir));

    const bool first_launch = mothervr::config::initialize(dir);
    const auto& cfg = mothervr::config::current();
    mothervr::diag::logf("config: vr_runtime={} pixel_scale={:.2f} snap_rot={} skip_intro={} dev_debug={}",
                         mothervr::config::to_string(cfg.vr_runtime),
                         cfg.rendering.pixel_scale,
                         cfg.input.snap_rotation,
                         cfg.debug.skip_startup_intro,
                         cfg.debug.debug);

    if (auto v = mothervr::integration::identify_ai_exe()) {
        mothervr::diag::logf("ai.exe: recognized build (timestamp=0x{:08X})",
                             static_cast<unsigned>(*v));
    } else {
        const auto stamp = mothervr::integration::ai_exe_timestamp_raw();
        mothervr::diag::logf("ai.exe: unknown build (timestamp=0x{:08X}), hooks will be skipped", stamp);
        mothervr::integration::post_message_box(std::format(
            "Exe check failed!\n[0x{:X}]\n\n"
            "This check is to ensure that MotherVR is correctly modifying the game, and is looking in "
            "the correct locations.\n\n"
            "If Alien: Isolation is on an out-dated version, make sure to update through Steam or Epic. "
            "If a new update to the game has just been released, or this check failed for some other "
            "reason, MotherVR may need to be updated.\n\n"
            "MotherVR will now be disabled, and Alien: Isolation will startup normally.",
            stamp), MB_OK | MB_ICONWARNING, nullptr, "unsupported build");
    }

    const bool installed = mothervr::hook::install(self, dir);
    if (!installed) {
        // Either nothing was patched (unknown build, a failed preflight) or
        // a write failed part way; the installer's own lines say which.
        mothervr::diag::log_line("hooks not (fully) installed, see the [hook] lines above");
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
        mothervr::integration::post_message_box(
            "Welcome to MotherVR for Alien: Isolation!\n\n"
            "VR starts with SteamVR. To use OpenXR or turn VR off, go to Options->MotherVR after the "
            "game starts up. This message is only shown on the first launch of MotherVR.",
            MB_OK | MB_ICONINFORMATION, &write_config_template, "welcome");
    } else if (first_launch) {
        mothervr::diag::log_line("config: mod not active this run; welcome and mothervr.json left for a run where it is");
    }
}

void on_attach(HINSTANCE self) noexcept {
    mothervr::diag::init(self);
    mothervr::diag::log_line("== MotherVR attach ==");
    mothervr::diag::install_crash_log();
    // Nothing may leave DllMain: a throw here would kill the game at launch.
    try {
        attach(self);
    } catch (const std::exception& e) {
        mothervr::diag::logf("attach failed: {}", e.what());
    } catch (...) {
        mothervr::diag::log_line("attach failed: unknown exception");
    }
    mothervr::integration::show_posted_message_boxes();
}

void on_detach(bool process_exit) noexcept {
    mothervr::diag::log_line("== MotherVR detach ==");
    // At process exit the game's D3D objects are still alive and other
    // threads are gone: leave everything to the OS.
    if (process_exit) return;
    mothervr::proxy::d3d11::shutdown();
}

} // namespace

BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            ::DisableThreadLibraryCalls(hinst);
            on_attach(hinst);
            return TRUE;
        case DLL_PROCESS_DETACH:
            on_detach(reserved != nullptr);
            return TRUE;
        default:
            return TRUE;
    }
}
