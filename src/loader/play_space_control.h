// OpenMotherVR-Provenance: clean
#pragma once

#include "core/diag/ports/log.h"
#include "core/ui/ports/play_space_control.h"

namespace omvr::loader {

// The options pages' view of the standing height calibration and the
// -vrscale switch (spec 22 §6.2, §6.3, §12.2). Game thread.
//
// TODO: this build has neither the standing height calibration nor the
// -vrscale override of Comfort.WorldScale, so the calibrate command is
// logged and does nothing and the page shows no calibration and no switch.
// Wire it to core/stereo's HeightCalibration and to
// LaunchSwitches::world_scale_switch once they are in.
class PlaySpaceControlView final : public core::ui::ports::PlaySpaceControl {
public:
    explicit PlaySpaceControlView(core::diag::ports::Log& log) noexcept : log_(log) {}

    void request_height_calibration() noexcept override;
    core::ui::ports::CalibrationView height_calibration() noexcept override { return {}; }
    std::optional<float> world_scale_switch() const noexcept override { return std::nullopt; }

private:
    core::diag::ports::Log& log_;
};

} // namespace omvr::loader
