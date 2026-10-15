// OpenMotherVR-Provenance: clean
#include "loader/launch_options.h"

#include "core/config/launch_options.h"
#include "platform/win32/command_line.h"

#include <string>
#include <vector>

namespace omvr::loader {

ProcessLaunchSwitches::ProcessLaunchSwitches() noexcept
    : forced_(core::config::forced_runtime_in(platform::win32::process_command_line())) {}

float ProcessLaunchSwitches::world_scale() const noexcept {
    const std::vector<std::wstring> args = platform::win32::process_arguments();
    return core::config::world_scale_in(args);
}

} // namespace omvr::loader
