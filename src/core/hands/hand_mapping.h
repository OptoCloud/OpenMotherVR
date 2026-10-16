// OpenMotherVR-Provenance: clean
#pragma once

#include "core/hands/types.h"
#include "util/math/matrix.h"
#include "util/math/vec.h"

// The hand mapping of spec 21 §3.1: from the runtime's head and hand poses
// to a pose in the game's world, relative to the eye the player sees from.
namespace omvr::core::hands {

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

} // namespace omvr::core::hands
