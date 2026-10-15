// The OpenXR HMD backend: the stereo device on any OpenXR runtime (Meta /
// Quest Link, SteamVR, WMR, Pico, Varjo...). Not in the original.
//
// The game creates the stereo device before its D3D11 device exists, so the
// instance and system come up at init (with the FOV where the runtime
// reports it through XR_EPIC_view_configuration_fov) and the session when the
// game configures rendering with its device.

#include "platform/openxr/openxr_runtime.h"

#include "core/config/settings_store.h" // R: vr (A1): settings and debug flag handed in
#include "core/diag/logger.h"           // R: vr (A1): log through an injected Log&
#include "core/diag/tuning.h"
#include "core/input/touchpad.h"
#include "core/input/tuning.h"
#include "core/stereo/tuning.h"
#include "core/vr/hand_offsets.h"
#include "core/vr/pose_convert.h"
#include "core/vr/tuning.h"
#include "platform/d3d11/handles.h"
#include "platform/openxr/session.h"
#include "util/math/pose.h"
#include "util/math/vec.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <string>

namespace omvr::platform::openxr {

namespace {

namespace xr = ::omvr::platform::openxr;
namespace config = core::config;
namespace input = core::input;
namespace diag {
using core::diag::log_line;
using core::diag::logf;
} // namespace diag
using core::stereo::kCinemaDistance;
using core::stereo::kCinemaWidth;
using core::vr::FovTangents;
using core::vr::HeadsetDesc;
using core::vr::TextureSize;
using core::vr::head_local_offset;
using core::vr::kDefaultEyeX;
using core::vr::kDefaultTangent;
using core::vr::kMinForwardLength;
using core::vr::to_game_pose;
using core::vr::to_game_velocity;
using util::conjugate;
using util::length;
using util::Matrix34;
using util::pose_matrix;
using util::rotate;
using util::sub;
using util::yaw_of;
constexpr auto kHandLogFrames = core::diag::kDebugLogPeriodFrames;

// ---- The runtime -------------------------------------------------------------------

class OpenXrHmd final : public core::vr::ports::VrRuntime {
public:
    explicit OpenXrHmd(const OpenXrRuntimeDeps& deps) noexcept
        : xr_(deps.session), controls_(deps.controls), audio_(deps.audio), signals_(deps.signals) {}

public:
    // Ends the session and destroys the instance. Only reached through the
    // stereo device's explicit teardown, never at process exit.
    ~OpenXrHmd() override { xr_.shutdown(); }

    bool init() override {
        if (!xr_.init_system()) return false;
        const auto& info = xr_.system_info();
        if (info.has_fov) set_fov(info.fov);
        else set_default_fov();
        audio_.redirect_to(info.audio_output);
        return true;
    }

    void headset_desc(HeadsetDesc& desc) override {
        for (int e = 0; e < 2; ++e) desc.default_eye_fov[e] = eye_fov_[e];
        desc.max_eye_fov[0] = desc.max_eye_fov[1] = FovTangents{max_v_, max_v_, max_h_, max_h_};
        const auto& info = xr_.system_info();
        desc.product_name = info.system_name;
        desc.manufacturer = info.runtime_name;
        diag::logf("openxr: render tangents v={:.3f} h={:.3f}{}", max_v_, max_h_,
                   fov_known_ ? "" : " (provisional)");
    }

    bool refresh_headset_desc(HeadsetDesc& desc) override {
        if (fov_known_) return false;
        const xr::FrameViews views = xr_.views();
        if (!views.valid) return false;
        const auto& v = views.eyes;
        set_fov({XrFovf{v[0].angle_left, v[0].angle_right, v[0].angle_up, v[0].angle_down},
                 XrFovf{v[1].angle_left, v[1].angle_right, v[1].angle_up, v[1].angle_down}});
        headset_desc(desc);
        return true;
    }

    TextureSize eye_texture_size(int, float pixel_scale) override {
        const auto& info = xr_.system_info();
        return {static_cast<std::int32_t>(static_cast<float>(info.recommended_w) * pixel_scale),
                static_cast<std::int32_t>(static_cast<float>(info.recommended_h) * pixel_scale)};
    }

    bool create_render_resources(core::vr::GpuDevice gpu, core::vr::GpuContext) override {
        ID3D11Device* const device = d3d11::to_d3d(gpu);
        if (session_) return true;
        if (!device) return false;
        session_ = xr_.init(device).ok;
        diag::logf("openxr: session {}", session_ ? "created on the game's device" : "failed");
        return session_;
    }

    void release_render_resources() override {}

    // Game thread. The views are those of the frame the render thread began
    // at the end of the previous EndFrame, predicted for this frame's own
    // display time; submit_frame declares exactly these.
    void head_pose(Matrix34& out, float world_scale) override {
        const xr::FrameViews v = xr_.views();
        render_.store(v);   // what the game renders this frame with
        head_read_.store(true, std::memory_order_relaxed);
        float m[3][4];
        pose_matrix(v.head, m);
        out = to_game_pose(m, world_scale);
    }

    // Game thread. The grip poses located with the views head_pose handed
    // out this frame, so hands and head share one display time.
    bool hand_pose(int hand, Matrix34& out, float world_scale) override {
        if (hand < 0 || hand > 1) return false;
        const xr::HandPose h = hand_state(hand);
        if (!h.valid) return false;
        float m[3][4];
        pose_matrix(h.pose, m);
        out = to_game_pose(m, world_scale);
        return true;
    }

    bool hand_velocity(int hand, float (&out)[3], float world_scale) override {
        if (hand < 0 || hand > 1) return false;
        const xr::HandPose h = hand_state(hand);
        if (!h.velocity_valid) return false;
        to_game_velocity({h.velocity.x, h.velocity.y, h.velocity.z}, world_scale, out);
        return true;
    }

    void head_yaw_pose(Matrix34& out, float world_scale) override {
        const xr::FrameViews v = xr_.views();
        float m[3][4];
        // Never from an invalid head (09 §5.5): the previous reference stays
        // (identity before any). A queued retake waits for headset_in_use,
        // which needs a valid head, so it happens once tracking is back.
        recenter_taken_ = v.head_valid;
        if (!v.head_valid) {
            pose_matrix(yaw_ref_.load(), m);
            out = to_game_pose(m, world_scale);
            diag::log_line("openxr: recenter skipped: head not tracked, the previous reference stays");
            return;
        }
        // Looking straight up or down leaves no heading: keep the previous yaw.
        const auto& q = v.head.orientation;
        const float fx = 2 * (q.w * q.y + q.x * q.z), fz = 1 - 2 * (q.x * q.x + q.y * q.y);
        if (std::sqrt(fx * fx + fz * fz) >= kMinForwardLength) yaw_ = yaw_of(q);
        const xr::Pose ref{{0, std::sin(yaw_ * 0.5f), 0, std::cos(yaw_ * 0.5f)}, v.head.position};
        yaw_ref_.store(ref);
        pose_matrix(ref, m);
        out = to_game_pose(m, world_scale);
    }

    void begin_frame(float eye_offsets[6], bool head_tracked) override {
        const xr::FrameViews v = xr_.views();
        // Only from views whose eye and head positions are this frame's;
        // otherwise the previous offsets stay (09 §5.4, §5.5), the 32 mm
        // default until then.
        if (v.valid && v.eye_positions_valid && v.head_valid) {
            for (int e = 0; e < 2; ++e)
                eye_offset_[e] = rotate(conjugate(v.head.orientation), sub(v.eyes[e].pose.position, v.head.position));
        }
        for (int e = 0; e < 2; ++e) {
            eye_offsets[e * 3 + 0] = eye_offset_[e].x;
            eye_offsets[e * 3 + 1] = eye_offset_[e].y;
            eye_offsets[e * 3 + 2] = eye_offset_[e].z;
        }
        if (cinema_ != !head_tracked && config::current().debug.debug)
            diag::logf("[diag] frames now {}", head_tracked ? "tracked" : "on the screen");
        cinema_ = !head_tracked;
    }

    void submit_frame(core::vr::GpuTexture texture) override {
        ID3D11Texture2D* const side_by_side = d3d11::to_d3d(texture);
        if (!session_ || !side_by_side) return;
        D3D11_TEXTURE2D_DESC d{};
        side_by_side->GetDesc(&d);
        const UINT half = d.Width / 2;

        const xr::FrameViews rendered = render_.load();
        head_read_.store(false, std::memory_order_relaxed);
        if (!cinema_ && rendered.valid) {
            // Both halves as a projection layer, declared with the pose and
            // the symmetric FOV the game rendered them with.
            xr::EyeImages images;
            images.rendered_for = rendered.display_time;
            for (int e = 0; e < 2; ++e) {
                images.texture[e] = side_by_side;
                images.region[e]  = {e * half, 0, 0, e * half + half, d.Height, 1};
                images.view[e]    = rendered.eyes[e];
                images.view[e].angle_left  = -std::atan(max_h_);
                images.view[e].angle_right =  std::atan(max_h_);
                images.view[e].angle_up    =  std::atan(max_v_);
                images.view[e].angle_down  = -std::atan(max_v_);
            }
            xr_.set_eye_images(images);
            xr_.set_screen_image(nullptr, {});
        } else {
            // Menus, videos, loading: the left eye's image on a screen 1.5 m
            // in front of where the head was facing at the last recenter.
            xr_.clear_eye_images();
            xr_.set_screen_image(side_by_side, {0, d.Height / 4, 0, half, d.Height * 3 / 4, 1});
            const xr::Pose yaw_ref = yaw_ref_.load();
            const auto& q = yaw_ref.orientation;
            const xr::Vec3 forward = rotate(q, {0, 0, -1});
            xr_.set_screen_pose({q, {yaw_ref.position.x + forward.x * kCinemaDistance, yaw_ref.position.y,
                                     yaw_ref.position.z + forward.z * kCinemaDistance}},
                                kCinemaWidth);
        }
        // Ends this frame (copies, flush, xrEndFrame), then blocks until the
        // runtime wants the next one and begins it: a running start, as
        // OpenVR's WaitGetPoses. The game thread then reads the new views.
        const bool begun = xr_.frame(nullptr);
        log_hands();
        // The controllers were synced as the next frame began; hand them to
        // the game now, before its next simulation step (P10). With no frame
        // begun (session not running, lost) nothing was synced: the game
        // sees every VR control released, never the last synced state
        // (04 §5.12, 09 §3).
        if (begun) write_controls();
        else controls_.write(input::Controls{}, false);
    }

    void register_control_hints() override { controls_.set_scheme(input::ControlScheme::Thumbstick); }

    void poll_input() override {
        // Quit when the runtime asks, like the original on ovr ShouldQuit.
        // From: FUN_10004960.
        if (xr_.exit_requested()) signals_.request_exit();
        // The controllers themselves go to the game at the end of submit_frame,
        // where they are synced, one frame earlier than BeginFrame would.
    }

    bool recenter_taken() const override { return recenter_taken_; }

    bool headset_in_use() override {
        if (!xr_.running()) return false;
        const xr::FrameViews v = xr_.views();
        if (!v.valid || !v.head_valid) return false;   // a recenter needs a tracked head (09 §6.2)
        const auto& q = v.head.orientation;
        return 1 - 2 * (q.x * q.x + q.z * q.z) > 0.7f;   // head's up axis near vertical
    }

private:
    // The hands of the frame the game is rendering: the views head_pose
    // took this frame, else (no head pose read since the last submit:
    // menus, or a call before head_pose) the newest.
    xr::HandPose hand_state(int hand) const {
        return (head_read_.load(std::memory_order_relaxed) ? render_.load() : xr_.views()).hands[hand];
    }

    // Dev. Debug Options: each hand's offset from the head, so moving the
    // controllers can be checked against the log. Render thread.
    void log_hands() {
        if (++hand_log_frames_ < kHandLogFrames) return;
        hand_log_frames_ = 0;
        if (!config::current().debug.debug) return;
        const xr::FrameViews v = xr_.views();
        float head[3][4];
        pose_matrix(v.head, head);
        std::string line = "openxr: hands (head-local m, +x right +y up -z fwd):";
        for (int h = 0; h < 2; ++h) {
            line += h == 0 ? " L " : " R ";
            if (!v.hands[h].valid) {
                const auto& in = xr_.input.state().hands[h];
                line += std::format("untracked (actions {}, grip {})", in.active ? "active" : "inactive",
                                    in.grip_valid ? "located" : "not located");
                continue;
            }
            float hand[3][4], d[3];
            pose_matrix(v.hands[h].pose, hand);
            head_local_offset(head, hand, d);
            line += std::format("({:+.2f} {:+.2f} {:+.2f})", d[0], d[1], d[2]);
        }
        diag::log_line(line);
    }

    // Map the controllers synced at the start of this frame to the game's
    // input action tiers.
    void write_controls() {
        // Once exit is requested every control reads released (09 §3), not
        // the last values latched.
        if (xr_.exit_requested()) {
            controls_.write(input::Controls{}, false);
            return;
        }
        
        const auto& s = xr_.input.state();
        const auto& L = s[Hand::Left];
        const auto& R = s[Hand::Right];
        const bool touchpad = s.family == ControllerFamily::Touchpad;
        if (s.family != ControllerFamily::Unknown && !glyphs_chosen_) {
            glyphs_chosen_ = true;
            controls_.set_scheme(touchpad ? input::ControlScheme::Touchpad : input::ControlScheme::Thumbstick);
        }

        input::Controls c;
        c.touchpad_scheme = touchpad;
        c.trigger_l = L.trigger;
        c.trigger_r = R.trigger;
        c.grip_l = L.squeeze > 0.5f;
        c.grip_r = R.squeeze > 0.5f;
        if (touchpad) {
            // Vive wands: the original touchpad scheme.
            const input::Vec2 pad_r{R.stick.x, R.stick.y};
            const auto regions = input::decode_touchpad(pad_r, R.stick_click);
            c.move = {L.stick.x, L.stick.y};
            c.turn = pad_r;
            c.turn_engaged = R.stick_click;
            c.sprint = L.stick_click;
            c.interact = regions.interact;
            c.reload = regions.reload;
            c.radial = regions.radial;
            c.map = L.menu;
            c.pause = R.menu;
        } else {
            // Thumbsticks with face buttons: the original's Touch layout.
            c.move = {L.stick.x, L.stick.y};
            c.turn = {R.stick.x, R.stick.y};
            c.sprint = L.stick_click;
            c.radial = R.stick_click;
            c.interact = R.primary;
            c.reload = R.secondary;
            c.map = L.primary;
            c.flashlight = L.secondary;
            c.pause = L.menu || R.menu;
        }

        const xr::Pose head = xr_.views().head;
        const bool near_head = R.grip_valid && length(sub(R.grip.position, head.position)) < input::kNearHead;
        controls_.write(c, near_head);
    }

    void set_fov(const std::array<XrFovf, 2>& fov) {
        for (int e = 0; e < 2; ++e) {
            eye_fov_[e] = {std::tan(fov[e].angleUp), std::tan(-fov[e].angleDown),
                           std::tan(-fov[e].angleLeft), std::tan(fov[e].angleRight)};
        }
        max_v_ = std::max({eye_fov_[0].up, eye_fov_[0].down, eye_fov_[1].up, eye_fov_[1].down});
        max_h_ = std::max({eye_fov_[0].left, eye_fov_[0].right, eye_fov_[1].left, eye_fov_[1].right});
        fov_known_ = true;
    }

    void set_default_fov() {
        for (auto& f : eye_fov_) f = {kDefaultTangent, kDefaultTangent, kDefaultTangent, kDefaultTangent};
        max_v_ = max_h_ = kDefaultTangent;
        fov_known_ = false;
    }

    Session&                          xr_;
    core::input::ports::ControlsSink& controls_;
    core::vr::ports::HeadsetAudio&    audio_;
    core::ui::ports::GameSignals&     signals_;
    bool session_ = false;
    bool cinema_ = true;
    bool glyphs_chosen_ = false;
    bool fov_known_ = false;
    FovTangents eye_fov_[2]{};
    float max_v_ = kDefaultTangent, max_h_ = kDefaultTangent;
    // Render thread: each eye's offset from the head, kept while the views
    // have no valid positions.
    xr::Vec3 eye_offset_[2] = {{-kDefaultEyeX, 0, 0}, {kDefaultEyeX, 0, 0}};
    float yaw_ = 0.0f;   // game thread: the latest reference's heading
    bool recenter_taken_ = true;   // game thread: the last head_yaw_pose took a new one
    // Written on the game thread (head_pose, head_yaw_pose), read on the
    // render thread (submit_frame).
    xr::SeqLock<xr::FrameViews> render_;
    xr::SeqLock<xr::Pose>       yaw_ref_;
    // head_pose ran since the last submit_frame: render_ is this frame's.
    std::atomic<bool>           head_read_{false};
    std::uint32_t               hand_log_frames_ = 0;   // render thread
};

} // namespace

std::unique_ptr<core::vr::ports::VrRuntime> make_openxr_runtime(const OpenXrRuntimeDeps& deps) {
    return std::make_unique<OpenXrHmd>(deps);
}

} // namespace omvr::platform::openxr
