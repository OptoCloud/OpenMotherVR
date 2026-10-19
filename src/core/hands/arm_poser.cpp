// OpenMotherVR-Provenance: clean
#include "core/hands/arm_poser.h"

#include "core/hands/hand_aim.h"
#include "core/hands/tuning.h"
#include "util/math/vec.h"

#include <string_view>

namespace omvr::core::hands {

namespace {

using diag::ports::logf;
using util::Vec3;

constexpr std::string_view kLogRig =
    "hands: the arms of skeleton {} follow the controllers (bones found by name{}): left shoulder {} arm {} "
    "forearm {} rolls {} {} hand {}, right shoulder {} arm {} forearm {} rolls {} {} hand {}; {}";
constexpr std::string_view kLogLimbIkOff =
    "the game's limb IK is not run for it while tracked hands are active";
constexpr std::string_view kLogLimbIkOn = "the game's limb IK runs for it as in v1";
constexpr std::string_view kLogRigFault =
    "hands: WARNING the arms of skeleton {} keep the game's animation: {}{}{}";
constexpr std::string_view kLogRigUnreadable =
    "hands: WARNING the arms of skeleton {} keep the game's animation until it can be read: {}; it is read "
    "again every {:.0f} s";
constexpr std::string_view kLogGrip =
    "hands: grip frame of {}, {} hand: wrist to grip {:.3f} m (expected {:.2f} to {:.2f} m), origin ({:+.3f} "
    "{:+.3f} {:+.3f}) in the hand bone";
constexpr std::string_view kLogNoGrip =
    "hands: WARNING the {} hand's finger bones of {} do not span a hand; that arm keeps the game's animation";
constexpr std::string_view kLogHead =
    "hands: Head bone {} of {} at ({:.3f} {:.3f} {:.3f}) in the world (BoneWorldTransform, final pose), "
    "active "
    "camera at ({:.3f} {:.3f} {:.3f}): {:.3f} m apart (expected within {:.2f} m)";
constexpr std::string_view kLogPoseFault =
    "hands: WARNING the player's pose cannot be edited this frame ({}); the arms keep the game's animation";
constexpr std::string_view kLogFirstEdit = "hands: the player's arms are drawn from the edited pose";
constexpr std::string_view kLogItem =
    "hands: the held item follows the moved right hand (the equipment update is run again on the final pose)";
constexpr std::string_view kLogReach =
    "[diag] hands: arms: left target ({:.3f} {:.3f} {:.3f}) reached ({:.3f} {:.3f} {:.3f}) {:.4f} m{}; right "
    "target ({:.3f} {:.3f} {:.3f}) reached ({:.3f} {:.3f} {:.3f}) {:.4f} m{}";

const char* rig_fault_text(RigFault f) noexcept {
    switch (f) {
        case RigFault::None: return "no fault";
        case RigFault::TooManyBones: return "it has too many bones";
        case RigFault::Unordered: return "a bone's parent does not come before it";
        case RigFault::MissingBone: return "a bone is missing by name";
        case RigFault::BadParent: return "a bone does not hang from the bone the arm chain needs";
    }
    return "unknown";
}

const char* skeleton_fault_text(SkeletonFault f) noexcept {
    switch (f) {
        case SkeletonFault::Unknown: return "the skeleton could not be read";
        case SkeletonFault::NoBones: return "the skeleton has no bones";
        case SkeletonFault::NoArrays: return "the skeleton has no bone or parent array";
        case SkeletonFault::TooManyBones: return "it has too many bones";
        case SkeletonFault::UnnamedBone: return "a bone has no name";
        case SkeletonFault::TableMismatch: return "it is not the first-person bone table's skeleton";
    }
    return "unknown";
}

const char* pose_fault_text(PoseFault f) noexcept {
    switch (f) {
        case PoseFault::None: return "no fault";
        case PoseFault::Size: return "its arrays are not of the skeleton's size";
        case PoseFault::Stale: return "a bone's model transform is out of date";
    }
    return "unknown";
}

bool any_active(const ArmFrame& f) noexcept {
    for (const auto& s : f.sides)
        if (s.active) return true;
    return false;
}

const char* reach_note(const ArmSideReport& s) noexcept {
    if (!s.edited) return " (not edited)";
    return s.in_reach ? "" : " (out of reach)";
}

} // namespace

bool ArmPoser::wants_rig(const PlayerSkeleton& s, TimePoint now) const noexcept {
    if (s.skeleton == 0) return false;
    if (!rig_key_ || *rig_key_ != RigKey{s.skeleton, s.pose}) return true;
    return rig_retry_at_ && now >= *rig_retry_at_;
}

void ArmPoser::set_rig(const PlayerSkeleton& s, const SkeletonBones* bones, const SkeletonReadFault& fault,
                       TimePoint now) noexcept {
    if (!rig_key_ || rig_key_->skeleton != s.skeleton) {
        logged_state_.reset();
        grip_logged_ = {};
        head_logged_ = false;
    }
    rig_key_  = RigKey{s.skeleton, s.pose};
    rig_name_ = s.name;
    if (!bones) {
        // 21 §10.2 (r8): a skeleton the game has not made readable yet is
        // read again until it is; logged once until that changes (21 §18).
        rig_.reset();
        rig_retry_at_ = util::after(now, kSkeletonRetry);
        if (logged_state_ != kStateUnreadable)
            logf(log_, kLogRigUnreadable, s.name, skeleton_fault_text(fault.fault), kSkeletonRetry.count());
        logged_state_ = kStateUnreadable;
        return;
    }
    rig_retry_at_.reset();
    const RigResult r        = find_arm_rig(*bones);
    rig_                     = r.rig;
    const std::uint8_t state = r.rig ? kStateFound : static_cast<std::uint8_t>(r.fault);
    if (logged_state_ == state) return;
    logged_state_ = state;
    log_rig(s, r);
}

void ArmPoser::log_rig(const PlayerSkeleton& s, const RigResult& r) noexcept {
    if (!r.rig) {
        logf(log_, kLogRigFault, s.name, rig_fault_text(r.fault), r.bone.empty() ? "" : ": ", r.bone);
        return;
    }
    const ArmSideBones& l  = r.rig->sides[kLeftHand];
    const ArmSideBones& rr = r.rig->sides[kRightHand];
    logf(log_, kLogRig, s.name, r.rig->first_person ? ", names from the first-person table" : "", l.shoulder,
         l.arm, l.forearm, l.roll, l.roll1, l.hand, rr.shoulder, rr.arm, rr.forearm, rr.roll, rr.roll1,
         rr.hand, r.rig->first_person ? kLogLimbIkOff : kLogLimbIkOn);
}

void ArmPoser::publish(const PlayerSkeleton& s, const CameraPose& camera, const ModelToWorld& w,
                       const ArmTargets& targets, TimePoint now) noexcept {
    if (!rig_ || !rig_key_ || rig_key_->skeleton != s.skeleton || s.pose == 0) {
        publish_none();
        return;
    }
    ArmFrame f;
    f.generation = ++generation_;
    if (f.generation == 0) f.generation = ++generation_; // 0 is "none applied"
    f.animation = s.animation;
    f.pose      = s.pose;
    f.skeleton  = s.skeleton;
    f.stamp     = util::stamp_ticks(now);
    f.anchor    = w.translation;
    f.rig       = *rig_;
    // 21 §4.2 item 4: the elbow hint from the camera's heading.
    const WorldPose view{camera.position, camera.orientation};
    const Vec3 forward = pointing(view);
    const Vec3 right   = util::rotate(camera.orientation, Vec3{1.0f, 0.0f, 0.0f});
    for (std::size_t h = 0; h < kHandCount; ++h) {
        const auto& t = targets[h];
        if (!t || !(t->weight > 0.0f)) continue;
        f.sides[h] = {true, t->grip, t->weight, elbow_hint(forward, right, h)};
    }
    frame_ = f;
    limb_ik_off_.store(any_active(f) && f.rig.first_person ? f.skeleton : 0, std::memory_order_release);
    published_.store(f.generation, std::memory_order_release);
}

void ArmPoser::publish_none() noexcept {
    limb_ik_off_.store(0, std::memory_order_release);
    item_follows_at_.store(0, std::memory_order_release);
    if (!any_active(frame_)) return;
    // The jobs, and the game thread after the pass, see no active side from
    // now on.
    frame_.sides      = {};
    frame_.generation = ++generation_;
    if (frame_.generation == 0) frame_.generation = ++generation_;
    published_.store(frame_.generation, std::memory_order_release);
}

const ArmFrame* ArmPoser::pending() const noexcept {
    const std::uint32_t g = published_.load(std::memory_order_acquire);
    if (g == 0 || g == applied_.load(std::memory_order_relaxed) || frame_.generation != g ||
        !any_active(frame_))
        return nullptr;
    return &frame_;
}

bool ArmPoser::claim(const ArmFrame& f) noexcept {
    return applied_.exchange(f.generation, std::memory_order_acq_rel) != f.generation;
}

void ArmPoser::apply(const ArmFrame& f, const ModelToWorld& w, PoseArrays& p) noexcept {
    ArmReport r;
    r.generation = f.generation;
    r.stamp      = f.stamp;
    r.fault      = prepare_arms(p, f.rig);
    if (r.fault == PoseFault::None) {
        // 21 §3.5: the grip frames, measured once per skeleton.
        if (grip_skeleton_ != f.skeleton) {
            grip_skeleton_ = f.skeleton;
            for (std::size_t h = 0; h < kHandCount; ++h)
                grips_[h] = grip_frame(finger_bases(p, f.rig.sides[h]), h);
        }
        for (std::size_t h = 0; h < kHandCount; ++h) {
            ArmSideReport& out = r.sides[h];
            out.grip_valid     = grips_[h].has_value();
            if (grips_[h]) out.grip = *grips_[h];
            const ArmSideFrame& side = f.sides[h];
            if (!side.active || !grips_[h]) continue;
            // 21 §4.2 item 10: the target moves with the body since it was placed.
            const WorldPose grip{follow_body(side.grip.position, f.anchor, w.translation),
                                 side.grip.orientation};
            const ArmGoal goal{hand_for_grip(to_model(w, grip), *grips_[h]), side.weight,
                               to_model_direction(w, side.elbow_hint)};
            const ArmOutcome o = pose_arm(p, f.rig.sides[h], goal);
            out.edited         = o.edited;
            out.in_reach       = o.in_reach;
            out.target         = to_world_point(w, o.goal.translation);
        }
        refresh_arms(p, f.rig);
        for (std::size_t h = 0; h < kHandCount; ++h) {
            ArmSideReport& out = r.sides[h];
            if (out.edited)
                out.reached = to_world_point(w, rigid_of(p.model[f.rig.sides[h].hand]).translation);
        }
        // 21 §6.2 (r11): the drawn left hand's direction, its knuckle line's
        // forward from the hand bone to the middle finger's base.
        const ArmSideBones& left = f.rig.sides[kLeftHand];
        if (r.sides[kLeftHand].edited) {
            const Vec3 hand    = to_world_point(w, rigid_of(p.model[left.hand]).translation);
            const Vec3 middle  = to_world_point(w, rigid_of(p.model[left.middle]).translation);
            r.drawn_left       = util::sub(middle, hand);
            r.drawn_left_valid = true;
        }
        r.applied = true;
    }
    report_.store(r);
}

bool ArmPoser::right_edited() const noexcept {
    const ArmReport r = report_.load();
    return r.generation == frame_.generation && r.applied && r.sides[kRightHand].edited;
}

std::optional<std::int16_t> ArmPoser::head_check_due() const noexcept {
    if (head_logged_ || !rig_ || rig_->head < 0) return std::nullopt;
    const ArmReport r = report_.load();
    if (r.generation != frame_.generation || !r.applied) return std::nullopt;
    return rig_->head;
}

void ArmPoser::after_pass(const std::optional<Vec3>& head_world, const CameraPose& camera, bool item_follows,
                          TimePoint now) noexcept {
    const ArmReport r     = report_.load();
    const bool this_frame = r.generation == frame_.generation && any_active(frame_);
    // Set on a frame whose edited right hand the item was placed on, cleared
    // on any other: the aim stops reading the weapon's transform as soon as
    // the game's animation poses the weapon again, not kStaleAfter later.
    item_follows_at_.store(this_frame && item_follows ? util::stamp_ticks(now) : 0,
                           std::memory_order_release);
    if (!this_frame) return;
    log_report(r, camera, head_world, item_follows);
}

void ArmPoser::log_report(const ArmReport& r, const CameraPose& camera, const std::optional<Vec3>& head_world,
                          bool item_follows) noexcept {
    if (r.fault != PoseFault::None) {
        if (pose_fault_logged_ != r.fault) logf(log_, kLogPoseFault, pose_fault_text(r.fault));
        pose_fault_logged_ = r.fault;
        return;
    }
    pose_fault_logged_.reset();
    if (!r.applied) return;
    for (std::size_t h = 0; h < kHandCount; ++h) {
        if (grip_logged_[h]) continue;
        grip_logged_[h]        = true;
        const ArmSideReport& s = r.sides[h];
        if (!s.grip_valid) {
            logf(log_, kLogNoGrip, hand_name(h), rig_name_);
            continue;
        }
        const Vec3 o = s.grip.in_hand.translation;
        logf(log_, kLogGrip, rig_name_, hand_name(h), s.grip.wrist_distance_m, kWristGripMinM, kWristGripMaxM,
             o.x, o.y, o.z);
    }
    if (head_world && rig_ && !head_logged_) {
        head_logged_  = true;
        const Vec3& c = camera.position;
        logf(log_, kLogHead, rig_->head, rig_name_, head_world->x, head_world->y, head_world->z, c.x, c.y,
             c.z, util::distance(*head_world, c), kHeadCameraExpectedM);
    }
    const bool edited = r.sides[kLeftHand].edited || r.sides[kRightHand].edited;
    if (edited && first_edit_logged_.first()) log_.line(kLogFirstEdit);
    if (item_follows && item_logged_.first()) log_.line(kLogItem);
    if (reach_log_.tick() && log_.debug_enabled()) {
        const ArmSideReport& l  = r.sides[kLeftHand];
        const ArmSideReport& rr = r.sides[kRightHand];
        logf(log_, kLogReach, l.target.x, l.target.y, l.target.z, l.reached.x, l.reached.y, l.reached.z,
             util::distance(l.target, l.reached), reach_note(l), rr.target.x, rr.target.y, rr.target.z,
             rr.reached.x, rr.reached.y, rr.reached.z, util::distance(rr.target, rr.reached), reach_note(rr));
    }
}

bool ArmPoser::fresh(std::int64_t stamp, TimePoint now) const noexcept {
    const auto at = util::from_stamp_ticks(stamp);
    return at && now >= *at && now <= util::after(*at, kStaleAfter);
}

std::optional<Vec3> ArmPoser::drawn_left(TimePoint now) const noexcept {
    const ArmReport r = report_.load();
    if (!r.drawn_left_valid || !fresh(r.stamp, now)) return std::nullopt;
    return r.drawn_left;
}

bool ArmPoser::weapon_in_moved_hand(TimePoint now) const noexcept {
    return fresh(item_follows_at_.load(std::memory_order_acquire), now);
}

} // namespace omvr::core::hands
