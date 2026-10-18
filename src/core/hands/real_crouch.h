// OpenMotherVR-Provenance: clean
#pragma once

#include "util/time/steady.h"

#include <cstdint>
#include <optional>

// Real crouch (spec 21 §11.5, §12.2 item 3): crouching for real in standing
// play crouches the character, through the game's own crouch action, and
// the view stays where the real head is while the game's crouch lowers its
// eye point. Pure: the game thread hands in the head height, the play mode
// and the player's requested crouch byte (21 §2.2) and starts the presses
// this asks for. Game thread.
namespace omvr::core::hands {

struct RealCrouchInput {
    bool enabled                = false; // Body.RealCrouch
    bool standing               = false; // the applied play mode is Standing (22 §6.1)
    float standing_eye_height_m = 0.0f;  // h, runtime metres; 0: not calibrated (22 §6.2)
    // The head's height above the floor, runtime metres; none without a
    // floor reference or while the headset is not in use (21 §19).
    std::optional<float> head_height_m;
    // First-person play the player controls (21 §12.2: no episode, alive).
    bool controllable = false;
    // The player's requested crouch byte (21 §2.2); none without a player.
    std::optional<bool> game_crouched;
};

enum class CrouchPress : std::uint8_t { None, Crouch, Stand };

struct RealCrouchOutput {
    CrouchPress press = CrouchPress::None; // start a press of the crouch action now
    // Added to the height the game receives, game metres (21 §11.5: no
    // double drop while the game's crouch was engaged by real crouch).
    float eye_raise_m = 0.0f;
    // A press the game's crouch byte did not follow within kCrouchConfirm.
    bool press_not_followed = false;
    // The thresholds this frame, runtime metres, for the press's log line.
    float crouch_below_m = 0.0f;
    float stand_above_m  = 0.0f;
};

class RealCrouch final {
public:
    RealCrouchOutput update(const RealCrouchInput& in, util::TimePoint now) noexcept;

    // 21 §12.2 item 3: an episode ended; the crouch is compared with the
    // head kHandBackSettle later and re-synced by one press if they differ.
    void on_hand_back(util::TimePoint now) noexcept;

    // Real crouch applies this frame (standing, calibrated, a head height).
    bool active() const noexcept { return active_; }

private:
    void press(bool crouch, util::TimePoint now, RealCrouchOutput& out) noexcept;
    void follow_press(bool game_crouched, util::TimePoint now, RealCrouchOutput& out) noexcept;
    void ease_raise(bool raised, util::TimePoint now) noexcept;
    void reset() noexcept;

    struct PendingCheck {
        bool crouch = false;
        util::TimePoint at{};
    };

    bool active_ = false;
    std::optional<bool> wanted_; // the head's state, with hysteresis
    bool crossing_ = false;      // a threshold crossing not yet matched
    std::optional<util::TimePoint> last_press_;
    std::optional<PendingCheck> check_; // the last press, until the byte follows
    std::optional<util::TimePoint> resync_at_;
    bool followed_once_ = false; // a press of this session was followed
    bool engaged_       = false; // the game crouched on a press of ours
    float raise_m_      = 0.0f;
    std::optional<util::TimePoint> last_update_;
};

} // namespace omvr::core::hands
