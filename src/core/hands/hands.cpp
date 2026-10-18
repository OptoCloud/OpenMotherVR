// OpenMotherVR-Provenance: clean
#include "core/hands/hands.h"

#include "core/hands/aim.h"
#include "core/hands/hand_mapping.h"
#include "core/hands/tuning.h"

#include <algorithm>
#include <string_view>

namespace omvr::core::hands {

namespace {

using diag::ports::logf;

constexpr std::string_view kLogActive   = "hands: tracked hands active";
constexpr std::string_view kLogInactive = "hands: tracked hands inactive ({})";
constexpr std::string_view kLogHand     = "hands: {} hand {}";
constexpr std::string_view kLogClass    = "[diag] hands: aim class {} (weapon type {})";
constexpr std::string_view kLogGate =
    "hands: firing allowed with the weapon away from the head (the fire alignment test passes while the "
    "hand aims)";
constexpr std::string_view kLogNoAim =
    "hands: the runtime gives no aim pose for the right hand; aiming stays on the head";
constexpr std::string_view kLogAim =
    "[diag] hands: aim ray start ({:.2f} {:.2f} {:.2f}) dir ({:+.2f} {:+.2f} {:+.2f}) weight {:.2f}";

const char* side(std::size_t h) noexcept { return h == kLeft ? "left" : "right"; }

const char* class_name(WeaponClass c) noexcept {
    switch (c) {
        case WeaponClass::NoWeapon: return "no weapon (aim pose)";
        case WeaponClass::Barrel: return "barrel";
        case WeaponClass::Nozzle: return "nozzle";
        case WeaponClass::Contact: return "contact (head ray)";
        case WeaponClass::Unknown: return "unknown (head ray)";
    }
    return "unknown";
}

// Seconds between two updates, 0 for the first and clamped to
// kMaxUpdateGap, so a menu or a hitch neither blends nor filters across it.
float seconds_since(const std::optional<TimePoint>& last, TimePoint now) noexcept {
    if (!last) return 0.0f;
    const float dt = util::seconds_between(*last, now).count();
    return std::clamp(dt, 0.0f, kMaxUpdateGap.count());
}

} // namespace

void Hands::note_activity(Activity a) noexcept {
    if (!activity_logged_.changed(a)) return;
    if (a == Activity::Active) {
        log_.line(kLogActive);
    } else if (!is_episode(a) || log_.debug_enabled()) {
        logf(log_, kLogInactive, to_string(a));
    }
}

void Hands::update_hand(std::size_t h, const HandFrame& frame, float dt_s, float world_scale) noexcept {
    const HandSample& s   = frame.hands[h];
    const float smoothing = settings_.smoothing;
    std::optional<HeadLocalPose> grip;
    std::optional<HeadLocalPose> aim;
    if (s.grip_valid) {
        grip = head_local(frame.head, grip_filter_[h].apply(s.grip, dt_s, smoothing, world_scale));
    } else {
        grip_filter_[h].reset();
    }
    if (s.grip_valid && s.aim_valid) {
        aim = head_local(frame.head, aim_filter_[h].apply(s.aim, dt_s, smoothing, world_scale));
    } else {
        aim_filter_[h].reset();
    }
    const auto change = track_[h].update(Seconds{dt_s}, grip, aim);
    if (change != HandTrack::Change::None) {
        logf(log_, kLogHand, side(h), change == HandTrack::Change::Lost ? "lost" : "tracked");
    }
}

void Hands::update(const HandFrame& frame, const ActivityInput& in, TimePoint now,
                   float world_scale) noexcept {
    if (!in.enabled) {
        // v1: nothing runs. Said once when the key goes off in play.
        if (!off_noted_) note_activity(Activity::Off);
        off_noted_ = true;
        if (last_update_) {
            last_update_.reset();
            track_ = {};
            activity_.set(0.0f);
            view_.store(View{});
        }
        return;
    }
    off_noted_       = false;
    const float dt_s = seconds_since(last_update_, now);
    last_update_     = now;
    const Activity a = activity(in);
    note_activity(a);
    for (std::size_t h = 0; h < kHandCount; ++h) update_hand(h, frame, dt_s, world_scale);
    publish(now, activity_.step(Seconds{dt_s}, a == Activity::Active));
}

void Hands::publish(TimePoint now, float activity) noexcept {
    View v;
    v.stamp = util::stamp_ticks(now);
    for (std::size_t h = 0; h < kHandCount; ++h) {
        const HandTrack& t = track_[h];
        v.hands[h]         = {t.grip(), t.aim(), t.weight() * activity, t.has_grip(), t.has_aim()};
    }
    view_.store(v);
}

std::optional<Hands::View> Hands::fresh_view(TimePoint now) const noexcept {
    const View v     = view_.load();
    const auto stamp = util::from_stamp_ticks(v.stamp);
    if (!stamp || now > util::after(*stamp, kStaleAfter)) return std::nullopt;
    return v;
}

bool Hands::aim_wanted(TimePoint now) const noexcept {
    if (!settings_.enabled || !settings_.aim_from_hand) return false;
    const auto view = fresh_view(now);
    return view && view->hands[kRight].weight > 0.0f;
}

Hands::AimResult Hands::aim(const AimRay& game, const CameraPose& camera,
                            std::optional<game::WeaponType> type, TimePoint now) noexcept {
    if (!settings_.enabled || !settings_.aim_from_hand) return {};
    const auto view = fresh_view(now);
    if (!view) return {};
    const WeaponClass c = weapon_class(type);
    if (class_logged_.changed(c) && log_.debug_enabled()) {
        logf(log_, kLogClass, class_name(c), type ? static_cast<int>(*type) : 0);
    }
    const HandView& right = view->hands[kRight];
    if (!aims_from_hand(c) || !(right.weight > 0.0f)) return {};
    if (!right.has_aim) {
        if (!no_aim_noted_) log_.line(kLogNoAim);
        no_aim_noted_ = true;
        return {};
    }
    AimResult r;
    r.ray         = hand_aim_ray(game, to_world(camera, right.aim), right.weight, muzzle_offset(c));
    r.hand_aiming = true;
    if (!fire_gate_noted_) log_.line(kLogGate);
    fire_gate_noted_ = true;
    if (aim_log_.tick() && log_.debug_enabled()) {
        const AimRay& a = *r.ray;
        logf(log_, kLogAim, a.start.x, a.start.y, a.start.z, a.direction.x, a.direction.y, a.direction.z,
             right.weight);
    }
    return r;
}

std::optional<util::Vec2> Hands::tracker_facing(const CameraPose& camera, TimePoint now) noexcept {
    const auto view = settings_.enabled && settings_.tracker_in_hand ? fresh_view(now) : std::nullopt;
    if (!view || !(view->hands[kLeft].weight > 0.0f) || !view->hands[kLeft].has_aim) {
        tracker_.reset();
        return std::nullopt;
    }
    return tracker_.update(pointing(to_world(camera, view->hands[kLeft].aim)));
}

} // namespace omvr::core::hands
