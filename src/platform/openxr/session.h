#pragma once

// The OpenXR session. Not part of the original MotherVR (which drives OpenVR
// and LibOVR); added so the mod runs on any OpenXR runtime.
//
// Lifecycle, all on the game's render thread:
//   probe()        from the runtime detector; finds a 32-bit runtime
//   init_system()  in Stereo::CreateDevice (OpenXrRuntime::init), before the
//                  game has a D3D11 device
//   init()         in ConfigureRendering, with the game's device; or on the
//                  first Present when there is no stereo device (virtual
//                  screen only, VirtualScreen)
//   frame()        every submitted frame: ends the frame the previous call
//                  began (copies, flush, xrEndFrame), then events, and
//                  begins the next (xrWaitFrame, xrBeginFrame, views, input)
//   shutdown()     when the stereo device drops its runtime (a failed
//                  session). Not at process exit: the runtime cleans up
//                  after a process that ends with a session open.
//
// R: vr (A1): the OpenXR types below stay out of this header (pimpl), and
// the members get the style's names.

#include "core/ui/ports/dialogs.h"
#include "platform/openxr/input.h"
#include "platform/openxr/swapchain.h"
#include "platform/openxr/xr_convert.h"
#include "util/sync/seqlock.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct IDXGISwapChain;

namespace omvr::platform::openxr {

struct session_info {
    bool          ok            = false;
    std::uint32_t recommended_w = 0; // per-eye image size
    std::uint32_t recommended_h = 0;
};

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
// Invalid locations follow 09 §5.5: `head` is always usable (the last valid
// orientation and position held where the runtime reports them invalid, the
// neutral pose before any), and the eyes keep their last valid positions.
struct FrameViews {
    std::array<EyeView, 2> eyes{};
    Pose                   head{};
    std::array<HandPose, 2> hands{};   // left, right; located with the views
    XrTime                 display_time = 0; // the predictedDisplayTime they were located for
    bool                   valid = false;    // eye orientations valid: a projection layer may use them
    bool                   eye_positions_valid = false; // eye positions are this frame's, not held
    bool                   head_valid = false;  // head orientation and position both this frame's
};

// The seqlock, shared with the OpenVR runtime.
using util::SeqLock;

// Per-eye images to present this frame. When set before frame(), the frame is
// submitted as a stereo projection layer; otherwise the game's back buffer is
// shown on a world-locked virtual screen.
struct EyeImages {
    std::array<ID3D11Texture2D*, 2> texture{};
    std::array<D3D11_BOX, 2>        region{}; // source rect per eye
    std::array<EyeView, 2>          view{};   // pose + FOV each eye was rendered with
    XrTime rendered_for = 0;  // display_time of the FrameViews the pose came from
};

class Session {
public:
    explicit Session(core::ui::ports::Dialogs& dialogs) noexcept : dialogs_(dialogs) {}
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Locate a runtime a 32-bit process can load. The loader in a 32-bit process
    // reads the 32-bit registry view, where runtimes often aren't registered
    // (SteamVR ships steamxr_win32.json but only registers the win64 manifest).
    // If needed this finds the 32-bit sibling of the 64-bit active runtime.
    // Probing changes nothing in the process: init_system hands the choice to
    // the loader (XR_RUNTIME_JSON and the loader property) once OpenXR is the
    // runtime that starts.
    //
    // Returns the manifest path in use, or empty if no usable runtime exists.
    std::filesystem::path probe() noexcept;

    // Create instance, system, session (bound to the game's device), reference
    // spaces, controller actions and swapchains. Logs and returns ok=false on
    // any failure; partial state is torn down.
    session_info init(ID3D11Device* device) noexcept;

    // Instance, system and view configuration, without a graphics device: what
    // the stereo device needs before the game has created its D3D11 device.
    // init() runs it first if it has not run.
    bool init_system() noexcept;

    const SystemInfo& system_info() const noexcept;

    void shutdown() noexcept;

    // True between xrBeginSession and xrEndSession.
    bool running() const noexcept;

    // The runtime asked the application to quit (session state EXITING).
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

    // The controllers, synced at the start of each frame.
    Input input;

private:
    // result_names.cpp
    void result_text(XrResult r, char (&text)[XR_MAX_RESULT_STRING_SIZE]) noexcept;
    bool check(XrResult r, const char* what);
    bool check_frame(XrResult r, const char* what, XrResult& last) noexcept;
    // swapchain.cpp
    DXGI_FORMAT pick_format(DXGI_FORMAT src);
    void destroy_chain(Swapchain& sc);
    bool ensure_chain(Swapchain& sc, DXGI_FORMAT src_format, std::uint32_t w, std::uint32_t h);
    bool copy_into(Swapchain& sc, ID3D11Texture2D* src, D3D11_BOX box);
    // session.cpp
    XrSpace make_reference_space(XrReferenceSpaceType type, const XrPosef& pose);
    bool do_recenter(XrTime t);
    void check_visible() noexcept;
    void handle_state(XrSessionState s);
    void poll_events();
    void teardown();
    // frame_loop.cpp
    void count_pose_age(XrTime rendered_for);
    void end_frame(IDXGISwapChain* swapchain);
    bool begin_frame();

    core::ui::ports::Dialogs& dialogs_;

struct State {
    XrInstance   instance = XR_NULL_HANDLE;
    XrSystemId   system   = XR_NULL_SYSTEM_ID;
    XrSession    session  = XR_NULL_HANDLE;
    XrSpace      local    = XR_NULL_HANDLE; // runtime's LOCAL origin
    XrSpace      seated   = XR_NULL_HANDLE; // LOCAL re-anchored by recenter()
    XrSpace      view     = XR_NULL_HANDLE;
    XrSessionState state  = XR_SESSION_STATE_UNKNOWN;
    bool         running  = false;
    bool         lost     = false;
    XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    std::vector<std::int64_t> formats;
    std::array<XrViewConfigurationView, 2> config_views{};
    // The frame begun by the last frame() and not yet ended.
    bool         frame_begun   = false;
    bool         should_render = false;
    XrTime       frame_time    = 0; // its predictedDisplayTime
    XrDuration   frame_period  = 0; // its predictedDisplayPeriod
    bool         recenter_pending = true; // anchor once tracking is up
    bool         exit_requested = false;

    ID3D11Device*        device  = nullptr;
    ID3D11DeviceContext* context = nullptr;

    std::array<Swapchain, 2> eye_chain{};
    Swapchain                screen_chain{};

    EyeImages    eye_images{};
    bool         eye_images_set = false;
    FrameViews   views{};    // the begun frame's views (render thread's copy)
    ID3D11Texture2D* screen_image = nullptr;
    D3D11_BOX        screen_region{};
    bool             screen_pose_set = false;
    Pose             screen_pose{};
    float            screen_width = 0.0f;
    SystemInfo       info{};
    bool         ipd_logged = false;

    // The last valid head and eye positions in the base space (09 §5.5):
    // what is published while tracking is lost. Neutral before any: the
    // identity head, the eyes half an average IPD to each side.
    Pose                 last_head{};
    std::array<Vec3, 2>  last_eye_pos{{{-0.032f, 0.0f, 0.0f}, {0.032f, 0.0f, 0.0f}}};
    float                anchor_yaw = 0.0f;   // the base space's yaw at the latest anchor

    // The per-frame calls' last results, logged on change (11 §3).
    XrResult     wait_result  = XR_SUCCESS;
    XrResult     begin_result = XR_SUCCESS;
    XrResult     end_result   = XR_SUCCESS;

    // Pose-age probe: how many display periods before the frame's own
    // display time the declared (and rendered) views were predicted for.
    // 0 is right; the old loop measured 1. Logged with Dev. Debug Options.
    std::uint32_t age_frames = 0, age_same = 0, age_one = 0, age_other = 0, age_known = 0;
    double        age_sum    = 0.0;
};
    State g;

    // g.views as published to other threads. Outside State: atomics do not
    // reset by assignment.
    SeqLock<FrameViews> g_views;

    // The manifest probe() chose; init_system hands it to the loader.
    std::filesystem::path g_manifest;

    // A running session that the runtime never makes visible is not shown in
    // the headset: with SteamVR, usually its Desktop Game Theatre showing the
    // game as a flat window. Said once, this long after the session started.
    std::chrono::steady_clock::time_point g_running_since{};
    bool g_visible_once = false;
};

// `space` in `base` at `t`, and its location flags; flags 0 when the call
// fails or the pose is not finite, so nothing NaN is ever handed on (09 §5.5).
XrSpaceLocationFlags locate_flags(XrSpace space, XrSpace base, XrTime t, Pose& out);

// Both orientation and position valid: what a re-anchor needs.
bool locate(XrSpace space, XrSpace base, XrTime t, Pose& out);

bool all_finite(const Pose& p) noexcept;

inline constexpr XrSpaceLocationFlags kOrientationAndPosition =
    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;

} // namespace omvr::platform::openxr
