#include "core/config/launch_options.h"

#include "platform/win32/command_line.h" // R: config (A4): the loader hands the command line in
#include "util/text/command_line_split.h"
#include "util/text/number_parse.h"

#include <cwchar>
#include <string>
#include <string_view>
#include <vector>

namespace omvr::core::config {

// The command line is split by hand rather than with CommandLineToArgvW, so
// the hook installer can call this under the loader lock without bringing
// in shell32.
std::optional<VRRuntime> forced_runtime() noexcept {
    static const std::optional<VRRuntime> forced = [] {
        struct Switch {
            std::wstring_view name;
            VRRuntime runtime;
        };
        static constexpr Switch kSwitches[] = {
            {L"-vrforcenone",   VRRuntime::None},
            {L"-vrforceopenvr", VRRuntime::OpenVR},
            {L"-vrforceoculus", VRRuntime::OpenXR},
            {L"-vrforceopenxr", VRRuntime::OpenXR},
        };
        std::optional<VRRuntime> chosen;
        for (const std::wstring_view token : util::split_command_line(platform::win32::process_command_line())) {
            for (const auto& s : kSwitches) {
                if (token.size() == s.name.size() &&
                    ::_wcsnicmp(token.data(), s.name.data(), token.size()) == 0) {
                    chosen = s.runtime;
                }
            }
        }
        return chosen;
    }();
    return forced;
}

float world_scale_from_command_line() noexcept {
    const std::vector<std::wstring> argv = platform::win32::process_arguments();
    if (argv.empty()) return 1.0f;
    const auto argc = static_cast<int>(argv.size());
    float scale = 1.0f;
    for (int i = 1; i < argc; ++i) {
        if (::_wcsicmp(argv[i].c_str(), L"-vrscale") != 0) continue;
        const std::optional<float> s = i + 1 < argc ? util::parse_decimal(argv[i + 1].c_str()) : std::nullopt;
        scale = (s && *s >= 0.495f && *s <= 1.505f) ? *s : 1.0f;
    }
    return scale;
}

} // namespace omvr::core::config
