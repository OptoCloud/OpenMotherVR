// OpenMotherVR-Provenance: clean
#pragma once

#include "core/input/types.h"

#include <optional>

// D-pad emulation (22 §7): the left stick's direction as a d-pad press while
// the stick does not walk the player, or while the d-pad modifier is held.
namespace omvr::core::input {

// The direction of `stick` as a d-pad, one direction at a time, with the
// hysteresis of 22 §7: `held` (the direction pressed so far) stays pressed
// until its axis falls below kDpadRelease; otherwise a direction is pressed
// once its axis is beyond kDpadPress and the larger of the two. Pure.
[[nodiscard]] Dpad stick_dpad(Vec2 stick, Dpad held) noexcept;

// What the emulation makes of one frame of controls.
struct DpadEmulationResult {
    Dpad dpad;                  // the emulated press
    bool stick_used    = false; // the stick gives a press: its movement value is 0
    bool modifier_held = false; // the d-pad modifier is engaged: the click is not sprint
};

// Render thread (the controls write).
class DpadEmulation final {
public:
    // `stick_free`: an interaction, the breathing game or a menu on the
    // virtual screen, where the left stick does not walk the player.
    // `enabled`: Controls.DpadEmulation; off resets the state and gives no
    // press (v1).
    [[nodiscard]] DpadEmulationResult update(const Controls& c, bool stick_free, bool enabled,
                                             TimePoint now) noexcept;

private:
    [[nodiscard]] bool modifier_engaged(const Controls& c, TimePoint now) noexcept;

    Dpad held_{};
    // When the thumbstick scheme's modifier (the left stick click) went down.
    std::optional<TimePoint> modifier_since_;
};

} // namespace omvr::core::input
