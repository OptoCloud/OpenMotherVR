// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

struct IDXGISwapChain;

// The game's DXGI swapchain. Render thread: the game creates, resizes and
// presents it there.
namespace omvr::platform::d3d11 {

// A one-time GetDesc snapshot of the chain (resolution, format, buffer
// count) in the log.
void log_swapchain_desc(IDXGISwapChain* sc, core::diag::ports::Log& log) noexcept;

// The window the chain presents to, or null.
void* output_window(IDXGISwapChain* sc) noexcept;

// While a headset is attached (`vr`), a window that does not fit its
// monitor becomes borderless (win32::fit_window).
void fit_game_window(IDXGISwapChain* sc, bool vr, core::diag::ports::Log& log) noexcept;

// DXGI's own Alt+Enter would also switch the side-by-side buffer to
// fullscreen. It goes through SetFullscreenState, which refuses, but the
// association is switched off as well, on the factory that made the
// swapchain, so the toggle is not attempted at all while VR (`vr`) is up.
void no_alt_enter_while_vr(IDXGISwapChain* sc, void* hwnd, bool vr, core::diag::ports::Log& log) noexcept;

} // namespace omvr::platform::d3d11
