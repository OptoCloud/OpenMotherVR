#pragma once

namespace omvr::game::input {

// keyboard.KeyDown(scancode), keyboard.AnyKey() and mouse.Active(), the
// game's own, on the devices its input manager holds.
bool key_down(void* keyboard, int scancode) noexcept;
bool any_key(void* keyboard) noexcept;
bool mouse_active(void* mouse) noexcept;

// Scan codes the stereo update reads.
inline constexpr int kScanLCtrl = 0x1D, kScanLAlt = 0x38;
// Pad codes the stereo update reads.
inline constexpr int kPadLB = 10, kPadRB = 11, kPadRStick = 15;

} // namespace omvr::game::input
