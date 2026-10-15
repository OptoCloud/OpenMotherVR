#pragma once

#include <cstdint>

struct HWND__;

// The game's window while a headset is attached: it is only a mirror.
namespace omvr::platform::win32 {

// A window procedure (WNDPROC).
// x86: LRESULT and LPARAM are long, WPARAM unsigned int.
using WindowProc = long(__stdcall*)(HWND__* hwnd, unsigned int msg, unsigned int wp, long lp);

// The game sizes its window for the display mode it picked (the monitor's
// full resolution in its fullscreen mode) and gives it a title bar and
// borders in every mode. Forced windowed under VR, that runs past the screen.
// A window that does not fit becomes a borderless one covering its monitor:
// what fullscreen looked like, with the client area at the monitor's size,
// so the game's idea of its window size and the mouse still match. Shrinking
// it instead left a client area smaller than the size the game kept (it
// ignores WM_SIZE in its fullscreen mode), which put the UI and the cursor
// off. A window that already fits is left alone, so a size the player
// picks stays.
void fit_window(void* wnd) noexcept;

// The process that took the keyboard focus, for the log.
void log_focus_loss() noexcept;

// Subclasses the game's window with `replacement` (once; `original`
// receives the game's procedure). Pins the module.
void subclass_game_window(void* hwnd, WindowProc replacement, WindowProc& original) noexcept;

// Calls the game's window procedure `original`.
long call_window_proc(WindowProc original, HWND__* hwnd, unsigned int msg, unsigned int wp, long lp) noexcept;

} // namespace omvr::platform::win32
