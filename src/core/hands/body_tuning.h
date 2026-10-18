// OpenMotherVR-Provenance: clean
#pragma once

#include <chrono>

// Behaviour parameters of the body features (spec 21 §10 to §12), with the
// section each one meets. Values marked [proposal] in the spec are the
// spec's.
namespace omvr::core::hands {

using BodySeconds = std::chrono::duration<float>;

// 21 §10.2 [proposal]: a hidden bone's scale. Not 0: skin bound to hidden
// bones only must shrink out of sight (a thousandth of its size is under a
// millimetre for a whole limb) while its normals stay finite, so a
// triangle shared with a drawn bone never shades from a zero normal.
inline constexpr float kCollapseScale = 1.0e-3f;

// 21 §10.2 (r8) [proposal]: a player skeleton that could not be read is
// read again this often until it can be, so a value set before the game
// has the player's skeleton ready (a menu, a level load) takes effect once
// it is; a read costs a walk of at most 256 bones.
inline constexpr BodySeconds kSkeletonRetry{1.0f};

// 21 §11.5 [proposal]: the character crouches when the head's height above
// the floor falls below kCrouchBelow * h and stands when it rises above
// kStandAbove * h (h: the calibrated standing eye height).
inline constexpr float kCrouchBelow = 0.78f;
inline constexpr float kStandAbove  = 0.86f;

// 21 §11.5 [proposal]: a press of the crouch action lasts this long, and
// the next comes no sooner than kCrouchRetry after it.
inline constexpr BodySeconds kCrouchPress{0.1f};
inline constexpr BodySeconds kCrouchRetry{0.5f};
// 21 §11.5: a press the game's requested crouch byte has not followed
// within this time did not take (logged once; no retry of a crouch press).
inline constexpr BodySeconds kCrouchConfirm{1.0f};
// 21 §12.2 item 3 [proposal]: the crouch is re-synced this long after an
// episode ends.
inline constexpr BodySeconds kHandBackSettle{0.5f};

// 21 §2.2, §11.5 [likely]: how far the game lowers the first-person camera
// when the player crouches, game metres: the difference of the game's
// `first_person_camera_offset_y` (0.01) and
// `first_person_camera_crouch_offset_y` (-0.62) in DATA\GLOBALCONSTANTS.TXT.
inline constexpr float kGameCrouchEyeDropM = 0.63f;
// 21 §11.5 [proposal]: the eye compensation follows the game's crouch over
// this time, so the view does not jump when the game's camera moves (the
// game's own transition time is not known, 21 §22 item 14).
inline constexpr BodySeconds kCrouchEyeBlend{0.3f};

} // namespace omvr::core::hands
