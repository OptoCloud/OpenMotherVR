#include "core/input/controller_mapping.h"

#include "core/input/touchpad.h"
#include "core/input/tuning.h"

namespace omvr::core::input {

namespace {

// What both schemes read the same way.
Controls common_controls(const HandInput& left, const HandInput& right) noexcept {
    Controls c;
    c.trigger_l = left.trigger;
    c.trigger_r = right.trigger;
    c.grip_l    = left.grip;
    c.grip_r    = right.grip;
    c.sprint    = left.stick_click;
    // A d-pad on either controller (the Steam Frame has one on the left).
    c.dpad = {.up    = left.dpad.up || right.dpad.up,
              .down  = left.dpad.down || right.dpad.down,
              .left  = left.dpad.left || right.dpad.left,
              .right = left.dpad.right || right.dpad.right};
    return c;
}

} // namespace

Controls touchpad_controls(const HandInput& left, const HandInput& right) noexcept {
    Controls c        = common_controls(left, right);
    c.touchpad_scheme = true;
    if (left.stick_touched) c.move = left.stick;
    if (right.stick_touched) c.turn = right.stick;
    c.turn_engaged                = right.stick_click;
    const TouchpadRegions regions = decode_touchpad(right.stick, right.stick_click);
    c.interact                    = regions.interact;
    c.reload                      = regions.reload;
    c.radial                      = regions.radial;
    c.map                         = left.menu;
    c.pause                       = right.menu;
    return c;
}

Controls thumbstick_controls(const HandInput& left, const HandInput& right) noexcept {
    Controls c   = common_controls(left, right);
    c.move       = left.stick;
    c.turn       = right.stick;
    c.radial     = right.stick_click;
    c.interact   = right.primary;
    c.reload     = right.secondary;
    c.map        = left.primary;
    c.flashlight = left.secondary;
    c.pause      = left.menu || right.menu;
    return c;
}

bool squeeze_pressed(float squeeze) noexcept { return squeeze > kSqueezePressed; }

bool near_head(const util::Vec3& hand, const util::Vec3& head) noexcept {
    return util::length(util::sub(hand, head)) < kNearHead;
}

} // namespace omvr::core::input
