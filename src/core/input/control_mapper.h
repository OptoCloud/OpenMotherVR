#pragma once

#include "core/input/control_scheme.h"
#include "core/input/ports/controls_sink.h"

namespace omvr::core::input {

class VirtualPad;

// VR controls into the gamepad action tiers.
//
// The tier offsets and the touchpad gestures are the original's (OpenVR
// PollInput, FUN_10007ad0; analysis dumps/06d_vr_input.md and 06b section 3);
// the thumbstick behaviour follows its Oculus Touch scheme (FUN_10004960).
class ControlMapper final : public ports::ControlsSink {
public:
    ControlMapper(VirtualPad& pad, ControlSchemes& schemes) noexcept : pad_(pad), schemes_(schemes) {}

    void write(const Controls& c, bool right_hand_near_head) noexcept override;
    void set_scheme(ControlScheme scheme) noexcept override;

private:
    VirtualPad&     pad_;
    ControlSchemes& schemes_;
    // Grip toggles flip on the rising edge. Original: DAT_1006af41 (right),
    // DAT_1006af40 (left). Render thread.
    bool g_prev_grip_r = false, g_prev_grip_l = false;
    bool g_aim_toggled = false, g_tracker_toggled = false;
};

} // namespace omvr::core::input
