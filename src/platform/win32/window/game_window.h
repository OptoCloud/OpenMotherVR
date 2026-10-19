// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

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
//
// Any thread (swapchain creation and ResizeBuffers, on the render thread).
// The style change sends messages to the window's thread and waits for
// them (spec 03 §9.3: it must not wait for the game thread, which may be
// waiting on this one), so on any other thread the fit is posted to the
// window, whose subclassed procedure hands it to handle_fit_message.
void fit_window(void* wnd, core::diag::ports::Log& log) noexcept;

// The window procedure's part of fit_window: true, after fitting the window,
// when `msg` is the fit posted from another thread. Game thread.
bool handle_fit_message(HWND__* hwnd, unsigned int msg, core::diag::ports::Log& log) noexcept;

// The process that took the keyboard focus, for the log. Game thread.
void log_focus_loss(core::diag::ports::Log& log) noexcept;

// Subclasses the game's window with `replacement` (once; `game_proc`
// receives the game's own procedure). Pins the module.
void subclass_game_window(void* hwnd, WindowProc replacement, WindowProc& game_proc,
                          core::diag::ports::Log& log) noexcept;

// Calls the game's own window procedure `game_proc`.
long call_window_proc(WindowProc game_proc, HWND__* hwnd, unsigned int msg, unsigned int wp,
                      long lp) noexcept;

} // namespace omvr::platform::win32
