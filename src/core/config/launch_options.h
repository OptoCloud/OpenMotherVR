#pragma once

#include "core/config/settings.h"

#include <optional>

// The game's command-line switches the mod reads.
namespace omvr::core::config {

// The runtime a -vrforce* switch on the command line picks, or none: an
// exact, case-insensitive argument, as the original matches them; the last
// one wins, and -vrforceoculus selects OpenXR. Probes nothing and loads
// nothing, so it is safe under the loader lock: the hook installer decides
// the patch set with it at attach. The command line does not change, so it
// is read once.
// From: FUN_1001f700.
std::optional<VRRuntime> forced_runtime() noexcept;

// -vrscale <s>: world scale, 1.0 outside [0.495, 1.505], for a value that is
// not a number, or with no value. Arguments are split as the shell does and
// compared whole, case-insensitively; the last -vrscale wins, so a switch
// added at the end of a shortcut overrides one already in it.
// From: FUN_1001f700 (the -vrforce* switches are handled above).
float world_scale_from_command_line() noexcept;

} // namespace omvr::core::config
