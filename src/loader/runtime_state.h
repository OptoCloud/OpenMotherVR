#pragma once

#include "core/config/ports/launch_switches.h"
#include "core/stereo/stereo_device.h"
#include "core/ui/ports/runtime_state.h"
#include "core/vr/runtime_detector.h"

namespace omvr::loader {

// The options page's view of this run's runtime, over the runtime detector
// and the stereo device. Game thread.
class RuntimeStateView final : public core::ui::ports::RuntimeState {
public:
    RuntimeStateView(core::vr::RuntimeDetector& detector, const core::stereo::StereoDevice& stereo,
                     const core::config::ports::LaunchSwitches& launch) noexcept
        : detector_(detector), stereo_(stereo), launch_(launch) {}

    std::optional<core::config::VRRuntime> forced() const noexcept override;
    bool failed() const noexcept override;
    core::vr::DetectResult::Chosen chosen() noexcept override;
    bool session_active() const noexcept override;

private:
    core::vr::RuntimeDetector& detector_;
    const core::stereo::StereoDevice& stereo_;
    const core::config::ports::LaunchSwitches& launch_;
};

} // namespace omvr::loader
