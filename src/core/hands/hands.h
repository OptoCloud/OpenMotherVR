// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/log.h"
#include "core/diag/tuning.h"
#include "core/hands/action_episode.h"
#include "core/hands/arm_poser.h"
#include "core/hands/activity.h"
#include "core/hands/hand_track.h"
#include "core/hands/pose_filter.h"
#include "core/hands/hand_aim.h"
#include "core/hands/motion_actions.h"
#include "core/hands/types.h"
#include "game/constants.h"
#include "util/sync/change_latch.h"
#include "util/sync/frame_period.h"
#include "util/sync/once_latch.h"
#include "util/sync/seqlock.h"

#include <array>
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

// What the game thread reads of the player's item and controls with each
// head pose, for melee and throwing by motion (21 §8.2, §9.2).
struct PlayInput {
    bool item_known = false;                // the current item was read (the hand sites are in place)
    std::optional<game::WeaponType> weapon; // its weapon type; nullopt: no weapon object
    bool aim_held = false;                  // the Aim control as the game is given it
};

// What an update asks of the game's controls.
struct HandsOutput {
    std::optional<TimePoint> press_fire_until; // a swing or a throw by motion (21 §8.2, §9.2)
};

class Hands final {
public:
    Hands(const config::HandsSettings& settings, diag::ports::Log& log) noexcept
        : settings_(settings), log_(log), arms_(log) {}

    // The arm pose edit (21 §4.2): the targets the arms reach, the edit on
    // the final-pose job and what it drew.
    ArmPoser& arms() noexcept { return arms_; }
    const ArmPoser& arms() const noexcept { return arms_; }

    // 21 §3.1, §4.2: each tracked hand's grip target in the world through
    // `camera` (the active camera now) and its weight (21 §3.3 times the
    // activity, and the melee hand-off for the right arm), for a hand whose
    // weight is above 0 in a recent update. Game thread.
    ArmTargets arm_targets(const CameraPose& camera, TimePoint now) const noexcept;

    // Game thread, at each head pose the game asks for: `frame` is that pose
    // and both hands of its prediction, `world_scale` the head's scale.
    // With Hands.Enabled off nothing runs (v1); turning it off in play is
    // logged once.
    // `play` feeds melee and throwing by motion; the result is the fire
    // press they ask for.
    HandsOutput update(const HandFrame& frame, const ActivityInput& in, const PlayInput& play, TimePoint now,
                       float world_scale) noexcept;

    // Any thread (the melee action's start methods, 21 §2.11): the
    // player's melee action started; the right arm goes to the game's
    // swing (21 §8.2, r11).
    void on_melee_start(TimePoint now) noexcept { melee_started_.store(now); }

    // Any thread (the scripted actions' start methods, 21 §2.11): a player
    // action started (21 §12.1, r11).
    void on_player_action(PlayerAction a, TimePoint now) noexcept { episode_.on_start(a, now); }

    // Game thread: a player action episode runs or has just started, so the
    // current item is worth reading for player_action().
    bool player_action_wanted() const noexcept { return episode_.pending_or_running(); }

    // Game thread, each frame (from the activity signals of the hands and
    // the body): `item` the current item (0: none), `follow_active` the
    // first-person camera. True while a player action episode runs.
    bool player_action(std::uintptr_t item, bool follow_active, TimePoint now) noexcept;

    // Any thread: the last throw by motion, while it is within
    // kThrowLaunchWatch of `now` (for the launch diagnostic, 21 §22 item 8).
    std::optional<ThrowRelease> recent_throw(TimePoint now) const noexcept;

    struct AimResult {
        std::optional<AimRay> ray; // the ray to write, or nullopt: the game's stays
        bool hand_aiming = false;  // the hand drives the ray (21 §5.3: the fire gate passes)
    };

    // The right hand may aim now (Hands.Enabled, Hands.AimFromHand, a recent
    // update with the hand's weight above 0): worth reading the weapon for
    // aim(). Game thread.
    bool aim_wanted(TimePoint now) const noexcept;

    // 21 §5.2: the player's aim ray right after the game wrote `game` from
    // `camera`, for the current item `weapon`. The revolver, shotgun and
    // flamethrower aim along the weapon's transform while it is drawn in
    // the moved hand and the transform can be used; otherwise, and for
    // every other class that aims from the hand, along the right hand's
    // aim pose. Game thread.
    AimResult aim(const AimRay& game, const CameraPose& camera, const WeaponSample& weapon,
                  TimePoint now) noexcept;

    // The weapon's transform is worth reading for aim() this frame: it is
    // the aim's source (21 §5.2), or, with Dev. Debug Options, it was not
    // yet logged for this weapon type (21 §18). Game thread.
    bool weapon_transform_wanted(std::optional<game::WeaponType> type, bool drawn_in_moved_hand,
                                 TimePoint now) const noexcept;

    // 21 §6.2: the horizontal facing the tracker scan should read now, or
    // nullopt to leave the game's. `camera` is the active camera; `drawn`
    // the drawn left hand's world direction from the pose the player saw
    // last frame (r11), nullopt when that pose is not available. One
    // thread (the one the game runs the scan on).
    std::optional<util::Vec2> tracker_facing(const CameraPose& camera, const std::optional<util::Vec3>& drawn,
                                             TimePoint now) noexcept;

private:
    struct HandView {
        HeadLocalPose grip{};
        HeadLocalPose aim{};
        float weight  = 0.0f; // 21 §3.3 times the activity blend
        bool has_grip = false;
        bool has_aim  = false;
    };
    struct View {
        std::int64_t stamp = 0; // util::stamp_ticks of the update; 0: none
        std::array<HandView, kHandCount> hands{};
    };

    void update_hand(std::size_t h, const HandFrame& frame, float dt_s, float world_scale) noexcept;
    void note_activity(Activity a) noexcept;
    void publish(TimePoint now, float activity) noexcept;
    HandsOutput motion_actions(const HandFrame& frame, const PlayInput& play, bool active, TimePoint now,
                               float world_scale) noexcept;
    void take_melee_start(TimePoint now) noexcept;
    void note_throw(const ThrowRelease& r) noexcept;
    void log_weapon_transform(const WeaponSample& weapon, const HandView& right,
                              const CameraPose& camera) noexcept;
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
    MeleeByMotion melee_;
    ThrowByMotion throw_;
    ArmHandOff arm_hand_off_;
    std::optional<TimePoint> melee_seen_; // the last melee start taken up
    bool hand_off_noted_ = false;
    ActionEpisode episode_; // also the activity signals' (game thread)

    // Written by the melee start methods' thread.
    util::AtomicStamp melee_started_;
    // Game thread to any thread: the last throw and when (stamp ticks).
    struct ThrowView {
        std::int64_t stamp = 0;
        ThrowRelease release{};
    };
    util::SeqLock<ThrowView> throw_view_;

    // Game thread (aim).
    util::ChangeLatch<WeaponClass> class_logged_;
    util::FramePeriod aim_log_{diag::kDebugLogPeriodFrames};
    util::OnceLatch fire_gate_noted_;
    util::OnceLatch no_aim_noted_;
    std::uint32_t transform_logged_ = 0; // a bit per weapon type whose transform was logged (21 §18)

    // The tracker scan's thread.
    TrackerFacing tracker_;

    ArmPoser arms_;
};

} // namespace omvr::core::hands
