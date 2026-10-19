// OpenMotherVR-Provenance: clean
#pragma once

// Session's OpenXR and D3D11 state and the functions that work on it, split
// over runtime_probe.cpp (discovery, loader), instance.cpp (instance and
// system), session.cpp (session, spaces, events), frame_loop.cpp (the frame
// boundary), swapchain.cpp (copies into runtime images) and result_names.cpp
// (result checks). Internal to this adapter: only its .cpp files include it.

#include "core/vr/pose_age.h"
#include "core/vr/tuning.h"
#include "platform/openxr/input.h"
#include "platform/openxr/session.h"
#include "platform/openxr/swapchain.h"
#include "platform/openxr/xr_convert.h"
#include "platform/openxr/xr_handle.h"
#include "platform/openxr/xr_headers.h"
#include "platform/win32/com_ptr.h"
#include "util/sync/once_latch.h"
#include "util/sync/seqlock.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace omvr::platform::openxr {

using win32::ComPtr;

struct Session::Impl {
    Impl(core::diag::ports::Log& log_port, const core::diag::ports::Clock& clock_port,
         core::ui::ports::Dialogs& dialogs_port, core::vr::RuntimeFailure& failure_port) noexcept
        : log(log_port), clock(clock_port), dialogs(dialogs_port), failure(failure_port), input(log_port) {}

    // ---- runtime_probe.cpp: discovery and the loader (09 §1)
    std::filesystem::path probe() noexcept;
    void point_loader_at(const std::filesystem::path& manifest_path) noexcept;
    void set_runtime_environment(const std::filesystem::path& manifest_path) noexcept;

    // ---- instance.cpp: instance and system (09 §2, §4.1)
    bool init_system() noexcept;
    bool create_instance(const std::vector<const char*>& extensions);
    void read_instance_properties(bool audio_ext);
    bool read_system(bool fov_ext);

    // ---- result_names.cpp: result checks (09 §7, 11 §3)
    void result_text(XrResult r, char (&text)[XR_MAX_RESULT_STRING_SIZE]) noexcept;
    bool check(XrResult r, const char* what);
    bool check_frame(XrResult r, const char* what, XrResult& last) noexcept;

    // ---- swapchain.cpp: copies into runtime images (09 §5.2)
    DXGI_FORMAT pick_format(DXGI_FORMAT src);
    void destroy_chain(Swapchain& sc);
    bool ensure_chain(Swapchain& sc, DXGI_FORMAT src_format, std::uint32_t w, std::uint32_t h);
    bool create_chain(Swapchain& sc, DXGI_FORMAT format, std::uint32_t w, std::uint32_t h);
    bool copy_into(Swapchain& sc, ID3D11Texture2D* src, D3D11_BOX box);

    // ---- session.cpp: session, spaces and events (09 §2, §3, §6)
    bool init(ID3D11Device* device) noexcept;
    void warn_adapter_mismatch(ID3D11Device* device, const XrGraphicsRequirementsD3D11KHR& reqs);
    bool create_session(ID3D11Device* device);
    bool create_spaces();
    void create_floor_space();
    XrSpace make_reference_space(XrReferenceSpaceType type, const XrPosef& pose);
    bool do_recenter(XrTime t);
    void check_visible() noexcept;
    void handle_state(XrSessionState s);
    void poll_events();
    void publish_flags() noexcept;
    void teardown();
    void shutdown() noexcept;
    void end_on_request() noexcept;
    bool wait_for_stopping();

    // ---- frame_loop.cpp: the frame boundary (09 §5)
    struct FrameLayers;
    bool frame(IDXGISwapChain* swapchain) noexcept;
    void end_frame(IDXGISwapChain* swapchain);
    const XrCompositionLayerBaseHeader* projection_layer(FrameLayers& layers);
    const XrCompositionLayerBaseHeader* screen_layer(FrameLayers& layers, IDXGISwapChain* swapchain);
    bool begin_frame();
    void read_hands(XrTime t);
    void locate_views(XrTime t);
    void locate_head(XrTime t);
    void locate_floor(XrTime t);
    void log_ipd_once();

    core::diag::ports::Log& log;
    const core::diag::ports::Clock& clock;
    core::ui::ports::Dialogs& dialogs;
    core::vr::RuntimeFailure& failure;

    // Everything a teardown resets. The handles are destroyed explicitly in
    // teardown(), in the order 09 §3 needs, before the reset.
    struct State {
        InstanceOwner instance;
        XrSystemId system = XR_NULL_SYSTEM_ID;
        SessionOwner session;
        SpaceOwner local;  // runtime's LOCAL origin
        SpaceOwner seated; // LOCAL re-anchored by recenter()
        SpaceOwner view;
        SpaceOwner floor; // STAGE, else LOCAL_FLOOR, when the runtime has one (22 §6.1)
        XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;
        bool running                 = false;
        bool lost                    = false;
        XrEnvironmentBlendMode blend = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        std::vector<std::int64_t> formats;
        std::array<XrViewConfigurationView, core::vr::kEyeCount> config_views{};
        // The frame begun by the last frame() and not yet ended.
        bool frame_begun        = false;
        bool should_render      = false;
        XrTime frame_time       = 0;    // its predictedDisplayTime
        XrDuration frame_period = 0;    // its predictedDisplayPeriod
        bool recenter_pending   = true; // anchor once tracking is up
        bool exit_requested     = false;

        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;

        std::array<Swapchain, core::vr::kEyeCount> eye_chain{};
        Swapchain screen_chain{};

        EyeImages eye_images{};
        bool eye_images_set = false;
        FrameViews views{}; // the begun frame's views (render thread's copy)
        // The stereo texture the screen shows, set with each submitted frame;
        // not owned (the game's).
        ID3D11Texture2D* screen_image = nullptr;
        TextureRegion screen_region{};
        bool screen_pose_set = false;
        util::Pose screen_pose{};
        float screen_width = 0.0f;
        SystemInfo info{};
        bool ipd_logged = false;

        // The last valid head and eye positions in the base space (09 §5.5):
        // what is published while tracking is lost. Neutral before any: the
        // identity head, the eyes half an average IPD to each side.
        util::Pose last_head{};
        std::array<util::Vec3, core::vr::kEyeCount> last_eye_pos{
            {{-core::vr::kDefaultEyeX, 0.0f, 0.0f}, {core::vr::kDefaultEyeX, 0.0f, 0.0f}}};
        float anchor_yaw = 0.0f; // the base space's yaw at the latest anchor

        // The per-frame calls' last results, logged on change (11 §3).
        XrResult wait_result  = XR_SUCCESS;
        XrResult begin_result = XR_SUCCESS;
        XrResult end_result   = XR_SUCCESS;

        // Pose-age probe, logged with Dev. Debug Options (11 §4.1).
        core::vr::PoseAge pose_age;
    };
    State state;

    // state.views as published to other threads. Outside State: atomics do
    // not reset by assignment.
    util::SeqLock<FrameViews> published_views;
    // state.floor exists; read on the game thread (22 §6.1).
    std::atomic<bool> floor_available{false};
    // state.running && !state.lost, and state.exit_requested, as
    // publish_flags last saw them: headset_in_use reads them on the game
    // thread while the render thread changes them (12 §3).
    std::atomic<bool> running_published{false};
    std::atomic<bool> exit_published{false};

    // The manifest probe() chose; init_system hands it to the loader.
    std::filesystem::path manifest;

    // A running session that the runtime never makes visible is not shown in
    // the headset: with SteamVR, usually its Desktop Game Theatre showing the
    // game as a flat window. Said once, this long after the session started
    // (09 §3.1).
    std::chrono::steady_clock::time_point running_since{};
    bool visible_reported = false;
    // The elevated-process note is logged once per process (09 §1).
    util::OnceLatch elevation_logged;

    // The controllers, synced at the start of each frame.
    Input input;
};

// `space` in `base` at `t`, and its location flags; flags 0 when the call
// fails or the pose is not finite, so nothing NaN is ever handed on (09 §5.5).
XrSpaceLocationFlags locate_flags(XrSpace space, XrSpace base, XrTime t, Pose& out);

// Both orientation and position valid: what a re-anchor needs.
bool locate(XrSpace space, XrSpace base, XrTime t, Pose& out);

bool all_finite(const Pose& p) noexcept;

} // namespace omvr::platform::openxr
