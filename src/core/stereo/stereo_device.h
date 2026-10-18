#pragma once

#include "core/config/ports/launch_switches.h"
#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/stereo/ports/desktop_mirror.h"
#include "core/stereo/stereo_update.h"
#include "core/stereo/types.h"
#include "core/ui/ports/dialogs.h"
#include "core/vr/ports/vr_runtime.h"
#include "core/vr/runtime_choice.h"
#include "core/vr/runtime_detector.h"
#include "core/vr/runtime_failure.h"
#include "core/vr/types.h"
#include "game/layout.h" // layer exception: the device works on the game's StereoInterface in place
#include "util/sync/once_latch.h"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

// The stereo device the game is given in place of its own: the VR runtime
// behind it, its start-up and its per-frame work (02 §5, 03).
//
// The device is the object the game holds, so it works on the game's
// StereoInterface in place: this and stereo_device.cpp are the one core
// pair that includes game layouts (scripts/check_layers.py lists them).
//
// From: FUN_10003510 / FUN_10003640 (init), FUN_1001eac0 (device init),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook), the
// StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300) and the
// StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290).
// Analysis: dumps/06a_stereo_interface.md.
namespace omvr::core::stereo {

class StereoDevice final {
public:
    // Makes the runtime for the chosen kind (OpenVR or OpenXR).
    using RuntimeFactory =
        std::move_only_function<std::unique_ptr<vr::ports::VrRuntime>(vr::DetectResult::Chosen)>;

    // `settings` is the live settings object (pixel scale and mirror are
    // read when used); `runtimes` says which runtime this run drives and
    // `failure` why it failed to start, for the player's message.
    StereoDevice(game::StereoManager& manager, StereoUpdate& update, ui::ports::Dialogs& dialogs,
                 const config::Settings& settings, const config::ports::LaunchSwitches& switches,
                 vr::RuntimeDetector& runtimes, const vr::RuntimeFailure& failure,
                 diag::ports::Log& log) noexcept
        : manager_(manager),
          update_(update),
          dialogs_(dialogs),
          settings_(settings),
          switches_(switches),
          runtimes_(runtimes),
          failure_(failure),
          log_(log) {}

    void set_runtime_factory(RuntimeFactory factory) noexcept { make_runtime_ = std::move(factory); }

    game::StereoManager& manager() noexcept { return manager_; }

    // True once the stereo device gave up on its VR runtime (init or session
    // failure). The player has been told and VR stays off for this run.
    bool runtime_failed() const noexcept { return runtime_failed_; }

    // The device is attached and runs stereo. Also called from the game's
    // job threads (the cull hooks), hence the atomic loads; only the main
    // thread changes the device and its state.
    bool running() const noexcept;

    // The game's stereo manager has a device attached.
    bool device_attached() const noexcept;

    // The desktop window's size as the game last gave it to
    // GetRenderTargetSize, while a device is attached; nullopt without one.
    std::optional<PixelSize> attached_window_size() const noexcept;

    // Set to true once the VR session object has been successfully
    // initialised (we own a live IVRSystem / XrSession). Read when the
    // settings are pushed to populate the "setMotherVROptionsEnabled" game
    // variable. Atomic: written on the game thread (CreateDevice) and on the
    // render thread (a runtime that fails late, in the stereo device's slot
    // 3, or the OpenXR start at the first Present), read on the game thread.
    // Corresponds to the *(StereoManager + 4) != nullptr check in FUN_10020870.
    bool vr_session_active() const noexcept { return vr_session_active_; }
    void set_vr_session_active(bool active) noexcept { vr_session_active_ = active; }

    // Manager::CreateDevice(1) on `self` (03 §6.4, §6.5): bring up the
    // runtime and attach the device. Only the first call starts the runtime;
    // later ones (a display reset) re-attach what it produced. Game thread.
    // From: FUN_1001f6a0.
    void create_device(game::StereoManager& self);

    // [3] ConfigureRendering (03 §4.4): describe the eye texture, create the
    // mirror resources. `texture_size` is the real size of the stereo
    // texture, nullopt without one; `device` and `context` are the game's.
    // If the runtime cannot drive the headset with the game's device (no
    // OpenXR session), the device detaches here; the mirror resources are
    // still made for any call already on its way. Render thread.
    void configure_rendering(game::StereoInterface& si, std::optional<PixelSize> texture_size,
                             vr::GpuDevice device, vr::GpuContext context, ports::DesktopMirror& mirror);

    // [5] BeginFrame (03 §5, §6.2): poll the controllers, take the eye
    // offsets. Render thread.
    void begin_frame(game::StereoInterface& si);

    // [16] GetRenderTargetSize (03 §4): both eyes side by side, and the eye
    // viewports. The original asks for half scale until stereo runs. The
    // game sizes its targets from that first answer (the same call picks the
    // display mode) and does not rebuild them when stereo starts, so the eyes
    // stay at half resolution. We use the configured pixel scale from the
    // start; the swapchain is kept windowed so the mode change cannot fail.
    PixelSize render_target_size(game::StereoInterface& si, vr::ports::VrRuntime& runtime);

    // The VR-off hotkey fired (22 §13): VR goes off at the end of the
    // current frame. Game thread.
    void request_vr_off() noexcept { vr_off_requested_.store(true, std::memory_order_release); }

    // The player turned VR off this session. Any thread.
    bool turned_off() const noexcept { return turned_off_.load(std::memory_order_acquire); }

    // After the device presented a frame: if VR-off was requested, detaches
    // the device as after a late runtime failure (02 §7.4) but without the
    // failure message, and stops the runtime (an OpenXR session is ended in
    // the order the API requires). True when it turned VR off, so the
    // game's targets can be rebuilt at the window's size. Render thread.
    bool turn_off_if_requested(game::StereoInterface& si) noexcept;

private:
    void detach(game::StereoInterface& si) noexcept;
    void fail_runtime(game::StereoInterface& si, vr::DetectResult::Chosen chosen, bool late) noexcept;
    void init_device(game::StereoInterface& si) noexcept;

    game::StereoManager& manager_;
    StereoUpdate& update_;
    ui::ports::Dialogs& dialogs_;
    const config::Settings& settings_;
    const config::ports::LaunchSwitches& switches_;
    vr::RuntimeDetector& runtimes_;
    const vr::RuntimeFailure& failure_;
    diag::ports::Log& log_;
    RuntimeFactory make_runtime_;

    // The runtime behind the device. Owned here; the context that holds the
    // device is never destroyed (loader/context.cpp), so no destructor calls
    // into the VR runtime at process exit: that runs in DLL_PROCESS_DETACH
    // under the loader lock, where VR_Shutdown or xrDestroyInstance (which
    // wait on runtime threads) can deadlock. A session still open at exit is
    // left to the OS and the runtime, which both handle a process that ends.
    // After a late detach (fail_runtime) it is kept, unused, until exit.
    std::unique_ptr<vr::ports::VrRuntime> runtime_;
    // Set on the game thread (init) or the render thread (late detach); read
    // by the Present hook and the options page.
    std::atomic<bool> runtime_failed_{false};
    std::atomic<bool> vr_session_active_{false};
    // 22 §13: set on the game thread by the hotkey, taken on the render
    // thread between frames.
    std::atomic<bool> vr_off_requested_{false};
    std::atomic<bool> turned_off_{false};
    // Game thread: CreateDevice has run once.
    bool created_ = false;
    // Game thread: the render target size last logged.
    PixelSize logged_size_{};
    util::OnceLatch eye_offsets_logged_;
};

} // namespace omvr::core::stereo
