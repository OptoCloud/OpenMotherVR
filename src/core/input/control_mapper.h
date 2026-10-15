#pragma once

#include "core/config/settings.h"
#include "core/input/control_scheme.h"
#include "core/input/ports/controls_sink.h"

namespace omvr::core::input {

class VirtualPad;

// VR controls into the gamepad action tiers (04 §5.8.5).
//
// The tier offsets and the touchpad gestures are the original's (OpenVR
// PollInput, FUN_10007ad0; analysis dumps/06d_vr_input.md and 06b section 3);
// the thumbstick behaviour follows its Oculus Touch scheme (FUN_10004960).
class ControlMapper final : public ports::ControlsSink {
public:
    // `settings` is read on every write, so a menu change applies on the
    // next frame (04 §5.6).
    ControlMapper(VirtualPad& pad, ControlSchemes& schemes, const config::InputSettings& settings) noexcept
        : pad_(pad), schemes_(schemes), settings_(settings) {}

    // Render thread.
    void write(const Controls& c, bool right_hand_near_head) noexcept override;
    void set_scheme(ControlScheme scheme) noexcept override;

private:
    // Aim and motion tracker with Input.ToggleGrips (04 §5.6): the edge
    // detectors follow the grips on every write whatever the option; the
    // toggled states start off, flip on a rising edge only while the option
    // is on, and keep their value while it is off. Original: DAT_1006af41
    // (right), DAT_1006af40 (left). Render thread.
    class GripToggles {
    public:
        void update(bool grip_r, bool grip_l, bool toggle) noexcept;
        [[nodiscard]] bool aim() const noexcept { return aim_; }
        [[nodiscard]] bool tracker() const noexcept { return tracker_; }

    private:
        bool prev_grip_r_ = false, prev_grip_l_ = false;
        bool aim_ = false, tracker_ = false;
    };

    VirtualPad&                  pad_;
    ControlSchemes&              schemes_;
    const config::InputSettings& settings_;
    GripToggles                  grips_;
};

} // namespace omvr::core::input
