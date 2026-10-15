#pragma once

#include "game/event_tap.h"
#include "game/sites/game_behaviour.h"
#include "platform/win32/window/game_window.h"

#include <atomic>

namespace omvr::loader {

// The game's and the system's own functions the thunks call: what the
// replaced vtable slots, call sites and detours held before. Each is set
// once, before its replacement can be called, and read on the threads the
// thunks run on.
struct HookOriginals {
    // AI.exe functions the game-side hooks call or wrap.
    game::sites::GameSideOriginals game_side{};

    // Runtime None: the pause menu's own open, wrapped to push the settings.
    void* pause_open_settings_only = nullptr;

    // The script event tap's trampoline.
    game::TapFn event_tap = nullptr;

    // The swapchain and device methods the proxy replaces.
    void* present             = nullptr;
    void* resize_buffers      = nullptr;
    void* set_fullscreen      = nullptr;
    void* create_pixel_shader = nullptr;

    // The vtable slots are shared by every swapchain of the class in the
    // process, including one an OpenXR runtime or an overlay creates
    // in-process. The hooks act only on the game's: the last one created
    // through our export.
    std::atomic<void*> game_swapchain{nullptr};

    // The game's window procedure, once the window is subclassed.
    platform::win32::WindowProc game_wndproc = nullptr;
};

} // namespace omvr::loader
