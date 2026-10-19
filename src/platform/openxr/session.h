// OpenMotherVR-Provenance: clean
#pragma once

// The OpenXR session, so the mod runs on any OpenXR runtime (09).
//
// Lifecycle, all on the game's render thread:
//   probe()        from the runtime detector; finds a 32-bit runtime (09 §1)
//   init_system()  in Stereo::CreateDevice (OpenXrRuntime::init), before the
//                  game has a D3D11 device (09 §2)
//   init()         in ConfigureRendering, with the game's device; or on the
//                  first Present when there is no stereo device (virtual
//                  screen only, VirtualScreen, 09 §8)
//   frame()        every submitted frame: ends the frame the previous call
//                  began (copies, flush, xrEndFrame), then events, and
//                  begins the next (xrWaitFrame, xrBeginFrame, views, input)
//   shutdown()     when the stereo device drops its runtime (a failed
//                  session). Not at process exit: the runtime cleans up
//                  after a process that ends with a session open.
//   end_on_request() when the player turns VR off (22 §13)
//
// This header exposes project types only; the OpenXR and D3D11 state lives
// in Session::Impl (session_impl.h).

#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/stereo/types.h"
#include "core/ui/ports/dialogs.h"
#include "core/vr/ports/haptic_source.h"
#include "core/vr/types.h"
#include "platform/openxr/constants.h"
#include "platform/openxr/controller_state.h"
#include "util/math/pose.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace omvr::core::vr {
class RuntimeFailure;
} // namespace omvr::core::vr

namespace omvr::platform::openxr {

// What the runtime reports about the system before a session exists.
struct SystemInfo {
    std::uint32_t recommended_w = 0, recommended_h = 0;         // per eye
    bool has_fov = false;                                       // the runtime reported its FOV
    std::array<core::vr::FovAngles, core::vr::kEyeCount> fov{}; // per eye, radians
    char system_name[kSystemNameSize]{};
    char runtime_name[kRuntimeNameSize]{};
    // The headset's audio output endpoint (XR_OCULUS_audio_device_guid); empty
    // when the runtime does not say.
    std::wstring audio_output;
};

// One eye's render parameters for the frame being submitted.
struct EyeView {
    util::Pose pose{};         // in the seated (LOCAL) space
    core::vr::FovAngles fov{}; // half-angles in radians (left/down negative)
};

// A controller's grip pose (/input/grip/pose: origin in the palm, -Z along
// the handle) and linear velocity, both in the seated space.
struct HandPose {
    util::Pose pose{};
    util::Vec3 velocity{}; // metres per second
    bool valid          = false;
    bool velocity_valid = false;
    util::Pose aim{}; // the aim pose (spec 21 §5.2), when aim_valid
    bool aim_valid = false;
};

// Per-frame view state, valid after a frame() that returned true.
// Invalid locations follow 09 §5.5: `head` is always usable (the last valid
// orientation and position held where the runtime reports them invalid, the
// neutral pose before any), and the eyes keep their last valid positions.
struct FrameViews {
    std::array<EyeView, core::vr::kEyeCount> eyes{};
    util::Pose head{};
    std::array<HandPose, core::vr::kHandCount> hands{}; // left, right; located with the views
    std::int64_t display_time = 0;                      // the predictedDisplayTime they were located for
    bool valid                = false; // eye orientations valid: a projection layer may use them
    bool eye_positions_valid  = false; // eye positions are this frame's, not held
    bool head_valid           = false; // head orientation and position both this frame's
    // The floor's height in the seated space at display_time (22 §6.1): the
    // last valid one, located with the head; floor_valid once it has been.
    float floor_y    = 0.0f;
    bool floor_valid = false;
};

// A region of a texture in pixels (the depth slice is always the first).
struct TextureRegion {
    std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
};

// Per-eye images to present this frame. When set before frame(), the frame is
// submitted as a stereo projection layer; otherwise the game's back buffer is
// shown on a world-locked virtual screen.
struct EyeImages {
    std::array<core::vr::GpuTexture, core::vr::kEyeCount> texture{};
    std::array<TextureRegion, core::vr::kEyeCount> region{}; // source rect per eye
    std::array<EyeView, core::vr::kEyeCount> view{};         // pose + FOV each eye was rendered with
    std::int64_t rendered_for = 0; // display_time of the FrameViews the pose came from
};

class Session final {
public:
    Session(core::diag::ports::Log& log, const core::diag::ports::Clock& clock,
            core::ui::ports::Dialogs& dialogs, core::vr::RuntimeFailure& failure);
    ~Session();
    Session(const Session&)            = delete;
    Session& operator=(const Session&) = delete;

    // Locate a runtime a 32-bit process can load. The loader in a 32-bit process
    // reads the 32-bit registry view, where runtimes often aren't registered
    // (SteamVR ships steamxr_win32.json but only registers the win64 manifest).
    // If needed this finds the 32-bit sibling of the 64-bit active runtime.
    // Probing changes nothing in the process: init_system hands the choice to
    // the loader (XR_RUNTIME_JSON and the loader property) once OpenXR is the
    // runtime that starts (09 §1).
    //
    // Returns the manifest path in use, or empty if no usable runtime exists.
    std::filesystem::path probe() noexcept;

    // Create instance, system, session (bound to the game's device), reference
    // spaces, controller actions and swapchains. Logs and returns false on
    // any failure; partial state is torn down.
    bool init(core::vr::GpuDevice device) noexcept;

    // Instance, system and view configuration, without a graphics device: what
    // the stereo device needs before the game has created its D3D11 device.
    // init() runs it first if it has not run.
    bool init_system() noexcept;

    // Whether the frames locate the hands' aim poses; the action exists
    // either way, so turning tracked hands on applies at once (spec 21
    // §15.1, §16). Before init().
    void set_aim_poses(bool on) noexcept;
    // Whether init() also creates the vibration output action (spec 21
    // §14.3: Hands.Haptics.Enabled at start). Before init().
    void set_haptics(bool on) noexcept;

    const SystemInfo& system_info() const noexcept;

    void shutdown() noexcept;

    // The player turned VR off (22 §13): the session is ended in the order
    // the API requires (the begun frame ended with no layer,
    // xrRequestExitSession, then xrEndSession once the state is STOPPING,
    // waiting at most kExitSessionWait with empty frames while the frame
    // loop is expected), then the session and the instance are destroyed.
    // Render thread.
    void end_on_request() noexcept;

    // True between xrBeginSession and xrEndSession, while the session is not
    // lost. Any thread.
    bool running() const noexcept;

    // The session has a floor reference space (STAGE, else LOCAL_FLOOR) for
    // standing play (22 §6.1). Any thread.
    bool has_floor() const noexcept;

    // The runtime asked the application to quit (session state EXITING).
    // Any thread.
    bool exit_requested() const noexcept;

    // The views of the frame begun by the last frame(), located for its own
    // predictedDisplayTime. Safe from any thread (the game thread reads them in
    // head_pose while the render thread may be beginning the next frame).
    FrameViews views() const noexcept;

    // Per-eye images to present this frame. When set before frame(), the frame is
    // submitted as a stereo projection layer; otherwise the game's back buffer is
    // shown on a world-locked virtual screen.
    void set_eye_images(const EyeImages& images) noexcept;
    void clear_eye_images() noexcept;

    // The OpenXR frame boundary (09 §5.3), called once per game frame when
    // that frame's image is finished (the stereo device's EndFrame, or the
    // game's Present on the virtual-screen path). Like OpenVR's Submit +
    // WaitGetPoses:
    //   1. ends the frame the previous call began: copies the eye images (or
    //      the virtual screen) into the runtime's swapchains, flushes, and
    //      calls xrEndFrame with that frame's own predictedDisplayTime;
    //   2. polls events, then begins the next frame: xrWaitFrame (blocks to
    //      pace the game to the headset), xrBeginFrame, xrLocateViews and the
    //      controllers (buttons and grip poses), all at the new frame's
    //      predictedDisplayTime, and publishes views and hands together.
    // The game reads views() for the next frame after this returns, so each
    // frame is rendered with, and declared with, the views predicted for the
    // time it is displayed. `swapchain` is the game's, for the back buffer
    // (empty on the stereo path). Returns false when no frame was begun
    // (session not running).
    bool frame(core::stereo::SwapchainHandle swapchain) noexcept;

    // Re-anchor the seated space at the current head position and yaw.
    void recenter() noexcept;

    // What the virtual screen shows: a region of `texture` instead of the back
    // buffer (an empty handle restores the back buffer), and where: `pose` in
    // the seated space, `width_m` wide (height from the region's aspect).
    void set_screen_image(core::vr::GpuTexture texture, const TextureRegion& region) noexcept;
    void set_screen_pose(const util::Pose& pose, float width_m) noexcept;

    // The controllers, synced at the start of each frame. Render thread.
    const ControllerState& controllers() const noexcept;

    // Plays this frame's vibration on the controllers (21 §14.3), after
    // the controllers were synced. Render thread.
    void play_haptics(const core::vr::ports::HapticFrame& frame) noexcept;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace omvr::platform::openxr
