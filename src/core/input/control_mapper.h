#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/input/control_scheme.h"
#include "core/input/dpad_emulation.h"
#include "core/input/gestures.h"
#include "core/input/interaction_state.h"
#include "core/input/ports/controls_sink.h"
#include "core/input/ports/stick_context.h"

namespace omvr::core::input {

class VirtualPad;

// What the control mapper writes to and reads. The settings are read on
// every write, so a menu change applies on the next frame (04 §5.6, 22 §3.1).
struct ControlMapperDeps {
    VirtualPad& pad;
    ControlSchemes& schemes;
    const config::InputSettings& input;
    const config::ControlsSettings& controls; // Controls.DpadEmulation (22 §7)
    const ports::StickContext& stick;         // where the left stick is free for the d-pad
    const InteractionState& interactions;     // look Y and weapon cycling (04 §5.8.5)
    const diag::ports::Clock& clock;          // the d-pad modifier's hold
    diag::ports::Log& log;                    // the d-pad's changes, with DebugOptions
    Gestures* gestures = nullptr;             // the body gestures of 22 §8, §9; none: v1
};

// VR controls into the gamepad action tiers (04 §5.8.5).
//
// The tier offsets and the touchpad gestures are the original's (OpenVR
// PollInput, FUN_10007ad0; analysis dumps/06d_vr_input.md and 06b section 3);
// the thumbstick behaviour follows its Oculus Touch scheme (FUN_10004960).
class ControlMapper final : public ports::ControlsSink {
public:
    explicit ControlMapper(const ControlMapperDeps& deps) noexcept : deps_(deps) {}

    // Render thread.
    void write(const Controls& c, bool right_hand_near_head) noexcept override;
    void set_scheme(ControlScheme scheme) noexcept override;
    void identify_controller(std::size_t hand, ControllerSource source,
                             std::string_view id) noexcept override;

private:
    // The d-pad of 22 §7 for this write: the press handed to the pad, and
    // the controls with the stick and click it takes cleared.
    [[nodiscard]] Controls apply_dpad(const Controls& in) noexcept;

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

    ControlMapperDeps deps_;
    GripToggles grips_;
    DpadEmulation dpad_;
    Dpad dpad_logged_{}; // the press last logged (render thread)
};

} // namespace omvr::core::input
