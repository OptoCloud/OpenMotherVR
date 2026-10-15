#pragma once

#include "core/input/types.h"

namespace omvr::core::input {

// Where a touchpad is pressed, and which region a click there is.
// From: OpenVR PollInput, FUN_10007ad0.
TouchpadRegions decode_touchpad(Vec2 pad, bool click) noexcept;

} // namespace omvr::core::input
