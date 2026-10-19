// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/types.h"
#include "core/vr/types.h"
#include "game/constants.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"
#include "util/time/steady.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// Values the hand features work on (spec 21 §3). Poses from the runtime are
// in the convention of the head pose the stereo device returns
// (core/vr/pose_convert.h: to_game_pose, metres times the world scale), so a
// hand and the head of one frame compare directly.
namespace omvr::core::hands {

using input::hand_name;
using input::kLeftHand;
using input::kRightHand;
using util::TimePoint;
using vr::kHandCount;

// One hand as the runtime reported it for a frame (21 §3.1, §16): the grip
// pose, the aim (pointing) pose and the grip's linear velocity. A pose that
// is not valid is never converted or used (03 §1 rule).
struct HandSample {
    util::Matrix34 grip{};
    util::Matrix34 aim{};
    util::Vec3 velocity{}; // game units per second
    bool grip_valid     = false;
    bool aim_valid      = false;
    bool velocity_valid = false;
};

// The head pose the game was given for a frame and both hands of the same
// prediction (21 §3.1, §3.4).
struct HandFrame {
    util::Matrix34 head{};
    std::array<HandSample, kHandCount> hands{};
};

// A pose relative to the eye the player sees from, in the game camera's own
// axes (x right, y up, +z forward): what §3.1 calls d' and Rrel.
struct HeadLocalPose {
    util::Vec3 offset{};
    util::Quat rotation{};
};

// The active camera's world pose (21 §2.4): orientation quaternion (local +Z
// forward, +Y up) and position in world metres.
struct CameraPose {
    util::Quat orientation{};
    util::Vec3 position{};
};

// A pose in the game's world. Local +Z is the direction the pose points.
struct WorldPose {
    util::Vec3 position{};
    util::Quat orientation{};
};

// The weapon classes of 21 §5.2, from the game's weapon type (21 §2.5).
enum class WeaponClass : std::uint8_t {
    NoWeapon, // NO_WEAPON or no weapon object: throwables, flare, medikit
    Barrel,   // PISTOL, SHOTGUN, BOLTGUN
    Nozzle,   // FLAMETHROWER
    Contact,  // MELEE, CATTLEPROD: the game's ray is kept
    Unknown,  // any other value: the game's ray is kept
};

// The player's aim ray as the game keeps it in the weapon component
// (21 §2.5): start, unit direction, end.
struct AimRay {
    util::Vec3 start{};
    util::Vec3 direction{};
    util::Vec3 end{};
};

// The current item as the aim reads it in one frame (21 §2.5, §5.2).
struct WeaponSample {
    std::optional<game::WeaponType> type; // nullopt: no weapon object
    // The item's world transform as the game last placed it (WeaponTransform,
    // 21 §2.5), when it was read this frame.
    std::optional<util::Matrix34> transform;
    // The item is drawn in the hand the arm edit moved (21 §4.2 item 6), so
    // its transform is where the player sees the weapon in the hand.
    bool drawn_in_moved_hand = false;
};

// A line the aim ray is laid along: a start and a unit direction.
struct AimLine {
    util::Vec3 start{};
    util::Vec3 direction{};
};

// The player character's health (21 §2.2, §14.2).
struct Health {
    std::uintptr_t player = 0; // which character: another one (a reload, a new level) starts afresh
    std::uint16_t current = 0;
    std::uint16_t maximum = 0;
};

// What the firing pulse reads in one game frame (21 §14.2).
struct FireSample {
    bool pressed = false;                   // the VR fire control
    std::optional<game::WeaponType> weapon; // the current item's weapon type; nullopt: no weapon
    bool gate_open = false;                 // the fire alignment byte allows firing (21 §2.5)
};

// A bone transform of the game's pose (21 §2.3): translation, rotation
// quaternion (x, y, z, w) and scale, four floats each.
struct BoneTransform {
    std::array<float, 4> translation{};
    std::array<float, 4> rotation{};
    std::array<float, 4> scale{};
};
inline constexpr std::size_t kBoneTransformSize = 0x30;
static_assert(sizeof(BoneTransform) == kBoneTransformSize);

// The arm pose edit (21 §2.3, r9): the character's model-to-world transform
// (read from the animation component, game/hands/arm_objects.h):
// world = T + rotate(Q, S * model), world rotation = Q * model rotation.
struct ModelToWorld {
    util::Vec3 translation{};
    util::Quat rotation{};
    util::Vec3 scale{1.0f, 1.0f, 1.0f};
};

// The game's pose as arrays of the skeleton's size (21 §2.3).
struct PoseArrays {
    std::span<BoneTransform> local;
    std::span<BoneTransform> model;
    std::span<std::uint32_t> flags;
    std::span<const std::int16_t> parents;
};

} // namespace omvr::core::hands
