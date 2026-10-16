#pragma once

#include "core/config/settings.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

// The game's command-line switches the mod reads. Pure: the caller hands
// the command line in.
namespace omvr::core::config {

// The runtime a -vrforce* switch in `command_line` picks, or none: an exact,
// case-insensitive token, as the original matches them; the last one wins,
// and -vrforceoculus selects OpenXR. The line is split by hand, not with
// CommandLineToArgvW, so this works under the loader lock without shell32.
// From: FUN_1001f700.
std::optional<VRRuntime> forced_runtime_in(std::wstring_view command_line);

// -vrscale <s> in `args` (the command line split as the shell does, program
// name first): world scale, kWorldScaleDefault outside [kWorldScaleMin,
// kWorldScaleMax], for a value that is not a number, or with no value.
// Arguments are compared whole, case-insensitively; the last -vrscale wins,
// so a switch added at the end of a shortcut overrides one already in it.
// From: FUN_1001f700 (the -vrforce* switches are handled above).
float world_scale_in(std::span<const std::wstring> args) noexcept;

// The value of the last -vrscale in `args` when it is accepted (a number in
// [kWorldScaleMin, kWorldScaleMax]); none when there is no -vrscale or the
// last one is not accepted. Spec 22 §6.3: an accepted switch overrides
// Comfort.WorldScale for the run; otherwise the setting applies.
std::optional<float> world_scale_switch_in(std::span<const std::wstring> args) noexcept;

} // namespace omvr::core::config
