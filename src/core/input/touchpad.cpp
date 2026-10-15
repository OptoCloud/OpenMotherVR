#include "core/input/touchpad.h"

#include "core/input/tuning.h"

#include <cmath>

namespace omvr::core::input {

TouchpadRegions decode_touchpad(Vec2 pad, bool click) noexcept {
    TouchpadRegions r;
    r.horizontal = std::fabs(pad.x) > std::fabs(pad.y);
    r.centre     = std::fabs(pad.x) < kPadDeadZone && std::fabs(pad.y) < kPadDeadZone;
    const bool vertical_edge = click && !r.horizontal && !r.centre;
    r.interact = vertical_edge && pad.y < 0.0f;
    r.reload   = vertical_edge && pad.y > 0.0f;
    r.radial   = click && r.centre;
    return r;
}

} // namespace omvr::core::input
