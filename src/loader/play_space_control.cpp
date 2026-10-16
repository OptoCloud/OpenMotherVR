// OpenMotherVR-Provenance: clean
#include "loader/play_space_control.h"

namespace omvr::loader {

namespace {

using core::stereo::CalibrationState;
using core::ui::ports::CalibrationPageState;

CalibrationPageState page_state(CalibrationState s) noexcept {
    switch (s) {
        case CalibrationState::Idle: return CalibrationPageState::Idle;
        case CalibrationState::Counting: return CalibrationPageState::Counting;
        case CalibrationState::Waiting: return CalibrationPageState::Waiting;
        case CalibrationState::Taken: return CalibrationPageState::Taken;
        case CalibrationState::OutOfRange: return CalibrationPageState::OutOfRange;
        case CalibrationState::NoFloor: return CalibrationPageState::NoFloor;
    }
    return CalibrationPageState::Idle;
}

} // namespace

void PlaySpaceControlView::request_height_calibration() noexcept { calibration_.request(clock_.now()); }

core::ui::ports::CalibrationView PlaySpaceControlView::height_calibration() noexcept {
    const core::stereo::CalibrationStatus s = calibration_.status(clock_.now());
    return {page_state(s.state), s.seconds_left};
}

std::optional<float> PlaySpaceControlView::world_scale_switch() const noexcept {
    return launch_.world_scale_switch();
}

} // namespace omvr::loader
