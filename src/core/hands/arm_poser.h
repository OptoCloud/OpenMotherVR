// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/diag/tuning.h"
#include "core/hands/arm_pose.h"
#include "core/hands/skeleton.h"
#include "core/hands/types.h"
#include "util/sync/frame_period.h"
#include "util/sync/once_latch.h"
#include "util/sync/seqlock.h"
#include "util/time/steady.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

// The arm pose edit over frames (spec 21 §4.2, §17, §18): the game thread
// finds the arms in the player's skeleton, places each tracked hand's grip
// target in the world just before the frame's final-pose pass and publishes
// it; the job thread that runs the player's final-pose step edits the
// player's pose in place from that copy only and reports what it reached;
// the game thread logs the report after the pass, and the aim and the
// tracker read what was drawn. Game objects are named by their addresses as
// plain integers, never held as pointers.
namespace omvr::core::hands {

// A tracked hand's grip target in the world and its weight (21 §3.1,
// §3.3).
struct ArmTarget {
    WorldPose grip{};
    float weight = 0.0f;
};
using ArmTargets = std::array<std::optional<ArmTarget>, kHandCount>;

// What the job reads for one frame: the player's pose, its arms and each
// side's target.
struct ArmSideFrame {
    bool active = false;
    WorldPose grip{};
    float weight = 0.0f;
    util::Vec3 elbow_hint{}; // world direction (21 §4.2 item 4)
};
struct ArmFrame {
    std::uint32_t generation = 0;
    std::uintptr_t animation = 0;
    std::uintptr_t pose      = 0;
    std::uintptr_t skeleton  = 0;
    std::int64_t stamp       = 0; // util::stamp_ticks of the game thread's time
    // 21 §4.2 item 10: the body's model-to-world translation when the
    // targets were placed.
    util::Vec3 anchor{};
    ArmRig rig{};
    std::array<ArmSideFrame, kHandCount> sides{};
};

// What the job reports for one frame.
struct ArmSideReport {
    bool edited     = false;
    bool in_reach   = false;
    bool grip_valid = false;
    util::Vec3 target{};  // the hand bone's goal, world
    util::Vec3 reached{}; // the hand bone after the edit, world
    GripFrame grip{};
};
struct ArmReport {
    std::uint32_t generation = 0;
    std::int64_t stamp       = 0;
    PoseFault fault          = PoseFault::None;
    bool applied             = false;
    std::array<ArmSideReport, kHandCount> sides{};
    bool drawn_left_valid = false;
    util::Vec3 drawn_left{}; // LeftHand to LeftHandMiddle1, world (21 §6.2)
};

class ArmPoser final {
public:
    explicit ArmPoser(diag::ports::Log& log) noexcept : log_(log) {}

    // Game thread. Whether set_rig needs the bones of `s` now: a new
    // skeleton or pose, or, after one could not be used for a reason the
    // game may still change (it could not be read), every kSkeletonRetry.
    bool wants_rig(const PlayerSkeleton& s, TimePoint now) const noexcept;

    // Game thread: the arms of `s`'s skeleton from `bones` (null when they
    // could not be read; `fault` says why). Logs once per skeleton what was
    // found (21 §18), or why the arms keep the game's animation (21 §4.6).
    void set_rig(const PlayerSkeleton& s, const SkeletonBones* bones, const SkeletonReadFault& fault,
                 TimePoint now) noexcept;

    // Game thread, just before the frame's final-pose pass: this frame's
    // targets for the player's pose `s`, placed in the world by `camera`
    // while the body's model-to-world transform is `w`. A side without a
    // target (or no rig for `s`) is left to the game's animation.
    void publish(const PlayerSkeleton& s, const CameraPose& camera, const ModelToWorld& w,
                 const ArmTargets& targets, TimePoint now) noexcept;
    // Game thread: nothing to edit this frame.
    void publish_none() noexcept;

    // Job threads: the frame to apply, or null when there is none that is
    // not yet applied. claim() is true for one caller per frame.
    const ArmFrame* pending() const noexcept;
    bool claim(const ArmFrame& f) noexcept;
    // The player's final-pose job: edits `p` (the player's pose) for `f`
    // with the body's model-to-world transform `w` as it is now. No
    // allocation, no logging (21 §17).
    void apply(const ArmFrame& f, const ModelToWorld& w, PoseArrays& p) noexcept;

    // Game thread, after the pass: the right arm was edited this frame (its
    // held item is to follow it, 21 §4.2 item 6).
    bool right_edited() const noexcept;
    // Game thread, after the pass: the bone whose world position is to be
    // logged next to the camera once per skeleton (21 §21 item 3), if due.
    std::optional<std::int16_t> head_check_due() const noexcept;
    // Game thread, after the pass: logs this frame's report as 21 §18
    // says; `head_world` the Head bone from BoneWorldTransform when asked
    // for, `item_follows` whether the held item was placed on the edited
    // hand.
    void after_pass(const std::optional<util::Vec3>& head_world, const CameraPose& camera, bool item_follows,
                    TimePoint now) noexcept;

    // Any thread (the game's limb IK, 21 §4.2 item 7): the limb IK is not
    // to run on a pose of `skeleton`: a first-person skeleton whose arms
    // are edited.
    bool limb_ik_off(std::uintptr_t skeleton) const noexcept {
        return skeleton != 0 && limb_ik_off_.load(std::memory_order_acquire) == skeleton;
    }

    // Any thread (the tracker scan, 21 §6.2 r11): the drawn left hand's
    // world direction from the last edited pose, when that is recent and
    // the left arm was edited.
    std::optional<util::Vec3> drawn_left(TimePoint now) const noexcept;

    // Any thread (the aim, 21 §5.2): the held item is drawn in the moved
    // right hand (recently edited and placed on it).
    bool weapon_in_moved_hand(TimePoint now) const noexcept;

private:
    void log_rig(const PlayerSkeleton& s, const RigResult& r) noexcept;
    void log_report(const ArmReport& r, const CameraPose& camera, const std::optional<util::Vec3>& head_world,
                    bool item_follows) noexcept;
    bool fresh(std::int64_t stamp, TimePoint now) const noexcept;

    diag::ports::Log& log_;

    // Game thread.
    struct RigKey {
        std::uintptr_t skeleton                              = 0;
        std::uintptr_t pose                                  = 0;
        friend bool operator==(const RigKey&, const RigKey&) = default;
    };
    std::optional<RigKey> rig_key_;
    std::optional<ArmRig> rig_;
    std::string_view rig_name_;
    std::optional<TimePoint> rig_retry_at_;
    // What was last logged about the skeleton: kStateFound, kStateUnreadable
    // or a RigFault's value (none: nothing yet for this skeleton).
    static constexpr std::uint8_t kStateFound      = 0xFE;
    static constexpr std::uint8_t kStateUnreadable = 0xFF;
    std::optional<std::uint8_t> logged_state_;
    std::array<bool, kHandCount> grip_logged_{};
    bool head_logged_ = false;
    std::optional<PoseFault> pose_fault_logged_;
    util::OnceLatch first_edit_logged_;
    util::OnceLatch item_logged_;
    util::FramePeriod reach_log_{diag::kDebugLogPeriodFrames};
    std::uint32_t generation_ = 0;

    // Written by the game thread before the pass, read by its jobs; the
    // pass returns only after its jobs (21 §2.3), so no job reads a frame
    // the game thread is writing (21 §17).
    ArmFrame frame_{};
    std::atomic<std::uint32_t> published_{0};
    std::atomic<std::uint32_t> applied_{0};
    std::atomic<std::uintptr_t> limb_ik_off_{0};

    // The player's job only (one per frame, the frames one after the other):
    // the grip frames of the skeleton last edited.
    std::uintptr_t grip_skeleton_ = 0;
    std::array<std::optional<GripFrame>, kHandCount> grips_{};

    // The job to the game thread and the aim's and tracker's threads.
    util::SeqLock<ArmReport> report_;
    // Game thread to the aim: when the held item was last placed on the
    // edited right hand (stamp ticks).
    std::atomic<std::int64_t> item_follows_at_{0};
};

} // namespace omvr::core::hands
