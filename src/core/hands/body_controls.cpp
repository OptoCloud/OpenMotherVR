// OpenMotherVR-Provenance: clean
#include "core/hands/body_controls.h"

#include <algorithm>

namespace omvr::core::hands {

namespace {

// A trigger fully pulled: past any threshold the mapper uses for a press.
constexpr float kTriggerFull = 1.0f;

} // namespace

void BodyControls::press_crouch_until(std::chrono::steady_clock::time_point until) noexcept {
    crouch_until_.store(until.time_since_epoch().count(), std::memory_order_relaxed);
}

void BodyControls::write(const input::Controls& c, bool right_hand_near_head) noexcept {
    const std::int64_t until = crouch_until_.load(std::memory_order_relaxed);
    if (until == 0 || clock_.now().time_since_epoch().count() >= until) {
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
