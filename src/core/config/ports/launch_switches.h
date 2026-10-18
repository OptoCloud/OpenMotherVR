// OpenMotherVR-Provenance: clean
#pragma once

#include "core/config/settings.h"

#include <optional>

namespace omvr::core::config::ports {

// The game's command-line switches the mod reads, taken from this process
// by the loader (which owns the platform call). The pure parsers are in
// core/config/launch_options.h.
class LaunchSwitches {
public:
    // The runtime a -vrforce* switch picks, or none (forced_runtime_in).
    // Safe under the loader lock: the hook installer decides the patch set
    // with it at attach.
    virtual std::optional<VRRuntime> forced_runtime() const noexcept = 0;
    // The accepted -vrscale value, none without one (world_scale_switch_in):
    // it overrides Comfort.WorldScale for the run (spec 22 §6.3). Splits the
    // command line as the shell does, so not under the loader lock.
    virtual std::optional<float> world_scale_switch() const noexcept = 0;

protected:
    ~LaunchSwitches() = default;
};

} // namespace omvr::core::config::ports
