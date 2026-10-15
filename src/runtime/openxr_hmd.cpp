// The OpenXR HMD backend: the stereo device on any OpenXR runtime (Meta /
// Quest Link, SteamVR, WMR, Pico, Varjo...). Not in the original.
//
// The game creates the stereo device before its D3D11 device exists, so the
// instance and system come up at init (with the FOV where the runtime
// reports it through XR_EPIC_view_configuration_fov) and the session when the
// game configures rendering with its device.

#include "hmd_backend.h"

#include "audio/audio_redirect.h"
#include "config/config.h"
#include "diag/log.h"
#include "input/gamepad.h"
#include "input/vr_controls.h"
#include "integration/script_bridge.h"
#include "openxr_backend.h"
#include "openxr_input.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <string>

namespace mothervr::runtime {

namespace {

namespace xr = openxr;

constexpr float kDefaultTangent = 1.2f;          // until the runtime tells us
constexpr float kDefaultEyeX    = 0.032f;
// Below this horizontal length of the head's forward axis the heading is
// undefined (looking straight up or down); the previous yaw is kept (09 §5.5).
constexpr float kMinForwardLength = 1e-4f;

// ---- Small vector maths --------------------------------------------------------

xr::Vec3 rotate(const xr::Quat& q, const xr::Vec3& v) noexcept {
    // v + 2w(q x v) + 2 q x (q x v)
    const xr::Vec3 u{q.x, q.y, q.z};
    const xr::Vec3 t{2 * (u.y * v.z - u.z * v.y), 2 * (u.z * v.x - u.x * v.z), 2 * (u.x * v.y - u.y * v.x)};
    return {v.x + q.w * t.x + (u.y * t.z - u.z * t.y),
            v.y + q.w * t.y + (u.z * t.x - u.x * t.z),
            v.z + q.w * t.z + (u.x * t.y - u.y * t.x)};
}

xr::Quat conjugate(const xr::Quat& q) noexcept { return {-q.x, -q.y, -q.z, q.w}; }

xr::Vec3 sub(const xr::Vec3& a, const xr::Vec3& b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

float length(const xr::Vec3& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// Row-major 3x4 of a pose (the OpenVR/OpenXR axis convention: +Y up, -Z forward).
void pose_matrix(const xr::Pose& p, float (&m)[3][4]) noexcept {
    const auto& q = p.orientation;
    m[0][0] = 1 - 2 * (q.y * q.y + q.z * q.z); m[0][1] = 2 * (q.x * q.y - q.z * q.w); m[0][2] = 2 * (q.x * q.z + q.y * q.w);
    m[1][0] = 2 * (q.x * q.y + q.z * q.w); m[1][1] = 1 - 2 * (q.x * q.x + q.z * q.z); m[1][2] = 2 * (q.y * q.z - q.x * q.w);
    m[2][0] = 2 * (q.x * q.z - q.y * q.w); m[2][1] = 2 * (q.y * q.z + q.x * q.w); m[2][2] = 1 - 2 * (q.x * q.x + q.y * q.y);
    m[0][3] = p.position.x;
    m[1][3] = p.position.y;
    m[2][3] = p.position.z;
}

float yaw_of(const xr::Quat& q) noexcept {
    return std::atan2(2 * (q.w * q.y + q.x * q.z), 1 - 2 * (q.x * q.x + q.y * q.y));
}

// ---- The backend -------------------------------------------------------------------

class OpenXrHmd final : public HmdBackend {
public:
    // Ends the session and destroys the instance. Only reached through the
    // stereo device's explicit teardown, never at process exit.
    ~OpenXrHmd() override { xr::shutdown(); }

    bool init() override {
        if (!xr::init_system()) return false;
        const auto& info = xr::system_info();
        if (info.has_fov) set_fov(info.fov);
        else set_default_fov();
        audio::redirect_to(info.audio_output);
        return true;
    }

    void get_hmd_desc(HmdDesc& desc) override {
        for (int e = 0; e < 2; ++e) desc.default_eye_fov[e] = eye_fov_[e];
        desc.max_eye_fov[0] = desc.max_eye_fov[1] = FovPort{max_v_, max_v_, max_h_, max_h_};
        const auto& info = xr::system_info();
        desc.product_name = info.system_name;
        desc.manufacturer = info.runtime_name;
        diag::logf("openxr: render tangents v={:.3f} h={:.3f}{}", max_v_, max_h_,
                   fov_known_ ? "" : " (provisional)");
    }

    bool refresh_hmd_desc(HmdDesc& desc) override {
        if (fov_known_) return false;
        const xr::FrameViews views = xr::views();
        if (!views.valid) return false;
        const auto& v = views.eyes;
        set_fov({XrFovf{v[0].angle_left, v[0].angle_right, v[0].angle_up, v[0].angle_down},
                 XrFovf{v[1].angle_left, v[1].angle_right, v[1].angle_up, v[1].angle_down}});
        get_hmd_desc(desc);
        return true;
    }

    Sizei eye_texture_size(int, float pixel_scale) override {
        const auto& info = xr::system_info();
        return {static_cast<std::int32_t>(static_cast<float>(info.recommended_w) * pixel_scale),
                static_cast<std::int32_t>(static_cast<float>(info.recommended_h) * pixel_scale)};
    }

    bool create_render_resources(ID3D11Device* device, ID3D11DeviceContext*) override {
        if (session_) return true;
        if (!device) return false;
        session_ = xr::init(device).ok;
        diag::logf("openxr: session {}", session_ ? "created on the game's device" : "failed");
        return session_;
    }

    void release_render_resources() override {}

    // Game thread. The views are those of the frame the render thread began
    // at the end of the previous EndFrame, predicted for this frame's own
    // display time; submit_frame declares exactly these.
    void head_pose(Matrix34& out, float world_scale) override {
        const xr::FrameViews v = xr::views();
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
        const xr::FrameViews v = xr::views();
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
        const xr::FrameViews v = xr::views();
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

    void submit_frame(ID3D11Texture2D* side_by_side) override {
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
            xr::set_eye_images(images);
            xr::set_screen_image(nullptr, {});
        } else {
            // Menus, videos, loading: the left eye's image on a screen 1.5 m
            // in front of where the head was facing at the last recenter.
            xr::clear_eye_images();
            xr::set_screen_image(side_by_side, {0, d.Height / 4, 0, half, d.Height * 3 / 4, 1});
            const xr::Pose yaw_ref = yaw_ref_.load();
            const auto& q = yaw_ref.orientation;
            const xr::Vec3 forward = rotate(q, {0, 0, -1});
            xr::set_screen_pose({q, {yaw_ref.position.x + forward.x * kCinemaDistance, yaw_ref.position.y,
                                     yaw_ref.position.z + forward.z * kCinemaDistance}},
                                kCinemaWidth);
        }
        // Ends this frame (copies, flush, xrEndFrame), then blocks until the
        // runtime wants the next one and begins it: a running start, as
        // OpenVR's WaitGetPoses. The game thread then reads the new views.
        xr::frame(nullptr);
        log_hands();
        // The controllers were synced as the next frame began; hand them to
        // the game now, before its next simulation step (P10).
        write_controls();
    }

    void register_control_hints() override { input::set_control_scheme(input::ControlScheme::Thumbstick); }

    void poll_input() override {
        // Quit when the runtime asks, like the original on ovr ShouldQuit.
        // From: FUN_10004960.
        if (xr::exit_requested() && integration::g_exit_request) *integration::g_exit_request = 1;
        // The controllers themselves go to the game at the end of submit_frame,
        // where they are synced, one frame earlier than BeginFrame would.
    }

    bool recenter_taken() const override { return recenter_taken_; }

    bool headset_in_use() override {
        if (!xr::running()) return false;
        const xr::FrameViews v = xr::views();
        if (!v.valid || !v.head_valid) return false;   // a recenter needs a tracked head (09 §6.2)
        const auto& q = v.head.orientation;
        return 1 - 2 * (q.x * q.x + q.z * q.z) > 0.7f;   // head's up axis near vertical
    }

private:
    // The hands of the frame the game is rendering: the views head_pose
    // took this frame, else (no head pose read since the last submit:
    // menus, or a call before head_pose) the newest.
    xr::HandPose hand_state(int hand) const {
        return (head_read_.load(std::memory_order_relaxed) ? render_.load() : xr::views()).hands[hand];
    }

    // Dev. Debug Options: each hand's offset from the head, so moving the
    // controllers can be checked against the log. Render thread.
    void log_hands() {
        if (++hand_log_frames_ < kHandLogFrames) return;
        hand_log_frames_ = 0;
        if (!config::current().debug.debug) return;
        const xr::FrameViews v = xr::views();
        float head[3][4];
        pose_matrix(v.head, head);
        std::string line = "openxr: hands (head-local m, +x right +y up -z fwd):";
        for (int h = 0; h < 2; ++h) {
            line += h == 0 ? " L " : " R ";
            if (!v.hands[h].valid) {
                const auto& in = xr::input::state().hands[h];
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
        if (xr::exit_requested()) return;
        namespace xi = xr::input;
        const auto& s = xi::state();
        const auto& L = s[xi::Hand::Left];
        const auto& R = s[xi::Hand::Right];
        const bool touchpad = s.family == xi::ControllerFamily::Touchpad;
        if (s.family != xi::ControllerFamily::Unknown && !glyphs_chosen_) {
            glyphs_chosen_ = true;
            input::set_control_scheme(touchpad ? input::ControlScheme::Touchpad : input::ControlScheme::Thumbstick);
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

        const xr::Pose head = xr::views().head;
        const bool near_head = R.grip_valid && length(sub(R.grip.position, head.position)) < input::kNearHead;
        input::write_controls(c, near_head);
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

    bool session_ = false;
    bool cinema_ = true;
    bool glyphs_chosen_ = false;
    bool fov_known_ = false;
    FovPort eye_fov_[2]{};
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

std::unique_ptr<HmdBackend> make_openxr_backend() {
    return std::make_unique<OpenXrHmd>();
}

} // namespace mothervr::runtime
