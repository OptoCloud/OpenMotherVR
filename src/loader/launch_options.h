// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/ports/launch_switches.h"

#include <optional>

namespace omvr::loader {

// This process's command line through the pure parsers of
// core/config/launch_options.h: the loader owns the platform call so core
// stays free of Windows.
class ProcessLaunchSwitches final : public core::config::ports::LaunchSwitches {
public:
    // Reads the -vrforce* switches now: the command line does not change,
    // and the hand splitter probes and loads nothing, so this is safe in
    // DLL_PROCESS_ATTACH.
    ProcessLaunchSwitches() noexcept;

    std::optional<core::config::VRRuntime> forced_runtime() const noexcept override { return forced_; }
    float world_scale() const noexcept override;

private:
    std::optional<core::config::VRRuntime> forced_;
};

} // namespace omvr::loader
