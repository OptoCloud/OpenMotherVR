// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/ports/launch_switches.h"
#include "core/diag/ports/clock.h"
#include "core/stereo/height_calibration.h"
#include "core/ui/ports/play_space_control.h"

namespace omvr::loader {

// The options pages' view of the standing height calibration and the
// -vrscale switch (spec 22 §6.2, §6.3, §12.2), over the stereo side's
// calibration and the command line. Game thread.
class PlaySpaceControlView final : public core::ui::ports::PlaySpaceControl {
public:
    PlaySpaceControlView(core::stereo::HeightCalibration& calibration, const core::diag::ports::Clock& clock,
                         const core::config::ports::LaunchSwitches& launch) noexcept
        : calibration_(calibration), clock_(clock), launch_(launch) {}

    void request_height_calibration() noexcept override;
    core::ui::ports::CalibrationView height_calibration() noexcept override;
    std::optional<float> world_scale_switch() const noexcept override;

private:
    core::stereo::HeightCalibration& calibration_;
    const core::diag::ports::Clock& clock_;
    const core::config::ports::LaunchSwitches& launch_;
};

} // namespace omvr::loader
