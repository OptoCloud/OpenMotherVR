// The OpenXR HMD backend: the stereo device on any OpenXR runtime (Meta /
// Quest Link, SteamVR, WMR, Pico, Varjo...). Not in the original (09).
//
// The game creates the stereo device before its D3D11 device exists, so the
// instance and system come up at init (with the FOV where the runtime
// reports it through XR_EPIC_view_configuration_fov) and the session when the
// game configures rendering with its device (09 §2.2).

#include "platform/openxr/openxr_runtime.h"

#include "core/diag/tuning.h"
#include "core/input/controller_mapping.h"
#include "core/stereo/tuning.h"
#include "core/vr/fov.h"
#include "core/vr/hand_offsets.h"
#include "core/vr/heading.h"
#include "core/vr/pose_convert.h"
#include "core/vr/space_change.h"
#include "core/vr/tuning.h"
#include "platform/d3d11/handles.h"
#include "platform/openxr/session.h"
#include "platform/openxr/xr_headers.h"
#include "util/math/pose.h"
#include "util/math/vec.h"
#include "util/sync/frame_period.h"
#include "util/sync/seqlock.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace omvr::platform::openxr {

namespace {

namespace input = core::input;
using core::diag::ports::logf;
using core::stereo::kCinemaDistance;
using core::stereo::kCinemaWidth;
using core::vr::FovAngles;
using core::vr::FovTangents;
using core::vr::HeadsetDesc;
using core::vr::kDefaultEyeX;
using core::vr::kDefaultTangent;
using core::vr::kEyeCount;
using core::vr::kHandCount;
using core::vr::RenderTangents;
using core::vr::TextureSize;
using core::vr::to_game_pose;
using core::vr::to_game_velocity;
using util::Matrix34;
using util::Pose;
using util::pose_matrix;
using util::Vec3;

// Leads this runtime's "frames now" line.
constexpr std::string_view kLogSource   = "openxr";
constexpr std::string_view kLogTangents = "openxr: render tangents v={:.3f} h={:.3f}{}";
constexpr std::string_view kLogSession  = "openxr: session {}";
constexpr std::string_view kLogRecenterSkipped =
    "openxr: recenter skipped: head not tracked, the previous reference stays";
constexpr std::string_view kLogHandsPrefix = "openxr: hands (head-local m, +x right +y up -z fwd):";

// OpenXR's forward axis (09 §5.2: the screen sits along the reference's
// forward direction).
constexpr Vec3 kForward{0, 0, -1};

bool valid_hand(int hand) noexcept { return hand >= 0 && static_cast<std::size_t>(hand) < kHandCount; }

// One hand as the controller schemes read it (04 §5.8.5). A thumbstick is
// always "touched"; on Vive wands the stick is the touchpad, which OpenXR
// reports as a position whether touched or not.
input::HandInput hand_input(const HandState& h) noexcept {
    return {.stick         = h.stick,
            .stick_touched = true,
            .stick_click   = h.stick_click,
            .trigger       = h.trigger,
            .grip          = input::squeeze_pressed(h.squeeze),
            .primary       = h.primary,
            .secondary     = h.secondary,
            .menu          = h.menu,
            .dpad          = h.dpad};
}

// The semantic inputs of the scheme the controllers use (04 §5.2, §5.3).
input::Controls map_controls(const ControllerState& s) noexcept {
    const input::HandInput left  = hand_input(s[Hand::Left]);
    const input::HandInput right = hand_input(s[Hand::Right]);
    return s.family == ControllerFamily::Touchpad ? input::touchpad_controls(left, right)
                                                  : input::thumbstick_controls(left, right);
}

// ---- The runtime -------------------------------------------------------------------

class OpenXrRuntime final : public core::vr::ports::VrRuntime {
public:
    explicit OpenXrRuntime(const OpenXrRuntimeDeps& deps) noexcept
        : log_(deps.log),
          xr_(deps.session),
          controls_(deps.controls),
          audio_(deps.audio),
          signals_(deps.signals),
          aim_poses_(deps.aim_poses),
          haptics_(deps.haptics) {}

    // Ends the session and destroys the instance. Only reached through the
    // stereo device's explicit teardown, never at process exit.
    ~OpenXrRuntime() override { xr_.shutdown(); }

    bool init() override {
        xr_.set_aim_poses(aim_poses_);
        xr_.set_haptics(haptics_ != nullptr);
        if (!xr_.init_system()) return false;
        const auto& info = xr_.system_info();
        if (info.has_fov)
            set_fov(info.fov);
        else
            set_default_fov();
        audio_.redirect_to(info.audio_output);
        return true;
    }

    void headset_desc(HeadsetDesc& desc) override {
        for (std::size_t e = 0; e < kEyeCount; ++e) desc.default_eye_fov[e] = eye_fov_[e];
        desc.max_eye_fov[0] = desc.max_eye_fov[1] = core::vr::symmetric_tangents(render_);
        const auto& info                          = xr_.system_info();
        desc.product_name                         = info.system_name;
        desc.manufacturer                         = info.runtime_name;
        logf(log_, kLogTangents, render_.vertical, render_.horizontal, fov_known_ ? "" : " (provisional)");
    }

    // Without the FOV extension the FOV becomes known with the first located
    // views (09 §4.2).
    bool refresh_headset_desc(HeadsetDesc& desc) override {
        if (fov_known_) return false;
        const FrameViews views = xr_.views();
        if (!views.valid) return false;
        set_fov({views.eyes[0].fov, views.eyes[1].fov});
        headset_desc(desc);
        return true;
    }

    TextureSize eye_texture_size(int, float pixel_scale) override {
        const auto& info = xr_.system_info();
        return core::vr::scaled_size(info.recommended_w, info.recommended_h, pixel_scale);
    }

    // The session is created on the game's device (09 §2.2).
    bool create_render_resources(core::vr::GpuDevice gpu, core::vr::GpuContext) override {
        if (session_) return true;
        if (gpu == core::vr::GpuDevice{}) return false;
        session_ = xr_.init(gpu);
        logf(log_, kLogSession, session_ ? "created on the game's device" : "failed");
        return session_;
    }

    void release_render_resources() override {}

    // Game thread. The views are those of the frame the render thread began
    // at the end of the previous EndFrame, predicted for this frame's own
    // display time; submit_frame declares exactly these (09 §5.3).
    void head_pose(Matrix34& out, float world_scale) override {
        const FrameViews v = xr_.views();
        render_views_.store(v); // what the game renders this frame with
        head_read_.store(true, std::memory_order_relaxed);
        floor_ = floor_of(v);
        out    = game_pose(v.head, world_scale);
    }

    // Game thread. The grip poses located with the views head_pose handed
    // out this frame, so hands and head share one display time.
    bool hand_pose(int hand, Matrix34& out, float world_scale) override {
        if (!valid_hand(hand)) return false;
        const HandPose h = hand_state(hand);
        if (!h.valid) return false;
        out = game_pose(h.pose, world_scale);
        return true;
    }

    bool hand_velocity(int hand, float (&out)[3], float world_scale) override {
        if (!valid_hand(hand)) return false;
        const HandPose h = hand_state(hand);
        if (!h.velocity_valid) return false;
        to_game_velocity({h.velocity.x, h.velocity.y, h.velocity.z}, world_scale, out);
        return true;
    }

    // Game thread. The aim pose located with the grip (21 §5.2, §16).
    bool aim_pose(int hand, Matrix34& out, float world_scale) override {
        if (!valid_hand(hand)) return false;
        const HandPose h = hand_state(hand);
        if (!h.aim_valid) return false;
        out = game_pose(h.aim, world_scale);
        return true;
    }

    void head_yaw_pose(Matrix34& out, float world_scale) override {
        const FrameViews v = xr_.views();
        floor_             = floor_of(v);
        // Never from an invalid head (09 §5.5): the previous reference stays
        // (identity before any). A queued retake waits for headset_in_use,
        // which needs a valid head, so it happens once tracking is back.
        recenter_taken_ = v.head_valid;
        if (!v.head_valid) {
            out = game_pose(yaw_ref_.load(), world_scale);
            log_.line(kLogRecenterSkipped);
            return;
        }
        // Looking straight up or down leaves no heading: keep the previous yaw.
        yaw_ = core::vr::heading_of(v.head.orientation, yaw_).yaw_rad;
        const Pose ref{core::vr::yaw_rotation(yaw_), v.head.position};
        yaw_ref_.store(ref);
        out = game_pose(ref, world_scale);
    }

    void begin_frame(float eye_offsets[6], bool head_tracked) override {
        update_eye_offsets();
        constexpr std::size_t kAxes = 3;
        for (std::size_t e = 0; e < kEyeCount; ++e) {
            eye_offsets[e * kAxes + 0] = eye_offset_[e].x;
            eye_offsets[e * kAxes + 1] = eye_offset_[e].y;
            eye_offsets[e * kAxes + 2] = eye_offset_[e].z;
        }
        // Once per change: always until the first tracked frame, then with
        // Dev. Debug Options (11 §4.5).
        frame_kind_log_.log(log_, head_tracked, kLogSource);
        cinema_ = !head_tracked;
    }

    void submit_frame(core::vr::GpuTexture texture) override {
        if (!session_ || texture == core::vr::GpuTexture{}) return;
        const TextureExtent extent = texture_extent(texture);

        const FrameViews rendered = render_views_.load();
        head_read_.store(false, std::memory_order_relaxed);
        if (!cinema_ && rendered.valid)
            present_eyes(texture, extent, rendered);
        else
            present_screen(texture, extent);

        // Ends this frame (copies, flush, xrEndFrame), then blocks until the
        // runtime wants the next one and begins it: a running start, as
        // OpenVR's WaitGetPoses (09 §5.3). The game thread then reads the
        // new views.
        const bool begun = xr_.frame(core::stereo::SwapchainHandle{});
        if (hand_log_.tick() && log_.debug_enabled()) log_hands();
        // The controllers were synced as the next frame began; hand them to
        // the game now, before its next simulation step (P10). With no frame
        // begun (session not running, lost) nothing was synced: the game
        // sees every VR control released, never the last synced state
        // (04 §5.12, 09 §3).
        if (begun)
            write_controls();
        else
            controls_.write(input::Controls{}, false);
        if (begun) play_haptics();
    }

    // 21 §14.3: this frame's vibration, right after the controls: nothing
    // without input focus (the runtime's dashboard) or for a hand whose
    // grip is not tracked.
    void play_haptics() {
        if (!haptics_) return;
        const ControllerState& s = xr_.controllers();
        xr_.play_haptics(haptics_->next(s.focused, {s.hands[0].grip_valid, s.hands[1].grip_valid}));
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

    // 22 §13: the game sees every VR control released, then the session
    // ends in API order and the instance goes.
    void stop() override {
        controls_.write(input::Controls{}, false);
        session_ = false;
        xr_.end_on_request();
    }

    // 22 §6.1: the floor space located with the views head_pose or
    // head_yaw_pose used.
    bool has_floor() const override { return xr_.has_floor(); }
    std::optional<float> floor_height() override { return floor_; }

    std::optional<float> head_height_above_floor() override {
        const FrameViews v = xr_.views();
        if (!v.head_valid) return std::nullopt;
        const std::optional<float> floor = floor_of(v);
        if (!floor) return std::nullopt;
        return v.head.position.y - *floor;
    }

    bool headset_in_use() override {
        if (!xr_.running()) return false;
        const FrameViews v = xr_.views();
        if (!v.valid || !v.head_valid) return false; // a recenter needs a tracked head (09 §6.2)
        return core::vr::is_upright(core::vr::up_y_of(v.head.orientation));
    }

private:
    // The size of the side-by-side texture and of each eye's half.
    struct TextureExtent {
        std::uint32_t height     = 0;
        std::uint32_t half_width = 0;
    };

    static TextureExtent texture_extent(core::vr::GpuTexture texture) noexcept {
        D3D11_TEXTURE2D_DESC d{};
        d3d11::to_d3d(texture)->GetDesc(&d);
        return {d.Height, d.Width / 2};
    }

    std::optional<float> floor_of(const FrameViews& v) const noexcept {
        if (!xr_.has_floor() || !v.floor_valid) return std::nullopt;
        return v.floor_y;
    }

    static Matrix34 game_pose(const Pose& p, float world_scale) noexcept {
        float m[3][4];
        pose_matrix(p, m);
        return to_game_pose(m, world_scale);
    }

    // The hands of the frame the game is rendering: the views head_pose
    // took this frame, else (no head pose read since the last submit:
    // menus, or a call before head_pose) the newest.
    HandPose hand_state(int hand) const {
        const FrameViews v = head_read_.load(std::memory_order_relaxed) ? render_views_.load() : xr_.views();
        return v.hands[static_cast<std::size_t>(hand)];
    }

    // Render thread. Only from views whose eye and head positions are this
    // frame's; otherwise the previous offsets stay (09 §5.4, §5.5), the
    // default half IPD until then.
    void update_eye_offsets() {
        const FrameViews v = xr_.views();
        if (!(v.valid && v.eye_positions_valid && v.head_valid)) return;
        for (std::size_t e = 0; e < kEyeCount; ++e) {
            eye_offset_[e] = util::rotate(util::conjugate(v.head.orientation),
                                          util::sub(v.eyes[e].pose.position, v.head.position));
        }
    }

    // Both halves as a projection layer, declared with the pose and the
    // symmetric FOV the game rendered them with (09 §5.2).
    void present_eyes(core::vr::GpuTexture texture, TextureExtent extent, const FrameViews& rendered) {
        EyeImages images;
        images.rendered_for = rendered.display_time;
        const FovAngles fov = core::vr::symmetric_angles(render_);
        for (std::size_t e = 0; e < kEyeCount; ++e) {
            const auto left   = static_cast<std::uint32_t>(e) * extent.half_width;
            images.texture[e] = texture;
            images.region[e]  = {left, 0, left + extent.half_width, extent.height};
            images.view[e]    = {rendered.eyes[e].pose, fov};
        }
        xr_.set_eye_images(images);
        xr_.set_screen_image(core::vr::GpuTexture{}, {});
    }

    // Menus, videos, loading: the left eye's image on a screen in front of
    // where the head was facing at the last recenter (03 §7, 09 §5.2).
    void present_screen(core::vr::GpuTexture texture, TextureExtent extent) {
        xr_.clear_eye_images();
        xr_.set_screen_image(
            texture, {0, extent.height * core::vr::kScreenTopQuarters / core::vr::kScreenHeightQuarters,
                      extent.half_width,
                      extent.height * core::vr::kScreenBottomQuarters / core::vr::kScreenHeightQuarters});
        const Pose yaw_ref = yaw_ref_.load();
        const auto& q      = yaw_ref.orientation;
        const Vec3 forward = util::rotate(q, kForward);
        xr_.set_screen_pose({q,
                             {yaw_ref.position.x + forward.x * kCinemaDistance, yaw_ref.position.y,
                              yaw_ref.position.z + forward.z * kCinemaDistance}},
                            kCinemaWidth);
    }

    // Dev. Debug Options: each hand's offset from the head, so moving the
    // controllers can be checked against the log (11 §4.2). Render thread.
    void log_hands() {
        const FrameViews v = xr_.views();
        float head[3][4];
        pose_matrix(v.head, head);
        core::vr::HandLogLine line(kLogHandsPrefix);
        for (std::size_t h = 0; h < kHandCount; ++h) {
            line.begin_hand(h);
            if (!v.hands[h].valid) {
                const auto& in = xr_.controllers().hands[h];
                line.append("untracked (actions {}, grip {})", in.active ? "active" : "inactive",
                            in.grip_valid ? "located" : "not located");
                continue;
            }
            float hand[3][4], d[3];
            pose_matrix(v.hands[h].pose, hand);
            core::vr::head_local_offset(head, hand, d);
            line.offset(d);
        }
        log_.line(line.text());
    }

    // Map the controllers synced at the start of this frame to the game's
    // input action tiers (04 §5.8.5).
    void write_controls() {
        // Once exit is requested every control reads released (09 §3), not
        // the last values latched.
        if (xr_.exit_requested()) {
            controls_.write(input::Controls{}, false);
            return;
        }
        const ControllerState& s = xr_.controllers();
        if (s.profile_generation != profiles_seen_) {
            profiles_seen_ = s.profile_generation;
            for (std::size_t h = 0; h < s.profiles.size(); ++h)
                controls_.identify_controller(h, input::ControllerSource::OpenXr, s.profiles[h].data());
        }
        if (s.family != ControllerFamily::Unknown && !glyphs_chosen_) {
            glyphs_chosen_ = true;
            controls_.set_scheme(s.family == ControllerFamily::Touchpad ? input::ControlScheme::Touchpad
                                 : s.family == ControllerFamily::Frame  ? input::ControlScheme::Frame
                                                                        : input::ControlScheme::Thumbstick);
        }
        input::Controls c = map_controls(s);
        // The right hand near the head turns Interact into the flashlight
        // (04 §5.10).
        const auto& R        = s[Hand::Right];
        const auto views     = xr_.views();
        const Pose head      = views.head;
        const bool near_head = R.grip_valid && input::near_head(R.grip.position, head.position);
        c.poses              = body_poses(s, head, views.head_valid);
        controls_.write(c, near_head);
    }

    // 22 §8.1: the head of the located views and the hands synced with
    // them, in the seated space (metres before the world scale), for the
    // gestures.
    static input::BodyPoses body_poses(const ControllerState& s, const Pose& head, bool head_valid) noexcept {
        input::BodyPoses b;
        b.focused    = s.focused;
        b.head       = head.position;
        b.head_yaw   = input::head_yaw(util::rotate(head.orientation, {0.0f, 0.0f, -1.0f}),
                                       util::rotate(head.orientation, {0.0f, 1.0f, 0.0f}));
        b.head_valid = head_valid;
        for (std::size_t h = 0; h < kHandCount; ++h) {
            b.hands[h]      = s.hands[h].grip.position;
            b.hand_valid[h] = s.hands[h].grip_valid;
        }
        return b;
    }

    void set_fov(const std::array<FovAngles, kEyeCount>& fov) {
        for (std::size_t e = 0; e < kEyeCount; ++e) eye_fov_[e] = core::vr::tangents_from_angles(fov[e]);
        render_    = core::vr::render_tangents(eye_fov_[0], eye_fov_[1]);
        fov_known_ = true;
    }

    // The provisional FOV until the runtime reports one (09 §4.2).
    void set_default_fov() {
        for (auto& f : eye_fov_) f = {kDefaultTangent, kDefaultTangent, kDefaultTangent, kDefaultTangent};
        render_    = {kDefaultTangent, kDefaultTangent};
        fov_known_ = false;
    }

    core::diag::ports::Log& log_;
    Session& xr_;
    core::input::ports::ControlsSink& controls_;
    core::vr::ports::HeadsetAudio& audio_;
    core::ui::ports::GameSignals& signals_;
    const bool aim_poses_; // the hands' aim pose action (spec 21 §16)
    // The haptic output (21 §14); null: off.
    core::vr::ports::HapticSource* const haptics_;
    bool session_       = false;
    bool cinema_        = true;
    bool glyphs_chosen_ = false;
    bool fov_known_     = false;
    core::vr::FrameKindLog frame_kind_log_; // render thread
    // Render thread: the profile changes handed to identify_controller (22 §10.1).
    unsigned profiles_seen_ = 0;
    FovTangents eye_fov_[kEyeCount]{};
    RenderTangents render_{kDefaultTangent, kDefaultTangent};
    // Render thread: each eye's offset from the head, kept while the views
    // have no valid positions.
    Vec3 eye_offset_[kEyeCount] = {{-kDefaultEyeX, 0, 0}, {kDefaultEyeX, 0, 0}};
    float yaw_                  = 0.0f; // game thread: the latest reference's heading
    bool recenter_taken_        = true; // game thread: the last head_yaw_pose took a new one
    std::optional<float> floor_;        // game thread: the floor of those views (22 §6.1)
    // Written on the game thread (head_pose, head_yaw_pose), read on the
    // render thread (submit_frame).
    util::SeqLock<FrameViews> render_views_;
    util::SeqLock<Pose> yaw_ref_;
    // head_pose ran since the last submit_frame: render_views_ is this frame's.
    std::atomic<bool> head_read_{false};
    util::FramePeriod hand_log_{core::diag::kDebugLogPeriodFrames}; // render thread
};

} // namespace

std::unique_ptr<core::vr::ports::VrRuntime> make_openxr_runtime(const OpenXrRuntimeDeps& deps) {
    return std::make_unique<OpenXrRuntime>(deps);
}

} // namespace omvr::platform::openxr
