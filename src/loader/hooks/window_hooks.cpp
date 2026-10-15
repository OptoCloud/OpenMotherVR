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
    if (msg == WM_KILLFOCUS && ctx.window.headset_attached()) {
        platform::win32::log_focus_loss();
        return 0;
    }
    return platform::win32::call_window_proc(ctx.originals.game_wndproc, hwnd, msg, wp, lp);
}

static_assert(std::is_convertible_v<decltype(&game_wndproc), WNDPROC>);

} // namespace

void keep_focus_while_vr(void* hwnd) noexcept {
    auto& ctx = context();
    if (!hwnd || ctx.originals.game_wndproc || !ctx.window.headset_attached()) return;
    platform::win32::subclass_game_window(hwnd, &game_wndproc, ctx.originals.game_wndproc);
}

} // namespace omvr::loader::hooks
