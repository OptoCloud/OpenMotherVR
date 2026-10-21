#pragma once

#include "core/input/types.h"
#include "util/math/vec.h"

// The two controller schemes as pure functions of what the runtimes read,
// so OpenVR and OpenXR give a control the same meaning (04 §5.2, §5.3,
// §5.8.3). Render thread in use; pure.
namespace omvr::core::input {

// Where a touchpad is pressed, and which region a click there is (04 §5.1).
// Pure; any thread.
// From: OpenVR PollInput, FUN_10007ad0.
[[nodiscard]] TouchpadRegions decode_touchpad(Vec2 pad, bool click) noexcept;

// 04 §5.2 and §5.8.3: the touchpad scheme (Vive wands, and every
// controller on the OpenVR legacy path). The right pad's click regions come
// from its position whether touched or not; move and turn only while their
// pad is touched.
[[nodiscard]] Controls touchpad_controls(const HandInput& left, const HandInput& right) noexcept;

// 04 §5.3: thumbsticks with face buttons (Touch, Index, WMR and anything
// else).
[[nodiscard]] Controls thumbstick_controls(const HandInput& left, const HandInput& right) noexcept;

// 04 §5.1: an analog grip (squeeze) counts as the grip button.
[[nodiscard]] bool squeeze_pressed(float squeeze) noexcept;

} // namespace omvr::core::input
