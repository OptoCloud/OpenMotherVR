// OpenMotherVR-Provenance: clean
#pragma once

#include "game/event_tap.h"
#include "game/hands/hand_objects.h"
#include "game/hands/haptic_sites.h"
#include "game/input/input_manager.h"
#include "game/sites/game_behaviour.h"
#include "game/sites/render_fixes.h"
#include "game/stereo/mode_stack.h"
#include "platform/win32/window/game_window.h"

#include <atomic>
#include <cstdint>

namespace omvr::loader {

// The game's and the system's own functions the thunks call: what the
// replaced vtable slots, call sites and detours held before, and the game
// functions the render fixes call. Each is set once, before its
// replacement can be called, and read on the threads the thunks run on.
struct HookTargets {
    // AI.exe functions the game-side hooks call or wrap.
    game::sites::GameSideOriginals game_side{};

    // Runtime None: the pause menu's own open, wrapped to push the settings.
    void* pause_open_settings_only = nullptr;

    // The AI.exe functions the render fixes call (07, 08), and the game's
    // mode-stack append (05 §4.4), null on a build without it.
    game::sites::RenderFixTargets render_fixes{};
    game::stereo::ModeStackTargets mode_stack{};

    // The game pad's five methods behind the replaced vtable slots (04 §3.3).
    game::input::StockPad stock_pad{};

    // The hand features' game functions (spec 21 §20), set when their
    // sites go in.
    game::hands::HandTargets hands{};
    // The haptics' game functions (21 §14), set when their sites go in.
    game::hands::HapticTargets haptics{};

    // The script event tap's trampoline.
    game::TapFn event_tap = nullptr;

    // The swapchain and device methods the proxy replaces. Atomic: stored
    // before the shared vtable slot is swapped (replace_pointer), and read on
    // any thread that calls through it.
    std::atomic<void*> present{nullptr};
    std::atomic<void*> resize_buffers{nullptr};
    std::atomic<void*> set_fullscreen{nullptr};
    std::atomic<void*> create_pixel_shader{nullptr};
    // ID3D11Device::CreateVertexShader, for the upright sprites (24 §14).
    std::atomic<void*> create_vertex_shader{nullptr};

    // The vtable slots are shared by every swapchain of the class in the
    // process, including one an OpenXR runtime or an overlay creates
    // in-process. The hooks act only on the game's: the last one created
    // through our export.
    std::atomic<void*> game_swapchain{nullptr};
    // The swapchain hooks go in once, on the game's first swapchain.
    std::atomic<bool> swapchain_hooks_installed{false};
    // Presents since the swapchain was created or last resized. Relaxed: a
    // counter for the first-frame line, on the render thread.
    std::atomic<unsigned long> present_count{0};

    // The game's window procedure, once the window is subclassed: stored
    // before the subclass, read on the game thread.
    std::atomic<platform::win32::WindowProc> game_wndproc{nullptr};
    // The game's window-focus byte (game/input/window_focus.h), null on a
    // build without it. Set at install, before the window is subclassed.
    std::uint8_t* window_focus = nullptr;
};

} // namespace omvr::loader
