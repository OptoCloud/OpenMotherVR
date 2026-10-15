#include "platform/win32/window/game_window.h"

#include "platform/win32/patching/memory.h"
#include "platform/win32/text.h"
#include "util/base/scope_exit.h"

#include <windows.h>

#include <filesystem>
#include <type_traits>

namespace omvr::platform::win32 {

namespace {

namespace ports = core::diag::ports;

static_assert(std::is_same_v<WindowProc, WNDPROC>);

constexpr std::string_view kMadeBorderlessLog =
    "proxy: mirror window {}x{} did not fit the monitor, now borderless {}x{}";
constexpr std::string_view kFocusTakenLog = "proxy: focus taken by process {} ({}), game kept running";
constexpr std::string_view kSubclassedLog = "proxy: focus loss no longer pauses the game";

// The frame parts a borderless window drops.
constexpr LONG kFrameStyles = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

LONG width(const RECT& r) noexcept { return r.right - r.left; }
LONG height(const RECT& r) noexcept { return r.bottom - r.top; }

bool fits(const RECT& window, const RECT& area) noexcept {
    return width(window) <= width(area) && height(window) <= height(area);
}

} // namespace

void fit_window(void* wnd, ports::Log& log) noexcept {
    const auto hwnd = static_cast<HWND>(wnd);
    if (!hwnd || ::IsZoomed(hwnd) || ::IsIconic(hwnd)) return;
    RECT window{};
    if (!::GetWindowRect(hwnd, &window)) return;
    MONITORINFO mi{sizeof(mi)};
    if (!::GetMonitorInfoW(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) return;

    const LONG style = ::GetWindowLongA(hwnd, GWL_STYLE);
    const bool framed = (style & WS_CAPTION) == WS_CAPTION;
    if (fits(window, framed ? mi.rcWork : mi.rcMonitor)) return;

    const RECT& mon = mi.rcMonitor;
    // The game's window class is ANSI: the A functions keep it so.
    ::SetWindowLongA(hwnd, GWL_STYLE, style & ~kFrameStyles);
    // Asynchronous: the window belongs to the game thread, which may be
    // waiting on this one.
    ::SetWindowPos(hwnd, nullptr, mon.left, mon.top, width(mon), height(mon),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS);
    ports::logf(log, kMadeBorderlessLog, width(window), height(window), width(mon), height(mon));
}

void log_focus_loss(ports::Log& log) noexcept {
    DWORD pid = 0;
    ::GetWindowThreadProcessId(::GetForegroundWindow(), &pid);
    wchar_t name[MAX_PATH] = L"?";  // what the log shows when the process cannot be opened
    if (HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        const util::ScopeExit close{[process] { ::CloseHandle(process); }};
        DWORD size = MAX_PATH;
        if (!::QueryFullProcessImageNameW(process, 0, name, &size)) name[0] = L'\0';
    }
    ports::logf(log, kFocusTakenLog, pid, utf8(std::filesystem::path{name}.filename()));
}

void subclass_game_window(void* window, WindowProc replacement, WindowProc& game_proc, ports::Log& log) noexcept {
    const auto hwnd = static_cast<HWND>(window);
    if (!hwnd || game_proc) return;
    // The window procedure points into this DLL for the rest of the process.
    pin_this_module(log);
    game_proc = reinterpret_cast<WNDPROC>(
        ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(replacement)));
    if (game_proc) log.line(kSubclassedLog);
}

long call_window_proc(WindowProc game_proc, HWND__* hwnd, unsigned int msg, unsigned int wp, long lp) noexcept {
    return ::CallWindowProcA(game_proc, hwnd, msg, wp, lp);
}

} // namespace omvr::platform::win32
