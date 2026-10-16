// OpenMotherVR-Provenance: clean
#include "core/diag/crash_state.h"

#include <format>
#include <string_view>

namespace omvr::core::diag {

namespace {

// One event, the same "crash:" level and area as v1's record lines, so a
// search for the record finds it too.
constexpr std::string_view kCrashStateLog =
    "crash: mod state: thread {}, device {} (state {}), stereo mode {}, interaction {}, hiding {}, "
    "menu screen {}, cutscene {}, VR turned off {}";

std::string_view yes_no(bool b) noexcept { return b ? "yes" : "no"; }

} // namespace

std::size_t format_crash_state(std::span<char> out, const CrashState& s) noexcept {
    if (out.empty()) return 0;
    try {
        const auto r = std::format_to_n(out.data(), static_cast<std::ptrdiff_t>(out.size()), kCrashStateLog,
                                        thread_name(s.thread), s.device_attached ? "attached" : "none",
                                        s.device_state, s.stereo_mode, s.interaction, yes_no(s.hiding),
                                        s.screen, yes_no(s.cutscene), yes_no(s.vr_turned_off));
        return static_cast<std::size_t>(r.out - out.data());
    } catch (...) {
        return 0;
    }
}

} // namespace omvr::core::diag
