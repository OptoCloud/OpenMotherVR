// The DLL entry. Thin orchestrator: the context owns every subsystem.
//
// DllMain runs under the loader lock, so attach does only what has to be in
// place before game code runs, in this order:
//   0. the context: every object, built without doing work.
//   1. log: nothing else can report problems until this is up.
//   2. config: read mothervr.json; later steps branch on its contents.
//   3. exe check, address table, MOTHER.PAK and the code and vtable patches
//      (install). MOTHER.PAK stays here: the game opens its UI archive
//      during startup, through the path patched here.
//   4. the dialog thread for the message boxes queued above; it starts once
//      the loader releases its lock.
// The real d3d11.dll is loaded on the first call of any of our exports
// (ensure_d3d11_loaded), and the stereo device and VR runtime start when the
// game creates its stereo device.

#include "loader/attach.h"
#include "loader/context.h"
#include "loader/hooks/crash_hook.h"
#include "platform/win32/module.h"

#include <windows.h>

#include <exception>

namespace {

namespace diag = omvr::core::diag;

void on_attach(HINSTANCE self) noexcept {
    auto& ctx = omvr::loader::create_context(self);
    if (const auto dir = omvr::platform::win32::module_directory(); !dir.empty()) {
        ctx.log_sink.open(dir / "mothervr.log");
    }
    diag::log_line("== MotherVR attach ==");
    omvr::loader::hooks::install_crash_hook();
    // Nothing may leave DllMain: a throw here would kill the game at launch.
    try {
        omvr::loader::attach(ctx);
    } catch (const std::exception& e) {
        diag::logf("attach failed: {}", e.what());
    } catch (...) {
        diag::log_line("attach failed: unknown exception");
    }
    ctx.dialogs.start();
}

void on_detach(bool process_exit) noexcept {
    diag::log_line(process_exit ? "== MotherVR detach == (process exit)"
                                : "== MotherVR detach == (unload)");
    // At process exit the game's D3D objects are still alive and other
    // threads are gone: leave everything to the OS.
    if (process_exit) return;
    omvr::loader::context().system_d3d11.shutdown();
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
