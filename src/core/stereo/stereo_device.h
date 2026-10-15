#pragma once

#include "core/stereo/stereo_update.h"
#include "core/ui/ports/dialogs.h"
#include "core/vr/ports/vr_runtime.h"
#include "core/vr/runtime_choice.h"
#include "game/layout.h" // R: stereo (A3): the device's state as value structs; thunks write the game layout

#include <atomic>
#include <functional>
#include <memory>

// The stereo device the game is given in place of its own: the VR runtime
// behind it, its start-up and its per-frame work.
//
// From: FUN_10003510 / FUN_10003640 (init), FUN_1001eac0 (device init),
// FUN_1001f6a0 (CreateDevice), FUN_100202a0 (GetRenderTargetSize hook), the
// StereoInterface vtable 0x1005a5c0 (FUN_1001ed50 .. FUN_1001f300) and the
// StereoManager vtable 0x1005a61c (FUN_10020230, FUN_10020290).
// Analysis: dumps/06a_stereo_interface.md.
namespace omvr::core::stereo {

class StereoDevice {
public:
    // Makes the runtime for the chosen kind (OpenVR or OpenXR).
    using RuntimeFactory =
        std::move_only_function<std::unique_ptr<vr::ports::VrRuntime>(vr::detect_result::Chosen)>;

    StereoDevice(game::StereoManager& manager, StereoUpdate& update, ui::ports::Dialogs& dialogs) noexcept
        : g_manager(manager), update_(update), dialogs_(dialogs) {}

    void set_runtime_factory(RuntimeFactory factory) noexcept { make_runtime_ = std::move(factory); }

    game::StereoManager& manager() noexcept { return g_manager; }

    // True once the stereo device gave up on its VR runtime (init or session
    // failure). The player has been told and VR stays off for this run.
    bool runtime_failed() const noexcept { return g_runtime_failed; }

    // The device is attached and runs stereo. Also called from the game's
    // job threads (the cull hooks), hence the atomic loads; only the main
    // thread changes the device and its state.
    bool running() const noexcept;

    // The game's stereo manager has a device attached.
    bool device_attached() const noexcept;

    // Set to true once the VR session object has been successfully
    // initialised (we own a live IVRSystem / XrSession). Read when the
    // settings are pushed to populate the "setMotherVROptionsEnabled" game
    // variable. Atomic: written on the game thread (CreateDevice) and on the
    // render thread (a runtime that fails late, in the stereo device's slot
    // 3, or the OpenXR start at the first Present), read on the game thread.
    // Corresponds to the *(StereoManager + 4) != nullptr check in FUN_10020870.
    bool vr_session_active() const noexcept { return g_vr_session_active; }
    void set_vr_session_active(bool active) noexcept { g_vr_session_active = active; }

    // Manager::CreateDevice(1): bring up the runtime and attach the device.
    // Only the first call starts the runtime; later ones (a display reset)
    // re-attach what it produced. From: FUN_1001f6a0.
    void create_device(game::StereoManager* self);

    // [3] ConfigureRendering(stereo render target): describe the eye texture,
    // create the mirror resources. If the runtime cannot drive the headset
    // with the game's device (no OpenXR session), the device detaches here;
    // the mirror resources are still made for any call already on its way.
    bool configure_rendering(game::StereoInterface& si, void* stereo_rt);

    // [5] BeginFrame: poll the controllers, take the eye offsets.
    void begin_frame(game::StereoInterface& si);

private:
    void fail_runtime(game::StereoInterface& si, vr::detect_result::Chosen chosen, bool late) noexcept;
    void init_device(game::StereoInterface& si) noexcept;

    game::StereoManager& g_manager;
    StereoUpdate&        update_;
    ui::ports::Dialogs&  dialogs_;
    RuntimeFactory       make_runtime_;

    // The runtime behind the device. A plain pointer, not a unique_ptr, so no
    // destructor calls into the VR runtime at process exit: that runs in
    // DLL_PROCESS_DETACH under the loader lock, where VR_Shutdown or
    // xrDestroyInstance (which wait on runtime threads) can deadlock. A session
    // still open at exit is left to the OS and the runtime, which both handle a
    // process that ends. Owns the runtime; after a late detach (fail_runtime)
    // it keeps it, unused, until exit.
    vr::ports::VrRuntime* g_backend = nullptr;
    // Set on the game thread (init) or the render thread (late detach); read
    // by the Present hook and the options page.
    std::atomic<bool>     g_runtime_failed{false};
    std::atomic<bool>     g_vr_session_active{false};
};

// R: stereo (A3) and its callers: take the StereoDevice at construction.
// Until then the loader registers its device here at attach.
void set_process_stereo(StereoDevice* stereo) noexcept;
bool runtime_failed() noexcept;
bool vr_session_active() noexcept;

} // namespace omvr::core::stereo
