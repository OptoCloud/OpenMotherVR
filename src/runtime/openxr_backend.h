#pragma once

// OpenXR backend. Not part of the original MotherVR (which drives OpenVR and
// LibOVR); added so the mod runs on any OpenXR runtime.
//
// Lifecycle, all on the game's render thread:
//   probe()        from vr_detect at DLL attach; finds a 32-bit runtime
//   init_system()  in Stereo::CreateDevice (OpenXrHmd::init), before the
//                  game has a D3D11 device
//   init()         in ConfigureRendering, with the game's device; or on the
//                  first Present when there is no stereo device (virtual
//                  screen only, stereo/present.cpp)
//   frame()        every submitted frame: ends the frame the previous call
//                  began (copies, flush, xrEndFrame), then events, and
//                  begins the next (xrWaitFrame, xrBeginFrame, views, input)
//   shutdown()     when the stereo device drops its backend (a failed
//                  session). Not at process exit: the runtime cleans up
//                  after a process that ends with a session open.

#include "xr_platform.h"

#include "seqlock.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

struct IDXGISwapChain;

namespace mothervr::runtime::openxr {

// Locate a runtime a 32-bit process can load. The loader in a 32-bit process
// reads the 32-bit registry view, where runtimes often aren't registered
// (SteamVR ships steamxr_win32.json but only registers the win64 manifest).
// If needed this finds the 32-bit sibling of the 64-bit active runtime and
// exports it via XR_RUNTIME_JSON for the loader.
//
// Returns the manifest path in use, or empty if no usable runtime exists.
std::filesystem::path probe() noexcept;

struct session_info {
    bool          ok            = false;
    std::uint32_t recommended_w = 0; // per-eye image size
    std::uint32_t recommended_h = 0;
};

// Create instance, system, session (bound to the game's device), reference
// spaces, controller actions and swapchains. Logs and returns ok=false on
// any failure; partial state is torn down.
session_info init(ID3D11Device* device) noexcept;

// Instance, system and view configuration, without a graphics device: what
// the stereo device needs before the game has created its D3D11 device.
// init() runs it first if it has not run.
bool init_system() noexcept;

struct SystemInfo {
    std::uint32_t recommended_w = 0, recommended_h = 0;  // per eye
    bool has_fov = false;            // the runtime reported its FOV
    std::array<XrFovf, 2> fov{};     // per eye, radians
    char system_name[XR_MAX_SYSTEM_NAME_SIZE]{};
    char runtime_name[XR_MAX_RUNTIME_NAME_SIZE]{};
    // The headset's audio output endpoint (XR_OCULUS_audio_device_guid); empty
    // when the runtime does not say.
    std::wstring audio_output;
};
const SystemInfo& system_info() noexcept;

void shutdown() noexcept;

// True between xrBeginSession and xrEndSession.
bool running() noexcept;

// The runtime asked the application to quit (session state EXITING).
bool exit_requested() noexcept;

// One eye's render parameters for the frame being submitted.
struct EyeView {
    Pose  pose{};             // in the seated (LOCAL) space
    float angle_left  = 0.0f; // FOV half-angles in radians (left/down negative)
    float angle_right = 0.0f;
    float angle_up    = 0.0f;
    float angle_down  = 0.0f;
};

// A controller's grip pose (/input/grip/pose: origin in the palm, -Z along
// the handle) and linear velocity, both in the seated space.
struct HandPose {
    Pose pose{};
    Vec3 velocity{};          // metres per second
    bool valid          = false;
    bool velocity_valid = false;
};

// Per-frame view state, valid after a frame() that returned true.
struct FrameViews {
    std::array<EyeView, 2> eyes{};
    Pose                   head{};
    std::array<HandPose, 2> hands{};   // left, right; located with the views
    XrTime                 display_time = 0; // the predictedDisplayTime they were located for
    bool                   valid = false;
};

// The seqlock, shared with the OpenVR backend.
using runtime::SeqLock;

// The views of the frame begun by the last frame(), located for its own
// predictedDisplayTime. Safe from any thread (the game thread reads them in
// head_pose while the render thread may be beginning the next frame).
FrameViews views() noexcept;

// Per-eye images to present this frame. When set before frame(), the frame is
// submitted as a stereo projection layer; otherwise the game's back buffer is
// shown on a world-locked virtual screen.
struct EyeImages {
    std::array<ID3D11Texture2D*, 2> texture{};
    std::array<D3D11_BOX, 2>        region{}; // source rect per eye
    std::array<EyeView, 2>          view{};   // pose + FOV each eye was rendered with
    XrTime rendered_for = 0;  // display_time of the FrameViews the pose came from
};
void set_eye_images(const EyeImages& images) noexcept;
void clear_eye_images() noexcept;

// The OpenXR frame boundary, called once per game frame when that frame's
// image is finished (the stereo device's EndFrame, or the game's Present on
// the virtual-screen path). Like OpenVR's Submit + WaitGetPoses:
//   1. ends the frame the previous call began: copies the eye images (or the
//      virtual screen) into the runtime's swapchains, flushes, and calls
//      xrEndFrame with that frame's own predictedDisplayTime;
//   2. polls events, then begins the next frame: xrWaitFrame (blocks to pace
//      the game to the headset), xrBeginFrame, xrLocateViews and the
//      controllers (buttons and grip poses), all at the new frame's
//      predictedDisplayTime, and publishes views and hands together.
// The game reads views() for the next frame after this returns, so each
// frame is rendered with, and declared with, the views predicted for the
// time it is displayed. Returns false when no frame was begun (session not
// running).
bool frame(IDXGISwapChain* swapchain) noexcept;

// Re-anchor the seated space at the current head position and yaw.
void recenter() noexcept;

// What the virtual screen shows: a region of `texture` instead of the back
// buffer (null restores the back buffer), and where: `pose` in the seated
// space, `width_m` wide (height from the region's aspect).
void set_screen_image(ID3D11Texture2D* texture, const D3D11_BOX& region) noexcept;
void set_screen_pose(const Pose& pose, float width_m) noexcept;

} // namespace mothervr::runtime::openxr
