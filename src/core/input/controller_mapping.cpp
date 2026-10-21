#include "core/input/controller_mapping.h"

#include "core/input/tuning.h"

#include <cmath>

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
    c.dpad = left.dpad | right.dpad;
    return c;
}

} // namespace

TouchpadRegions decode_touchpad(Vec2 pad, bool click) noexcept {
    TouchpadRegions r;
    r.horizontal = std::fabs(pad.x) > std::fabs(pad.y);
    r.centre     = std::fabs(pad.x) < kTouchpadCentre && std::fabs(pad.y) < kTouchpadCentre;
    // A click on the left or right edge drives none of the three regions.
    const bool vertical_edge = click && !r.horizontal && !r.centre;
    r.interact               = vertical_edge && pad.y < 0.0f;
    r.reload                 = vertical_edge && pad.y > 0.0f;
    r.radial                 = click && r.centre;
    return r;
}

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

} // namespace omvr::core::input
