// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/hands/body_visibility.h"
#include "core/hands/types.h"
#include "util/sync/once_latch.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string_view>

// Body visibility over frames (spec 21 §10, §17, §18): the game thread
// builds the collapse plan for the player's skeleton and the Body.Visible
// value, and publishes it with the pose it belongs to; the job thread that
// runs the player's final-pose step reads the published plan only. Game
// objects are named by their addresses as plain integers, never held as
// pointers.
namespace omvr::core::hands {

// What the job thread reads: the plan for one pose of one skeleton.
struct HiddenPose {
    std::uintptr_t animation = 0; // the animation component the pose is of
    std::uintptr_t pose      = 0;
    std::uintptr_t skeleton  = 0;
    CollapsePlan plan{};
};

// The player's skeleton as the game thread found it this frame.
struct PlayerSkeleton {
    std::uintptr_t animation = 0;
    std::uintptr_t pose      = 0; // 0: no player pose
    std::uintptr_t skeleton  = 0;
    std::string_view name; // the skeleton's own name, for the log
};

// Why the game side could not read the player's skeleton (21 §10.2, §18),
// for the log.
enum class SkeletonFault : std::uint8_t {
    Unknown,      // not said
    NoBones,      // the bone count is not above 0
    NoArrays,     // the bone or parent array pointer is null
    TooManyBones, // more bones than the reader holds
    UnnamedBone,  // a bone without a name
    // A skeleton of a first-person table's name whose bone count or parents
    // are not the table's (21 §2.3, r10): rejected, not posed with the
    // table's bone meanings.
    TableMismatch,
};
struct SkeletonReadFault {
    SkeletonFault fault = SkeletonFault::Unknown;
    std::int32_t count  = 0; // the skeleton's bone count as read
    // UnnamedBone: the first bone without a name; TableMismatch: the first
    // index whose parent differs from the table's.
    std::size_t bone = 0;
};

class BodyHider final {
public:
    explicit BodyHider(diag::ports::Log& log) noexcept : log_(log) {}

    // Game thread. Whether update() needs the bones of `skeleton` this frame:
    // a new skeleton, pose or value while one is to be hidden, or, after a
    // skeleton could not be read, every kSkeletonRetry until it is (21 §10.2).
    // Never while the stereo device is not `running`: update() hides
    // nothing then and records no build, so the bones would be read every
    // frame for nothing.
    bool wants_bones(config::BodyVisible v, bool running, const PlayerSkeleton& s,
                     TimePoint now) const noexcept;
    // The same with the device taken as running. For a caller that does not
    // pass `running` yet; the BodyHider hook moves to the overload above.
    bool wants_bones(config::BodyVisible v, const PlayerSkeleton& s, TimePoint now) const noexcept {
        return wants_bones(v, true, s, now);
    }

    // Game thread, each frame. `running`: the stereo device runs (20 §1.2:
    // nothing hidden otherwise). `bones` as read for `s.skeleton` when
    // wants_bones asked for them; null when they were not asked for or could
    // not be read, and then `fault` says why. Logs once per skeleton, per
    // change of what is hidden, and an unreadable skeleton once until the
    // skeleton, pose, value or fault changes (21 §18).
    void update(config::BodyVisible v, bool running, const PlayerSkeleton& s, const SkeletonBones* bones,
                TimePoint now, const SkeletonReadFault& fault = {}) noexcept;

    // Job threads: the plan to apply, or null when nothing is hidden. Valid
    // until the game thread's next update; the final-pose pass returns only
    // after its jobs finish (21 §2.3, §17), so no job outlives it.
    const HiddenPose* current() const noexcept { return current_.load(std::memory_order_acquire); }

    // Job threads report what they saw; the game thread logs it once.
    void note_applied() noexcept { applied_.store(true, std::memory_order_relaxed); }
    void note_other_pose() noexcept { other_pose_.store(true, std::memory_order_relaxed); }

private:
    void build(config::BodyVisible v, const PlayerSkeleton& s, const SkeletonBones& bones) noexcept;
    void log_skeleton(const PlayerSkeleton& s, const SkeletonBones& bones, PlanResult r) noexcept;
    void log_job_reports() noexcept;
    void log_unreadable(const PlayerSkeleton& s, const SkeletonReadFault& fault) noexcept;

    diag::ports::Log& log_;
    std::array<HiddenPose, 2> buffers_{};
    std::size_t next_ = 0; // the buffer the next build writes
    std::atomic<const HiddenPose*> current_{nullptr};

    // What a build was for: the value, skeleton and pose.
    struct BuildKey {
        config::BodyVisible value                                = config::BodyVisible::Full;
        std::uintptr_t skeleton                                  = 0;
        std::uintptr_t pose                                      = 0;
        friend bool operator==(const BuildKey&, const BuildKey&) = default;
    };
    // `v` and `s` differ from the last build, or there was none.
    bool changed(config::BodyVisible v, const PlayerSkeleton& s) const noexcept {
        return built_ != BuildKey{v, s.skeleton, s.pose};
    }

    // The last build (none: not yet), and whether it gave a plan.
    std::optional<BuildKey> built_;
    bool built_ok_ = false;
    // The last build could not read the skeleton: when to read it again, and
    // the fault last logged for this skeleton, pose and value (none: not yet).
    std::optional<TimePoint> retry_at_;
    std::optional<SkeletonFault> unreadable_logged_;
    // The skeletons already logged (21 §18: the first time each is seen).
    std::array<std::uintptr_t, 8> logged_skeletons_{};
    std::size_t logged_count_ = 0;

    std::atomic<bool> applied_{false};
    std::atomic<bool> other_pose_{false};
    util::OnceLatch applied_logged_;
    util::OnceLatch other_pose_logged_;
};

} // namespace omvr::core::hands
