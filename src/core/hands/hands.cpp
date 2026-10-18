// OpenMotherVR-Provenance: clean
#include "core/hands/hands.h"

#include "core/hands/hand_aim.h"
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
constexpr std::string_view kLogTransform =
    "[diag] hands: weapon transform, weapon type {}: columns ({:+.3f} {:+.3f} {:+.3f}) ({:+.3f} {:+.3f} "
    "{:+.3f}) ({:+.3f} {:+.3f} {:+.3f}), origin ({:.3f} {:.3f} {:.3f}); aim pose forward ({:+.3f} {:+.3f} "
    "{:+.3f}), position ({:.3f} {:.3f} {:.3f}); {}";
constexpr std::string_view kLogAim =
    "[diag] hands: aim ray start ({:.2f} {:.2f} {:.2f}) dir ({:+.2f} {:+.2f} {:+.2f}) weight {:.2f}";

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

// The bit of a weapon type in Hands::transform_logged_; none for a type
// outside the game's enumeration.
std::uint32_t type_bit(std::optional<game::WeaponType> type) noexcept {
    if (!type || *type < game::WeaponType::NoWeapon || *type > game::WeaponType::Cattleprod) return 0;
    return 1U << static_cast<unsigned>(*type);
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
        logf(log_, kLogHand, hand_name(h), change == HandTrack::Change::Lost ? "lost" : "tracked");
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
    return view && view->hands[kRightHand].weight > 0.0f;
}

bool Hands::weapon_transform_wanted(std::optional<game::WeaponType> type, bool drawn_in_moved_hand,
                                    TimePoint now) const noexcept {
    if (!barrel_from_transform(type) || !aim_wanted(now)) return false;
    if (aim_source(type, drawn_in_moved_hand) == AimSource::WeaponTransform) return true;
    return log_.debug_enabled() && (transform_logged_ & type_bit(type)) == 0;
}

// 21 §18 (r11): once per weapon type, with Dev. Debug Options, the
// transform next to the aim pose, so the barrel column and the offsets can
// be read from a tester's log (21 §21 item 5).
void Hands::log_weapon_transform(const WeaponSample& weapon, const HandView& right,
                                 const CameraPose& camera) noexcept {
    const std::uint32_t bit = type_bit(weapon.type);
    if (!weapon.transform || !log_.debug_enabled() || (transform_logged_ & bit) != 0 || !right.has_aim)
        return;
    transform_logged_ |= bit;
    const auto& m          = weapon.transform->m;
    const WorldPose pose   = to_world(camera, right.aim);
    const util::Vec3 ahead = pointing(pose);
    logf(log_, kLogTransform, static_cast<int>(*weapon.type), m[0][0], m[1][0], m[2][0], m[0][1], m[1][1],
         m[2][1], m[0][2], m[1][2], m[2][2], m[0][3], m[1][3], m[2][3], ahead.x, ahead.y, ahead.z,
         pose.position.x, pose.position.y, pose.position.z,
         weapon.drawn_in_moved_hand ? "drawn in the moved hand" : "drawn by the game's animation");
}

Hands::AimResult Hands::aim(const AimRay& game, const CameraPose& camera, const WeaponSample& weapon,
                            TimePoint now) noexcept {
    if (!settings_.enabled || !settings_.aim_from_hand) return {};
    const auto view = fresh_view(now);
    if (!view) return {};
    const WeaponClass c = weapon_class(weapon.type);
    if (class_logged_.changed(c) && log_.debug_enabled()) {
        logf(log_, kLogClass, class_name(c), weapon.type ? static_cast<int>(*weapon.type) : 0);
    }
    const HandView& right = view->hands[kRightHand];
    if (!aims_from_hand(c) || !(right.weight > 0.0f)) return {};
    log_weapon_transform(weapon, right, camera);
    // 21 §5.2 (r11): the weapon's own barrel where it is the source and can
    // be read this frame; the aim pose otherwise.
    std::optional<AimLine> barrel;
    if (aim_source(weapon.type, weapon.drawn_in_moved_hand) == AimSource::WeaponTransform &&
        weapon.transform) {
        barrel = weapon_barrel(*weapon.transform, transform_muzzle_offset(c));
    }
    if (!barrel && !right.has_aim) {
        if (no_aim_noted_.first()) log_.line(kLogNoAim);
        return {};
    }
    AimResult r;
    r.ray         = barrel ? aim_ray_along(game, *barrel, right.weight)
                           : hand_aim_ray(game, to_world(camera, right.aim), right.weight, muzzle_offset(c));
    r.hand_aiming = true;
    if (fire_gate_noted_.first()) log_.line(kLogGate);
    if (aim_log_.tick() && log_.debug_enabled()) {
        const AimRay& a = *r.ray;
        logf(log_, kLogAim, a.start.x, a.start.y, a.start.z, a.direction.x, a.direction.y, a.direction.z,
             right.weight);
    }
    return r;
}

std::optional<util::Vec2> Hands::tracker_facing(const CameraPose& camera,
                                                const std::optional<util::Vec3>& drawn,
                                                TimePoint now) noexcept {
    const auto view      = settings_.enabled && settings_.tracker_in_hand ? fresh_view(now) : std::nullopt;
    const HandView* left = view ? &view->hands[kLeftHand] : nullptr;
    if (!left || !(left->weight > 0.0f) || (!drawn && !left->has_aim)) {
        tracker_.reset();
        return std::nullopt;
    }
    // 21 §6.2 (r11): the drawn left hand when its pose is there, the hand's
    // aim direction otherwise.
    return tracker_.update(drawn ? *drawn : pointing(to_world(camera, left->aim)));
}

} // namespace omvr::core::hands
