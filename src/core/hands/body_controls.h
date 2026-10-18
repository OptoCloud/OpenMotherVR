// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/clock.h"
#include "core/input/ports/controls_sink.h"
#include "core/input/types.h"

#include "util/time/steady.h"

// The body features' presses on the game's actions (spec 21 §8.2, §9.2,
// §11.5, §16): real crouch presses the crouch action, which v1 drives from
// the Crouch control, and melee and throwing by motion press the fire
// action, which v1 drives from the Fire control, by adding a press to the
// controls the runtime hands in, merged by OR (20 §5: the player's own
// control keeps working). Sits between the
// runtime and the control mapper; everything else passes through as it
// is.
namespace omvr::core::hands {

class BodyControls final : public input::ports::ControlsSink {
public:
    BodyControls(input::ports::ControlsSink& next, const diag::ports::Clock& clock) noexcept
        : next_(next), clock_(clock) {}

    // Game thread: press the crouch action until `until` (21 §11.5).
    void press_crouch_until(util::TimePoint until) noexcept;
    // Game thread: press the fire action until `until` (21 §8.2, §9.2).
    void press_fire_until(util::TimePoint until) noexcept;

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
    // The end of the press; none before the first. Game thread to render
    // thread (a press seen a frame late is harmless).
    util::AtomicStamp crouch_until_;
    util::AtomicStamp fire_until_;
};

} // namespace omvr::core::hands
