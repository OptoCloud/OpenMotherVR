// OpenMotherVR-Provenance: clean
#include "core/hands/arm_pose.h"

#include "core/hands/tuning.h"
#include "util/math/matrix.h"
#include "util/math/quat.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace omvr::core::hands {

namespace {

using util::Quat;
using util::Vec3;

constexpr std::size_t kX = 0;
constexpr std::size_t kY = 1;
constexpr std::size_t kZ = 2;

Vec3 vec_of(const std::array<float, 4>& a) noexcept { return {a[kX], a[kY], a[kZ]}; }

Quat quat_of(const std::array<float, 4>& a) noexcept { return {a[kX], a[kY], a[kZ], a[3]}; }

// v / |v|, or nullopt below kMinBoneSpan.
std::optional<Vec3> unit(const Vec3& v) noexcept {
    const float n = util::length(v);
    if (!(n > kMinBoneSpan)) return std::nullopt;
    return util::scaled(v, 1.0f / n);
}

// The part of `v` perpendicular to the unit `axis`.
Vec3 perpendicular(const Vec3& v, const Vec3& axis) noexcept {
    return util::sub(v, util::scaled(axis, util::dot(v, axis)));
}

// Any unit vector perpendicular to the unit `v`.
Vec3 any_perpendicular(const Vec3& v) noexcept {
    const Vec3 other = std::fabs(v.x) < std::fabs(v.y) ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    return unit(util::cross(v, other)).value_or(Vec3{0, 0, 1});
}

// The rotation whose matrix has these unit columns (a proper rotation).
Quat from_columns(const Vec3& x, const Vec3& y, const Vec3& z) noexcept {
    util::Matrix34 m{};
    const std::array cols{x, y, z};
    for (std::size_t c = 0; c < cols.size(); ++c) {
        m.m[kX][c] = cols[c].x;
        m.m[kY][c] = cols[c].y;
        m.m[kZ][c] = cols[c].z;
    }
    return util::quat_from_rotation(m);
}

float safe_divide(float a, float b) noexcept { return std::fabs(b) > 0.0f ? a / b : a; }

// A model-to-world scale component that is not a positive finite number (a
// record not yet set) is taken as 1: the first-person rigs are not scaled.
float usable_scale(float s) noexcept { return s > 0.0f && std::isfinite(s) ? s : 1.0f; }

Vec3 usable_scale(const Vec3& s) noexcept {
    return {usable_scale(s.x), usable_scale(s.y), usable_scale(s.z)};
}

// The model-to-world rotation as a unit quaternion (the identity when the
// record has none).
Quat rotation_of(const ModelToWorld& w) noexcept { return util::normalized(w.rotation); }

bool in_range(std::int16_t bone, std::size_t count) noexcept {
    return bone >= 0 && static_cast<std::size_t>(bone) < count;
}

std::size_t at(std::int16_t bone) noexcept { return static_cast<std::size_t>(bone); }

// ---- The skeleton's arms -------------------------------------------------------

constexpr std::array<std::string_view, kHandCount> kSidePrefix = {"Left", "Right"};

// The bone named <prefix><suffix> (the namespace prefix of 21 §2.3 ignored),
// or -1.
std::int16_t find_bone(const SkeletonBones& s, std::string_view prefix, std::string_view suffix) noexcept {
    for (std::size_t i = 0; i < s.names.size(); ++i) {
        const std::string_view n = bare_bone_name(s.names[i]);
        if (n.size() == prefix.size() + suffix.size() && n.starts_with(prefix) && n.ends_with(suffix))
            return static_cast<std::int16_t>(i);
    }
    return -1;
}

// Every parent comes before its bone (the order the edit refreshes in).
bool parents_ordered(std::span<const std::int16_t> parents) noexcept {
    for (std::size_t i = 0; i < parents.size(); ++i) {
        const std::int16_t p = parents[i];
        if (p < -1 || (p >= 0 && at(p) >= i)) return false;
    }
    return true;
}

struct SideLookup {
    ArmSideBones bones;
    RigFault fault = RigFault::None;
    std::string_view bone;
};

// One side's chain and finger bases, each hanging where 21 §2.3 says.
SideLookup find_side(const SkeletonBones& s, std::string_view prefix) noexcept {
    SideLookup r;
    ArmSideBones& b = r.bones;
    struct Need {
        std::int16_t* index;
        std::string_view suffix;
        const std::int16_t* parent; // the bone it must hang from; null: any
    };
    b.shoulder = find_bone(s, prefix, "Shoulder");
    b.arm      = find_bone(s, prefix, "Arm");
    b.forearm  = find_bone(s, prefix, "ForeArm");
    b.hand     = find_bone(s, prefix, "Hand");
    b.index    = find_bone(s, prefix, "HandIndex1");
    b.middle   = find_bone(s, prefix, "HandMiddle1");
    b.ring     = find_bone(s, prefix, "HandRing1");
    b.pinky    = find_bone(s, prefix, "HandPinky1");
    b.thumb    = find_bone(s, prefix, "HandThumb1");
    const std::array<Need, 9> needs{{
        {&b.shoulder, "Shoulder", nullptr},
        {&b.arm, "Arm", &b.shoulder},
        {&b.forearm, "ForeArm", &b.arm},
        {&b.hand, "Hand", &b.forearm},
        {&b.index, "HandIndex1", &b.hand},
        {&b.middle, "HandMiddle1", &b.hand},
        {&b.ring, "HandRing1", &b.hand},
        {&b.pinky, "HandPinky1", &b.hand},
        {&b.thumb, "HandThumb1", &b.hand},
    }};
    for (const Need& n : needs) {
        if (*n.index < 0) return {b, RigFault::MissingBone, n.suffix};
        if (n.parent && s.parents[at(*n.index)] != *n.parent) return {b, RigFault::BadParent, n.suffix};
    }
    // 21 §4.2 item 3: the roll bones are turned only as siblings of the hand.
    const auto roll_of = [&](std::string_view suffix) {
        const std::int16_t i = find_bone(s, prefix, suffix);
        return i >= 0 && s.parents[at(i)] == b.forearm ? i : std::int16_t{-1};
    };
    b.roll  = roll_of("ForeArmRoll");
    b.roll1 = roll_of("ForeArmRoll1");
    return r;
}

// The bones the edit moves: both shoulders and everything below them.
// `moved` must hold p.parents.size() entries.
void mark_arms(std::span<const std::int16_t> parents, const ArmRig& rig, std::span<bool> moved) noexcept {
    std::ranges::fill(moved, false);
    for (const auto& side : rig.sides) moved[at(side.shoulder)] = true;
    for (std::size_t i = 0; i < parents.size(); ++i)
        if (parents[i] >= 0 && moved[at(parents[i])]) moved[i] = true;
}

// The bone's parent's model transform, or the identity for a root.
BoneTransform parent_model(const PoseArrays& p, std::size_t i) noexcept {
    BoneTransform identity{};
    identity.rotation[3]      = 1.0f;
    identity.scale            = {1.0f, 1.0f, 1.0f, 1.0f};
    const std::int16_t parent = p.parents[i];
    return parent >= 0 ? p.model[at(parent)] : identity;
}

// The new model rotation of a bone turned by `turn` (model space) about a
// point: its rotation turned, its origin turned about `pivot`.
RigidTransform turned(const RigidTransform& t, const Quat& turn, const Vec3& pivot) noexcept {
    return {util::add(pivot, util::rotate(turn, util::sub(t.translation, pivot))),
            util::multiply(turn, t.rotation)};
}

RigidTransform relative_rigid(const RigidTransform& parent, const RigidTransform& model) noexcept {
    const Quat inv = util::conjugate(parent.rotation);
    return {util::rotate(inv, util::sub(model.translation, parent.translation)),
            util::normalized(util::multiply(inv, model.rotation))};
}

} // namespace

// ---- Transforms ------------------------------------------------------------------

RigidTransform rigid_of(const BoneTransform& b) noexcept {
    return {vec_of(b.translation), util::normalized(quat_of(b.rotation))};
}

void set_rigid(BoneTransform& b, const RigidTransform& r) noexcept {
    b.translation[kX] = r.translation.x;
    b.translation[kY] = r.translation.y;
    b.translation[kZ] = r.translation.z;
    b.rotation        = {r.rotation.x, r.rotation.y, r.rotation.z, r.rotation.w};
}

BoneTransform compose(const BoneTransform& parent, const BoneTransform& local) noexcept {
    const RigidTransform pr = rigid_of(parent);
    const RigidTransform lr = rigid_of(local);
    BoneTransform out       = local;
    set_rigid(out, {util::add(pr.translation, util::rotate(pr.rotation, lr.translation)),
                    util::normalized(util::multiply(pr.rotation, lr.rotation))});
    for (std::size_t k = 0; k < out.scale.size(); ++k) out.scale[k] = parent.scale[k] * local.scale[k];
    return out;
}

BoneTransform relative(const BoneTransform& parent, const BoneTransform& model) noexcept {
    BoneTransform out = model;
    set_rigid(out, relative_rigid(rigid_of(parent), rigid_of(model)));
    for (std::size_t k = 0; k < out.scale.size(); ++k)
        out.scale[k] = safe_divide(model.scale[k], parent.scale[k]);
    return out;
}

Vec3 to_world_point(const ModelToWorld& w, const Vec3& model) noexcept {
    const Vec3 k = usable_scale(w.scale);
    const Vec3 s{model.x * k.x, model.y * k.y, model.z * k.z};
    return util::add(w.translation, util::rotate(rotation_of(w), s));
}

Vec3 to_model_point(const ModelToWorld& w, const Vec3& world) noexcept {
    const Vec3 r = util::rotate(util::conjugate(rotation_of(w)), util::sub(world, w.translation));
    const Vec3 k = usable_scale(w.scale);
    return {r.x / k.x, r.y / k.y, r.z / k.z};
}

Vec3 to_model_direction(const ModelToWorld& w, const Vec3& world) noexcept {
    return util::rotate(util::conjugate(rotation_of(w)), world);
}

RigidTransform to_model(const ModelToWorld& w, const WorldPose& world) noexcept {
    return {to_model_point(w, world.position),
            util::normalized(util::multiply(util::conjugate(rotation_of(w)), world.orientation))};
}

Vec3 follow_body(const Vec3& world, const Vec3& anchor_then, const Vec3& anchor_now) noexcept {
    return util::add(world, util::sub(anchor_now, anchor_then));
}

// ---- Rotations -------------------------------------------------------------------

Quat axis_angle(const Vec3& axis, float angle_rad) noexcept {
    const float s = std::sin(angle_rad / 2);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle_rad / 2)};
}

Quat from_to(const Vec3& from, const Vec3& to) noexcept {
    const auto a = unit(from);
    const auto b = unit(to);
    if (!a || !b) return {};
    const float d = util::dot(*a, *b);
    if (d < -1.0f + kMinBoneSpan) return axis_angle(any_perpendicular(*a), std::numbers::pi_v<float>);
    const Vec3 c = util::cross(*a, *b);
    return util::normalized({c.x, c.y, c.z, 1.0f + d});
}

float twist_angle(const Quat& q, const Vec3& axis) noexcept {
    const float along = q.x * axis.x + q.y * axis.y + q.z * axis.z;
    float angle       = 2 * std::atan2(along, q.w);
    // q and -q are one rotation: the angle in -pi..pi.
    if (angle > std::numbers::pi_v<float>) angle -= 2 * std::numbers::pi_v<float>;
    if (angle < -std::numbers::pi_v<float>) angle += 2 * std::numbers::pi_v<float>;
    return angle;
}

// ---- Grip to hand bone -------------------------------------------------------------

std::optional<GripFrame> grip_frame(const FingerBases& f, std::size_t hand) noexcept {
    const auto a       = unit(util::sub(f.index, f.pinky));
    const auto forward = unit(f.middle);
    if (!a || !forward) return std::nullopt;
    auto n = unit(util::cross(*forward, *a));
    if (!n) return std::nullopt;
    if (util::dot(*n, f.thumb) < 0.0f) n = util::scaled(*n, -1.0f);
    n = unit(perpendicular(*n, *a));
    if (!n) return std::nullopt;
    // The grip's +X is the palm's normal as the runtime defines it (21 §3.1):
    // out of the palm on the left hand, into it on the right, which for
    // hands held as mirror images is one side of both. `n` points out of the
    // palm (the thumb's side), so the left hand takes n and the right -n.
    const Vec3 x = hand == kLeftHand ? *n : util::scaled(*n, -1.0f);
    // 21 §3.1: the grip's -Z maps to the game's local +Z; +Y completes a
    // proper rotation (z = x cross y in the game's axes, the mirror M).
    const Vec3 z = *a;
    const Vec3 y = util::cross(z, x);
    const Vec3 mean =
        util::scaled(util::add(util::add(f.index, f.middle), util::add(f.ring, f.pinky)), 1.0f / 4.0f);
    GripFrame g;
    g.in_hand          = {util::add(mean, util::scaled(*n, kGripPalmInset)), from_columns(x, y, z)};
    g.wrist_distance_m = util::length(g.in_hand.translation);
    return g;
}

RigidTransform hand_for_grip(const RigidTransform& grip_target, const GripFrame& grip) noexcept {
    const Quat r =
        util::normalized(util::multiply(grip_target.rotation, util::conjugate(grip.in_hand.rotation)));
    return {util::sub(grip_target.translation, util::rotate(r, grip.in_hand.translation)), r};
}

// ---- Two-bone reach --------------------------------------------------------------

TwoBoneSolution solve_two_bone(const Vec3& root, const Vec3& elbow, const Vec3& wrist, const Vec3& target,
                               const Vec3& hint) noexcept {
    TwoBoneSolution s{{}, {}, elbow, wrist, false};
    const float upper = util::distance(elbow, root);
    const float fore  = util::distance(wrist, elbow);
    if (!(upper > kMinBoneSpan) || !(fore > kMinBoneSpan)) return s;
    const Vec3 to_target = util::sub(target, root);
    // A target on the root has no direction: the arm keeps its own.
    const Vec3 u       = unit(to_target).value_or(unit(util::sub(wrist, root)).value_or(Vec3{0, 0, 1}));
    const float reach  = (upper + fore) * (1.0f - kReachSlack);
    const float least  = std::fabs(upper - fore) * (1.0f + kReachSlack);
    const float wanted = util::length(to_target);
    const float d      = std::clamp(wanted, least, reach);
    s.in_reach         = wanted <= upper + fore;
    // 21 §4.2 item 4: the elbow bends towards the hint; with the hint on the
    // line to the target, as it was bent before; failing that, anywhere.
    const Vec3 bend =
        unit(perpendicular(hint, u))
            .value_or(unit(perpendicular(util::sub(elbow, root), u)).value_or(any_perpendicular(u)));
    const float along  = (upper * upper - fore * fore + d * d) / (2 * d);
    const float height = std::sqrt(std::max(0.0f, upper * upper - along * along));
    s.elbow            = util::add(root, util::add(util::scaled(u, along), util::scaled(bend, height)));
    s.wrist            = util::add(root, util::scaled(u, d));
    s.upper            = from_to(util::sub(elbow, root), util::sub(s.elbow, root));
    const Vec3 wrist_after_upper = util::add(root, util::rotate(s.upper, util::sub(wrist, root)));
    s.fore = from_to(util::sub(wrist_after_upper, s.elbow), util::sub(s.wrist, s.elbow));
    return s;
}

Quat shoulder_turn(const Vec3& shoulder, const Vec3& root, float reach, const Vec3& target) noexcept {
    if (!(reach > kMinBoneSpan)) return {};
    const float excess =
        (util::distance(target, root) - kShoulderEngage * reach) / ((1.0f - kShoulderEngage) * reach);
    const float share = std::clamp(excess, 0.0f, 1.0f);
    if (!(share > 0.0f)) return {};
    const Vec3 from = util::sub(root, shoulder);
    const Vec3 to   = util::sub(target, shoulder);
    const auto axis = unit(util::cross(from, to));
    const auto a    = unit(from);
    const auto b    = unit(to);
    if (!axis || !a || !b) return {};
    const float full = std::acos(std::clamp(util::dot(*a, *b), -1.0f, 1.0f));
    return axis_angle(*axis, std::min(full, kMaxShoulderTurnRad) * share);
}

Vec3 elbow_hint(const Vec3& forward, const Vec3& right, std::size_t hand) noexcept {
    const Vec3 heading = unit({forward.x, 0.0f, forward.z}).value_or(Vec3{0, 0, 1});
    const Vec3 side    = unit({right.x, 0.0f, right.z}).value_or(Vec3{1, 0, 0});
    const float out    = hand == kLeftHand ? -kElbowHintOut : kElbowHintOut;
    return util::add(util::add(Vec3{0.0f, -kElbowHintDown, 0.0f}, util::scaled(heading, -kElbowHintBack)),
                     util::scaled(side, out));
}

// ---- The skeleton's arms ---------------------------------------------------------

RigResult find_arm_rig(const SkeletonBones& s) noexcept {
    if (s.names.size() != s.parents.size() || s.names.size() > kMaxBodyBones)
        return {std::nullopt, RigFault::TooManyBones, {}};
    if (!parents_ordered(s.parents)) return {std::nullopt, RigFault::Unordered, {}};
    ArmRig rig;
    rig.count        = s.names.size();
    rig.first_person = s.names_from_table;
    for (std::size_t h = 0; h < kHandCount; ++h) {
        const SideLookup side = find_side(s, kSidePrefix[h]);
        if (side.fault != RigFault::None) return {std::nullopt, side.fault, side.bone};
        rig.sides[h] = side.bones;
    }
    for (std::size_t i = 0; i < s.names.size(); ++i)
        if (bare_bone_name(s.names[i]) == "Head") rig.head = static_cast<std::int16_t>(i);
    return {rig, RigFault::None, {}};
}

// ---- The pose edit ---------------------------------------------------------------

PoseFault prepare_arms(PoseArrays& p, const ArmRig& rig) noexcept {
    const std::size_t n = rig.count;
    if (n == 0 || n > kMaxBodyBones || p.local.size() != n || p.model.size() != n || p.flags.size() != n ||
        p.parents.size() != n || !parents_ordered(p.parents))
        return PoseFault::Size;
    std::array<bool, kMaxBodyBones> moved{};
    const std::span marks = std::span{moved}.first(n);
    mark_arms(p.parents, rig, marks);
    // Every model transform the edit starts from is current: the arms, what
    // hangs from them and what they hang from.
    for (std::size_t i = 0; i < n; ++i) {
        if (!marks[i]) continue;
        const std::int16_t parent = p.parents[i];
        if ((p.flags[i] & kBoneModelStale) != 0 ||
            (parent >= 0 && (p.flags[at(parent)] & kBoneModelStale) != 0))
            return PoseFault::Stale;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (!marks[i] || (p.flags[i] & kBoneLocalStale) == 0) continue;
        p.local[i] = relative(parent_model(p, i), p.model[i]);
        p.flags[i] &= ~kBoneLocalStale;
    }
    return PoseFault::None;
}

FingerBases finger_bases(const PoseArrays& p, const ArmSideBones& side) noexcept {
    const auto t = [&](std::int16_t bone) { return vec_of(p.local[at(bone)].translation); };
    return {t(side.index), t(side.middle), t(side.ring), t(side.pinky), t(side.thumb)};
}

ArmOutcome pose_arm(PoseArrays& p, const ArmSideBones& side, const ArmGoal& goal) noexcept {
    ArmOutcome out;
    const std::size_t n = p.model.size();
    for (const std::int16_t b : {side.shoulder, side.arm, side.forearm, side.hand})
        if (!in_range(b, n)) return out;
    if (!(goal.weight > 0.0f)) return out;
    const float w = std::min(goal.weight, 1.0f);

    const RigidTransform sh0   = rigid_of(p.model[at(side.shoulder)]);
    const RigidTransform arm0  = rigid_of(p.model[at(side.arm)]);
    const RigidTransform fore0 = rigid_of(p.model[at(side.forearm)]);
    const RigidTransform hand0 = rigid_of(p.model[at(side.hand)]);
    // 21 §3.3: the weight blends from the game's hand to the tracked one.
    out.goal = {util::lerp(hand0.translation, goal.hand.translation, w),
                util::nlerp(hand0.rotation, goal.hand.rotation, w)};

    // 21 §4.2 item 3: the shoulder's extra turn, then the two-bone reach.
    const float reach = util::distance(fore0.translation, arm0.translation) +
                        util::distance(hand0.translation, fore0.translation);
    const Quat turn   = shoulder_turn(sh0.translation, arm0.translation, reach, out.goal.translation);
    const Vec3 pivot  = sh0.translation;
    const RigidTransform sh1{sh0.translation, util::normalized(util::multiply(turn, sh0.rotation))};
    const RigidTransform arm1  = turned(arm0, turn, pivot);
    const RigidTransform fore1 = turned(fore0, turn, pivot);
    const RigidTransform hand1 = turned(hand0, turn, pivot);
    const TwoBoneSolution s    = solve_two_bone(arm1.translation, fore1.translation, hand1.translation,
                                                out.goal.translation, goal.elbow_hint);
    const RigidTransform arm2{arm1.translation, util::normalized(util::multiply(s.upper, arm1.rotation))};
    const RigidTransform fore2{
        s.elbow, util::normalized(util::multiply(s.fore, util::multiply(s.upper, fore1.rotation)))};
    // 21 §4.2 items 1, 2: the hand at the wrist the reach gives (the goal
    // when in reach), with the goal's rotation.
    const RigidTransform hand2{s.wrist, out.goal.rotation};
    out.in_reach = s.in_reach;

    // 21 §4.2 item 3 (r11): the hand's twist about the forearm that the edit
    // adds goes a third to ForeArmRoll and two thirds to ForeArmRoll1.
    const Vec3 axis   = unit(vec_of(p.local[at(side.hand)].translation)).value_or(Vec3{1, 0, 0});
    const Quat rel0   = util::multiply(util::conjugate(fore0.rotation), hand0.rotation);
    const Quat rel2   = util::multiply(util::conjugate(fore2.rotation), hand2.rotation);
    const float twist = twist_angle(util::normalized(util::multiply(rel2, util::conjugate(rel0))), axis);
    const auto roll   = [&](std::int16_t bone, float share) {
        if (!in_range(bone, n)) return;
        RigidTransform l = rigid_of(p.local[at(bone)]);
        l.rotation       = util::normalized(util::multiply(axis_angle(axis, twist * share), l.rotation));
        set_rigid(p.local[at(bone)], l);
    };
    roll(side.roll, kForeArmRollShare);
    roll(side.roll1, kForeArmRoll1Share);

    // The chain's local transforms; refresh_arms derives the model ones.
    const RigidTransform head = rigid_of(parent_model(p, at(side.shoulder)));
    set_rigid(p.local[at(side.shoulder)], relative_rigid(head, sh1));
    set_rigid(p.local[at(side.arm)], relative_rigid(sh1, arm2));
    set_rigid(p.local[at(side.forearm)], relative_rigid(arm2, fore2));
    set_rigid(p.local[at(side.hand)], relative_rigid(fore2, hand2));
    out.edited = true;
    return out;
}

void refresh_arms(PoseArrays& p, const ArmRig& rig) noexcept {
    const std::size_t n = std::min(rig.count, p.model.size());
    std::array<bool, kMaxBodyBones> moved{};
    const std::span marks = std::span{moved}.first(n);
    mark_arms(p.parents.first(n), rig, marks);
    for (std::size_t i = 0; i < n; ++i) {
        if (!marks[i]) continue;
        p.model[i] = compose(parent_model(p, i), p.local[i]);
        p.flags[i] &= ~(kBoneLocalStale | kBoneModelStale);
    }
}

} // namespace omvr::core::hands
