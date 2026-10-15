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
// exist in both.

#include "hmd_backend.h"

#include "config/config.h"
#include "diag/log.h"
#include "input/gamepad.h"
#include "input/vr_controls.h"
#include "integration/dialogs.h"
#include "openvr_actions.h"
#include "openvr_backend.h"
#include "seqlock.h"
#include "vr_detect.h"

#include <openvr.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace mothervr::runtime {

namespace {

constexpr std::uint64_t kButtonMenu     = 1ull << vr::k_EButton_ApplicationMenu;
constexpr std::uint64_t kButtonGrip     = 1ull << vr::k_EButton_Grip;
constexpr std::uint64_t kButtonTouchpad = 1ull << vr::k_EButton_SteamVR_Touchpad;

// Frames after the first submit at which the compositor is asked whether it
// shows this process: SteamVR has settled by then (about 3 s at 90 Hz).
constexpr std::uint32_t kSceneCheckFrame = 270;

// Name of a process's executable, for the log; empty if unknown.
std::string process_name(DWORD pid) {
    HANDLE p = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const BOOL ok = ::QueryFullProcessImageNameW(p, 0, buf, &size);
    ::CloseHandle(p);
    return ok ? diag::utf8(std::filesystem::path{buf}.filename()) : std::string{};
}

float length(float x, float y, float z) noexcept { return std::sqrt(x * x + y * y + z * z); }

// The neutral pose: the reference before any recenter (03 §7) and the head
// before any valid pose (03 §1).
constexpr vr::HmdMatrix34_t kIdentity34{{{1.0f, 0.0f, 0.0f, 0.0f},
                                          {0.0f, 1.0f, 0.0f, 0.0f},
                                          {0.0f, 0.0f, 1.0f, 0.0f}}};

// Start-up wait for the compositor and then a valid headset pose, in total
// (03 §6.4): a cold SteamVR start serves them some seconds after init.
constexpr ULONGLONG kStartupWaitMs = 10'000;

// Below this horizontal length of the head's forward axis the heading is
// undefined (looking straight up or down); the previous yaw is kept (03 §2.1).
constexpr float kMinForwardLength = 1e-4f;

bool all_finite(const vr::HmdMatrix34_t& m) noexcept {
    for (const auto& row : m.m)
        for (float v : row)
            if (!std::isfinite(v)) return false;
    return true;
}

class OpenVrHmd final : public HmdBackend {
public:
    OpenVrHmd() noexcept {
        render_pose_.store(kIdentity34);
        yaw_matrix_.store(kIdentity34);
    }

    // Closes the session init() opened. Only reached through the stereo
    // device's explicit teardown, never at process exit.
    ~OpenVrHmd() override { openvr::shutdown(); }

    bool init() override;
    void get_hmd_desc(HmdDesc& desc) override;
    Sizei eye_texture_size(int eye, float pixel_scale) override;
    bool create_render_resources(ID3D11Device*, ID3D11DeviceContext*) override;
    void release_render_resources() override;
    void head_pose(Matrix34& out, float world_scale) override;
    void head_yaw_pose(Matrix34& out, float world_scale) override;
    void begin_frame(float eye_offsets[6], bool head_tracked) override;
    void submit_frame(ID3D11Texture2D* side_by_side) override;
    void register_control_hints() override;
    void poll_input() override;
    bool headset_in_use() override;
    bool recenter_taken() const override { return recenter_taken_; }
    bool hand_pose(int hand, Matrix34& out, float world_scale) override;
    bool hand_velocity(int hand, float (&out)[3], float world_scale) override;

private:
    using Hands = std::array<openvr::HandPose, 2>;
    const Hands& hands_now();
    void read_hands();
    void write_controls();
    void check_scene_app();
    openvr::Controls legacy_controls(const vr::TrackedDeviceIndex_t (&index)[2]);
    const vr::HmdMatrix34_t& read_head();

    vr::IVRSystem*     system_     = nullptr;
    vr::IVRCompositor* compositor_ = nullptr;
    vr::IVROverlay*    overlay_    = nullptr;

    // Game thread (head_pose, head_yaw_pose, headset_in_use; init before).
    vr::TrackedDevicePose_t poses_[vr::k_unMaxTrackedDeviceCount]{};
    // The last valid headset pose (OpenVR space, unscaled): what the game
    // gets while the pose is invalid, with the new orientation but this
    // position while position tracking is degraded (03 §1).
    vr::HmdMatrix34_t       head_ = kIdentity34;
    bool                    head_valid_ = false;   // the latest read was valid
    int                     head_state_ = -1;      // debug log once per change
    float                   yaw_ = 0.0f;           // the latest reference's heading
    bool                    recenter_taken_ = true;   // the last head_yaw_pose took a new one
    // Written on the game thread, read on the render thread (03 §11): the
    // pose head_pose returned, which submit_frame declares, and the
    // unconverted yaw-only reference Y, which places the virtual screen.
    SeqLock<vr::HmdMatrix34_t> render_pose_;
    SeqLock<vr::HmdMatrix34_t> yaw_matrix_;
    bool                    cinema_mode_ = false;
    vr::VROverlayHandle_t   overlay_handle_ = vr::k_ulOverlayHandleInvalid;

    vr::VRTextureBounds_t bounds_[2]{};         // crop of each eye's half of the texture
    char manufacturer_[64]{};
    char model_[64]{};

    vr::TrackedDeviceIndex_t logged_controller_[2] = {vr::k_unTrackedDeviceIndexInvalid,
                                                     vr::k_unTrackedDeviceIndexInvalid};

    openvr::Actions actions_;
    bool glyphs_chosen_ = false;
    // Render thread: submit_frame handed the game this frame's controls, so
    // the next poll_input has nothing to do.
    bool controls_written_ = false;

    // The hands' grip poses, read on the render thread right after each
    // WaitGetPoses (the poses head_pose's GetLastPoses returns) and handed
    // to the game thread through the seqlock.
    SeqLock<Hands>     hands_;
    int                tracking_origin_ = vr::TrackingUniverseStanding;  // WaitGetPoses' universe
    std::uint32_t      hand_log_frames_ = 0;                             // render thread
    std::uint32_t      submitted_frames_ = 0;                            // render thread
    // Game thread: the hands as of this frame's head_pose, and whether
    // head_pose ran since the last submit.
    Hands              hands_rendered_{};
    std::atomic<bool>  head_read_{false};
    Hands              hands_scratch_{};
};

// ---- slot 0 ------------------------------------------------------------------

bool OpenVrHmd::init() {
    if (!openvr::init(last_result().openvr_runtime_path)) return false;

    // The original spins until the compositor is up. One bound covers the
    // compositor and the first valid pose together (03 §6.4).
    const ULONGLONG deadline = ::GetTickCount64() + kStartupWaitMs;
    int system_err = 0, overlay_err = 0, compositor_err = 0;
    system_ = static_cast<vr::IVRSystem*>(openvr::generic_interface(vr::IVRSystem_Version, &system_err));
    overlay_ = static_cast<vr::IVROverlay*>(openvr::generic_interface(vr::IVROverlay_Version, &overlay_err));
    for (;;) {
        compositor_ = static_cast<vr::IVRCompositor*>(
            openvr::generic_interface(vr::IVRCompositor_Version, &compositor_err));
        if (compositor_ || ::GetTickCount64() >= deadline) break;
        ::Sleep(50);
    }
    // A missing interface is a start-up failure that names the interface and
    // carries the runtime's error code (03 §6.4, 13 §5.3).
    const auto missing = [](const char* version, int err) {
        diag::logf("openvr: runtime lacks interface {} (error {})", version, err);
        set_init_failure(std::format("SteamVR does not provide {}", version), err);
        openvr::shutdown();
        return false;
    };
    if (!system_) return missing(vr::IVRSystem_Version, system_err);
    if (!compositor_) return missing(vr::IVRCompositor_Version, compositor_err);
    if (!overlay_) return missing(vr::IVROverlay_Version, overlay_err);
    diag::logf("openvr: interfaces {}, {}, {}", vr::IVRSystem_Version, vr::IVRCompositor_Version,
               vr::IVROverlay_Version);

    // Wait, within the same bound, for a valid headset pose. None in time is
    // not a failure: slot 11 then returns the neutral pose and the first
    // reference is queued until the headset is in use.
    bool pose_valid = false;
    while (!pose_valid && ::GetTickCount64() < deadline) {
        const auto err = compositor_->WaitGetPoses(poses_, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
        pose_valid = poses_[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid;
        // A failing call may return at once; do not spin on it.
        if (!pose_valid && err != vr::VRCompositorError_None) ::Sleep(10);
    }
    tracking_origin_ = compositor_->GetTrackingSpace();
    diag::logf("openvr: compositor ready, headset pose {}", pose_valid ? "valid" : "not yet valid");
    actions_.init(diag::module_directory() / "mothervr_input");
    return true;
}

// ---- slot 1 ------------------------------------------------------------------

void OpenVrHmd::get_hmd_desc(HmdDesc& desc) {
    float l[2], r[2], t[2], b[2];
    for (int eye = 0; eye < 2; ++eye) {
        system_->GetProjectionRaw(static_cast<vr::EVREye>(eye), &l[eye], &r[eye], &t[eye], &b[eye]);
        // OpenVR's raw t is negative and b positive; in image terms (v = 0
        // at the top) b is the upward extent (03 §3.1). The original stores
        // -t as Up, which the game never sees (it reads only the symmetric
        // EyeFov) but which crops the mirror's eye modes on the wrong rows
        // on a vertically asymmetric headset.
        desc.default_eye_fov[eye] = {b[eye], -t[eye], -l[eye], r[eye]};
    }
    const float max_v = std::max({-t[0], b[0], -t[1], b[1]});
    const float max_h = std::max({-l[0], r[0], -l[1], r[1]});
    desc.max_eye_fov[0] = desc.max_eye_fov[1] = FovPort{max_v, max_v, max_h, max_h};

    // Each eye renders a symmetric frustum into its half of the texture;
    // crop it back to the eye's real FOV (the SteamVR Unity plugin formula).
    for (int eye = 0; eye < 2; ++eye) {
        const float half = eye == 0 ? 0.0f : 0.5f;
        bounds_[eye].uMin = (0.5f + 0.5f * l[eye] / max_h) * 0.5f + half;
        bounds_[eye].uMax = (0.5f + 0.5f * r[eye] / max_h) * 0.5f + half;
        bounds_[eye].vMin = 0.5f - 0.5f * b[eye] / max_v;
        bounds_[eye].vMax = 0.5f - 0.5f * t[eye] / max_v;
    }

    system_->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                            vr::Prop_ManufacturerName_String,
                                            manufacturer_, sizeof(manufacturer_));
    system_->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
                                            vr::Prop_ModelNumber_String,
                                            model_, sizeof(model_));
    desc.manufacturer = manufacturer_;
    desc.product_name = model_;
    diag::logf("openvr: {} {}, render tangents v={:.3f} h={:.3f}", manufacturer_, model_, max_v, max_h);
}

// ---- slots 2-4 ---------------------------------------------------------------

Sizei OpenVrHmd::eye_texture_size(int /*eye*/, float pixel_scale) {
    std::uint32_t w = 0, h = 0;
    system_->GetRecommendedRenderTargetSize(&w, &h);
    return {static_cast<std::int32_t>(static_cast<float>(w) * pixel_scale),
            static_cast<std::int32_t>(static_cast<float>(h) * pixel_scale)};
}

// Never fails: without the overlay only the virtual screen is missing.
bool OpenVrHmd::create_render_resources(ID3D11Device*, ID3D11DeviceContext*) {
    // The game may reconfigure rendering more than once; one overlay will do.
    if (overlay_handle_ != vr::k_ulOverlayHandleInvalid) return true;
    const auto err = overlay_->CreateOverlay("MotherVROverlay", "MotherVROverlay", &overlay_handle_);
    if (err != vr::VROverlayError_None) {
        diag::logf("openvr: CreateOverlay failed ({})", static_cast<int>(err));
        overlay_handle_ = vr::k_ulOverlayHandleInvalid;
    }
    return true;
}

void OpenVrHmd::release_render_resources() {
    if (overlay_handle_ == vr::k_ulOverlayHandleInvalid) return;
    overlay_->DestroyOverlay(overlay_handle_);
    overlay_handle_ = vr::k_ulOverlayHandleInvalid;
}

// ---- slots 5-6 ---------------------------------------------------------------

// Game thread. The headset's latest pose with the invalid-pose rules of
// 03 §1: an invalid (or non-finite) pose leaves the last valid one; a valid
// pose with degraded tracking takes the new orientation and keeps the last
// valid position. Before any valid pose that is the neutral pose.
const vr::HmdMatrix34_t& OpenVrHmd::read_head() {
    compositor_->GetLastPoses(poses_, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
    const auto& p = poses_[vr::k_unTrackedDeviceIndex_Hmd];
    head_valid_ = p.bPoseIsValid && all_finite(p.mDeviceToAbsoluteTracking);
    const bool position_ok = p.eTrackingResult == vr::TrackingResult_Running_OK;
    if (head_valid_) {
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) head_.m[r][c] = p.mDeviceToAbsoluteTracking.m[r][c];
            if (position_ok) head_.m[r][3] = p.mDeviceToAbsoluteTracking.m[r][3];
        }
    }
    // Dev. Debug Options: tracking lost or degraded, once per change.
    const int state = !head_valid_ ? 0 : position_ok ? 2 : 1;
    if (state != head_state_ && config::current().debug.debug) {
        diag::logf("[diag] headset pose {} (tracking result {})",
                   state == 0 ? "invalid, holding the last valid pose"
                   : state == 1 ? "degraded, holding the last valid position" : "tracked",
                   static_cast<int>(p.eTrackingResult));
    }
    head_state_ = state;
    return head_;
}

void OpenVrHmd::head_pose(Matrix34& out, float world_scale) {
    const vr::HmdMatrix34_t& m = read_head();
    render_pose_.store(m);   // the pose this frame renders with and is declared with
    out = to_game_pose(m.m, world_scale);
    hands_rendered_ = hands_.load();
    head_read_.store(true, std::memory_order_relaxed);
}

// ---- hands (extension) ---------------------------------------------------------

// Game thread. The hands of the frame head_pose was read for, else (menus,
// or a call before head_pose this frame) the newest.
const OpenVrHmd::Hands& OpenVrHmd::hands_now() {
    if (head_read_.load(std::memory_order_relaxed)) return hands_rendered_;
    hands_scratch_ = hands_.load();
    return hands_scratch_;
}

bool OpenVrHmd::hand_pose(int hand, Matrix34& out, float world_scale) {
    if (hand < 0 || hand > 1) return false;
    const auto& h = hands_now()[hand];
    if (!h.valid) return false;
    out = to_game_pose(h.m, world_scale);
    return true;
}

bool OpenVrHmd::hand_velocity(int hand, float (&out)[3], float world_scale) {
    if (hand < 0 || hand > 1) return false;
    const auto& h = hands_now()[hand];
    if (!h.valid) return false;
    to_game_velocity(h.velocity, world_scale, out);
    return true;
}

// Render thread, right after WaitGetPoses: GetPoseActionDataForNextFrame
// gives the hands with that call's prediction, the display time of the
// frame the game renders next, as GetLastPoses gives the head. (A head_pose
// landing between WaitGetPoses and the store below pairs the new head with
// the previous hands for that one frame; the window is microseconds.)
void OpenVrHmd::read_hands() {
    const Hands h = actions_.hand_poses(tracking_origin_);
    hands_.store(h);

    // Dev. Debug Options: each hand's offset from the head, so moving the
    // controllers can be checked against the log.
    if (++hand_log_frames_ < kHandLogFrames) return;
    hand_log_frames_ = 0;
    if (!config::current().debug.debug) return;
    vr::TrackedDevicePose_t hmd{};
    compositor_->GetLastPoses(&hmd, 1, nullptr, 0);
    std::string line = "openvr: hands (head-local m, +x right +y up -z fwd):";
    for (int i = 0; i < 2; ++i) {
        line += i == 0 ? " L " : " R ";
        if (!h[i].valid) {
            line += actions_.ready() ? "untracked" : "no SteamVR Input";
            continue;
        }
        float d[3];
        head_local_offset(hmd.mDeviceToAbsoluteTracking.m, h[i].m, d);
        line += std::format("({:+.2f} {:+.2f} {:+.2f})", d[0], d[1], d[2]);
    }
    diag::log_line(line);
}

void OpenVrHmd::head_yaw_pose(Matrix34& out, float world_scale) {
    const auto& m = read_head().m;
    // Never from an invalid pose (03 §2.1): the previous reference stays. The
    // stereo device's queued retake waits for headset_in_use, which needs a
    // valid pose, so a retake is taken once tracking is back.
    recenter_taken_ = head_valid_;
    if (!head_valid_) {
        out = to_game_pose(yaw_matrix_.load().m, world_scale);
        diag::log_line("openvr: recenter skipped: headset pose invalid, the previous reference stays");
        return;
    }
    // Yaw about +Y: the original goes through a quaternion,
    // atan2(2(wy + xz), w^2 - x^2 - y^2 + z^2), which is this. Looking
    // straight up or down leaves no heading: keep the previous one.
    const bool vertical = std::sqrt(m[0][2] * m[0][2] + m[2][2] * m[2][2]) < kMinForwardLength;
    const float yaw = vertical ? yaw_ : std::atan2(m[0][2], m[2][2]);
    yaw_ = yaw;
    const float s = std::sin(yaw), c = std::cos(yaw);
    const vr::HmdMatrix34_t y{{
        {   c, 0.0f,    s, m[0][3]},
        {0.0f, 1.0f, 0.0f, m[1][3]},
        {  -s, 0.0f,    c, m[2][3]},
    }};
    yaw_matrix_.store(y);
    out = to_game_pose(y.m, world_scale);
    diag::logf("openvr: recenter: head x=({:.2f} {:.2f} {:.2f}) y=({:.2f} {:.2f} {:.2f}) z=({:.2f} {:.2f} {:.2f}) "
               "pos=({:.2f} {:.2f} {:.2f}) yaw={:.1f} deg{}, activity={}",
               m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2], m[2][2],
               m[0][3], m[1][3], m[2][3], yaw * 57.29578f, vertical ? " (kept: looking straight up or down)" : "",
               static_cast<int>(system_->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd)));
}

// ---- slots 7-8 ---------------------------------------------------------------

void OpenVrHmd::begin_frame(float eye_offsets[6], bool head_tracked) {
    for (int eye = 0; eye < 2; ++eye) {
        const vr::HmdMatrix34_t e = system_->GetEyeToHeadTransform(static_cast<vr::EVREye>(eye));
        for (int i = 0; i < 3; ++i) eye_offsets[eye * 3 + i] = e.m[i][3];
    }
    // Debug: screen frames (menus, videos, loading) against tracked ones,
    // once per change.
    if (cinema_mode_ != !head_tracked && config::current().debug.debug)
        diag::logf("[diag] frames now {}", head_tracked ? "tracked" : "on the screen");
    cinema_mode_ = !head_tracked;
}

void OpenVrHmd::submit_frame(ID3D11Texture2D* side_by_side) {
    head_read_.store(false, std::memory_order_relaxed);   // the next frame's head pose is yet to come
    if (!side_by_side) return;
    vr::VRTextureWithPose_t tex{};
    tex.handle      = side_by_side;
    tex.eType       = vr::TextureType_DirectX;
    tex.eColorSpace = vr::ColorSpace_Gamma;
    tex.mDeviceToAbsoluteTracking = render_pose_.load();   // the pose head_pose gave the game

    if (!cinema_mode_) {
        if (overlay_handle_ != vr::k_ulOverlayHandleInvalid && overlay_->IsOverlayVisible(overlay_handle_)) {
            overlay_->HideOverlay(overlay_handle_);
        }
        compositor_->Submit(vr::Eye_Left,  &tex, &bounds_[0], vr::Submit_TextureWithPose);
        compositor_->Submit(vr::Eye_Right, &tex, &bounds_[1], vr::Submit_TextureWithPose);
    } else {
        // Menus, videos, loading: the left eye's image on a screen 1.5 m in
        // front of where the head was facing, the 3D view left black.
        if (overlay_handle_ != vr::k_ulOverlayHandleInvalid) {
            vr::Texture_t plain{side_by_side, vr::TextureType_DirectX, vr::ColorSpace_Gamma};
            overlay_->SetOverlayTexture(overlay_handle_, &plain);
            vr::HmdMatrix34_t m = yaw_matrix_.load();
            m.m[0][3] -= kCinemaDistance * m.m[0][2];
            m.m[2][3] -= kCinemaDistance * m.m[2][2];
            overlay_->SetOverlayTransformAbsolute(overlay_handle_, vr::TrackingUniverseStanding, &m);
            overlay_->SetOverlayWidthInMeters(overlay_handle_, kCinemaWidth);
            const vr::VRTextureBounds_t screen{0.0f, 0.25f, 0.5f, 0.75f};
            overlay_->SetOverlayTextureBounds(overlay_handle_, &screen);
            overlay_->ShowOverlay(overlay_handle_);
        }
        const vr::VRTextureBounds_t nothing{0.0f, 0.0f, 1e-5f, 1e-5f};
        compositor_->Submit(vr::Eye_Left,  &tex, &nothing, vr::Submit_TextureWithPose);
        compositor_->Submit(vr::Eye_Right, &tex, &nothing, vr::Submit_TextureWithPose);
    }
    // Running start: block until the next frame's poses are due.
    compositor_->WaitGetPoses(nullptr, 0, nullptr, 0);
    read_hands();
    // The controllers as of this WaitGetPoses go to the game now, before its
    // next simulation step, not at the next BeginFrame (P10, as OpenXR).
    write_controls();
    if (++submitted_frames_ == kSceneCheckFrame) check_scene_app();
}

// SteamVR shows one scene application. If it is not this process (SteamVR's
// Desktop Game Theatre showing the game as a flat window, or an application
// that has not exited), the frames submitted here never reach the headset.
// Render thread, once.
void OpenVrHmd::check_scene_app() {
    auto* apps = static_cast<vr::IVRApplications*>(openvr::generic_interface(vr::IVRApplications_Version));
    if (!apps) {
        // Advisory check only; say why no warning can come (03 §6.7).
        diag::logf("openvr: no {}: cannot check which application the headset shows; VR is otherwise unaffected",
                   vr::IVRApplications_Version);
        return;
    }
    const DWORD scene = apps->GetCurrentSceneProcessId();
    const DWORD self  = ::GetCurrentProcessId();
    if (scene == self) {
        diag::log_line("openvr: the headset shows the game");
        return;
    }
    const std::string name = scene ? process_name(scene) : std::string{};
    diag::logf("openvr: the headset shows process {} ({}), not the game", scene, name.empty() ? "unknown" : name);
    integration::show_message_box_async(
        std::format("SteamVR is not showing the game in the headset{}.\n\n"
                    "This is usually SteamVR's Desktop Game Theatre. In Steam, open the "
                    "Properties of Alien: Isolation and uncheck 'Use Desktop Game Theatre while "
                    "SteamVR is active', then restart SteamVR and the game.",
                    name.empty() ? std::string{} : " (it shows " + name + ")"),
        MB_OK | MB_ICONWARNING, "headset not showing the game");
}

// ---- extension -----------------------------------------------------------------

bool OpenVrHmd::headset_in_use() {
    // Moving (or worn, with a proximity sensor) and the head's up axis
    // within ~45 degrees of vertical, on a valid pose: a queued recenter
    // must wait for tracking (03 §2.1). Read fresh: in menus head_pose is
    // not called, and a pose from the last gameplay frame would be stale.
    const bool active = system_->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd)
                        == vr::k_EDeviceActivityLevel_UserInteraction;
    if (!active) return false;
    const auto& m = read_head().m;
    return head_valid_ && m[1][1] > 0.7f;
}

// ---- slots 9-10 --------------------------------------------------------------

void OpenVrHmd::register_control_hints() {
    input::set_control_scheme(input::ControlScheme::Touchpad);   // until a controller shows up
}

// BeginFrame. Only when no submit has handed over the controls since the
// last one (before the first frame, or a frame without an eye texture).
void OpenVrHmd::poll_input() {
    if (!std::exchange(controls_written_, false)) write_controls();
}

// Render thread. Reads its own copy of the device poses: poses_ belongs to
// the game thread (head_pose).
void OpenVrHmd::write_controls() {
    controls_written_ = true;
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
    compositor_->GetLastPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

    const vr::ETrackedControllerRole roles[2] = {vr::TrackedControllerRole_LeftHand,
                                                 vr::TrackedControllerRole_RightHand};
    vr::TrackedDeviceIndex_t index[2]{};
    for (int i = 0; i < 2; ++i) {
        index[i] = system_->GetTrackedDeviceIndexForControllerRole(roles[i]);
        if (index[i] != vr::k_unTrackedDeviceIndexInvalid && logged_controller_[i] != index[i]) {
            logged_controller_[i] = index[i];
            char type[64]{};
            system_->GetStringTrackedDeviceProperty(index[i], vr::Prop_ControllerType_String, type, sizeof(type));
            diag::logf("openvr: {} hand controller: {} (device {})", i == 0 ? "left" : "right", type, index[i]);
            if (i == 1 && actions_.ready() && !glyphs_chosen_) {
                glyphs_chosen_ = true;
                const std::string_view t(type);
                input::set_control_scheme(t == "vive_controller" ? input::ControlScheme::Touchpad
                                          : t == "knuckles"      ? input::ControlScheme::Index
                                                                 : input::ControlScheme::Thumbstick);
            }
        }
    }

    // The right hand near the head turns Interact into the flashlight.
    bool near_head = false;
    if (index[1] != vr::k_unTrackedDeviceIndexInvalid && index[1] < vr::k_unMaxTrackedDeviceCount) {
        const auto& head = poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking.m;
        const auto& hand = poses[index[1]].mDeviceToAbsoluteTracking.m;
        near_head = poses[index[1]].bPoseIsValid &&
                    length(hand[0][3] - head[0][3], hand[1][3] - head[1][3], hand[2][3] - head[2][3]) < input::kNearHead;
    }

    openvr::Controls c = actions_.ready() ? actions_.poll() : legacy_controls(index);
    input::write_controls(c, near_head);
}

// The original's reading: legacy controller state, everything a touchpad.
openvr::Controls OpenVrHmd::legacy_controls(const vr::TrackedDeviceIndex_t (&index)[2]) {
    vr::VRControllerState_t state[2]{};
    for (int i = 0; i < 2; ++i) {
        if (index[i] == vr::k_unTrackedDeviceIndexInvalid) continue;
        system_->GetControllerState(index[i], &state[i], sizeof(state[i]));
    }
    const auto& L = state[0];
    const auto& R = state[1];
    const bool click_r = R.ulButtonPressed & kButtonTouchpad;
    const input::Vec2 pad_r{R.rAxis[0].x, R.rAxis[0].y};
    const auto regions = input::decode_touchpad(pad_r, click_r);

    openvr::Controls c;
    c.touchpad_scheme = true;
    if (L.ulButtonTouched & kButtonTouchpad) c.move = {L.rAxis[0].x, L.rAxis[0].y};
    if (R.ulButtonTouched & kButtonTouchpad) c.turn = pad_r;
    c.turn_engaged = click_r;
    c.trigger_l = L.rAxis[1].x;
    c.trigger_r = R.rAxis[1].x;
    c.grip_l = L.ulButtonPressed & kButtonGrip;
    c.grip_r = R.ulButtonPressed & kButtonGrip;
    c.sprint = L.ulButtonPressed & kButtonTouchpad;
    c.interact = regions.interact;
    c.reload = regions.reload;
    c.radial = regions.radial;
    c.map = L.ulButtonPressed & kButtonMenu;
    c.pause = R.ulButtonPressed & kButtonMenu;
    return c;
}


} // namespace

std::unique_ptr<HmdBackend> make_openvr_backend() {
    return std::make_unique<OpenVrHmd>();
}

} // namespace mothervr::runtime
