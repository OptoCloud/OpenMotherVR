// MotherVR::OpenVR, the OpenVR HMD backend.
//
// From: the OpenVR vtable 0x1005ddac: FUN_100068b0 (Init), 10006b40 (desc),
// 10006f20 (texture size), 10006fe0 / 10007060 (overlay), 100070e0 (head
// pose), 10007220 (yaw pose), 100085c0 (eye offsets), 10008710 (submit),
// 10007500 (control hints), 10007ad0 (input). Analysis:
// dumps/06b_hmd_backend.md, input mapping dumps/06d_vr_input.md.
//
// The original talks to openvr 1.0.10 interfaces (IVRSystem_017,
// IVRCompositor_021, IVROverlay_016). We request the versions of the SDK
// header we build against; SteamVR serves both, and the calls used here
// exist in both (03 §6.4).

#include "platform/openvr/openvr_runtime.h"

#include "core/diag/tuning.h"
#include "core/hands/tuning.h"
#include "core/input/controller_mapping.h"
#include "core/ui/notices.h"
#include "core/vr/fov.h"
#include "core/vr/hand_offsets.h"
#include "core/vr/heading.h"
#include "core/vr/pose_convert.h"
#include "core/vr/pose_hold.h"
#include "core/vr/runtime_failure.h"
#include "core/vr/space_change.h"
#include "core/vr/tuning.h"
#include "platform/d3d11/handles.h"
#include "platform/openvr/api_loader.h"
#include "platform/openvr/compat_names.h"
#include "platform/openvr/constants.h"
#include "platform/openvr/input_actions.h"
#include "platform/win32/module.h"
#include "platform/win32/process_info.h"
#include "util/math/angle.h"
#include "util/math/vec.h"
#include "util/sync/change_latch.h"
#include "util/sync/frame_period.h"
#include "util/sync/seqlock.h"

#include <openvr.h>
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace omvr::platform::openvr {

namespace {

namespace input = core::input;
using core::diag::ports::logf;
using core::vr::FovTangents;
using core::vr::HeadsetDesc;
using core::vr::HeadTracking;
using core::vr::kCinemaDistance;
using core::vr::kCinemaWidth;
using core::vr::kHandCount;
using core::vr::RawProjection;
using core::vr::RenderTangents;
using core::vr::TextureSize;
using core::vr::to_game_pose;
using core::vr::to_game_velocity;
using util::Matrix34;
using util::SeqLock;
using Clock = std::chrono::steady_clock;

constexpr std::string_view kLogMissingInterface = "openvr: runtime lacks interface {} (error {})";
constexpr std::string_view kLogInterfaces       = "openvr: interfaces {}, {}, {}";
// Leads this runtime's "frames now" line.
constexpr std::string_view kLogSource          = "openvr";
constexpr std::string_view kLogCompositorReady = "openvr: compositor ready, headset pose {}";
constexpr std::string_view kLogHeadset         = "openvr: {} {}, render tangents v={:.3f} h={:.3f}";
constexpr std::string_view kLogOverlayFailed   = "openvr: CreateOverlay failed ({})";
constexpr std::string_view kLogHeadTracking    = "[diag] headset pose {} (tracking result {})";
constexpr std::string_view kLogRecenterSkipped =
    "openvr: recenter skipped: headset pose invalid, the previous reference stays";
constexpr std::string_view kLogRecenter =
    "openvr: recenter: head x=({:.2f} {:.2f} {:.2f}) y=({:.2f} {:.2f} {:.2f}) z=({:.2f} {:.2f} {:.2f}) "
    "pos=({:.2f} {:.2f} {:.2f}) yaw={:.1f} deg{}, activity={}";
constexpr std::string_view kLogSpaceChanged =
    "openvr: {}: the tracking space moved; the virtual screen is hidden and the reference retaken once the "
    "headset is in use";
constexpr std::string_view kLogHeadsetActivity = "openvr: headset {} (event {})";
constexpr std::string_view kLogNoApplications =
    "openvr: no {}: cannot check which application the headset shows; VR is otherwise unaffected";
constexpr std::string_view kLogShowsGame   = "openvr: the headset shows the game";
constexpr std::string_view kLogShowsOther  = "openvr: the headset shows process {} ({}), not the game";
constexpr std::string_view kLogController  = "openvr: {} hand controller: {} (device {})";
constexpr std::string_view kLogHandsPrefix = "openvr: hands (head-local m, +x right +y up -z fwd):";
constexpr std::string_view kLogStopped =
    "openvr: stopped at the player's request; the headset shows SteamVR, the runtime stays loaded";

// The player's start-up failure text for a missing interface (13 §5.3).
constexpr std::string_view kFailMissingInterface = "SteamVR does not provide {}";
// The log label of the headset-not-showing-the-game warning (03 §6.7).
constexpr std::string_view kNotShowingLabel = "headset not showing the game";

// SteamVR's controller type names that pick a control scheme.
constexpr std::string_view kViveController  = "vive_controller";
constexpr std::string_view kIndexController = "knuckles";
constexpr std::string_view kFrameController = "frame_controller";

// The neutral pose: the reference before any recenter (03 §7) and the head
// before any valid pose (03 §1).
constexpr vr::HmdMatrix34_t kIdentity34{
    {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}};

bool all_finite(const vr::HmdMatrix34_t& m) noexcept {
    for (const auto& row : m.m)
        for (float v : row)
            if (!std::isfinite(v)) return false;
    return true;
}

bool valid_hand(int hand) noexcept { return hand >= 0 && static_cast<std::size_t>(hand) < kHandCount; }

vr::VRTextureBounds_t to_vr(const core::vr::TextureBounds& b) noexcept {
    return {b.u_min, b.v_min, b.u_max, b.v_max};
}

// The level reference of a recenter: rotation by `yaw_rad` about +Y, at the
// head's position `m`.
vr::HmdMatrix34_t yaw_only(float yaw_rad, const float (&m)[3][4]) noexcept {
    const float s = std::sin(yaw_rad), c = std::cos(yaw_rad);
    return {{
        {c, 0.0f, s, m[0][3]},
        {0.0f, 1.0f, 0.0f, m[1][3]},
        {-s, 0.0f, c, m[2][3]},
    }};
}

const char* tracking_text(HeadTracking t) noexcept {
    switch (t) {
        case HeadTracking::Invalid: return "invalid, holding the last valid pose";
        case HeadTracking::Degraded: return "degraded, holding the last valid position";
        case HeadTracking::Tracked: return "tracked";
    }
    return "tracked";
}

input::ControlScheme scheme_for(std::string_view controller_type) noexcept {
    if (controller_type == kViveController) return input::ControlScheme::Touchpad;
    if (controller_type == kIndexController) return input::ControlScheme::Index;
    if (controller_type == kFrameController) return input::ControlScheme::Frame;
    return input::ControlScheme::Thumbstick;
}

void sleep_for(std::chrono::milliseconds d) noexcept { ::Sleep(static_cast<DWORD>(d.count())); }

// One hand of the legacy controller state as the touchpad scheme reads it
// (04 §5.8.3): axis 0 is the touchpad, axis 1 the trigger.
input::HandInput legacy_hand(const vr::VRControllerState_t& s) noexcept {
    return {.stick         = {s.rAxis[0].x, s.rAxis[0].y},
            .stick_touched = (s.ulButtonTouched & kButtonTouchpad) != 0,
            .stick_click   = (s.ulButtonPressed & kButtonTouchpad) != 0,
            .trigger       = s.rAxis[1].x,
            .grip          = (s.ulButtonPressed & kButtonGrip) != 0,
            .menu          = (s.ulButtonPressed & kButtonMenu) != 0};
}

// The translation column of a 3x4 tracking matrix.
util::Vec3 translation(const float (&m)[3][4]) noexcept { return {m[0][3], m[1][3], m[2][3]}; }

// 22 §8.1: the head and hands of one controls write, in tracking space
// (metres before the world scale), for the gestures.
core::input::BodyPoses body_poses(const vr::TrackedDevicePose_t (&poses)[vr::k_unMaxTrackedDeviceCount],
                                  const vr::TrackedDeviceIndex_t (&index)[kHandCount],
                                  bool focused) noexcept {
    core::input::BodyPoses b;
    b.focused       = focused;
    const auto& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (hmd.bPoseIsValid) {
        const auto& m = hmd.mDeviceToAbsoluteTracking.m;
        b.head        = translation(m);
        b.head_yaw    = core::input::head_yaw({-m[0][2], -m[1][2], -m[2][2]}, {m[0][1], m[1][1], m[2][1]});
        b.head_valid  = true;
    }
    for (std::size_t h = 0; h < kHandCount; ++h) {
        if (index[h] >= vr::k_unMaxTrackedDeviceCount || !poses[index[h]].bPoseIsValid) continue;
        b.hands[h]      = translation(poses[index[h]].mDeviceToAbsoluteTracking.m);
        b.hand_valid[h] = true;
    }
    return b;
}

// The events after which the poses are in a moved tracking space (03 §6.8),
// by name for the log; null for any other event. A re-anchor of one
// universe may move the other on some runtimes, so each counts whatever
// universe the poses are in: a needless retake only faces forward again.
const char* space_change_name(std::uint32_t event) noexcept {
    switch (event) {
        case vr::VREvent_ChaperoneUniverseHasChanged: return "chaperone universe changed";
        case vr::VREvent_SeatedZeroPoseReset: return "seated zero pose reset";
        case vr::VREvent_StandingZeroPoseReset: return "standing zero pose reset";
        default: return nullptr;
    }
}

class OpenVrRuntime final : public core::vr::ports::VrRuntime {
public:
    explicit OpenVrRuntime(const OpenVrRuntimeDeps& deps) noexcept
        : log_(deps.log),
          clock_(deps.clock),
          failure_(deps.failure),
          payloads_(deps.payloads),
          controls_(deps.controls),
          dialogs_(deps.dialogs),
          runtime_root_(deps.runtime_root),
          api_(deps.log, deps.failure),
          actions_(deps.log, deps.aim_poses, deps.haptics != nullptr),
          haptics_(deps.haptics) {
        render_pose_.store(kIdentity34);
        yaw_matrix_.store(kIdentity34);
    }

    // Closes the session init() opened. Only reached through the stereo
    // device's explicit teardown, never at process exit.
    ~OpenVrRuntime() override { api_.shutdown(); }

    bool init() override;
    void headset_desc(HeadsetDesc& desc) override;
    TextureSize eye_texture_size(int eye, float pixel_scale) override;
    bool create_render_resources(core::vr::GpuDevice, core::vr::GpuContext) override;
    void release_render_resources() override;
    void head_pose(Matrix34& out, float world_scale) override;
    void head_yaw_pose(Matrix34& out, float world_scale) override;
    void begin_frame(float eye_offsets[6], bool head_tracked) override;
    void submit_frame(core::vr::GpuTexture side_by_side) override;
    void register_control_hints() override;
    void poll_input() override;
    bool headset_in_use() override;
    bool recenter_taken() const override { return recenter_taken_; }
    bool tracking_space_changed() override { return space_change_.take(); }
    bool has_floor() const override { return tracking_origin_ == vr::TrackingUniverseStanding; }
    std::optional<float> floor_height() override;
    std::optional<float> head_height_above_floor() override;
    bool hand_pose(int hand, Matrix34& out, float world_scale) override;
    bool hand_velocity(int hand, float (&out)[3], float world_scale) override;
    bool aim_pose(int hand, Matrix34& out, float world_scale) override;
    void stop() override;

private:
    using Hands = std::array<HandPose, kHandCount>;

    bool acquire_interfaces(Clock::time_point deadline);
    bool missing(const char* version, std::int32_t error);
    bool wait_for_pose(Clock::time_point deadline);
    const Hands& hands_now();
    void read_hands();
    void log_hands(const Hands& h);
    void submit_eyes(const vr::VRTextureWithPose_t& tex);
    void show_screen(void* side_by_side, const vr::VRTextureWithPose_t& tex);
    void place_screen(void* side_by_side);
    void hide_screen();
    void poll_events();
    void write_controls();
    void play_haptics(const vr::TrackedDevicePose_t (&poses)[vr::k_unMaxTrackedDeviceCount],
                      const vr::TrackedDeviceIndex_t (&index)[kHandCount]);
    void note_controller(std::size_t hand, vr::TrackedDeviceIndex_t index);
    void check_scene_app();
    Controls legacy_controls(const vr::TrackedDeviceIndex_t (&index)[kHandCount]);
    const vr::HmdMatrix34_t& read_head();
    void log_recenter(const float (&m)[3][4], core::vr::Heading heading);

    core::diag::ports::Log& log_;
    const core::diag::ports::Clock& clock_;
    core::vr::RuntimeFailure& failure_;
    win32::ArchiveFiles& payloads_;
    core::input::ports::ControlsSink& controls_;
    core::ui::ports::Dialogs& dialogs_;
    const std::filesystem::path runtime_root_;
    ApiLoader api_;

    vr::IVRSystem* system_         = nullptr;
    vr::IVRCompositor* compositor_ = nullptr;
    vr::IVROverlay* overlay_       = nullptr;

    // Game thread (head_pose, head_yaw_pose, headset_in_use; init before).
    vr::TrackedDevicePose_t poses_[vr::k_unMaxTrackedDeviceCount]{};
    // The last valid headset pose (OpenVR space, unscaled): what the game
    // gets while the pose is invalid, with the new orientation but this
    // position while position tracking is degraded (03 §1).
    vr::HmdMatrix34_t head_ = kIdentity34;
    bool head_valid_        = false;             // the latest read was valid
    util::ChangeLatch<HeadTracking> head_state_; // debug log once per change
    float yaw_           = 0.0f;                 // the latest reference's heading
    bool recenter_taken_ = true;                 // the last head_yaw_pose took a new one
    // Written on the game thread, read on the render thread (03 §11): the
    // pose head_pose returned, which submit_frame declares, and the
    // unconverted yaw-only reference Y, which places the virtual screen.
    SeqLock<vr::HmdMatrix34_t> render_pose_;
    SeqLock<vr::HmdMatrix34_t> yaw_matrix_;
    // yaw_matrix_ holds a reference taken in the current tracking space: set
    // by a valid recenter (game thread), cleared by a tracking space change
    // (render thread). Until then no screen is shown (03 §7).
    std::atomic<bool> screen_reference_{false};
    // Render thread: tracking space changes, handed to the game thread
    // (tracking_space_changed) after the next pose wait.
    core::vr::SpaceChange space_change_;
    core::vr::FrameKindLog frame_kind_log_; // render thread
    bool cinema_mode_                     = false;
    vr::VROverlayHandle_t overlay_handle_ = vr::k_ulOverlayHandleInvalid;

    vr::VRTextureBounds_t bounds_[core::vr::kEyeCount]{}; // crop of each eye's half of the texture
    char manufacturer_[kPropertyBufferSize]{};
    char model_[kPropertyBufferSize]{};

    vr::TrackedDeviceIndex_t logged_controller_[kHandCount] = {vr::k_unTrackedDeviceIndexInvalid,
                                                               vr::k_unTrackedDeviceIndexInvalid};

    Actions actions_;
    // The haptic output (21 §14); null: off.
    core::vr::ports::HapticSource* const haptics_;
    bool glyphs_chosen_ = false;
    // Render thread: submit_frame handed the game this frame's controls, so
    // the next poll_input has nothing to do.
    bool controls_written_ = false;

    // The hands' grip poses, read on the render thread right after each
    // WaitGetPoses (the poses head_pose's GetLastPoses returns) and handed
    // to the game thread through the seqlock.
    SeqLock<Hands> hands_;
    int tracking_origin_ = vr::TrackingUniverseStanding;            // WaitGetPoses' universe
    util::FramePeriod hand_log_{core::diag::kDebugLogPeriodFrames}; // render thread
    std::uint32_t submitted_frames_ = 0;                            // render thread
    // Game thread: the hands as of this frame's head_pose, and whether
    // head_pose ran since the last submit.
    Hands hands_rendered_{};
    std::atomic<bool> head_read_{false};
    Hands hands_scratch_{};
};

// ---- slot 0 ------------------------------------------------------------------

bool OpenVrRuntime::init() {
    if (!api_.init(runtime_root_, payloads_)) return false;

    // The original spins until the compositor is up. One bound covers the
    // compositor and the first valid pose together (03 §6.4).
    const auto deadline = clock_.now() + core::vr::kStartupWait;
    if (!acquire_interfaces(deadline)) return false;

    // None in time is not a failure: slot 11 then returns the neutral pose
    // and the first reference is queued until the headset is in use.
    const bool pose_valid = wait_for_pose(deadline);
    tracking_origin_      = compositor_->GetTrackingSpace();
    logf(log_, kLogCompositorReady, pose_valid ? "valid" : "not yet valid");
    actions_.init(api_, win32::module_directory() / kInputDirectory);
    return true;
}

// A missing interface is a start-up failure that names the interface and
// carries the runtime's error code (03 §6.4, 13 §5.3).
bool OpenVrRuntime::missing(const char* version, std::int32_t error) {
    logf(log_, kLogMissingInterface, version, error);
    failure_.report(std::format(kFailMissingInterface, version), error);
    api_.shutdown();
    return false;
}

// The three required interfaces; the compositor is asked for again until
// it is served or the start-up wait is over.
bool OpenVrRuntime::acquire_interfaces(Clock::time_point deadline) {
    const auto system  = api_.generic_interface(vr::IVRSystem_Version);
    const auto overlay = api_.generic_interface(vr::IVROverlay_Version);
    auto compositor    = api_.generic_interface(vr::IVRCompositor_Version);
    while (!compositor && clock_.now() < deadline) {
        sleep_for(core::vr::kCompositorPollInterval);
        compositor = api_.generic_interface(vr::IVRCompositor_Version);
    }
    if (!system) return missing(vr::IVRSystem_Version, system.error().code);
    if (!compositor) return missing(vr::IVRCompositor_Version, compositor.error().code);
    if (!overlay) return missing(vr::IVROverlay_Version, overlay.error().code);
    system_     = static_cast<vr::IVRSystem*>(*system);
    compositor_ = static_cast<vr::IVRCompositor*>(*compositor);
    overlay_    = static_cast<vr::IVROverlay*>(*overlay);
    logf(log_, kLogInterfaces, vr::IVRSystem_Version, vr::IVRCompositor_Version, vr::IVROverlay_Version);
    return true;
}

// Within the same bound, a valid headset pose (03 §6.4).
bool OpenVrRuntime::wait_for_pose(Clock::time_point deadline) {
    bool pose_valid = false;
    while (!pose_valid && clock_.now() < deadline) {
        const auto err = compositor_->WaitGetPoses(poses_, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
        pose_valid     = poses_[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid;
        // A failing call may return at once; do not spin on it.
        if (!pose_valid && err != vr::VRCompositorError_None) sleep_for(core::vr::kPoseRetryInterval);
    }
    return pose_valid;
}

// ---- slot 1 ------------------------------------------------------------------

void OpenVrRuntime::headset_desc(HeadsetDesc& desc) {
    RawProjection raw[core::vr::kEyeCount]{};
    for (int eye = 0; eye < static_cast<int>(core::vr::kEyeCount); ++eye) {
        system_->GetProjectionRaw(static_cast<vr::EVREye>(eye), &raw[eye].left, &raw[eye].right,
                                  &raw[eye].top, &raw[eye].bottom);
        // The original stores -top as Up, which the game never sees (it
        // reads only the symmetric EyeFov) but which crops the mirror's eye
        // modes on the wrong rows on a vertically asymmetric headset.
        desc.default_eye_fov[eye] = core::vr::tangents_from_raw(raw[eye]);
    }
    const RenderTangents t = core::vr::render_tangents(desc.default_eye_fov[0], desc.default_eye_fov[1]);
    desc.max_eye_fov[0] = desc.max_eye_fov[1] = core::vr::symmetric_tangents(t);

    // Each eye renders the symmetric frustum into its half of the texture;
    // crop it back to the eye's real FOV.
    for (int eye = 0; eye < static_cast<int>(core::vr::kEyeCount); ++eye) {
        bounds_[eye] = to_vr(core::vr::crop_bounds(eye, raw[eye], t));
    }

    system_->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String,
                                            manufacturer_, sizeof(manufacturer_));
    system_->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String,
                                            model_, sizeof(model_));
    desc.manufacturer = manufacturer_;
    desc.product_name = model_;
    logf(log_, kLogHeadset, manufacturer_, model_, t.vertical, t.horizontal);
}

// ---- slots 2-4 ---------------------------------------------------------------

TextureSize OpenVrRuntime::eye_texture_size(int /*eye*/, float pixel_scale) {
    std::uint32_t w = 0, h = 0;
    system_->GetRecommendedRenderTargetSize(&w, &h);
    return core::vr::scaled_size(w, h, pixel_scale);
}

// Never fails: without the overlay only the virtual screen is missing
// (03 §7).
bool OpenVrRuntime::create_render_resources(core::vr::GpuDevice, core::vr::GpuContext) {
    // The game may reconfigure rendering more than once; one overlay will do.
    if (overlay_handle_ != vr::k_ulOverlayHandleInvalid) return true;
    const auto err = overlay_->CreateOverlay(kOverlayKey, kOverlayKey, &overlay_handle_);
    if (err != vr::VROverlayError_None) {
        logf(log_, kLogOverlayFailed, static_cast<int>(err));
        overlay_handle_ = vr::k_ulOverlayHandleInvalid;
    }
    return true;
}

void OpenVrRuntime::release_render_resources() {
    if (overlay_handle_ == vr::k_ulOverlayHandleInvalid) return;
    overlay_->DestroyOverlay(overlay_handle_);
    overlay_handle_ = vr::k_ulOverlayHandleInvalid;
}

// 22 §13. The headset stops showing the game: the virtual screen goes and
// the compositor drops the last frame (it shows its own scene until a new
// submit, which never comes). VR_Shutdown is not called: the game thread
// may still be inside a pose read that began before the detach, so the
// runtime stays loaded until exit, unused (02 §7.4).
void OpenVrRuntime::stop() {
    controls_.write(Controls{}, false);
    release_render_resources();
    compositor_->ClearLastSubmittedFrame();
    log_.line(kLogStopped);
}

// ---- slots 5-6 ---------------------------------------------------------------

// Game thread. The headset's latest pose with the invalid-pose rules of
// 03 §1: an invalid (or non-finite) pose leaves the last valid one; a valid
// pose with degraded tracking takes the new orientation and keeps the last
// valid position. Before any valid pose that is the neutral pose.
const vr::HmdMatrix34_t& OpenVrRuntime::read_head() {
    compositor_->GetLastPoses(poses_, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
    const auto& p          = poses_[vr::k_unTrackedDeviceIndex_Hmd];
    head_valid_            = p.bPoseIsValid && all_finite(p.mDeviceToAbsoluteTracking);
    const bool position_ok = p.eTrackingResult == vr::TrackingResult_Running_OK;
    core::vr::hold_pose(head_.m, p.mDeviceToAbsoluteTracking.m, head_valid_, position_ok);
    // Dev. Debug Options: tracking lost or degraded, once per change.
    const HeadTracking state = core::vr::head_tracking(head_valid_, position_ok);
    if (head_state_.changed(state) && log_.debug_enabled()) {
        logf(log_, kLogHeadTracking, tracking_text(state), static_cast<int>(p.eTrackingResult));
    }
    return head_;
}

void OpenVrRuntime::head_pose(Matrix34& out, float world_scale) {
    const vr::HmdMatrix34_t& m = read_head();
    render_pose_.store(m); // the pose this frame renders with and is declared with
    out             = to_game_pose(m.m, world_scale);
    hands_rendered_ = hands_.load();
    head_read_.store(true, std::memory_order_relaxed);
}

void OpenVrRuntime::head_yaw_pose(Matrix34& out, float world_scale) {
    const auto& m = read_head().m;
    // Never from an invalid pose (03 §2.1): the previous reference stays. The
    // stereo device's queued retake waits for headset_in_use, which needs a
    // valid pose, so a retake is taken once tracking is back.
    recenter_taken_ = head_valid_;
    if (!head_valid_) {
        out = to_game_pose(yaw_matrix_.load().m, world_scale);
        log_.line(kLogRecenterSkipped);
        return;
    }
    // Yaw about +Y: the original goes through a quaternion,
    // atan2(2(wy + xz), w^2 - x^2 - y^2 + z^2), which is this. Looking
    // straight up or down leaves no heading: keep the previous one.
    const core::vr::Heading heading = core::vr::heading_from_forward(m[0][2], m[2][2], yaw_);
    yaw_                            = heading.yaw_rad;
    const vr::HmdMatrix34_t y       = yaw_only(heading.yaw_rad, m);
    yaw_matrix_.store(y);
    screen_reference_.store(true, std::memory_order_release);
    out = to_game_pose(y.m, world_scale);
    log_recenter(m, heading);
}

void OpenVrRuntime::log_recenter(const float (&m)[3][4], core::vr::Heading heading) {
    logf(log_, kLogRecenter, m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2], m[2][2],
         m[0][3], m[1][3], m[2][3], heading.yaw_rad * util::kRadToDeg,
         heading.vertical ? " (kept: looking straight up or down)" : "",
         static_cast<int>(system_->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd)));
}

// ---- hands (extension) ---------------------------------------------------------

// Game thread. The hands of the frame head_pose was read for, else (menus,
// or a call before head_pose this frame) the newest.
const OpenVrRuntime::Hands& OpenVrRuntime::hands_now() {
    if (head_read_.load(std::memory_order_relaxed)) return hands_rendered_;
    hands_scratch_ = hands_.load();
    return hands_scratch_;
}

bool OpenVrRuntime::hand_pose(int hand, Matrix34& out, float world_scale) {
    if (!valid_hand(hand)) return false;
    const auto& h = hands_now()[static_cast<std::size_t>(hand)];
    if (!h.valid) return false;
    out = to_game_pose(h.m, world_scale);
    return true;
}

bool OpenVrRuntime::hand_velocity(int hand, float (&out)[3], float world_scale) {
    if (!valid_hand(hand)) return false;
    const auto& h = hands_now()[static_cast<std::size_t>(hand)];
    if (!h.valid) return false;
    to_game_velocity(h.velocity, world_scale, out);
    return true;
}

bool OpenVrRuntime::aim_pose(int hand, Matrix34& out, float world_scale) {
    if (!valid_hand(hand)) return false;
    const auto& h = hands_now()[static_cast<std::size_t>(hand)];
    if (!h.aim_valid) return false;
    out = to_game_pose(h.aim, world_scale);
    return true;
}

// Render thread, right after WaitGetPoses: GetPoseActionDataForNextFrame
// gives the hands with that call's prediction, the display time of the
// frame the game renders next, as GetLastPoses gives the head. (A head_pose
// landing between WaitGetPoses and the store below pairs the new head with
// the previous hands for that one frame; the window is microseconds.)
void OpenVrRuntime::read_hands() {
    Hands h = actions_.hand_poses(tracking_origin_);
    // 21 §3.3: with the dashboard up (no input focus) the hands count as lost.
    if (!system_->IsInputAvailable()) h = {};
    hands_.store(h);
    if (hand_log_.tick() && log_.debug_enabled()) log_hands(h);
}

// Dev. Debug Options: each hand's offset from the head, so moving the
// controllers can be checked against the log (11 §4.2). Render thread.
void OpenVrRuntime::log_hands(const Hands& h) {
    vr::TrackedDevicePose_t hmd{};
    compositor_->GetLastPoses(&hmd, 1, nullptr, 0);
    core::vr::HandLogLine line(kLogHandsPrefix);
    for (std::size_t i = 0; i < h.size(); ++i) {
        line.begin_hand(i);
        if (!h[i].valid) {
            line.append("{}", actions_.ready() ? "untracked" : "no SteamVR Input");
            continue;
        }
        float d[3];
        core::vr::head_local_offset(hmd.mDeviceToAbsoluteTracking.m, h[i].m, d);
        line.offset(d);
    }
    log_.line(line.text());
}

// ---- slots 7-8 ---------------------------------------------------------------

void OpenVrRuntime::begin_frame(float eye_offsets[6], bool head_tracked) {
    constexpr int kAxes           = 3;
    constexpr int kPositionColumn = 3;
    for (int eye = 0; eye < static_cast<int>(core::vr::kEyeCount); ++eye) {
        const vr::HmdMatrix34_t e = system_->GetEyeToHeadTransform(static_cast<vr::EVREye>(eye));
        for (int i = 0; i < kAxes; ++i) eye_offsets[eye * kAxes + i] = e.m[i][kPositionColumn];
    }
    // Screen frames (menus, videos, loading) against tracked ones, once per
    // change: always until the first tracked frame, then with Dev. Debug
    // Options (11 §4.5).
    frame_kind_log_.log(log_, head_tracked, kLogSource);
    cinema_mode_ = !head_tracked;
}

void OpenVrRuntime::submit_frame(core::vr::GpuTexture texture) {
    void* const side_by_side = d3d11::to_d3d(texture);
    head_read_.store(false, std::memory_order_relaxed); // the next frame's head pose is yet to come
    if (!side_by_side) return;
    vr::VRTextureWithPose_t tex{};
    tex.handle                    = side_by_side;
    tex.eType                     = vr::TextureType_DirectX;
    tex.eColorSpace               = vr::ColorSpace_Gamma;
    tex.mDeviceToAbsoluteTracking = render_pose_.load(); // the pose head_pose gave the game

    if (!cinema_mode_)
        submit_eyes(tex);
    else
        show_screen(side_by_side, tex);

    // Events before the pose wait, a change handed over after it: a pose the
    // game thread reads for the retake is then from a wait that followed
    // the change (03 §6.8).
    poll_events();
    // Running start: block until the next frame's poses are due (03 §6.3).
    compositor_->WaitGetPoses(nullptr, 0, nullptr, 0);
    space_change_.publish();
    read_hands();
    // The controllers as of this WaitGetPoses go to the game now, before its
    // next simulation step, not at the next BeginFrame (P10, as OpenXR).
    write_controls();
    controls_written_ = true; // the next BeginFrame need not read them again
    if (++submitted_frames_ == core::vr::kSceneCheckFrame) check_scene_app();
}

// A head-tracked frame: both eyes, cropped to their real FOV.
void OpenVrRuntime::submit_eyes(const vr::VRTextureWithPose_t& tex) {
    hide_screen();
    compositor_->Submit(vr::Eye_Left, &tex, &bounds_[0], vr::Submit_TextureWithPose);
    compositor_->Submit(vr::Eye_Right, &tex, &bounds_[1], vr::Submit_TextureWithPose);
}

// Menus, videos, loading: the left eye's image on a screen in front of where
// the head was facing at the last recenter, the 3D view left black (03 §7).
// Without a reference in the current tracking space (none taken yet, or the
// space moved since) the screen would stand wherever that space's origin
// is, often behind or below the player: only black is shown until the
// retake.
void OpenVrRuntime::show_screen(void* side_by_side, const vr::VRTextureWithPose_t& tex) {
    if (screen_reference_.load(std::memory_order_acquire))
        place_screen(side_by_side);
    else
        hide_screen();
    compositor_->Submit(vr::Eye_Left, &tex, &kNoImageBounds, vr::Submit_TextureWithPose);
    compositor_->Submit(vr::Eye_Right, &tex, &kNoImageBounds, vr::Submit_TextureWithPose);
}

// In the universe the poses, and so the reference, are in (03 §1), not a
// fixed one: in another universe the screen would stand at the same
// coordinates of a different origin.
void OpenVrRuntime::place_screen(void* side_by_side) {
    if (overlay_handle_ == vr::k_ulOverlayHandleInvalid) return;
    vr::Texture_t plain{side_by_side, vr::TextureType_DirectX, vr::ColorSpace_Gamma};
    overlay_->SetOverlayTexture(overlay_handle_, &plain);
    vr::HmdMatrix34_t m = yaw_matrix_.load();
    m.m[0][3] -= kCinemaDistance * m.m[0][2];
    m.m[2][3] -= kCinemaDistance * m.m[2][2];
    overlay_->SetOverlayTransformAbsolute(overlay_handle_,
                                          static_cast<vr::ETrackingUniverseOrigin>(tracking_origin_), &m);
    overlay_->SetOverlayWidthInMeters(overlay_handle_, kCinemaWidth);
    const vr::VRTextureBounds_t screen{core::vr::kScreenRegion.u_min, core::vr::kScreenRegion.v_min,
                                       core::vr::kScreenRegion.u_max, core::vr::kScreenRegion.v_max};
    overlay_->SetOverlayTextureBounds(overlay_handle_, &screen);
    overlay_->ShowOverlay(overlay_handle_);
}

void OpenVrRuntime::hide_screen() {
    if (overlay_handle_ != vr::k_ulOverlayHandleInvalid && overlay_->IsOverlayVisible(overlay_handle_)) {
        overlay_->HideOverlay(overlay_handle_);
    }
}

// Render thread, once per submitted frame: the process's only reader of
// IVRSystem's event queue, which every reader in the process shares (the
// overlay's own events are not asked for). Bounded, so a runtime that
// floods the queue cannot hold the frame; the rest wait for the next one.
// Logs once per event, and these events are rare.
void OpenVrRuntime::poll_events() {
    vr::VREvent_t e{};
    for (std::size_t n = 0; n < kMaxEventsPerFrame && system_->PollNextEvent(&e, sizeof(e)); ++n) {
        if (const char* name = space_change_name(e.eventType)) {
            logf(log_, kLogSpaceChanged, name);
            screen_reference_.store(false, std::memory_order_release);
            space_change_.note();
        } else if (e.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd &&
                   (e.eventType == vr::VREvent_TrackedDeviceUserInteractionStarted ||
                    e.eventType == vr::VREvent_TrackedDeviceUserInteractionEnded)) {
            // Not a retake by itself (03 §2.2); logged so a report shows
            // whether a moved screen followed putting the headset on.
            const bool started = e.eventType == vr::VREvent_TrackedDeviceUserInteractionStarted;
            logf(log_, kLogHeadsetActivity, started ? "in use" : "idle", e.eventType);
        }
    }
}

// SteamVR shows one scene application. If it is not this process (SteamVR's
// Desktop Game Theatre showing the game as a flat window, or an application
// that has not exited), the frames submitted here never reach the headset
// (03 §6.7). Render thread, once.
void OpenVrRuntime::check_scene_app() {
    const auto served = api_.generic_interface(vr::IVRApplications_Version);
    if (!served) {
        // Advisory check only; say why no warning can come (03 §6.7).
        logf(log_, kLogNoApplications, vr::IVRApplications_Version);
        return;
    }
    auto* apps        = static_cast<vr::IVRApplications*>(*served);
    const DWORD scene = apps->GetCurrentSceneProcessId();
    const DWORD self  = ::GetCurrentProcessId();
    if (scene == self) {
        log_.line(kLogShowsGame);
        return;
    }
    const std::string name = scene ? win32::process_name(scene) : std::string{};
    logf(log_, kLogShowsOther, scene, name.empty() ? "unknown" : name);
    dialogs_.show_async(core::ui::openvr_not_showing_game(name), core::ui::ports::DialogKind::Warning,
                        kNotShowingLabel);
}

// ---- extension -----------------------------------------------------------------

bool OpenVrRuntime::headset_in_use() {
    // Moving (or worn, with a proximity sensor) and the head's up axis
    // within ~45 degrees of vertical, on a valid pose: a queued recenter
    // must wait for tracking (03 §2.3). Read fresh: in menus head_pose is
    // not called, and a pose from the last gameplay frame would be stale.
    const bool active = system_->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd) ==
                        vr::k_EDeviceActivityLevel_UserInteraction;
    if (!active) return false;
    const auto& m = read_head().m;
    return head_valid_ && core::vr::is_upright(m[1][1]);
}

// 22 §6.1: the standing universe's Y is the height above the calibrated
// floor (03 §1), so the floor is at 0.
std::optional<float> OpenVrRuntime::floor_height() {
    if (!has_floor()) return std::nullopt;
    return 0.0f;
}

// 22 §6.2: read fresh, as headset_in_use (menus call no head_pose).
std::optional<float> OpenVrRuntime::head_height_above_floor() {
    if (!has_floor()) return std::nullopt;
    const auto& m = read_head().m;
    if (!head_valid_) return std::nullopt;
    return m[1][3];
}

// ---- slots 9-10 --------------------------------------------------------------

void OpenVrRuntime::register_control_hints() {
    controls_.set_scheme(input::ControlScheme::Touchpad); // until a controller shows up
}

// BeginFrame. Only when no submit has handed over the controls since the
// last one (before the first frame, or a frame without an eye texture).
// Only the submit path sets the flag, so with no submits every BeginFrame
// reads.
void OpenVrRuntime::poll_input() {
    if (!std::exchange(controls_written_, false)) write_controls();
}

// A hand's controller, logged once per device; the right one's type picks
// the control scheme (04 §5.5).
void OpenVrRuntime::note_controller(std::size_t hand, vr::TrackedDeviceIndex_t index) {
    if (index == vr::k_unTrackedDeviceIndexInvalid || logged_controller_[hand] == index) return;
    logged_controller_[hand] = index;
    char type[kPropertyBufferSize]{};
    system_->GetStringTrackedDeviceProperty(index, vr::Prop_ControllerType_String, type, sizeof(type));
    logf(log_, kLogController, hand == 0 ? "left" : "right", type, index);
    // 22 §10.1: each hand's family picks its glyphs (Controls.ControllerGlyphs).
    controls_.identify_controller(
        hand,
        actions_.ready() ? input::ControllerSource::SteamVrInput : input::ControllerSource::OpenVrLegacy,
        type);
    if (hand == 1 && actions_.ready() && !glyphs_chosen_) {
        glyphs_chosen_ = true;
        controls_.set_scheme(scheme_for(type));
    }
}

// Render thread. Reads its own copy of the device poses: poses_ belongs to
// the game thread (head_pose).
void OpenVrRuntime::write_controls() {
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
    compositor_->GetLastPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

    constexpr vr::ETrackedControllerRole kRoles[kHandCount] = {vr::TrackedControllerRole_LeftHand,
                                                               vr::TrackedControllerRole_RightHand};
    vr::TrackedDeviceIndex_t index[kHandCount]{};
    for (std::size_t i = 0; i < kHandCount; ++i) {
        index[i] = system_->GetTrackedDeviceIndexForControllerRole(kRoles[i]);
        note_controller(i, index[i]);
    }

    // The right hand near the head turns Interact into the flashlight.
    bool near_head = false;
    if (index[1] != vr::k_unTrackedDeviceIndexInvalid && index[1] < vr::k_unMaxTrackedDeviceCount) {
        const auto& head = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m;
        const auto& hand = poses[index[1]].mDeviceToAbsoluteTracking.m;
        near_head = poses[index[1]].bPoseIsValid && input::near_head(translation(hand), translation(head));
    }

    Controls c = actions_.ready() ? actions_.poll() : legacy_controls(index);
    c.poses    = body_poses(poses, index, system_->IsInputAvailable());
    controls_.write(c, near_head);
    play_haptics(poses, index);
}

// 21 §14.3: this frame's vibration, right after the controls are read:
// SteamVR Input's vibration actions, or on legacy input a pulse each frame
// on the hand's device. Nothing while the dashboard holds input focus, or
// for a hand that is not tracked.
void OpenVrRuntime::play_haptics(const vr::TrackedDevicePose_t (&poses)[vr::k_unMaxTrackedDeviceCount],
                                 const vr::TrackedDeviceIndex_t (&index)[kHandCount]) {
    if (!haptics_) return;
    std::array<bool, kHandCount> tracked{};
    for (std::size_t h = 0; h < kHandCount; ++h) {
        tracked[h] = index[h] < vr::k_unMaxTrackedDeviceCount && poses[index[h]].bPoseIsValid;
    }
    const core::vr::ports::HapticFrame frame = haptics_->next(system_->IsInputAvailable(), tracked);
    for (std::size_t h = 0; h < kHandCount; ++h) {
        const auto& out = frame[h];
        if (actions_.haptics_ready()) {
            if (out.play) actions_.vibrate(h, out.seconds, out.amplitude);
            if (out.stop) actions_.vibrate(h, 0.0f, 0.0f);
        } else if (out.level > 0.0f && tracked[h]) {
            const float us = out.level * core::hands::kLegacyPulseMicrosecondsPerAmplitude;
            system_->TriggerHapticPulse(index[h], kLegacyHapticAxis, static_cast<unsigned short>(us));
        }
    }
}

// The legacy reading: controller state, everything a touchpad (04 §5.8.3).
Controls OpenVrRuntime::legacy_controls(const vr::TrackedDeviceIndex_t (&index)[kHandCount]) {
    vr::VRControllerState_t state[kHandCount]{};
    for (std::size_t i = 0; i < kHandCount; ++i) {
        if (index[i] == vr::k_unTrackedDeviceIndexInvalid) continue;
        system_->GetControllerState(index[i], &state[i], sizeof(state[i]));
    }
    return input::touchpad_controls(legacy_hand(state[0]), legacy_hand(state[1]));
}

} // namespace

std::unique_ptr<core::vr::ports::VrRuntime> make_openvr_runtime(const OpenVrRuntimeDeps& deps) {
    return std::make_unique<OpenVrRuntime>(deps);
}

} // namespace omvr::platform::openvr
