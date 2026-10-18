// OpenMotherVR-Provenance: clean
#include "core/hands/hand_aim.h"

#include "core/hands/tuning.h"
#include "util/math/quat.h"

#include <cmath>

namespace omvr::core::hands {

HeadLocalPose head_local(const util::Matrix34& head, const util::Matrix34& hand) noexcept {
    const util::Vec3 d = util::sub(util::translation_of(hand), util::translation_of(head));
    return {util::rotate_inverse(head, d), util::quat_from_rotation(util::relative_rotation(head, hand))};
}

WorldPose to_world(const CameraPose& camera, const HeadLocalPose& local) noexcept {
    return {util::add(camera.position, util::rotate(camera.orientation, local.offset)),
            util::normalized(util::multiply(camera.orientation, local.rotation))};
}

util::Vec3 pointing(const WorldPose& pose) noexcept {
    return util::rotate(pose.orientation, {0.0f, 0.0f, 1.0f});
}

WeaponClass weapon_class(std::optional<game::WeaponType> type) noexcept {
    if (!type) return WeaponClass::NoWeapon;
    switch (*type) {
        case game::WeaponType::NoWeapon: return WeaponClass::NoWeapon;
        case game::WeaponType::Pistol:
        case game::WeaponType::Shotgun:
        case game::WeaponType::Boltgun: return WeaponClass::Barrel;
        case game::WeaponType::Flamethrower: return WeaponClass::Nozzle;
        case game::WeaponType::Melee:
        case game::WeaponType::Cattleprod: return WeaponClass::Contact;
        case game::WeaponType::Unknown: break;
    }
    return WeaponClass::Unknown;
}

bool aims_from_hand(WeaponClass c) noexcept {
    return c == WeaponClass::NoWeapon || c == WeaponClass::Barrel || c == WeaponClass::Nozzle;
}

float muzzle_offset(WeaponClass c) noexcept {
    switch (c) {
        case WeaponClass::Barrel: return kBarrelOffsetMetres;
        case WeaponClass::Nozzle: return kNozzleOffsetMetres;
        default: return 0.0f;
    }
}

AimRay hand_aim_ray(const AimRay& game, const WorldPose& aim, float weight, float offset) noexcept {
    const float range           = util::length(util::sub(game.end, game.start));
    const util::Vec3 hand_dir   = pointing(aim);
    const util::Vec3 hand_start = util::add(aim.position, util::scaled(hand_dir, offset));
    AimRay out;
    out.start          = util::lerp(game.start, hand_start, weight);
    const util::Vec3 d = util::lerp(game.direction, hand_dir, weight);
    const float length = util::length(d);
    // Opposite directions half way blend to nothing: take the nearer one.
    if (length > 0.0f) {
        out.direction = util::scaled(d, 1.0f / length);
    } else {
        out.direction = weight < 1.0f / 2 ? game.direction : hand_dir;
    }
    out.end = util::add(out.start, util::scaled(out.direction, range));
    return out;
}

float fire_gate_threshold(bool hand_aiming, float game_threshold) noexcept {
    return hand_aiming ? kFireGateAlwaysPass : game_threshold;
}

std::optional<util::Vec2> TrackerFacing::update(const util::Vec3& dir) noexcept {
    const float horizontal = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    if (horizontal >= kTrackerMinHorizontal) last_ = util::Vec2{dir.x / horizontal, dir.z / horizontal};
    return last_;
}

} // namespace omvr::core::hands
