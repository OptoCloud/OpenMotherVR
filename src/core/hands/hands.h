// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/diag/tuning.h"
#include "core/hands/activity.h"
#include "core/hands/hand_track.h"
#include "core/hands/pose_filter.h"
#include "core/hands/tracker_facing.h"
#include "core/hands/types.h"
#include "game/constants.h"
#include "util/sync/change_latch.h"
#include "util/sync/frame_period.h"
#include "util/sync/seqlock.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

// The tracked hands (spec 21 §3, §4.4) and the features that use them: aim
// from the hand (§5) and the motion tracker in the left hand (§6).
//
// The game thread hands in each head pose the game asks for with the hand
// poses of the same prediction (update); the hands' poses relative to the
// head, their weights and the activity are then published torn-free
// (21 §3.4) for the game's own calls that read them later in the frame
// (aim, tracker_facing), which place them in the world through the active
// camera of that moment (§3.1).
namespace omvr::core::hands {

using TimePoint = std::chrono::steady_clock::time_point;

class Hands final {
public:
    Hands(const config::HandsSettings& settings, diag::ports::Log& log) noexcept
        : settings_(settings), log_(log) {}

    // Game thread, at each head pose the game asks for: `frame` is that pose
    // and both hands of its prediction, `world_scale` the head's scale.
    // With Hands.Enabled off nothing runs (v1); turning it off in play is
    // logged once.
    void update(const HandFrame& frame, const ActivityInput& in, TimePoint now, float world_scale) noexcept;

    struct AimResult {
        std::optional<AimRay> ray; // the ray to write, or nullopt: the game's stays
        bool hand_aiming = false;  // the hand drives the ray (21 §5.3: the fire gate passes)
    };

    // The right hand may aim now (Hands.Enabled, Hands.AimFromHand, a recent
    // update with the hand's weight above 0): worth reading the weapon for
    // aim(). Game thread.
    bool aim_wanted(TimePoint now) const noexcept;

    // 21 §5.2: the player's aim ray right after the game wrote `game` from
    // `camera`, for the current item's weapon type `type` (nullopt: no
    // weapon object). Game thread.
    AimResult aim(const AimRay& game, const CameraPose& camera, std::optional<game::WeaponType> type,
                  TimePoint now) noexcept;

    // 21 §6.2: the horizontal facing the tracker scan should read now, or
    // nullopt to leave the game's. `camera` is the active camera. One
    // thread (the one the game runs the scan on).
    std::optional<util::Vec2> tracker_facing(const CameraPose& camera, TimePoint now) noexcept;

private:
    struct HandView {
        HeadLocalPose grip{};
        HeadLocalPose aim{};
        float weight  = 0.0f; // 21 §3.3 times the activity blend
        bool has_grip = false;
        bool has_aim  = false;
    };
    struct View {
        std::int64_t stamp = 0; // steady_clock ticks of the update; 0: none
        std::array<HandView, kHandCount> hands{};
    };

    void update_hand(std::size_t h, const HandFrame& frame, float dt_s, float world_scale) noexcept;
    void note_activity(Activity a) noexcept;
    void publish(TimePoint now, float activity) noexcept;
    std::optional<View> fresh_view(TimePoint now) const noexcept;

    const config::HandsSettings& settings_;
    diag::ports::Log& log_;

    // Game thread (update).
    std::optional<TimePoint> last_update_;
    std::array<PoseFilter, kHandCount> grip_filter_{};
    std::array<PoseFilter, kHandCount> aim_filter_{};
    std::array<HandTrack, kHandCount> track_{};
    Blend activity_;
    util::ChangeLatch<Activity> activity_logged_;
    bool off_noted_ = true; // Hands.Enabled off at start is v1: nothing to say

    util::SeqLock<View> view_;

    // Game thread (aim).
    util::ChangeLatch<WeaponClass> class_logged_;
    util::FramePeriod aim_log_{diag::kDebugLogPeriodFrames};
    bool fire_gate_noted_ = false;
    bool no_aim_noted_    = false;

    // The tracker scan's thread.
    TrackerFacing tracker_;
};

} // namespace omvr::core::hands
