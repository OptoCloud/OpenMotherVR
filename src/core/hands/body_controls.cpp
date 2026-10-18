// OpenMotherVR-Provenance: clean
#include "core/hands/body_controls.h"

#include <algorithm>

namespace omvr::core::hands {

namespace {

// A trigger fully pulled: past any threshold the mapper uses for a press.
constexpr float kTriggerFull = 1.0f;

} // namespace

void BodyControls::press_crouch_until(util::TimePoint until) noexcept { crouch_until_.store(until); }

void BodyControls::write(const input::Controls& c, bool right_hand_near_head) noexcept {
    const auto until = crouch_until_.load();
    if (!until || clock_.now() >= *until) {
        next_.write(c, right_hand_near_head);
        return;
    }
    // 04 §5.8.5: the Crouch control is the left trigger's place in the
    // controls; the mapper turns it into map 0's crouch action (0x09).
    input::Controls pressed = c;
    pressed.trigger_l       = std::max(pressed.trigger_l, kTriggerFull);
    next_.write(pressed, right_hand_near_head);
}

} // namespace omvr::core::hands
