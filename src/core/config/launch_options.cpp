#include "core/config/launch_options.h"

#include "core/config/tuning.h"
#include "util/text/ascii.h"
#include "util/text/command_line_split.h"
#include "util/text/number_parse.h"

#include <array>

namespace omvr::core::config {

namespace {

constexpr std::wstring_view kWorldScaleSwitch = L"-vrscale";

struct RuntimeSwitch {
    std::wstring_view name;
    VRRuntime runtime;
};

// Oculus headsets run through OpenXR (spec 06 §1.1). Matched whole and
// ignoring ASCII case, whatever the C locale.
constexpr std::array<RuntimeSwitch, 4> kRuntimeSwitches{{
    {L"-vrforcenone", VRRuntime::None},
    {L"-vrforceopenvr", VRRuntime::OpenVR},
    {L"-vrforceoculus", VRRuntime::OpenXR},
    {L"-vrforceopenxr", VRRuntime::OpenXR},
}};

// The runtime `token` forces, if it is a -vrforce* switch.
std::optional<VRRuntime> runtime_switch(std::wstring_view token) noexcept {
    for (const auto& s : kRuntimeSwitches) {
        if (util::equals_ignoring_ascii_case(token, s.name)) return s.runtime;
    }
    return std::nullopt;
}

bool in_world_scale_range(float s) noexcept { return s >= kWorldScaleMin && s <= kWorldScaleMax; }

} // namespace

std::optional<VRRuntime> forced_runtime_in(std::wstring_view command_line) {
    std::optional<VRRuntime> chosen;
    for (const std::wstring_view token : util::split_command_line(command_line)) {
        if (const auto r = runtime_switch(token)) chosen = r;
    }
    return chosen;
}

std::optional<float> world_scale_switch_in(std::span<const std::wstring> args) noexcept {
    std::optional<float> scale;
    // args[0] is the program.
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (!util::equals_ignoring_ascii_case(std::wstring_view{args[i]}, kWorldScaleSwitch)) continue;
        const std::optional<float> s =
            i + 1 < args.size() ? util::parse_decimal(args[i + 1].c_str()) : std::nullopt;
        scale = s && in_world_scale_range(*s) ? s : std::nullopt;
    }
    return scale;
}

float world_scale_in(std::span<const std::wstring> args) noexcept {
    return world_scale_switch_in(args).value_or(kWorldScaleDefault);
}

} // namespace omvr::core::config
