// OpenMotherVR-Provenance: clean
#pragma once

namespace omvr::loader::hooks {

// The game treats losing the keyboard focus in its fullscreen mode as a
// pause request (its WM_KILLFOCUS handler posts the event that opens the
// pause menu) and drops its input. Under VR it keeps that mode while the
// swapchain is forced windowed, and the window is only a mirror: whatever
// takes the desktop focus (SteamVR, a streaming client) paused the game or
// left it stuck behind a loading screen. The focus loss is swallowed while
// a headset is attached; WM_ACTIVATE still goes through, so the game
// releases its cursor clip. Subclasses `hwnd` once, while a headset is
// attached.
void keep_focus_while_vr(void* hwnd) noexcept;

} // namespace omvr::loader::hooks
