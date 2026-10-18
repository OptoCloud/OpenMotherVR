// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/types.h"
#include "game/constants.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"

#include <optional>

// The hand in the game's world and what it aims (spec 21 §3.1, §5, §6.2):
// the hand mapping from the runtime's head and hand poses to a pose
// relative to the eye the player sees from and into the world; aiming from
// the hand (which weapons aim from it, the aim ray that replaces the game's
// head ray); and the motion tracker's facing from the left hand. Game
// thread, the tracker on the scan's thread.
namespace omvr::core::hands {

// ---- Hand mapping (21 §3.1) ----------------------------------------------------

// The hand's pose relative to the head, in the camera's axes (x right, y up,
// +z forward; positions scaled by the world scale like the head):
//   d'   = (d.x, d.y, -d.z) * s,  d = Rh^T (pg - ph)
//   Rrel = M (Rh^T Rg) M,         M = diag(1, 1, -1)
// with H and G in the runtime's tracking space. `head` and `hand` are in the
// convention the stereo device hands the game (to_game_pose: x and y basis
// columns negated, position times -s), where both reduce to
//   d' = Rh'^T (pg' - ph'),  Rrel = Rh'^T Rg'
// (to_game_pose's diag(-1, -1, 1) and M differ by a sign, which cancels).
HeadLocalPose head_local(const util::Matrix34& head, const util::Matrix34& hand) noexcept;

// 21 §3.1: Pw = Pc + rotate(Qc, d'), Rw = Qc * Rrel.
WorldPose to_world(const CameraPose& camera, const HeadLocalPose& local) noexcept;

// The direction a world pose points: its local +Z (the game's camera looks
// along its local +Z, 21 §2.4; a runtime pose's -Z maps to it, §3.1).
util::Vec3 pointing(const WorldPose& pose) noexcept;

// ---- Aim from the hand (21 §5) -------------------------------------------------

// The class of 21 §5.2 for the current item's weapon type; nullopt is no
// weapon object (throwables, flare, medikit).
WeaponClass weapon_class(std::optional<game::WeaponType> type) noexcept;

// The weapons that fire: barrel and nozzle (21 §5.2, §14.2).
[[nodiscard]] constexpr bool is_ranged(WeaponClass c) noexcept {
    return c == WeaponClass::Barrel || c == WeaponClass::Nozzle;
}

// The class aims from the hand: ranged weapons and no weapon; contact
// weapons and unknown types keep the game's ray (21 §5.2).
bool aims_from_hand(WeaponClass c) noexcept;

// From the hand's aim pose to the muzzle or nozzle along the aim
// direction, in game world metres (21 §5.2, §22 item 5).
float muzzle_offset(WeaponClass c) noexcept;

// The aim ray of 21 §5.2 from the hand's world aim pose `aim`: it starts
// `offset` along the aim direction, points along it, and ends at the range
// the game used (the length of the game's ray). `weight` (21 §3.3) blends
// from the game's ray (0) to the hand's (1), so a hand that is lost hands
// the aim back to the head smoothly. The direction is unit length.
AimRay hand_aim_ray(const AimRay& game, const WorldPose& aim, float weight, float offset) noexcept;

// The threshold the fire alignment test compares with (21 §2.5, §5.3):
// while the hand aims, one every length passes; otherwise the game's own.
float fire_gate_threshold(bool hand_aiming, float game_threshold) noexcept;

// ---- Tracker facing (21 §6.2) --------------------------------------------------

// The horizontal facing (world x, z; unit length) the tracker scan should
// read for the hand's world aim direction `dir`: its horizontal part. When
// that is shorter than kTrackerMinHorizontal (the hand points nearly
// straight up or down) the previous facing is kept; before any, nullopt
// (the game's own facing stays). Never NaN.
class TrackerFacing final {
public:
    std::optional<util::Vec2> update(const util::Vec3& dir) noexcept;
    // The hand stopped driving the tracker: no previous facing to keep.
    void reset() noexcept { last_.reset(); }

private:
    std::optional<util::Vec2> last_;
};

} // namespace omvr::core::hands
