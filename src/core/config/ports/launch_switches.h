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
    // The -vrscale world scale (world_scale_in). Splits the command line as
    // the shell does, so not under the loader lock.
    virtual float world_scale() const noexcept = 0;

protected:
    ~LaunchSwitches() = default;
};

} // namespace omvr::core::config::ports
