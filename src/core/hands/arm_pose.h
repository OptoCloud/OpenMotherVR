// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/body_visibility.h"
#include "core/hands/types.h"
#include "util/math/vec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// The arm pose edit of the tracked hands (spec 21 §2.3, §3.5, §4.2): the
// model-to-world transform, the grip frame derived from the hand's finger
// bones, the two-bone reach of the arm with the shoulder's extra turn and
// the forearm's roll split, and the edit of the game's pose in place (its
// local and model-space bone transforms and their flags). Pure: the game
// side hands the pose in as spans; nothing here allocates, so the job
// thread that runs the player's final-pose step calls it (21 §17).
namespace omvr::core::hands {

// ---- Transforms ------------------------------------------------------------------

// A bone's rigid part: translation and rotation. The scale is left as the
// game has it.
struct RigidTransform {
    util::Vec3 translation{};
    util::Quat rotation{};
};

// The bone transform's rigid part, and writing one back (the scale and the
// translation's fourth float are kept).
RigidTransform rigid_of(const BoneTransform& b) noexcept;
void set_rigid(BoneTransform& b, const RigidTransform& r) noexcept;

// parent * local, as the game's (Havok's) pose composes model space: the
// child's translation rotated by the parent, not scaled; scales multiply.
BoneTransform compose(const BoneTransform& parent, const BoneTransform& local) noexcept;
// The local transform that composes with `parent` to `model`.
BoneTransform relative(const BoneTransform& parent, const BoneTransform& model) noexcept;

// 21 §2.3 (r9): ModelToWorld (types.h) maps model space to the world:
// world = T + rotate(Q, S * model), world rotation = Q * model rotation.
util::Vec3 to_world_point(const ModelToWorld& w, const util::Vec3& model) noexcept;
util::Vec3 to_model_point(const ModelToWorld& w, const util::Vec3& world) noexcept;
util::Vec3 to_model_direction(const ModelToWorld& w, const util::Vec3& world) noexcept;
RigidTransform to_model(const ModelToWorld& w, const WorldPose& world) noexcept;

// 21 §4.2 item 10 (r11): a world target placed while the body's
// model-to-world translation was `anchor_then` belongs to the body that now
// stands at `anchor_now`: it moves by the body's displacement in between.
util::Vec3 follow_body(const util::Vec3& world, const util::Vec3& anchor_then,
                       const util::Vec3& anchor_now) noexcept;

// ---- Rotations -------------------------------------------------------------------

// The rotation by `angle_rad` about the unit `axis`.
util::Quat axis_angle(const util::Vec3& axis, float angle_rad) noexcept;
// The shortest rotation that turns the direction of `from` into that of
// `to`; the identity when either has no length.
util::Quat from_to(const util::Vec3& from, const util::Vec3& to) noexcept;
// The angle in radians (-pi..pi) of the twist of the unit `q` about the
// unit `axis` (its swing-twist decomposition).
float twist_angle(const util::Quat& q, const util::Vec3& axis) noexcept;

// ---- Grip to hand bone (21 §3.5) -----------------------------------------------

// The local translations of a hand's finger bones (their bases: the
// knuckles and the thumb), in the hand bone's frame.
struct FingerBases {
    util::Vec3 index{};
    util::Vec3 middle{};
    util::Vec3 ring{};
    util::Vec3 pinky{};
    util::Vec3 thumb{};
};

// The grip pose's frame in the hand bone's frame, and the wrist to grip
// distance (its origin's length).
struct GripFrame {
    RigidTransform in_hand{};
    float wrist_distance_m = 0.0f;
};

// 21 §3.5: knuckle line a = unit(I - P) is the grip's -Z (its local +Z in
// the game's axes, 21 §3.1); forward f = unit(M); palm normal n =
// unit(f x a) on the thumb's side, made orthogonal to a; origin the
// knuckles' mean moved kGripPalmInset along n; +X normal to the palm as
// the runtime's grip (21 §3.1: away from the palm on the left hand, into it
// on the right); +Y completes a proper rotation. nullopt when the bones do
// not span a hand (a degenerate knuckle line or forward).
std::optional<GripFrame> grip_frame(const FingerBases& f, std::size_t hand) noexcept;

// 21 §3.5: the hand bone's model pose that puts the grip frame on
// `grip_target` (model space): rotation target * grip^-1, origin target
// minus the grip origin rotated by it.
RigidTransform hand_for_grip(const RigidTransform& grip_target, const GripFrame& grip) noexcept;

// ---- Two-bone reach (21 §4.2 items 2 to 4) ---------------------------------------

// The arm's solve in model space: rotate the upper arm about `root` by
// `upper`, then the forearm about the new elbow by `fore`.
struct TwoBoneSolution {
    util::Quat upper{};
    util::Quat fore{};
    util::Vec3 elbow{};
    util::Vec3 wrist{};
    bool in_reach = false;
};

// 21 §4.2 items 2, 4: the elbow placed so the wrist reaches `target`, in the
// plane of the root, the target and `hint` (a direction from the root
// towards which the elbow bends); beyond the arm's reach the arm is straight
// towards the target. The bone lengths are those of root-elbow-wrist.
TwoBoneSolution solve_two_bone(const util::Vec3& root, const util::Vec3& elbow, const util::Vec3& wrist,
                               const util::Vec3& target, const util::Vec3& hint) noexcept;

// 21 §4.2 item 3: the shoulder's extra turn about its origin `shoulder`
// that brings the arm's root `root` towards a target the arm of length
// `reach` cannot reach: none until the target is kShoulderEngage of the
// reach away, rising to at most kMaxShoulderTurnRad at full reach.
util::Quat shoulder_turn(const util::Vec3& shoulder, const util::Vec3& root, float reach,
                         const util::Vec3& target) noexcept;

// 21 §4.2 item 4 [proposal]: the elbow hint as a world direction: down,
// behind and outwards of the camera's heading (`forward` the camera's
// pointing direction, `right` its local +X).
util::Vec3 elbow_hint(const util::Vec3& forward, const util::Vec3& right, std::size_t hand) noexcept;

// ---- The skeleton's arms (21 §2.3) -----------------------------------------------

// One side's bones by index; -1 for a bone not found.
struct ArmSideBones {
    std::int16_t shoulder = -1;
    std::int16_t arm      = -1;
    std::int16_t forearm  = -1;
    std::int16_t roll     = -1; // <Side>ForeArmRoll
    std::int16_t roll1    = -1; // <Side>ForeArmRoll1
    std::int16_t hand     = -1;
    std::int16_t index    = -1; // <Side>HandIndex1 and the other finger bases
    std::int16_t middle   = -1;
    std::int16_t ring     = -1;
    std::int16_t pinky    = -1;
    std::int16_t thumb    = -1;
};

struct ArmRig {
    std::array<ArmSideBones, kHandCount> sides{};
    std::int16_t head = -1;    // `Head`, for the model-to-world check (21 §21 item 3)
    std::size_t count = 0;     // bones in the skeleton
    bool first_person = false; // names from the first-person table (21 §2.3, §4.2 item 7)
};

enum class RigFault : std::uint8_t {
    None,
    TooManyBones, // more than kMaxBodyBones, or the arrays disagree in size
    Unordered,    // a parent index not before its bone (or out of range)
    MissingBone,  // a bone of the arm chain or a finger base is not there by name
    BadParent,    // the chain or a finger base does not hang from the bone 21 §2.3 says
};

struct RigResult {
    std::optional<ArmRig> rig;
    RigFault fault = RigFault::None;
    std::string_view bone; // the bone the fault is about
};

// 21 §2.3, §3.5, §4.6: the arms' bones by name (`Left` and `Right` with
// `Shoulder`, `Arm`, `ForeArm`, `ForeArmRoll`, `ForeArmRoll1`, `Hand` and
// the finger bases), each hanging from the bone before it in the chain, the
// rolls and finger bases optional only as far as the method allows: a roll
// bone that is not a child of the forearm is not turned; a finger base that
// is missing or not a child of the hand leaves the skeleton unsupported.
RigResult find_arm_rig(const SkeletonBones& s) noexcept;

// ---- The pose edit (21 §4.2) -----------------------------------------------------

// 21 §2.3: the pose's per-bone flags: the local transform out of date (the
// model one is current), the model transform out of date.
inline constexpr std::uint32_t kBoneLocalStale = 1U << 0;
inline constexpr std::uint32_t kBoneModelStale = 1U << 1;

// The game's pose as arrays of the skeleton's size: PoseArrays (types.h).

enum class PoseFault : std::uint8_t {
    None,
    Size,  // the arrays are not of the rig's size
    Stale, // a model transform the edit starts from is out of date
};

// Checks the pose against `rig` and makes the local transforms of both arms
// and everything below them current (from their model transforms where
// the flags say they are out of date).
PoseFault prepare_arms(PoseArrays& p, const ArmRig& rig) noexcept;

// The finger bases of one side, from the (current) local transforms.
FingerBases finger_bases(const PoseArrays& p, const ArmSideBones& side) noexcept;

// What one arm is to reach: the hand bone's model pose and the weight of
// the tracked pose over the game's (21 §3.3), and the elbow hint in model
// space.
struct ArmGoal {
    RigidTransform hand{};
    float weight = 0.0f;
    util::Vec3 elbow_hint{};
};

struct ArmOutcome {
    bool edited   = false;
    bool in_reach = false;
    RigidTransform goal{}; // the hand's goal after the weight (model space)
};

// 21 §4.2 items 1 to 5: rotates the shoulder (at most kMaxShoulderTurnRad),
// the arm and the forearm so the hand bone reaches its goal, gives the hand
// the goal's rotation, and spreads the hand's twist about the forearm over
// the roll bones (a third and two thirds). Writes the local transforms of
// the chain only; refresh_arms brings the model transforms after it.
// prepare_arms must have succeeded on `p`.
ArmOutcome pose_arm(PoseArrays& p, const ArmSideBones& side, const ArmGoal& goal) noexcept;

// 21 §4.2 item 9: the model transforms of both arms and everything below
// them from their local transforms, with both out-of-date flags cleared, so
// the pose's local and model transforms agree.
void refresh_arms(PoseArrays& p, const ArmRig& rig) noexcept;

} // namespace omvr::core::hands
