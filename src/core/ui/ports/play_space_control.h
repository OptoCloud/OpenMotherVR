// OpenMotherVR-Provenance: clean
#pragma once

#include <optional>

namespace omvr::core::ui::ports {

// The standing height calibration as the Comfort page shows it (spec 22
// §6.2, §12.2). The integers are what the page is pushed.
enum class CalibrationPageState : int {
    Idle       = 0, // nothing measured this session: the stored height, if any
    Counting   = 1, // the delay before measuring runs
    Waiting    = 2, // waiting for a valid head pose with the headset in use
    Taken      = 3, // measured and stored
    OutOfRange = 4, // measured outside the range: not taken
    NoFloor    = 5, // the runtime has no floor reference
};

struct CalibrationView {
    CalibrationPageState state = CalibrationPageState::Idle;
    int seconds_left           = 0; // Counting: 3, 2, 1
};

// What the v2 options pages reach that is not a setting: the standing height
// calibration (22 §3.3, §6.2) and the -vrscale switch that overrides
// Comfort.WorldScale for the run (22 §6.3). Implemented by the loader over
// the stereo side. Game thread.
class PlaySpaceControl {
public:
    // The calibrate command: measure the standing eye height after the delay.
    virtual void request_height_calibration() noexcept    = 0;
    virtual CalibrationView height_calibration() noexcept = 0;
    // The accepted -vrscale value of this run, none without one.
    virtual std::optional<float> world_scale_switch() const noexcept = 0;

protected:
    ~PlaySpaceControl() = default;
};

} // namespace omvr::core::ui::ports
