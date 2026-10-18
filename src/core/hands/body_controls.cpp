// OpenMotherVR-Provenance: clean
#include "core/hands/body_controls.h"

#include <algorithm>

namespace omvr::core::hands {

namespace {

// A trigger fully pulled: past any threshold the mapper uses for a press.
constexpr float kTriggerFull = 1.0f;

bool pressing(const util::AtomicStamp& until, util::TimePoint now) noexcept {
    const auto end = until.load();
    return end && now < *end;
}

} // namespace

void BodyControls::press_crouch_until(util::TimePoint until) noexcept { crouch_until_.store(until); }

void BodyControls::press_fire_until(util::TimePoint until) noexcept { fire_until_.store(until); }

void BodyControls::write(const input::Controls& c, bool right_hand_near_head) noexcept {
    const util::TimePoint now = clock_.now();
    const bool crouch         = pressing(crouch_until_, now);
    const bool fire           = pressing(fire_until_, now);
    if (!crouch && !fire) {
        next_.write(c, right_hand_near_head);
        return;
    }
    // 04 §5.8.5: the Crouch control is the left trigger's place in the
    // controls and the Fire control the right trigger's; the mapper turns
    // them into map 0's crouch (0x09) and fire (0x0B) actions.
    input::Controls pressed = c;
    if (crouch) pressed.trigger_l = std::max(pressed.trigger_l, kTriggerFull);
    if (fire) pressed.trigger_r = std::max(pressed.trigger_r, kTriggerFull);
    next_.write(pressed, right_hand_near_head);
}

} // namespace omvr::core::hands
