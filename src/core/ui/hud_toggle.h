// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"
#include "core/diag/ports/clock.h"
#include "core/diag/ports/log.h"
#include "core/input/gesture_geometry.h"
#include "core/input/ports/controls_sink.h"
#include "core/input/types.h"
#include "core/ui/hud_visibility.h"

#include <chrono>
#include <cstddef>
#include <string_view>

// The HUD toggle gesture (spec 22 §8.3): the left hand within the near-head
// distance (04 §5.10) and a press of Map flips the player's "HUD hidden"
// state. While the left hand is near the head and the gesture is on, a Map
// press drives the toggle and not the map; a press keeps the owner it began
// with until it is released (one owner per input, 20 §5).
namespace omvr::core::ui {

class HudToggleGesture final {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    struct Result {
        bool take_map         = false; // the map does not see this write's Map
        bool fired            = false; // flip the HUD state
        bool suppressed_focus = false; // a press began at the head without input focus (22 §8.1)
    };

    // One controls write: `enabled` is Controls.HudToggleGesture, `c` the
    // write with its poses (22 §8.1), `now` its time. Render thread.
    Result update(bool enabled, const input::Controls& c, TimePoint now) noexcept;

private:
    enum class Owner : unsigned char { None, Map, Toggle };

    input::FocusGate gate_;
    bool prev_map_ = false;
    Owner owner_   = Owner::None;
};

struct HudToggleDeps {
    input::ports::ControlsSink& next; // the control mapper
    const config::ControlsSettings& controls;
    HudVisibility& visibility;
    const diag::ports::Clock& clock;
    diag::ports::Log& log;
};

// The runtimes' controls sink with the HUD toggle in front of the control
// mapper: takes Map from a write while the gesture owns it and hands the
// toggle to the game thread. Everything else passes through. Render
// thread.
class HudToggleSink final : public input::ports::ControlsSink {
public:
    explicit HudToggleSink(const HudToggleDeps& deps) noexcept : deps_(deps) {}

    void write(const input::Controls& c, bool right_hand_near_head) noexcept override;
    void set_scheme(input::ControlScheme scheme) noexcept override { deps_.next.set_scheme(scheme); }
    void identify_controller(std::size_t hand, input::ControllerSource source,
                             std::string_view id) noexcept override {
        deps_.next.identify_controller(hand, source, id);
    }

private:
    HudToggleDeps deps_;
    HudToggleGesture gesture_;
};

} // namespace omvr::core::ui
