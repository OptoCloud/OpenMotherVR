// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/clock.h"
#include "core/input/ports/controls_sink.h"
#include "core/input/types.h"

#include <atomic>
#include <chrono>
#include <cstdint>

// The body features' presses on the game's actions (spec 21 §11.5, §16):
// real crouch presses the crouch action, which v1 drives from the Crouch
// control, by adding a press to the controls the runtime hands in, merged
// by OR (20 §5: the player's own control keeps working). Sits between the
// runtime and the control mapper; everything else passes through as it
// is.
namespace omvr::core::hands {

class BodyControls final : public input::ports::ControlsSink {
public:
    BodyControls(input::ports::ControlsSink& next, const diag::ports::Clock& clock) noexcept
        : next_(next), clock_(clock) {}

    // Game thread: press the crouch action until `until` (21 §11.5).
    void press_crouch_until(std::chrono::steady_clock::time_point until) noexcept;

    // Render thread.
    void write(const input::Controls& c, bool right_hand_near_head) noexcept override;
    void set_scheme(input::ControlScheme scheme) noexcept override { next_.set_scheme(scheme); }
    void identify_controller(std::size_t hand, input::ControllerSource source,
                             std::string_view id) noexcept override {
        next_.identify_controller(hand, source, id);
    }

private:
    input::ports::ControlsSink& next_;
    const diag::ports::Clock& clock_;
    // The end of the press as steady_clock ticks; 0 for none. Game thread
    // to render thread; relaxed: a press seen a frame late is harmless.
    std::atomic<std::int64_t> crouch_until_{0};
};

} // namespace omvr::core::hands
