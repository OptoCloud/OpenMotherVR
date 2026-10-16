// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"

#include <chrono>
#include <optional>

// The standing eye height calibration (spec 22 §6.2): on request, wait
// kCalibrationDelay, then take the head's height above the floor from the
// first valid head pose while the headset is in use; and, in standing mode
// with no calibration, take it at the first recenter in use. A height
// outside [kStandingEyeHeightMin, kStandingEyeHeightMax] is not taken. The
// caller stores a height it returns in Comfort.StandingEyeHeight and saves
// the file. Game thread.
namespace omvr::core::stereo {

enum class CalibrationState {
    Idle,       // nothing measured this session (the page shows the stored height, if any)
    Counting,   // the delay before measuring runs
    Waiting,    // the delay is over; waiting for a valid head pose with the headset in use
    Taken,      // measured; height_m is the new height
    OutOfRange, // measured, outside the range: not taken, the old value stays
    NoFloor,    // the runtime has no floor reference: nothing can be measured
};

// What the options page shows (22 §12.2).
struct CalibrationStatus {
    CalibrationState state = CalibrationState::Idle;
    int seconds_left       = 0;    // Counting: 3, 2, 1
    float height_m         = 0.0f; // Taken or OutOfRange: the height measured
};

// What one stereo update knows about the head (VrRuntime::has_floor,
// headset_in_use, head_height_above_floor).
struct CalibrationReading {
    bool has_floor = false;
    bool in_use    = false;
    std::optional<float> head_height_m; // a valid pose's height above the floor
};

class HeightCalibration final {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    explicit HeightCalibration(diag::ports::Log& log) noexcept : log_(log) {}

    // The calibrate command (22 §3.3): measure after the delay. A request
    // during a countdown starts it again. Game thread.
    void request(TimePoint now) noexcept;

    // Whether a measurement waits (counting down or for a pose): the caller
    // reads the head only then.
    bool pending() const noexcept {
        return state_ == CalibrationState::Counting || state_ == CalibrationState::Waiting;
    }

    // Each stereo update. Returns the height to store when one was taken.
    std::optional<float> update(TimePoint now, const CalibrationReading& reading) noexcept;

    // 22 §6.2: the automatic first calibration, from a recenter taken with
    // the headset in use in standing mode while not calibrated. Returns the
    // height to store when it is in range.
    std::optional<float> take_automatic(float height_m) noexcept;

    CalibrationStatus status(TimePoint now) const noexcept;

private:
    std::optional<float> take(float height_m, bool automatic) noexcept;

    diag::ports::Log& log_;
    CalibrationState state_ = CalibrationState::Idle;
    TimePoint due_{};
    float height_m_ = 0.0f;
};

// A calibrated standing eye height lies in the range (22 §3.1, §6.2).
bool calibration_in_range(float height_m) noexcept;

} // namespace omvr::core::stereo
