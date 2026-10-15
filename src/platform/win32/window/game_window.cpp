#include "platform/win32/window/game_window.h"

#include "core/diag/logger.h" // R: proxy (A2): log through an injected Log&
#include "platform/win32/patching/memory.h"
#include "platform/win32/text.h"

#include <windows.h>

#include <filesystem>
#include <type_traits>

namespace omvr::platform::win32 {

namespace {

namespace diag {
using core::diag::log_line;
using core::diag::logf;
using win32::utf8;
} // namespace diag

static_assert(std::is_same_v<WindowProc, WNDPROC>);

} // namespace

void fit_window(void* wnd) noexcept {
    const auto hwnd = static_cast<HWND>(wnd);
    if (!hwnd || ::IsZoomed(hwnd) || ::IsIconic(hwnd)) return;
    RECT window{};
    if (!::GetWindowRect(hwnd, &window)) return;
    MONITORINFO mi{sizeof(mi)};
    if (!::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) return;

    const LONG style = ::GetWindowLongA(hwnd, GWL_STYLE);
    const bool framed = (style & WS_CAPTION) == WS_CAPTION;
    const RECT& area = framed ? mi.rcWork : mi.rcMonitor;
    const LONG area_w = area.right - area.left, area_h = area.bottom - area.top;
    const LONG win_w = window.right - window.left, win_h = window.bottom - window.top;
    if (win_w <= area_w && win_h <= area_h) return;

    const RECT& mon = mi.rcMonitor;
    // The game's window class is ANSI: the A functions keep it so.
    ::SetWindowLongA(hwnd, GWL_STYLE, style & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |
                                                WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
    // Asynchronous: the window belongs to the game thread, which may be
    // waiting on this one.
    ::SetWindowPos(hwnd, nullptr, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS);
    diag::logf("proxy: mirror window {}x{} did not fit the monitor, now borderless {}x{}", win_w, win_h,
               mon.right - mon.left, mon.bottom - mon.top);
}

void log_focus_loss() noexcept {
    DWORD pid = 0;
    ::GetWindowThreadProcessId(::GetForegroundWindow(), &pid);
    wchar_t name[MAX_PATH] = L"?";
    if (HANDLE p = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        DWORD size = MAX_PATH;
        if (!::QueryFullProcessImageNameW(p, 0, name, &size)) name[0] = L'\0';
        ::CloseHandle(p);
    }
    diag::logf("proxy: focus taken by process {} ({}), game kept running", pid,
               diag::utf8(std::filesystem::path{name}.filename()));
}

void subclass_game_window(void* window, WindowProc replacement, WindowProc& original) noexcept {
    const auto hwnd = static_cast<HWND>(window);
    if (!hwnd || original) return;
    // The window procedure points into this DLL for the rest of the process.
    pin_this_module();
    original = reinterpret_cast<WNDPROC>(
        ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(replacement)));
    if (original) diag::log_line("proxy: focus loss no longer pauses the game");
}

long call_window_proc(WindowProc original, HWND__* hwnd, unsigned int msg, unsigned int wp, long lp) noexcept {
    return ::CallWindowProcA(original, hwnd, msg, wp, lp);
}

} // namespace omvr::platform::win32
