// OpenMotherVR-Provenance: clean
#include "loader/hooks/window_hooks.h"

#include "loader/context.h"
#include "platform/win32/window/game_window.h"

#include <windows.h>

#include <type_traits>

namespace omvr::loader::hooks {

namespace {

// The game's window procedure while a headset is attached. Game thread.
LRESULT CALLBACK game_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) noexcept {
    auto& ctx = context();
    // A mirror window fit asked for on the render thread (spec 03 §9.3).
    if (platform::win32::handle_fit_message(hwnd, msg, ctx.log)) return 0;
    if (msg == WM_KILLFOCUS && ctx.window.headset_attached()) {
        platform::win32::log_focus_loss(ctx.log);
        return 0;
    }
    return platform::win32::call_window_proc(ctx.targets.game_wndproc.load(std::memory_order_acquire), hwnd,
                                             msg, wp, lp);
}

static_assert(std::is_convertible_v<decltype(&game_wndproc), WNDPROC>);
static_assert(std::is_convertible_v<decltype(&game_wndproc), platform::win32::WindowProc>);

} // namespace

void keep_focus_while_vr(void* hwnd) noexcept {
    auto& ctx = context();
    if (!hwnd || ctx.targets.game_wndproc.load(std::memory_order_acquire) || !ctx.window.headset_attached())
        return;
    platform::win32::subclass_game_window(hwnd, &game_wndproc, ctx.targets.game_wndproc, ctx.log);
}

} // namespace omvr::loader::hooks
